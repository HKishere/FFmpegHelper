// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 FFMPEGHELPER_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// FFMPEGHELPER_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef FFMPEGHELPER_EXPORTS
#define FFMPEGHELPER_API __declspec(dllexport)
#else
#define FFMPEGHELPER_API __declspec(dllimport)
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}


#define FH_NAME_MAX_LEN 256

struct FFMPEGHELPER_API FFmpegInterface {
	virtual void FFmpegGetDecodecFrame(char* pData, int nHeight, int nWidth);
	virtual void FFmpegGetRGBFrame(char* pData, int nHeight, int nWidth);
};

// 此类是从 dll 导出的
class FFMPEGHELPER_API FFmpegHelper {
public:
	FFmpegHelper(void);
	
	bool SetURLOrFileName(char* pUrl);
	int InitFFmpeg();

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
};

extern FFMPEGHELPER_API int nFFmpegHelper;

FFMPEGHELPER_API int fnFFmpegHelper(void);
