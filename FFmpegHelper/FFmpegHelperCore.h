#include "FFmpegHelper.h"
#include <stack>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
};

class FFmpegHelperCore : public FFmpegHelper{
public:
	FFmpegHelperCore(void);
	~FFmpegHelperCore();

	bool SetURLOrFileName(char* pUrl);
	int InitFFmpeg();

	int StartDecode();

	void SetCallBack(FFmpegInterface* p);

	void StartDecodec();

	void DecdecThread();

	double GetFrameRate();



private:
	char m_strURLorFileName[FH_NAME_MAX_LEN];	//视频URL或文件名
	int					m_nVideoIndex;		//视频所在流索引
	uint8_t *			m_pBuffer;			//缓存储解码后的图像

	AVFormatContext*	m_avFormatCtx;		//封装、复用格式上下文
	const AVCodec*		m_avCodec;			//解码器
	AVCodecContext*		m_avCodecCtx;		//解码器	上下文
	SwsContext*			m_avSwsCtx;			//图像转换上下文
	AVPixelFormat		m_dstFormat;		//转换目标像素格式

	AVFrame*			m_avFrameDecodec;	//解码图像YUV
	AVFrame*			m_avFrameRGB;		//转换后RGB图像
	AVPacket*			m_avPacket;			//视频数据包

	int					m_VideoH;			//视频高
	int					m_VideoW;			//视频宽

	bool				bStartDecodec;		//解码开始标志
	bool				bExitThread;		//解码线程退出标志（保证未StartDecodec时也能正常析构）
	double				m_fFps;				//帧率，从av_guess_frame_rate中获取，它内部会按 avg_frame_rate -> r_frame_rate -> codecpar 的顺序自动选一个最可信的值

	FFmpegInterface*	CallBackInterface;	//回调接口
	std::thread tDecodec;					//解码线程
	std::condition_variable condition;		//条件变量，用来控制线程启动（其实可以不用，但是暂时还没想出来好的结构，就先用着）
	std::mutex condition_mutex;
	std::stack<AVFrame> m_DisplayFrameStack;//显示堆栈，用来重新排列p、b帧
};
