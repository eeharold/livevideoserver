/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2009 Live Networks, Inc.  All rights reserved.
// H.264 Video File sinks
// Implementation

#include "H264VideoFileSink.hh"
#include "OutputFile.hh"
#include "LogMacros.hh"

////////// H264VideoFileSink //////////

#if defined(_TEST_CLIENT_DISPLAY)
const int VIDEO_WIDTH = 320, VIDEO_HEIGHT = 240;

#include "cv.h"
#include "highgui.h"
#include "convert.h"
#include "H264DecWrapper.h"
IplImage* g_pFrame = NULL;
H264DecWrapper* g_pH264Dec;
#endif

H264VideoFileSink
::H264VideoFileSink(UsageEnvironment& env, FILE* fid, unsigned bufferSize,
		   char const* perFrameFileNamePrefix)
  : FileSink(env, fid, bufferSize, perFrameFileNamePrefix) {
}

H264VideoFileSink::~H264VideoFileSink() {
#if defined(_TEST_CLIENT_DISPLAY)
    g_pH264Dec->Destroy();
    delete g_pH264Dec;
    g_pH264Dec = NULL;

    cvDestroyWindow("TestRTSPServer");
    cvReleaseImage(&g_pFrame);
#endif
}

H264VideoFileSink*
H264VideoFileSink::createNew(UsageEnvironment& env, char const* fileName,
			    unsigned bufferSize, Boolean oneFilePerFrame) {
//jiangqi
#if defined(_TEST_CLIENT_DISPLAY)
    // 初始化解码器
    g_pH264Dec = new H264DecWrapper;
    if(g_pH264Dec->Initialize() < 0)
    {
        DEBUG_LOG(ERR, "Initialize H.264 decoder error.");
        return NULL;
    }

    RGBYUVConvert::InitConvertTable();
    //初始化opencv
    cvNamedWindow("openRTSP");
    g_pFrame = cvCreateImage(cvSize(VIDEO_WIDTH, VIDEO_HEIGHT), IPL_DEPTH_8U, 3);
    if(NULL == g_pFrame)
    {
        DEBUG_LOG(ERR, "Initialize OpenCV error.");
        return NULL;
    }
#endif

  do {
    FILE* fid;
    char const* perFrameFileNamePrefix;
    if (oneFilePerFrame) {
      // Create the fid for each frame
      fid = NULL;
      perFrameFileNamePrefix = fileName;
    } else {
      // Normal case: create the fid once
      fid = OpenOutputFile(env, fileName);
      if (fid == NULL) break;
      perFrameFileNamePrefix = NULL;
    }

    return new H264VideoFileSink(env, fid, bufferSize, perFrameFileNamePrefix);
  } while (0);

  return NULL;
}

Boolean H264VideoFileSink::sourceIsCompatibleWithUs(MediaSource& source) {
  // Just return true, should be checking for H.264 video streams though
    return True;
}

void H264VideoFileSink::afterGettingFrame1(unsigned frameSize,
					  struct timeval presentationTime) {

#if defined(_TEST_CLIENT_DISPLAY)
    //解码 begin ////////////////////////////////////////////////
    DEBUG_LOG(INF, "nal: %s", binToHex(fBuffer, 16));
        
    static const int YUV_IMG_SIZE = VIDEO_WIDTH * VIDEO_HEIGHT *3 /2;
    static unsigned char yuv[YUV_IMG_SIZE] = {0};
    static int iDecodedFrame = 0;
    int iYuvSize = 0;
    bool bGetFrame = true;
    int iDecodedLen = 0;    
    unsigned char* nal_ptr = fBuffer;
    int len = frameSize;
    //这里是以NAL为单位进行解码，不可能解码出多幅图像
    while(len > 0)
    {
        iDecodedLen = g_pH264Dec->Decode(fBuffer, frameSize, yuv, iYuvSize, bGetFrame);
        if(bGetFrame)
        {
            //显示原始的RGB图像
            RGBYUVConvert::ConvertYUV2RGB(yuv, (unsigned char*)g_pFrame->imageData, VIDEO_WIDTH, VIDEO_HEIGHT);
            cvFlip(g_pFrame, NULL, 1);
            cvShowImage("openRTSP", g_pFrame);
            cvWaitKey(5);

            DEBUG_LOG(INF, "Success to decode one frame[%d]", iDecodedFrame);
            iDecodedFrame++;
        }
        len -= iDecodedLen;
        nal_ptr += iDecodedLen;
    }
    //解码 end ////////////////////////////////////////////////
#endif

  unsigned char start_code[4] = {0x00, 0x00, 0x00, 0x01};
  addData(start_code, 4, presentationTime);
  DEBUG_LOG(INF, "Get data: size = %d", frameSize+4);

  // Call the parent class to complete the normal file write with the input data:
  FileSink::afterGettingFrame1(frameSize, presentationTime);

}
