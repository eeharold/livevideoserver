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
// Copyright (c) 1996-2009, Live Networks, Inc.  All rights reserved
// A test program that demonstrates how to stream - via unicast RTP
// - various kinds of file on demand, using a built-in RTSP server.
// 示范如何通过单播RTP来响应多种类型文件流
// main program

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "LogMacros.hh"

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

UsageEnvironment* env;

// To make the second and subsequent client for each stream reuse the same
// input stream as the first client (rather than playing the file from the
// start for each client), change the following "False" to "True":
// 如果要让第二个以及后面的客户端重用相同的输入流，则需要改成True
Boolean reuseFirstSource = False;

// To stream *only* MPEG-1 or 2 video "I" frames
// (e.g., to reduce network bandwidth),
// change the following "False" to "True":
Boolean iFramesOnly = False;

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName = "Live"); // fwd

int main(int argc, char** argv) {

  if(argc > 1 && 0 == strcmp(argv[1], "logon"))
  {
      initDebugLog("RTSPServer.log");
  }
  DEBUG_LOG(INF, "*** Begin testOnDemandRTSPServer ***");
  
  // 设置使用环境。Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  // 权限设置
  UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
  DEBUG_LOG(INF, "*** Initialize UserAuthenticationDatabase ***");
  // To implement client access control to the RTSP server, do the following:
  authDB = new UserAuthenticationDatabase;
  authDB->addUserRecord("username1", "password1"); // replace these with real strings
  // Repeat the above with each <username>, <password> that you wish to allow
  // access to the server.
#endif

  // 创建RTSP服务器，开始接收并发送数据。Create the RTSP server:
  DEBUG_LOG(INF, "*** Create RTSPServer, port = %d ***", 8554);
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  char const* descriptionString
    = "Session streamed by \"testOnDemandRTSPServer\"";

  // Set up each of the possible streams that can be served by the
  // RTSP server.  Each such stream is implemented using a
  // "ServerMediaSession" object, plus one or more
  // "ServerMediaSubsession" objects for each audio/video substream.

//   // 启动MPEG-4视频流。A MPEG-4 video elementary stream:
//   {
//     char const* streamName = "mpeg4ESVideoTest";
//     char const* inputFileName = "test.m4e";
//     ServerMediaSession* sms
//       = ServerMediaSession::createNew(*env, streamName, streamName,
// 				      descriptionString);
//     sms->addSubsession(MPEG4VideoFileServerMediaSubsession
// 		       ::createNew(*env, inputFileName, reuseFirstSource));
//     rtspServer->addServerMediaSession(sms);
// 
//     announceStream(rtspServer, sms, streamName, inputFileName);
//   }

  //jiangqi
  {
    Boolean reuseSource = True;//jiangqi
    char const* streamName = "h264";
    ServerMediaSession* sms
      = ServerMediaSession::createNew(*env, streamName, streamName,
				      descriptionString);
    sms->addSubsession(H264LiveVideoServerMediaSubsession::createNew(*env, reuseSource));
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName);
  }
  //jiangqi

  DEBUG_LOG(INF, "*** Begin doEventLoop ***");
  env->taskScheduler().doEventLoop(); // does not return

  DEBUG_LOG(INF, "*** Exit testOnDemandRTSPServer ***");
  return 0; // only to prevent compiler warning
}

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,
			   char const* streamName, char const* inputFileName) {
  char* url = rtspServer->rtspURL(sms);
  UsageEnvironment& env = rtspServer->envir();
  env << "\n\"" << streamName << "\" stream, from the file \""
      << inputFileName << "\"\n";
  env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;
}
