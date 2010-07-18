#ifndef _X264WRAPPER_H_
#define _X264WRAPPER_H_

#include "DllManager.h"

extern "C" 
{
#include "stdint.h"
#include "x264.h"
}

struct DLL_EXPORT TNAL
{
    int size;
    unsigned char* data;
    TNAL(): size(0), data(NULL) {}
};


class DLL_EXPORT H264EncWrapper
{
public:
    H264EncWrapper();
    virtual ~H264EncWrapper();

    // ��ʼ��������
    int Initialize(int iWidth, int iHeight, int iRateBit = 96, int iFps = 25);
    // ��һ֡������б��룬����NAL����
    int Encode(unsigned char* szYUVFrame, TNAL*& pNALArray, int& iNalNum);
    // ����NAL����
    void H264EncWrapper::CleanNAL(TNAL* pNALArray, int iNalNum);
    // ���ٱ�����
    int Destroy();

private:
    x264_param_t m_param;
    x264_picture_t m_pic;
    x264_t* m_h;
    
    unsigned char *m_pBuffer; //��ŵ���NAL
    int m_iBufferSize;//NAL�Ĵ�С
    int m_iFrameNum;//֡��
};

#endif
