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
// Any source that feeds into a "H264VideoRTPSink" must be of this class.
// This is a virtual base class; subclasses must implement the
// "currentNALUnitEndsAccessUnit()" virtual function.
// Implementation

#include "H264VideoStreamFramer.hh"
#include "LogMacros.hh"
#include "GroupsockHelper.hh" // gettimeofday

H264VideoStreamFramer::H264VideoStreamFramer(UsageEnvironment& env, FramedSource* inputSource)
  : FramedFilter(env, inputSource) {
}

H264VideoStreamFramer::~H264VideoStreamFramer() {
}

Boolean H264VideoStreamFramer::isH264VideoStreamFramer() const {
  return True;
}

//*********************************************************************
//jiangqi
#include "ICameraCaptuer.h"
#include "H264EndWrapper.h"
#include "H264DecWrapper.h"

ICameraCaptuer* MyH264VideoStreamFramer::m_pCamera = NULL;

//jiangqi
//测试输出控制
#undef _TEST_OUTPUT_264  //禁止输出264文件
#undef _TEST_OUTPUT_YUV  //禁止输出解码后的YUV
#undef _TEST_DISPLAY     //禁止实时输出压缩视频   

#if defined(_TEST_DISPLAY) || defined(_TEST_OUTPUT_YUV)
#define _TEST_DECODE
#endif

#if defined(_TEST_OUTPUT_264)
FILE* f264;
#endif

#if defined(_TEST_OUTPUT_YUV)
FILE* fyuv;
#endif

//#undef _OUTPUT_LOCAL
// jiangqi
#if defined(_TEST_DISPLAY)
#include "cv.h"
#include "highgui.h"
#include "convert.h"
IplImage* g_IplImage = NULL;
#endif

MyH264VideoStreamFramer::MyH264VideoStreamFramer(UsageEnvironment& env, 
    FramedSource* inputSource, H264EncWrapper* pH264Enc, H264DecWrapper* pH264Dec):
      H264VideoStreamFramer(env, inputSource), 
      m_pNalArray(NULL), m_iCurNalNum(0), m_iCurNal(0), m_iCurFrame(0),
      m_pH264Enc(pH264Enc), m_pH264Dec(pH264Dec)
{
}

MyH264VideoStreamFramer::~MyH264VideoStreamFramer()
{
    m_pCamera->CloseCamera();
    
    m_pH264Enc->Destroy();
    delete m_pH264Enc;
    m_pH264Enc = NULL;

    m_pH264Dec->Destroy();
    delete m_pH264Dec;
    m_pH264Dec = NULL;

#if defined(_TEST_OUTPUT_264)
    fclose(f264);
#endif
    
#if defined(_TEST_OUTPUT_YUV)
    fclose(fyuv);
#endif

#if defined(_TEST_DISPLAY)
    cvDestroyWindow("TestRTSPServer");
    cvReleaseImage(&g_IplImage);
#endif
}

const int VIDEO_WIDTH = 320, VIDEO_HEIGHT = 240;

MyH264VideoStreamFramer* MyH264VideoStreamFramer::createNew(
                                                         UsageEnvironment& env,
                                                         FramedSource* inputSource)
{
#if defined(_TEST_OUTPUT_264)
    f264 = fopen("TestRTSPServer.264", "wb");
#endif
    
 #if defined(_TEST_OUTPUT_YUV)
    fyuv = fopen("TestRTSPServer.yuv", "wb");
#endif
   
    //打开摄像头
    if(NULL == m_pCamera && NULL == (m_pCamera = CamCaptuerMgr::GetCamCaptuer()))
    {
        DEBUG_LOG(ERR, "Create camera instance error");
        return NULL;
    }
    
    if( !m_pCamera->OpenCamera(0, VIDEO_WIDTH, VIDEO_HEIGHT)) 
    {
        DEBUG_LOG(ERR, "Can not open camera.");
        return NULL;
    }

    //初始化x264编码器，通过传入的码率可以控制图像的质量
    //jiangqi 注意:H264VideoFileServerMediaSubsession的构造函数中也有码率控制
    H264EncWrapper* pH264Enc = new H264EncWrapper;
    if(pH264Enc->Initialize(VIDEO_WIDTH, VIDEO_HEIGHT, 96, 25) < 0)
    {
        DEBUG_LOG(ERR, "Initialize x264 encoder error.");
        return NULL;
    }

    // 初始化解码器
    H264DecWrapper* pH264Dec = new H264DecWrapper;
    if(pH264Dec->Initialize() < 0)
    {
        DEBUG_LOG(ERR, "Initialize H.264 decoder error.");
        return NULL;
    }

#if defined(_TEST_DISPLAY)
    RGBYUVConvert::InitConvertTable();
    //初始化opencv
    cvNamedWindow("TestRTSPServer");
    g_IplImage = cvCreateImage(cvSize(VIDEO_WIDTH, VIDEO_HEIGHT), IPL_DEPTH_8U, 3);
    if(NULL == g_IplImage)
    {
        DEBUG_LOG(ERR, "Initialize OpenCV error.");
        return NULL;
    }
#endif

    // Need to add source type checking here???  #####
    MyH264VideoStreamFramer* fr;
    fr = new MyH264VideoStreamFramer(env, inputSource, pH264Enc, pH264Dec);
    return fr;
}

Boolean MyH264VideoStreamFramer::currentNALUnitEndsAccessUnit()
{
    if(m_iCurNal >= m_iCurNalNum)
    {
        m_iCurFrame++;
        return True;
    }
    else
    {
        return False;
    }
}

void MyH264VideoStreamFramer::doGetNextFrame()
{
    DEBUG_LOG(INF, "MyH264VideoStreamFramer::doGetNextFrame()");
    TNAL* pNal = NULL;
    unsigned char* pOrgImg;
    
    //获取NAL
    if((m_pNalArray != NULL) && (m_iCurNal < m_iCurNalNum))
    {
        pNal = &m_pNalArray[m_iCurNal];
        DEBUG_LOG(INF, "Frame[%d], Nal[%d:%d]: size = %d", m_iCurFrame, m_iCurNalNum, m_iCurNal, pNal->size);
    }
    else
    {
        m_pH264Enc->CleanNAL(m_pNalArray, m_iCurNalNum);
        m_iCurNal = 0;
        
        pOrgImg = m_pCamera->QueryFrame();
        gettimeofday(&fPresentationTime, NULL);//同一帧的NAL具有相同的时间戳

        m_pH264Enc->Encode(pOrgImg, m_pNalArray, m_iCurNalNum);
        pNal = &m_pNalArray[m_iCurNal];
        DEBUG_LOG(INF, "Frame[%d], Nal[%d:%d]: size = %d", m_iCurFrame, m_iCurNalNum, m_iCurNal, pNal->size);
    }
    m_iCurNal++;

#if defined(_TEST_DECODE)
    //解码 begin ////////////////////////////////////////////////
    static const int YUV_IMG_SIZE = VIDEO_WIDTH * VIDEO_HEIGHT *3 /2;
    static unsigned char yuv[YUV_IMG_SIZE] = {0};
    static int iDecodedFrame = 0;
    int iYuvSize = 0;
    bool bGetFrame = true;
    int iDecodedLen = 0;    
    unsigned char* nal_ptr = pNal->data;
    int len =  pNal->size;
    //这里是以NAL为单位进行解码，不可能解码出多幅图像
    while(len > 0)
    {
        iDecodedLen = m_pH264Dec->Decode(pNal->data, pNal->size, yuv, iYuvSize, bGetFrame);
        if(bGetFrame)
        {
#if defined(_TEST_DISPLAY)
            //显示原始的RGB图像
            RGBYUVConvert::ConvertYUV2RGB(yuv, (unsigned char*)g_IplImage->imageData, VIDEO_WIDTH, VIDEO_HEIGHT);
            cvFlip(g_IplImage, NULL, 1);
            cvShowImage("TestRTSPServer", g_IplImage);
            cvWaitKey(5);
#endif       

#if defined(_TEST_OUTPUT_YUV)
            fwrite(yuv, 1, iYuvSize, fyuv);     
#endif       

            DEBUG_LOG(INF, "Success to decode one frame[%d]", iDecodedFrame);
            iDecodedFrame++;
        }
        len -= iDecodedLen;
        nal_ptr += iDecodedLen;
    }
    //解码 end ////////////////////////////////////////////////
#endif
    
#if defined(_TEST_OUTPUT_264)
    fwrite(pNal->data, 1, pNal->size, f264);
#endif

    //复制到缓冲区
    unsigned char* realData = pNal->data;
    unsigned int realLen = pNal->size;
    
    if(realLen < fMaxSize)        
    {            
      memcpy(fTo, realData, realLen);      
    }        
    else        
    {           
      //this probably does not work!!!!!!  
      DEBUG_LOG(ERR, "Too large NAL");
      memcpy(fTo, realData, fMaxSize);            
      fNumTruncatedBytes = realLen - fMaxSize;        
    } 

    fDurationInMicroseconds = 40000;//控制播放速度
    //gettimeofday(&fPresentationTime, NULL);
    DEBUG_LOG(INF, "fPresentationTime = %d.%d", fPresentationTime.tv_sec, fPresentationTime.tv_usec);

    fFrameSize = realLen ;        
    afterGetting(this);  
}


H264LiveVideoServerMediaSubsession*
H264LiveVideoServerMediaSubsession::createNew(UsageEnvironment& env,
						  Boolean reuseFirstSource) {
  return new H264LiveVideoServerMediaSubsession(env, reuseFirstSource);
}

H264LiveVideoServerMediaSubsession
::H264LiveVideoServerMediaSubsession(UsageEnvironment& env,
					 Boolean reuseFirstSource)
  : OnDemandServerMediaSubsession(env, reuseFirstSource) {
}

H264LiveVideoServerMediaSubsession::~H264LiveVideoServerMediaSubsession() {
}

FramedSource* H264LiveVideoServerMediaSubsession
::createNewStreamSource(unsigned /*clientSessionId*/, unsigned& estBitrate) {
  estBitrate = 96; // kbps, estimate ??

  // Create the video source:
  //ByteStreamFileSource* fileSource = ByteStreamFileSource::createNew(envir(), fFileName);
  //if (fileSource == NULL) return NULL;
  //fFileSize = fileSource->fileSize();

  // Create a framer for the Video Elementary Stream:
  return MyH264VideoStreamFramer::createNew(envir(), NULL);
}

RTPSink* H264LiveVideoServerMediaSubsession::createNewRTPSink(Groupsock* rtpGroupsock,
								  unsigned char rtpPayloadTypeIfDynamic,
								  FramedSource* /*inputSource*/) {
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, 96, 0, "H264");
}

//jiangqi: 避免两次创建source和sink
//SDP需要根据实际的媒体信息来生成
char const* H264LiveVideoServerMediaSubsession::sdpLines()
{
    return fSDPLines = 
        "m=video 0 RTP/AVP 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "b=AS:96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=000000;sprop-parameter-sets=H264\r\n"
        "a=control:track1\r\n";
}
//jiangqi

