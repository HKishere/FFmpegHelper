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
	char m_strURLorFileName[FH_NAME_MAX_LEN];	//��ƵURL���ļ���
	int					m_nVideoIndex;		//��Ƶ����������
	uint8_t *			m_pBuffer;			//���洢������ͼ��

	AVFormatContext*	m_avFormatCtx;		//��װ�����ø�ʽ������
	const AVCodec*		m_avCodec;			//������	
	AVCodecContext*		m_avCodecCtx;		//������	������
	SwsContext*			m_avSwsCtx;			//ͼ��ת��������
	AVPixelFormat		m_dstFormat;		//ת��Ŀ�����ظ�ʽ

	AVFrame*			m_avFrameDecodec;	//����ͼ��YUV
	AVFrame*			m_avFrameRGB;		//ת����RGBͼ��
	AVPacket*			m_avPacket;			//��Ƶ���ݰ�

	int					m_VideoH;			//��Ƶ��
	int					m_VideoW;			//��Ƶ��

	bool				bStartDecodec;		//���뿪ʼ��־

	FFmpegInterface*		CallBackInterface;	//�ص��ӿ�
	std::thread tDecodec;					//�����߳�
	std::condition_variable condition;		//�������������������߳���������ʵ���Բ��ã�������ʱ��û������õĽṹ���������ţ�
	std::mutex condition_mutex;
};
