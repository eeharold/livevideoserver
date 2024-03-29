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
// C++ header

#ifndef _BASIC_USAGE_ENVIRONMENT_HH
#define _BASIC_USAGE_ENVIRONMENT_HH

#ifndef _BASIC_USAGE_ENVIRONMENT0_HH
#include "BasicUsageEnvironment0.hh"
#endif

//为了适应不同操作系统，重载了getErrno()
//为了控制打屏输出，重载了操作符<<
//该类必须通过createNew方法来生成对象
class BasicUsageEnvironment: public BasicUsageEnvironment0 {
public:
  static BasicUsageEnvironment* createNew(TaskScheduler& taskScheduler);

  // redefined virtual functions:
  virtual int getErrno() const;

  virtual UsageEnvironment& operator<<(char const* str);
  virtual UsageEnvironment& operator<<(int i);
  virtual UsageEnvironment& operator<<(unsigned u);
  virtual UsageEnvironment& operator<<(double d);
  virtual UsageEnvironment& operator<<(void* p);

protected:
  BasicUsageEnvironment(TaskScheduler& taskScheduler);
      // called only by "createNew()" (or subclass constructors)
  virtual ~BasicUsageEnvironment();
};


class BasicTaskScheduler: public BasicTaskScheduler0 {
public:
  static BasicTaskScheduler* createNew();
  virtual ~BasicTaskScheduler();

protected:
  BasicTaskScheduler();
      // called only by "createNew()"

protected:
  // Redefined virtual functions:
  virtual void SingleStep(unsigned maxDelayTime);

  virtual void turnOnBackgroundReadHandling(int socketNum,
				    BackgroundHandlerProc* handlerProc,
				    void* clientData);
  virtual void turnOffBackgroundReadHandling(int socketNum);

protected:
  // To implement background reads:
  int fMaxNumSockets;//保存的描述符个数加1(用于select的第一个参数)
  fd_set fReadSet;
};

#endif
