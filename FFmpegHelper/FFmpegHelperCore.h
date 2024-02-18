#include "FFmpegHelper.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
};

class FFmpegHelperCore : public FFmpegHelperCore{
public:
	FFmpegHelperCore(void);
	~FFmpegHelperCore();

	bool SetURLOrFileName(char* pUrl);
	int InitFFmpeg();

	int StartDecode();

	void SetCallBack(FFmpegInterface* p);

	void StartDecodec();

	void DecdecThread();


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

	FFmpegInterface*		CallBackInterface;	//回调接口
	std::thread tDecodec;					//解码线程
	std::condition_variable condition;		//条件变量，用来控制线程启动（其实可以不用，但是暂时还没想出来好的结构，就先用着）
	std::mutex condition_mutex;
};
