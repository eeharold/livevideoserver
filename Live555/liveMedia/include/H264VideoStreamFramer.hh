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
// C++ header

#ifndef _H264_VIDEO_STREAM_FRAMER_HH
#define _H264_VIDEO_STREAM_FRAMER_HH

#ifndef _FRAMED_FILTER_HH
#include "FramedFilter.hh"
#endif

class H264VideoStreamFramer: public FramedFilter {
public:
  virtual Boolean currentNALUnitEndsAccessUnit() = 0;
  // subclasses must define this function.  It returns True iff the
  // most recently received NAL unit ends a video 'access unit' (i.e., 'frame')

protected:
  H264VideoStreamFramer(UsageEnvironment& env, FramedSource* inputSource);
  virtual ~H264VideoStreamFramer();

private:
  // redefined virtual functions:
  virtual Boolean isH264VideoStreamFramer() const;
};

//*********************************************************************
//jiangqi

class ICameraCaptuer;
class H264EncWrapper;
class H264DecWrapper;
struct TNAL;

class MyH264VideoStreamFramer: public H264VideoStreamFramer
{
public:
  virtual ~MyH264VideoStreamFramer();
  MyH264VideoStreamFramer(UsageEnvironment& env, 
    FramedSource* inputSource, H264EncWrapper* pH264Enc, H264DecWrapper* pH264Dec);
  
  static MyH264VideoStreamFramer* createNew(UsageEnvironment& env, FramedSource* inputSource);
  virtual Boolean currentNALUnitEndsAccessUnit();
  virtual void doGetNextFrame();

private:
  static ICameraCaptuer* m_pCamera;
  
  H264EncWrapper* m_pH264Enc;
  H264DecWrapper* m_pH264Dec;
  
  TNAL* m_pNalArray;
  int m_iCurNalNum; //当前frame一共有多少个nal
  int m_iCurNal;//当前使用的是第几个nal
  unsigned int m_iCurFrame;
};

#include "H264VideoRTPSink.hh"
#include "ByteStreamFileSource.hh"
#include "H264VideoStreamFramer.hh"
#include "FileServerMediaSubsession.hh"

class H264LiveVideoServerMediaSubsession: public OnDemandServerMediaSubsession{
public:
  static H264LiveVideoServerMediaSubsession*
  createNew(UsageEnvironment& env, Boolean reuseFirstSource);

private:
  H264LiveVideoServerMediaSubsession(UsageEnvironment& env,
					 Boolean reuseFirstSource);
      // called only by createNew();
  virtual ~H264LiveVideoServerMediaSubsession();

private: // redefined virtual functions
  virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
					      unsigned& estBitrate);
  virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                    unsigned char rtpPayloadTypeIfDynamic,
				                    FramedSource* inputSource);
protected:
  virtual char const* sdpLines();
};
//jiangqi

#endif
