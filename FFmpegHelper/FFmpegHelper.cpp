// FFmpegHelper.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
#include "framework.h"
#include "FFmpegHelper.h"
#include "iostream"
#include <thread>

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avdevice.lib")

// 这是导出变量的一个示例
FFMPEGHELPER_API int nFFmpegHelper=0;

// 这是导出函数的一个示例。
FFMPEGHELPER_API int fnFFmpegHelper(void)
{
    return 0;
}

// 这是已导出类的构造函数。
FFmpegHelper::FFmpegHelper()
{
	memset(m_strURLorFileName, 0, FH_NAME_MAX_LEN);
	m_nVideoIndex = -1;
	m_pBuffer;

	m_avFormatCtx = NULL;
	m_avCodec = NULL;
	m_avCodec = NULL;
	m_avSwsCtx = NULL;
	m_dstFormat = AV_PIX_FMT_NONE;

	m_avFrameDecodec = NULL;
	m_avFrameRGB = NULL;
	m_avPacket = NULL;

	m_VideoH = -1;
	m_VideoW = -1;

    return;
}

bool FFmpegHelper::SetURLOrFileName(char * pUrl)
{
	if (sizeof(pUrl) > FH_NAME_MAX_LEN) {
		return false;
	}
	memset(m_strURLorFileName, 0, FH_NAME_MAX_LEN);
	memcpy(m_strURLorFileName, pUrl, strlen(pUrl) + 1);
#ifdef _DEBUG
	printf("URL = %s", m_strURLorFileName);
#endif // DEBUG
	return true;
}

int FFmpegHelper::InitFFmpeg()
{
	avformat_network_init();

    // 打开码流前指定各种参数
	AVDictionary *optionsDict = nullptr;
	av_dict_set(&optionsDict, "buffer_size", "1024000", 0);
    av_dict_set(&optionsDict, "rtsp_transport", "tcp", 0);
	av_dict_set(&optionsDict, "stimeout", "2000000", 0);

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
		std::cout << "can't find a video stream.";
		return 3;
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

	// 预分配好内存
	m_avPacket = av_packet_alloc();
	m_avFrameDecodec = av_frame_alloc();
	m_avFrameRGB = av_frame_alloc();
    m_pBuffer = (uint8_t*)av_malloc(av_image_get_buffer_size(m_dstFormat, m_avCodecCtx->width, m_avCodecCtx->height, 1) * sizeof(uint8_t));
	
    // 缓存一帧数据
	av_image_fill_arrays(m_avFrameDecodec->data, m_avFrameDecodec->linesize, m_pBuffer, m_dstFormat, m_avCodecCtx->width, m_avCodecCtx->height, 1);
	return 0;
}

void FFmpegHelper::SetCallBack(FFmpegInterface * p)
{
	CallBackInterface = p;
}

void FFmpegHelper::StartDecodec()
{
	std::thread tDecodec(&FFmpegHelper::DecdecThread, this);
	//tDecodec.join();
	tDecodec.detach();
}

void FFmpegHelper::DecdecThread()
{
	m_dstFormat = AV_PIX_FMT_RGB32;
	m_avSwsCtx = sws_getContext(m_avCodecCtx->width, m_avCodecCtx->height, m_avCodecCtx->pix_fmt,
        m_VideoW, m_VideoH, m_dstFormat,
        SWS_FAST_BILINEAR, NULL, NULL, NULL);
	av_image_alloc(m_avFrameRGB->data, m_avFrameRGB->linesize, m_avCodecCtx->width, m_avCodecCtx->height, m_dstFormat, 1);

	while (bStartDecodec) 
	{
		int nGotPicture = 0;
        int nRet = av_read_frame(m_avFormatCtx, m_avPacket);
        if (nRet >= 0)
        {
            if (m_avPacket->stream_index == m_nVideoIndex)
            {
                // 将AVPacket中的数据解码至pFrame
                nRet = avcodec_send_packet(m_avCodecCtx, m_avPacket);
                nGotPicture = avcodec_receive_frame(m_avCodecCtx, m_avFrameDecodec);
                if (nRet < 0)
                {
                    //std::cout << "Decode error.";
                    continue;
                }

                if (nGotPicture < 0)
                {
                    //std::cout << "Not get image.";
                    continue;
                }

                // nGotPicture为0表示成功
                if (!nGotPicture)
                {
                    sws_scale(m_avSwsCtx, (const uint8_t* const*)m_avFrameDecodec->data, m_avFrameDecodec->linesize, 0,
                        m_avCodecCtx->height, m_avFrameRGB->data, m_avFrameRGB->linesize);

                    // 调用回调
					//CallBackInterface.FFmpegGetDecodecFrame((char*)m_avFrameDecodec->data[0], m_avCodecCtx->height, m_avCodecCtx->width);
					CallBackInterface->FFmpegGetRGBFrame((char*)m_avFrameRGB->data[0], m_avCodecCtx->height, m_avCodecCtx->width);
                    //Sleep(1);
                }
            }
        }
        av_packet_unref(m_avPacket);
        av_freep(m_avPacket);
        Sleep(1);
	}
	sws_freeContext(m_avSwsCtx);
	bStartDecodec = false;
}

void FFmpegInterface::FFmpegGetDecodecFrame(char * pData, int nHeight, int nWidth)
{
	char* p = pData;
}

void FFmpegInterface::FFmpegGetRGBFrame(char * pData, int nHeight, int nWidth)
{
	return;
}
