// FFmpegHelper.cpp : 定义 DLL 的导出函数。
//
#include "pch.h"

#include "FFmpegHelperCore.h"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avdevice.lib")
#else
#include <unistd.h>
#endif // _WIN32



FFmpegHelperCore::FFmpegHelperCore()
{
	memset(m_strURLorFileName, 0, FH_NAME_MAX_LEN);
	m_nVideoIndex = -1;
	m_pBuffer = NULL;

	m_avFormatCtx = NULL;
	m_avCodec = NULL;
	m_avCodecCtx = NULL;
	m_avSwsCtx = NULL;
	m_dstFormat = AV_PIX_FMT_NONE;

	m_avFrameDecodec = NULL;
	m_avFrameRGB = NULL;
	m_avPacket = NULL;

	m_VideoH = -1;
	m_VideoW = -1;
	bStartDecodec = false;
	bExitThread = false;
	m_fFps = 25.0;

	tDecodec = std::thread(&FFmpegHelperCore::DecdecThread, this);

}

FFmpegHelperCore::~FFmpegHelperCore()
{
	// 通知解码线程退出并等待其结束后，再统一释放资源
	{
		std::lock_guard<std::mutex> lock(condition_mutex);
		bStartDecodec = false;
		bExitThread = true;
	}
	condition.notify_all();
	if (tDecodec.joinable())
	{
		tDecodec.join();
	}

	if (m_avPacket) av_packet_free(&m_avPacket);
	if (m_avFrameDecodec) av_frame_free(&m_avFrameDecodec);
	if (m_avFrameRGB) av_frame_free(&m_avFrameRGB);
	if (m_pBuffer) av_freep(&m_pBuffer);
	if (m_avSwsCtx) sws_freeContext(m_avSwsCtx);
	if (m_avCodecCtx) avcodec_free_context(&m_avCodecCtx);
	if (m_avFormatCtx) avformat_close_input(&m_avFormatCtx);
}

bool FFmpegHelperCore::SetURLOrFileName(char * pUrl)
{
	if (strlen(pUrl) >= FH_NAME_MAX_LEN) {
		return false;
	}
	memset(m_strURLorFileName, 0, FH_NAME_MAX_LEN);
	memcpy(m_strURLorFileName, pUrl, strlen(pUrl) + 1);
#ifdef _DEBUG
	printf("URL = %s\n", m_strURLorFileName);
#endif // DEBUG
	return true;
}

int FFmpegHelperCore::InitFFmpeg()
{
	avformat_network_init();
	unsigned int version = avformat_version();

	std::cout << "ffmpeg version : " << version << std::endl;
    // 打开码流前指定各种参数
	AVDictionary *optionsDict = nullptr;
	av_dict_set(&optionsDict, "buffer_size", "1024000", 0);
    //av_dict_set(&optionsDict, "rtsp_transport", "tcp", 0);
	av_dict_set(&optionsDict, "rtsp_transport", "udp", 0);
	//av_dict_set(&optionsDict, "timeout", "5000000", 0);// 设置超时，否则在avformat_open_input会一直阻塞
	av_dict_set(&optionsDict, "stimeout", "3000000", 0);	// 最多阻塞3秒


	m_avFormatCtx = avformat_alloc_context();
	if (0 != avformat_open_input(&m_avFormatCtx, m_strURLorFileName,NULL, &optionsDict))
	{
		std::cout << "Open input error! url:" << m_strURLorFileName << std::endl;
		return 1;
	}
	if(avformat_find_stream_info(m_avFormatCtx, NULL) < 0)
	{
		std::cout << "Find stream info error!" << std::endl;
		return 2;
	}

	for (unsigned int i = 0; i < m_avFormatCtx->nb_streams; i++)
	{
		if (m_avFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_nVideoIndex = i;
			break;
		}
	}
	if (-1 == m_nVideoIndex)
	{
		std::cout << "can't find a video stream." << std::endl;
		return 3;
	}

	AVStream* st = m_avFormatCtx->streams[m_nVideoIndex];
	if (st->avg_frame_rate.num && st->avg_frame_rate.den)
	{
		m_fFps = av_q2d(st->avg_frame_rate);
	}
	else if (st->r_frame_rate.num && st->r_frame_rate.den)
	{
		m_fFps = av_q2d(st->r_frame_rate);
	}

	//寻找一个匹配当前视频流的解码器
	m_avCodecCtx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(m_avCodecCtx, m_avFormatCtx->streams[m_nVideoIndex]->codecpar);
	m_avCodec = avcodec_find_decoder(m_avCodecCtx->codec_id);
	if (m_avCodec == NULL)
	{
		std::cout << "Codec not find.";
		return 4;
	}
	//设置加速解码
	m_avCodecCtx->lowres = m_avCodec->max_lowres;
	m_avCodecCtx->flags2 |= AV_CODEC_FLAG2_FAST;

	//打开解码器
	if (avcodec_open2(m_avCodecCtx, m_avCodec, NULL) < 0)//为啥不设option
	{
		std::cout <<"Codec open failed.";
		return 5;
	}

    m_VideoH = m_avFormatCtx->streams[m_nVideoIndex]->codecpar->height;
    m_VideoW = m_avFormatCtx->streams[m_nVideoIndex]->codecpar->width;

	// 预分配好内存（m_avFrameDecodec的缓冲由解码器自行分配，m_avFrameRGB的缓冲在解码线程中通过av_image_alloc分配）
	m_avPacket = av_packet_alloc();
	m_avFrameDecodec = av_frame_alloc();
	m_avFrameRGB = av_frame_alloc();
	return 0;
}

int FFmpegHelperCore::StartDecode()
{
	return 0;
}

void FFmpegHelperCore::SetCallBack(FFmpegInterface * p)
{
	CallBackInterface = p;
}

void FFmpegHelperCore::StartDecodec()
{

	{
		std::lock_guard<std::mutex> lock(condition_mutex);
		bStartDecodec = true;
	}
	condition.notify_all();
}

void FFmpegHelperCore::DecdecThread()
{
	std::unique_lock<std::mutex> lk(condition_mutex);
	// 用谓词等待“开始解码”或“线程退出”指令，避免错过notify而永久阻塞
	condition.wait(lk, [this] { return bStartDecodec || bExitThread; });
	if (bExitThread || !bStartDecodec)
	{
		return;
	}

	// 解码后统一转成RGB32交给上层显示
	m_dstFormat = AV_PIX_FMT_RGB32;
	m_avSwsCtx = sws_getContext(m_avCodecCtx->width, m_avCodecCtx->height, m_avCodecCtx->pix_fmt,
		m_VideoW, m_VideoH, m_dstFormat,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);
	av_image_alloc(m_avFrameRGB->data, m_avFrameRGB->linesize, m_avCodecCtx->width, m_avCodecCtx->height, m_dstFormat, 1);
	lk.unlock();

	if (m_avSwsCtx == NULL)
	{
		std::cout << "sws_getContext failed." << std::endl;
		bStartDecodec = false;
		return;
	}

	// 播放时间轴基准：第n帧应在 tStart + (pts_n - pts_0) 时刻送到回调。
	// 之前仅靠固定延时（Sleep(1)+固定帧间隔）控制节奏，Windows默认定时器精度约15.6ms，
	// 实际帧间隔忽长忽短，表现为画面“一卡一卡”。
	double dStartPts = -1.0;
	double dLastPts = -1.0;
	std::chrono::steady_clock::time_point tStart = std::chrono::steady_clock::now();
	AVRational tb = m_avFormatCtx->streams[m_nVideoIndex]->time_base;

	while (bStartDecodec)
	{
		int nRet = av_read_frame(m_avFormatCtx, m_avPacket);
		if (nRet < 0)
		{
			// 读取失败或播放结束：清空解码器缓存并seek回开头循环播放；直播流seek失败则稍等重试
			avcodec_flush_buffers(m_avCodecCtx);
			if (av_seek_frame(m_avFormatCtx, m_nVideoIndex, 0, AVSEEK_FLAG_BACKWARD) >= 0)
			{
				// 循环播放：重置时间轴基准，下一帧重新计时
				dStartPts = -1.0;
				tStart = std::chrono::steady_clock::now();
			}
			else
			{
#ifdef _WIN32
				Sleep(10);
#else
				usleep(10000);
#endif
			}
			continue;
		}

		bool bDisplayed = false;
		if (m_avPacket->stream_index == m_nVideoIndex)
		{
			// 将AVPacket中的数据解码至m_avFrameDecodec
			if (avcodec_send_packet(m_avCodecCtx, m_avPacket) >= 0)
			{
				int nGotPicture = avcodec_receive_frame(m_avCodecCtx, m_avFrameDecodec);
				// nGotPicture为0表示成功拿到一帧
				if (0 == nGotPicture)
				{
					//std::cout << "[AVPacket]frame flags: " << m_avPacket->flags << " pts: " << m_avPacket->pts << " dts: " << m_avPacket->dts << std::endl;
					//std::cout << "[AVFrame]frame type: " << m_avFrameDecodec->pict_type << " pts: " << m_avFrameDecodec->pts <<  std::endl << std::endl;
					sws_scale(m_avSwsCtx, (const uint8_t* const*)m_avFrameDecodec->data, m_avFrameDecodec->linesize, 0,
						m_avCodecCtx->height, m_avFrameRGB->data, m_avFrameRGB->linesize);

					// 按帧PTS调度显示时刻：等到“起始时刻 + 该帧相对首帧的pts偏移”再回调。
					// 已落后于时间轴时不等待（丢帧策略），避免延迟越积越多。
					double dPts = -1.0;
					if (m_avFrameDecodec->pts != AV_NOPTS_VALUE && tb.num > 0)
					{
						dPts = m_avFrameDecodec->pts * av_q2d(tb);
					}
					if (dStartPts < 0.0 && dPts >= 0.0)
					{
						dStartPts = dPts;	// 第一帧作为时间轴零点
					}
					if (dPts >= 0.0 && dStartPts >= 0.0)
					{
						auto tDue = tStart + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
							std::chrono::duration<double>(dPts - dStartPts));
						//先粗睡再让出CPU自旋，兼顾CPU占用与定时精度
						while (std::chrono::steady_clock::now() < tDue)
						{
							if (tDue - std::chrono::steady_clock::now() > std::chrono::milliseconds(2))
							{
								std::this_thread::sleep_for(std::chrono::milliseconds(1));
							}
							else
							{
								std::this_thread::yield();
							}
						}
					}

					// 调用回调，把RGB32数据交给上层显示
					if (CallBackInterface)
					{
						CallBackInterface->FFmpegGetRGBFrame((char*)m_avFrameRGB->data[0], m_VideoH, m_VideoW);
					}
					bDisplayed = true;
				}
			}
		}
		av_packet_unref(m_avPacket);
		if (!bDisplayed)
		{
			// 非视频显示帧（音频包/解码失败等）稍微让出CPU，节奏由PTS调度控制
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
		}
	}

	sws_freeContext(m_avSwsCtx);
	m_avSwsCtx = NULL;
	bStartDecodec = false;
}

double FFmpegHelperCore::GetFrameRate()
{
	return m_fFps;
}

void FFmpegInterface::FFmpegGetDecodecFrame(char * pData, int nHeight, int nWidth)
{
	char* p = pData;
}

void FFmpegInterface::FFmpegGetRGBFrame(char * pData, int nHeight, int nWidth)
{
	return;
}

FFMPEGHELPER_API FFmpegHelper * CreateFFmpegHelper()
{
	return (FFmpegHelper *)new FFmpegHelperCore();
}
