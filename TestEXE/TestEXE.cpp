// TestEXE : 从FFmpegHelper获取解码帧并通过SDL2显示
//
// 数据流:
//   [FFmpegHelper解码线程] av_read_frame -> avcodec解码 -> sws转RGB32
//        -> 按帧PTS调度显示时刻 -> FFmpegGetRGBFrame回调 -> TestEXE缓存最新一帧并推送REFRESH_EVENT
//   [TestEXE主线程] SDL_WaitEvent收到REFRESH_EVENT
//        -> 锁内直接把共享帧缓冲上传纹理 -> SDL_RenderCopy -> SDL_RenderPresent
#include <stdio.h>
#include <string.h>
#include <mutex>
#include <vector>
#include "../FFmpegHelper/FFmpegHelper.h"

extern "C"
{
#define SDL_MAIN_HANDLED	// console app owns its entry, avoid SDL main redefine
#include "SDL.h"
};

#pragma comment(lib, "FFmpegHelper.lib")
#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "winmm.lib")	// timeBeginPeriod

// 自定义事件：通知主线程刷新画面
#define REFRESH_EVENT  (SDL_USEREVENT + 1)

// 解码线程与主线程之间共享的“最新一帧”RGB32数据
static std::mutex g_frameMutex;
static std::vector<unsigned char> g_frameBuf;
static int g_frameW = 0;
static int g_frameH = 0;
static bool g_newFrame = false;
static long g_cbFrameCount = 0;

class DoCB : public FFmpegInterface
{
public:
	bool bFirstFrame = true;

	// YUV解码帧回调：本例直接显示RGB帧，此处留空
	virtual void FFmpegGetDecodecFrame(char* pData, int nHeight, int nWidth) override {}

	// RGB32帧回调（在FFmpegHelper的解码线程中被调用，DLL内部已按PTS控制好节奏）
	virtual void FFmpegGetRGBFrame(char* pData, int nHeight, int nWidth) override
	{
		size_t nSize = (size_t)nWidth * nHeight * 4;	// RGB32每像素4字节
		{
			std::lock_guard<std::mutex> lock(g_frameMutex);
			g_frameBuf.resize(nSize);
			memcpy(g_frameBuf.data(), pData, nSize);
			g_frameW = nWidth;
			g_frameH = nHeight;
			g_newFrame = true;
		}

		g_cbFrameCount++;
		if (bFirstFrame) {
			bFirstFrame = false;
			printf("callback: get first decoded frame %d x %d\n", nWidth, nHeight);
		}
		if (g_cbFrameCount % 100 == 0) {
			printf("callback: %ld frames decoded\n", g_cbFrameCount);
		}

		// 通知主线程刷新
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
	}
};

int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);	// 输出不缓冲，便于重定向时观察日志
	const char* url = (argc > 1) ? argv[1] : "202403262221.mp4";


	SDL_SetMainReady();
	if (SDL_Init(SDL_INIT_VIDEO)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	// [1] 创建并初始化FFmpegHelper
	FFmpegHelper* pHelper = CreateFFmpegHelper();
	if (pHelper == NULL) {
		printf("CreateFFmpegHelper failed\n");
		SDL_Quit();
		return -1;
	}
	if (!pHelper->SetURLOrFileName(const_cast<char*>(url))) {
		printf("URL too long: %s\n", url);
		delete pHelper;
		SDL_Quit();
		return -1;
	}
	int ret = pHelper->InitFFmpeg();
	if (ret != 0) {
		printf("InitFFmpeg error: %d, url: %s\n", ret, url);
		delete pHelper;
		SDL_Quit();
		return -1;
	}

	printf("url: %s, fps: %.2f\n", url, pHelper->GetFrameRate());

	DoCB cb;

	// [2] 创建SDL窗口与渲染器（纹理等拿到第一帧尺寸后再创建）
	int screen_w = 640, screen_h = 480;
	SDL_Window* screen = SDL_CreateWindow("FFmpegHelper + SDL2 Player",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_RESIZABLE);
	if (!screen) {
		printf("SDL: could not create window - %s\n", SDL_GetError());
		delete pHelper;
		SDL_Quit();
		return -1;
	}
	SDL_Renderer* sdlRenderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!sdlRenderer) {
		sdlRenderer = SDL_CreateRenderer(screen, -1, 0);	// 硬件渲染不可用则退回软件渲染
	}
	if (!sdlRenderer) {
		printf("SDL: could not create renderer - %s\n", SDL_GetError());
		delete pHelper;
		SDL_DestroyWindow(screen);
		SDL_Quit();
		return -1;
	}
	SDL_RendererInfo ri;
	if (SDL_GetRendererInfo(sdlRenderer, &ri) == 0) {
		printf("renderer: %s\n", ri.name);
	}
	SDL_Texture* sdlTexture = NULL;
	int texW = 0, texH = 0;

	// [3] 注册回调并启动解码
	pHelper->SetCallBack(&cb);
	pHelper->StartDecodec();
	printf("decode started, waiting frames...\n");

	// [4] 主线程事件循环：收到REFRESH_EVENT就取最新帧并渲染
	SDL_Event event;
	SDL_Rect sdlRect;
	long nRenderCount = 0;
	bool bQuit = false;
	while (!bQuit) {
		SDL_WaitEvent(&event);

		if (event.type == REFRESH_EVENT) {
			bool bHasFrame = false;
			{
				std::lock_guard<std::mutex> lock(g_frameMutex);
				if (g_newFrame) {
					// 首帧或视频尺寸变化时（重）建纹理
					if (!sdlTexture || texW != g_frameW || texH != g_frameH) {
						if (sdlTexture) {
							SDL_DestroyTexture(sdlTexture);
						}
						// FFmpeg的RGB32在内存中的字节序与SDL_PIXELFORMAT_ARGB8888匹配
						sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
							SDL_TEXTUREACCESS_STREAMING, g_frameW, g_frameH);
						if (!sdlTexture) {
							printf("SDL_CreateTexture failed - %s\n", SDL_GetError());
						}
						else {
							texW = g_frameW;
							texH = g_frameH;
							printf("create texture: %d x %d\n", texW, texH);
						}
					}
					// 锁内直接上传纹理，避免4K大帧再拷贝一份（回调线程短暂等待即可）
					if (sdlTexture && SDL_UpdateTexture(sdlTexture, NULL, g_frameBuf.data(), g_frameW * 4) != 0) {
						printf("SDL_UpdateTexture failed - %s\n", SDL_GetError());
					}
					g_newFrame = false;
					bHasFrame = (sdlTexture != NULL);
				}
			}
			if (!bHasFrame) {
				continue;
			}

			// 拉伸铺满窗口（窗口缩放时跟随）
			SDL_GetWindowSize(screen, &screen_w, &screen_h);
			sdlRect.x = 0;
			sdlRect.y = 0;
			sdlRect.w = screen_w;
			sdlRect.h = screen_h;

			SDL_RenderClear(sdlRenderer);
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
			SDL_RenderPresent(sdlRenderer);

			nRenderCount++;
			if (nRenderCount % 100 == 0) {
				printf("render: %ld frames presented\n", nRenderCount);
			}
		}
		else if (event.type == SDL_QUIT) {
			bQuit = true;
		}
	}

	// [5] 释放：先停解码线程，再销毁SDL资源
	delete pHelper;
	if (sdlTexture) SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(screen);
	SDL_Quit();
	printf("exit, rendered %ld frames, decoded %ld frames\n", nRenderCount, g_cbFrameCount);
	return 0;
}
