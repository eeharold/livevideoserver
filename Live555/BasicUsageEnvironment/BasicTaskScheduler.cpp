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
// Copyright (c) 1996-2009 Live Networks, Inc.  All rights reserved.
// Basic Usage Environment: for a simple, non-scripted, console application
// Implementation

#ifndef IMN_PIM
#include "BasicUsageEnvironment.hh"
#include "HandlerSet.hh"
#include <stdio.h>
#if defined(_QNX4)
#include <sys/select.h>
#include <unix.h>
#endif

#include "LogMacros.hh"

////////// BasicTaskScheduler //////////

BasicTaskScheduler* BasicTaskScheduler::createNew() {
	return new BasicTaskScheduler();
}

BasicTaskScheduler::BasicTaskScheduler()
  : fMaxNumSockets(0) {
  FD_ZERO(&fReadSet);
}

BasicTaskScheduler::~BasicTaskScheduler() {
}

#ifndef MILLION
#define MILLION 1000000
#endif

//处理延迟任务。此函数中不能添加正常日志，否则日志量太大
void BasicTaskScheduler::SingleStep(unsigned maxDelayTime) {
  fd_set readSet = this->fReadSet; // make a copy for this select() call

  //1 设置超时时间//////////////////////////
  //第一个事件的延迟时间
  DelayInterval const& timeToDelay = BasicTaskScheduler0::fDelayQueue.timeToNextAlarm();
  
  struct timeval tv_timeToDelay;
  tv_timeToDelay.tv_sec = timeToDelay.seconds();
  tv_timeToDelay.tv_usec = timeToDelay.useconds();
  // Very large "tv_sec" values cause select() to fail.
  // Don't make it any larger than 1 million seconds (11.5 days)
  // 延迟时间不得超过一百万秒
  const long MAX_TV_SEC = MILLION;
  if (tv_timeToDelay.tv_sec > MAX_TV_SEC) {
    tv_timeToDelay.tv_sec = MAX_TV_SEC;
  }
  // Also check our "maxDelayTime" parameter (if it's > 0):
  // 延迟时间也不能超过maxDelayTime
  if (maxDelayTime > 0 &&
      (tv_timeToDelay.tv_sec > (long)maxDelayTime/MILLION ||
       (tv_timeToDelay.tv_sec == (long)maxDelayTime/MILLION &&
	tv_timeToDelay.tv_usec > (long)maxDelayTime%MILLION))) {
    tv_timeToDelay.tv_sec = maxDelayTime/MILLION;
    tv_timeToDelay.tv_usec = maxDelayTime%MILLION;
  }

  //1 执行select，等待超时/////////////////////////
  int selectResult = select(fMaxNumSockets, &readSet, NULL, NULL,  &tv_timeToDelay);
  if (selectResult < 0) {
#if defined(__WIN32__) || defined(_WIN32)
    int err = WSAGetLastError();
    // For some unknown reason, select() in Windoze sometimes fails with WSAEINVAL if
    // it was called with no entries set in "readSet".  If this happens, ignore it:
    if (err == WSAEINVAL && readSet.fd_count == 0) {
      err = EINTR;
      // To stop this from happening again, create a dummy readable socket:
      int dummySocketNum = socket(AF_INET, SOCK_DGRAM, 0);
      FD_SET((unsigned)dummySocketNum, &fReadSet);
    }
    if (err != EINTR) {
#else
    if (errno != EINTR && errno != EAGAIN) {
#endif
	// Unexpected error - treat this as fatal:
#if !defined(_WIN32_WCE)
	perror("BasicTaskScheduler::SingleStep(): select() fails");
#endif
    DEBUG_LOG(ERR, "SingleStep error: %s", strerror(errno));
	exit(0);
      }
  }

  //1 响应读事件//////////////////////////////
  // Call the handler function for one readable socket:
  HandlerIterator iter(*BasicTaskScheduler0::fReadHandlers);
  HandlerDescriptor* handler;// socket响应函数链表
  
  // To ensure forward progress through the handlers, begin past the last
  // socket number that we handled:
  // 获取上次个被处理过的句柄
  if (BasicTaskScheduler0::fLastHandledSocketNum >= 0) 
  {
    while ((handler = iter.next()) != NULL) 
    {
      if (handler->socketNum == BasicTaskScheduler0::fLastHandledSocketNum) break;
    }
    if (handler == NULL) 
    {
      //达到最后一个，从头开始
      BasicTaskScheduler0::fLastHandledSocketNum = -1;
      iter.reset(); // start from the beginning instead
    }
  }

  //处理下一个任务(读标志位被置位)
  while ((handler = iter.next()) != NULL) 
  {
    if (FD_ISSET(handler->socketNum, &readSet) &&
	FD_ISSET(handler->socketNum, &fReadSet) /* sanity check */ &&
	handler->handlerProc != NULL)
    {
      BasicTaskScheduler0::fLastHandledSocketNum = handler->socketNum;
          // Note: we set "fLastHandledSocketNum" before calling the handler,
          // in case the handler calls "doEventLoop()" reentrantly.
      (*handler->handlerProc)(handler->clientData, SOCKET_READABLE);
      break;
    }
  }

  //1 如果前面没处理，则再看下是否需要有待处理的读时间//////////////////////////
  //没有调用任务，但是还存在未处理的读句柄。需要从头开始遍历
  if (handler == NULL && BasicTaskScheduler0::fLastHandledSocketNum >= 0) 
  {
    // We didn't call a handler, but we didn't get to check all of them,
    // so try again from the beginning:
    iter.reset();
    while ((handler = iter.next()) != NULL) 
    {
      if (FD_ISSET(handler->socketNum, &readSet) &&
	  FD_ISSET(handler->socketNum, &fReadSet) /* sanity check */ &&
	  handler->handlerProc != NULL) {
	BasicTaskScheduler0::fLastHandledSocketNum = handler->socketNum;
	    // Note: we set "fLastHandledSocketNum" before calling the handler,
            // in case the handler calls "doEventLoop()" reentrantly.
	(*handler->handlerProc)(handler->clientData, SOCKET_READABLE);
	break;
      }
    }
    if (handler == NULL) 
      BasicTaskScheduler0::fLastHandledSocketNum = -1;//because we didn't call a handler
  }

  //1 删除已经处理的超时事件/////////////////////////
  // Also handle any delayed event that may have come due.  (Note that we do this *after* calling a socket
  // handler, in case the delayed event handler modifies the set of readable socket.)
  BasicTaskScheduler0::fDelayQueue.handleAlarm();
}

//设置读操作的方式，将socketNum与某个响应函数关联起来。socketNum是socket()返回的句柄
void BasicTaskScheduler::turnOnBackgroundReadHandling(int socketNum,
				BackgroundHandlerProc* handlerProc,
				void* clientData) {
  if (socketNum < 0) return;
  //设置文件描述符集fReadSet中对应于文件描述符socketNum的位(设置为1)
  FD_SET((unsigned)socketNum, &fReadSet);
  //保存处理函数指针及其参数。内部是一个双向链表
  fReadHandlers->assignHandler(socketNum, handlerProc, clientData);

  if (socketNum+1 > fMaxNumSockets) {
    fMaxNumSockets = socketNum+1;
  }
}

//取消读操作
void BasicTaskScheduler::turnOffBackgroundReadHandling(int socketNum) {
  if (socketNum < 0) return;
  //清除文件描述符集fReadSet中对应于文件描述符socketNum的位（设置为0）
  FD_CLR((unsigned)socketNum, &fReadSet);
  fReadHandlers->removeHandler(socketNum);

  if (socketNum+1 == fMaxNumSockets) {
    --fMaxNumSockets;
  }
}
#endif

