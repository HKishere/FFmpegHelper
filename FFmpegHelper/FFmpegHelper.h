// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 FFMPEGHELPER_EXPORTS
// 符号编译的。在使用此 DLL 的
#pragma once
#ifdef FFMPEGHELPER_EXPORTS
#define FFMPEGHELPER_API __declspec(dllexport)
#else
#define FFMPEGHELPER_API __declspec(dllimport)
#endif
// 任何项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// FFMPEGHELPER_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。


#include <thread>
#include <condition_variable>

#define FH_NAME_MAX_LEN 256

struct FFMPEGHELPER_API FFmpegInterface {
	virtual void FFmpegGetDecodecFrame(char* pData, int nHeight, int nWidth);
	virtual void FFmpegGetRGBFrame(char* pData, int nHeight, int nWidth);
};

// 此类是从 dll 导出的
class FFMPEGHELPER_API FFmpegHelper {
public:

	virtual bool SetURLOrFileName(char* pUrl) = 0;

	virtual int InitFFmpeg() = 0;
	 
	virtual int StartDecode() = 0;

	virtual void SetCallBack(FFmpegInterface* p) = 0;

	virtual void StartDecodec() = 0;

	virtual void DecdecThread() = 0;
};

FFMPEGHELPER_API FFmpegHelper* CreateFFmpegHelper();
