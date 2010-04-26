// USBCamera.cpp : 定义控制台应用程序的入口点。
//

//#include "stdafx.h"
#include "cv.h"
#include "highgui.h"
#include "convert.h"
#include "ICameraCaptuer.h"

#ifdef _DEBUG
    #pragma comment(lib,"cv200d.lib")
    #pragma comment(lib,"cvaux200d.lib")
    #pragma comment(lib,"cxcore200d.lib")
    #pragma comment(lib,"cxts200d.lib")
    #pragma comment(lib,"highgui200d.lib")
    #pragma comment(lib,"ml200d.lib")
#else
    #pragma comment(lib,"cv200.lib")
    #pragma comment(lib,"cvaux200.lib")
    #pragma comment(lib,"cxcore200.lib")
    #pragma comment(lib,"cxts200.lib")
    #pragma comment(lib,"highgui200.lib")
    #pragma comment(lib,"ml200.lib")
#endif

#define VIDEO_WIDTH  640
#define VIDEO_HEIGHT 480

//#define _OUTPUT_YUV

bool CaptureFromCamera();

//可去掉控制台窗口
//#pragma comment( linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"" )

int main(int argc, char* argv[])
{
    return CaptureFromCamera();
}

//从摄像机中获取视频,采用DirectShow的方式
bool CaptureFromCamera()
{
    RGBYUVConvert::InitConvertTable();

    ICameraCaptuer* pCamera = CamCaptuerMgr::GetCamCaptuer();
    if(NULL == pCamera || !pCamera->OpenCamera(0, VIDEO_WIDTH,VIDEO_HEIGHT)) 
    {
        printf("Can not open camera.");
        return false;
    }


#if defined(_OUTPUT_YUV)
    FILE* fyuv = fopen("TestCamera.yuv", "wb");
#endif
    int rgbsize = VIDEO_WIDTH*VIDEO_HEIGHT*3;

    cvNamedWindow("camera");
    IplImage* pFrame = cvCreateImage(cvSize(VIDEO_WIDTH, VIDEO_HEIGHT), IPL_DEPTH_8U, 3);
    while(1)
    {
        //获取一帧
        
        unsigned char* yuv = pCamera->QueryFrame();
#if defined(_OUTPUT_YUV)
        fwrite(yuv, 1, VIDEO_WIDTH*VIDEO_HEIGHT*3/2, fyuv);
#endif

        RGBYUVConvert::ConvertYUV2RGB(yuv, (unsigned char*)pFrame->imageData, VIDEO_WIDTH, VIDEO_HEIGHT);
        cvFlip(pFrame, NULL, 1);
        cvShowImage("camera", pFrame);
        if (cvWaitKey(20) == 'q')
            break;
    }
#if defined(_OUTPUT_YUV)
    fclose(fyuv);
#endif
    pCamera->CloseCamera(); //可不调用此函数，CCameraDS析构时会自动关闭摄像头

    cvDestroyWindow("camera");
    return true;
}
