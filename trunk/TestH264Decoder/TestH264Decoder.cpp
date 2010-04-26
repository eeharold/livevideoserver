#if defined(_TEST_DISPLAY)
#include "cv.h"
#include "highgui.h"
#include "convert.h"

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

//IplImage* g_IplImage = NULL;
const char* g_OpenCV_Window_Name = "TestH264Decoder";
#endif

#include "H264DecWrapper.h"
#include <stdio.h>
#include <stdlib.h>

const int VIDEO_WIDTH = 352;
const int VIDEO_HEIGHT = 288;

int main()
{
    printf("Decoding...\n");
    H264DecWrapper* pH264Dec = new H264DecWrapper;
    printf("Create H264DecWrapper\n");
    RGBYUVConvert::InitConvertTable();

#if defined(_TEST_DISPLAY)
    cvNamedWindow(g_OpenCV_Window_Name);
    IplImage* pIplImage = cvCreateImage(cvSize(VIDEO_WIDTH, VIDEO_HEIGHT), IPL_DEPTH_8U, 3);
    if(NULL == pIplImage)
    {
        fprintf(stderr, "Initialize OpenCV error.");
        return NULL;
    }
#endif

    if(pH264Dec->Initialize() < 0)
    {
        fprintf(stderr, "Initialize H.264 decoder error.");
        return -1;
    }

    FILE* fin = fopen("TestRTSPServer.264", "rb");
    FILE* fout = fopen("TestH264Decoder.yuv", "wb");
    if(NULL == fin || NULL == fout)
    {
        printf("open file error\n");
        return -1;
    }

    int frame = 0, size, len;

    const int INBUF_SIZE = 2301;
    const int OUTBUF_SIZE = VIDEO_WIDTH*VIDEO_HEIGHT*3/2;

    unsigned char inbuf[INBUF_SIZE] = {0};
    unsigned char *inbuf_ptr = NULL;

    unsigned char outbuf[OUTBUF_SIZE] = {0};
    int iOutSize = 0;
    bool bGetFrame = false;

    for(;;) 
    {
        if (0 == (size = fread(inbuf, 1, INBUF_SIZE, fin)))
        {
            break;
        }
        inbuf_ptr = inbuf;

        while (size > 0)
        {
            len = pH264Dec->Decode(inbuf_ptr, size, outbuf, iOutSize, bGetFrame);
            if(bGetFrame)
            {
                if(frame < 100)
                {
#if defined(_TEST_DISPLAY)
                    RGBYUVConvert::ConvertYUV2RGB(outbuf, (unsigned char*)pIplImage->imageData, VIDEO_WIDTH, VIDEO_HEIGHT);
                    cvFlip(pIplImage, NULL, 1);
                    cvShowImage(g_OpenCV_Window_Name, pIplImage);
                    cvWaitKey(10);
#endif
                    fwrite(outbuf, 1, iOutSize, fout);
                    printf("saving frame %d\n", frame);
                }
                else
                {
                    printf("ignore frame %d\n", frame);
                }
                frame++;
            }
            size -= len;
            inbuf_ptr += len;
        }

    }

    fclose(fin);
    fclose(fout);

    pH264Dec->Destroy();
    delete pH264Dec;
    pH264Dec = NULL;

#if defined(_TEST_DISPLAY)
    cvDestroyWindow("TestRTSPServer");
    cvReleaseImage(&pIplImage);
#endif

    printf("End\n");
    system("pause");

    return 0;
}
