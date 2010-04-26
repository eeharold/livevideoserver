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
// A file source that is a plain byte stream (rather than frames)
// Implementation

#if (defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WCE)
#include <io.h>
#include <fcntl.h>
#define READ_FROM_FILES_SYNCHRONOUSLY 1
    // Because Windows is a silly toy operating system that doesn't (reliably) treat
    // open files as being readable sockets (which can be handled within the default
    // "BasicTaskScheduler" event loop, using "select()"), we implement file reading
    // in Windows using synchronous, rather than asynchronous, I/O.  This can severely
    // limit the scalability of servers using this code that run on Windows.
    // If this is a problem for you, then either use a better operating system,
    // or else write your own Windows-specific event loop ("TaskScheduler" subclass)
    // that can handle readable data in Windows open files as an event.
#endif

#include "ByteStreamFileSource.hh"
#include "InputFile.hh"
#include "GroupsockHelper.hh"
#include "LogMacros.hh"
////////// ByteStreamFileSource //////////

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, char const* fileName,
				unsigned preferredFrameSize,
				unsigned playTimePerFrame) {
  FILE* fid = OpenInputFile(env, fileName);
  if (fid == NULL) return NULL;

  Boolean deleteFidOnClose = fid == stdin ? False : True;
  ByteStreamFileSource* newSource
    = new ByteStreamFileSource(env, fid, deleteFidOnClose,
			       preferredFrameSize, playTimePerFrame);
  newSource->fFileSize = GetFileSize(fileName, fid);
  DEBUG_LOG(INF, "Create ByteStreamFileSource1:"
    "File size = %lld, preferredFrameSize = %u, playTimePerFrame = %u", 
    newSource->fFileSize, preferredFrameSize, playTimePerFrame);
  
  return newSource;
}

ByteStreamFileSource*
ByteStreamFileSource::createNew(UsageEnvironment& env, FILE* fid,
				Boolean deleteFidOnClose,
				unsigned preferredFrameSize,
				unsigned playTimePerFrame) {
  if (fid == NULL) return NULL;

  ByteStreamFileSource* newSource
    = new ByteStreamFileSource(env, fid, deleteFidOnClose,
			       preferredFrameSize, playTimePerFrame);
  newSource->fFileSize = GetFileSize(NULL, fid);
  DEBUG_LOG(INF, "Create ByteStreamFileSource2:"
    "File size = %lld, preferredFrameSize = %u, playTimePerFrame = %u", 
    newSource->fFileSize, preferredFrameSize, playTimePerFrame);

  return newSource;
}

//偏移一个绝对值。从文件头往后偏移byteNumber个字节
void ByteStreamFileSource::seekToByteAbsolute(u_int64_t byteNumber) {
  SeekFile64(fFid, (int64_t)byteNumber, SEEK_SET);
}

//偏移一个相对值。从当前位置往后偏移offset
void ByteStreamFileSource::seekToByteRelative(int64_t offset) {
  SeekFile64(fFid, offset, SEEK_CUR);
}

ByteStreamFileSource::ByteStreamFileSource(UsageEnvironment& env, FILE* fid,
					   Boolean deleteFidOnClose,
					   unsigned preferredFrameSize,
					   unsigned playTimePerFrame)
  : FramedFileSource(env, fid), fPreferredFrameSize(preferredFrameSize),
    fPlayTimePerFrame(playTimePerFrame), fLastPlayTime(0), fFileSize(0),
    fDeleteFidOnClose(deleteFidOnClose), fHaveStartedReading(False) {
}

ByteStreamFileSource::~ByteStreamFileSource() {
  if (fFid == NULL) return;

#ifndef READ_FROM_FILES_SYNCHRONOUSLY
  envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
#endif

  if (fDeleteFidOnClose) fclose(fFid);
}

void ByteStreamFileSource::doGetNextFrame() {
  DEBUG_LOG(INF, "ByteStreamFileSource::doGetNextFrame");
  if (feof(fFid) || ferror(fFid)) {
    handleClosure(this);
    return;
  }

#ifdef READ_FROM_FILES_SYNCHRONOUSLY //win32下是同步读
  doReadFromFile();
#else
  if (!fHaveStartedReading) {
    // Await readable data from the file:
    // fileno返回打开文件的文件描述符
    envir().taskScheduler().turnOnBackgroundReadHandling(fileno(fFid),
	       (TaskScheduler::BackgroundHandlerProc*)&fileReadableHandler, this);
    fHaveStartedReading = True;
  DEBUG_LOG(INF, "turnOnBackgroundReadHandling: handle=%d, func='fileReadableHandler'", fileno(fFid));
  }
#endif
}

void ByteStreamFileSource::doStopGettingFrames() {
#ifndef READ_FROM_FILES_SYNCHRONOUSLY
  envir().taskScheduler().turnOffBackgroundReadHandling(fileno(fFid));
  fHaveStartedReading = False;
#endif
}

void ByteStreamFileSource::fileReadableHandler(ByteStreamFileSource* source, int /*mask*/) {
  if (!source->isCurrentlyAwaitingData()) {
    DEBUG_LOG(INF, "Data is not readly");
    source->doStopGettingFrames(); // 尚未准备好数据，we're not ready for the data yet
    return;
  }
  source->doReadFromFile();
}

//从文件中读取fMaxSize个字节到fTo
void ByteStreamFileSource::doReadFromFile() {
  // Try to read as many bytes as will fit in the buffer provided
  // (or "fPreferredFrameSize" if less)
  if (fPreferredFrameSize > 0 && fPreferredFrameSize < fMaxSize) {
    fMaxSize = fPreferredFrameSize;
  }
  
  DEBUG_LOG(INF, "Read file: start = %ld, size= %u; Write to %p", ftell(fFid), fMaxSize, fTo);
  
  fFrameSize = fread(fTo, 1, fMaxSize, fFid);//fread(buffer,size,count,fp);
  if (fFrameSize == 0) {
    handleClosure(this);
    return;
  }

  // 设置图像时间，Set the 'presentation time':
  if (fPlayTimePerFrame > 0 && fPreferredFrameSize > 0) 
  {
    if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) 
    {
      // fPresentationTime为零，表示这是第一帧，This is the first frame, so use the current time:
      gettimeofday(&fPresentationTime, NULL);
    } 
    else 
    {
      // Increment by the play time of the previous data:
      // 根据上次的fPresentationTime和上次的持续时间计算新的fPresentationTime
      unsigned uSeconds	= fPresentationTime.tv_usec + fLastPlayTime;
      fPresentationTime.tv_sec += uSeconds/1000000;
      fPresentationTime.tv_usec = uSeconds%1000000;
    }

    // Remember the play time of this data:
    fLastPlayTime = (fPlayTimePerFrame*fFrameSize)/fPreferredFrameSize;
    fDurationInMicroseconds = fLastPlayTime;
  } 
  else 
  {
    // We don't know a specific play time duration for this data,
    // so just record the current time as being the 'presentation time':
    gettimeofday(&fPresentationTime, NULL);
  }

  // Inform the reader that he has data:
#ifdef READ_FROM_FILES_SYNCHRONOUSLY
  // To avoid possible infinite recursion, we need to return to the event loop to do this:
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
				(TaskFunc*)FramedSource::afterGetting, this);
#else
  // Because the file read was done from the event loop, we can call the
  // 'after getting' function directly, without risk of infinite recursion:
  FramedSource::afterGetting(this);
#endif
}
