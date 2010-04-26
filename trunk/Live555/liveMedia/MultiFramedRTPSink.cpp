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
// RTP sink for a common kind of payload format: Those which pack multiple,
// complete codec frames (as many as possible) into each RTP packet.
// Implementation

#include "MultiFramedRTPSink.hh"
#include "GroupsockHelper.hh"
#include "LogMacros.hh"
////////// MultiFramedRTPSink //////////

void MultiFramedRTPSink::setPacketSizes(unsigned preferredPacketSize,
					unsigned maxPacketSize) {
  if (preferredPacketSize > maxPacketSize || preferredPacketSize == 0) return;
      // sanity check

  delete fOutBuf;
  fOutBuf = new OutPacketBuffer(preferredPacketSize, maxPacketSize);
  fOurMaxPacketSize = maxPacketSize; // save value, in case subclasses need it
}

MultiFramedRTPSink::MultiFramedRTPSink(UsageEnvironment& env,
				       Groupsock* rtpGS,
				       unsigned char rtpPayloadType,
				       unsigned rtpTimestampFrequency,
				       char const* rtpPayloadFormatName,
				       unsigned numChannels)
  : RTPSink(env, rtpGS, rtpPayloadType, rtpTimestampFrequency,
	    rtpPayloadFormatName, numChannels),
  fOutBuf(NULL), fCurFragmentationOffset(0), fPreviousFrameEndedFragmentation(False) {
  setPacketSizes(1000, 1448);
      // 默认的最大包大小是1500(减去额外的IP等头的大小)，另外，必须是4的整数倍，所以定为1448
      // Default max packet size (1500, minus allowance for IP, UDP, UMTP headers)
      // (Also, make it a multiple of 4 bytes, just in case that matters.)
}

MultiFramedRTPSink::~MultiFramedRTPSink() {
  delete fOutBuf;
}

void MultiFramedRTPSink
::doSpecialFrameHandling(unsigned /*fragmentationOffset*/,
			 unsigned char* /*frameStart*/,
			 unsigned /*numBytesInFrame*/,
			 struct timeval frameTimestamp,
			 unsigned /*numRemainingBytes*/) {
  // default implementation: If this is the first frame in the packet,
  // use its timestamp for the RTP timestamp:
  if (isFirstFrameInPacket()) {
    setTimestamp(frameTimestamp);
  }
}

Boolean MultiFramedRTPSink::allowFragmentationAfterStart() const {
  return False; // by default
}

Boolean MultiFramedRTPSink::allowOtherFramesAfterLastFragment() const {
  return False; // by default
}

Boolean MultiFramedRTPSink
::frameCanAppearAfterPacketStart(unsigned char const* /*frameStart*/,
				 unsigned /*numBytesInFrame*/) const {
  return True; // by default
}

unsigned MultiFramedRTPSink::specialHeaderSize() const {
  // default implementation: Assume no special header:
  return 0;
}

unsigned MultiFramedRTPSink::frameSpecificHeaderSize() const {
  // default implementation: Assume no frame-specific header:
  return 0;
}

unsigned MultiFramedRTPSink::computeOverflowForNewFrame(unsigned newFrameSize) const {
  // default implementation: Just call numOverflowBytes()
  return fOutBuf->numOverflowBytes(newFrameSize);
}

void MultiFramedRTPSink::setMarkerBit() {
  unsigned rtpHdr = fOutBuf->extractWord(0);
  rtpHdr |= 0x00800000;
  fOutBuf->insertWord(rtpHdr, 0);
}

void MultiFramedRTPSink::setTimestamp(struct timeval timestamp) {
  // First, convert the timestamp to a 32-bit RTP timestamp:
  fCurrentTimestamp = convertToRTPTimestamp(timestamp);

  // Then, insert it into the RTP packet:
  fOutBuf->insertWord(fCurrentTimestamp, fTimestampPosition);
}

void MultiFramedRTPSink::setSpecialHeaderWord(unsigned word,
					      unsigned wordPosition) {
  fOutBuf->insertWord(word, fSpecialHeaderPosition + 4*wordPosition);
}

void MultiFramedRTPSink::setSpecialHeaderBytes(unsigned char const* bytes,
					       unsigned numBytes,
					       unsigned bytePosition) {
  fOutBuf->insert(bytes, numBytes, fSpecialHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderWord(unsigned word,
						    unsigned wordPosition) {
  fOutBuf->insertWord(word, fCurFrameSpecificHeaderPosition + 4*wordPosition);
}

void MultiFramedRTPSink::setFrameSpecificHeaderBytes(unsigned char const* bytes,
						     unsigned numBytes,
						     unsigned bytePosition) {
  fOutBuf->insert(bytes, numBytes, fCurFrameSpecificHeaderPosition + bytePosition);
}

void MultiFramedRTPSink::setFramePadding(unsigned numPaddingBytes) {
  if (numPaddingBytes > 0) {
    // Add the padding bytes (with the last one being the padding size):
    unsigned char paddingBuffer[255]; //max padding
    memset(paddingBuffer, 0, numPaddingBytes);
    paddingBuffer[numPaddingBytes-1] = numPaddingBytes;
    fOutBuf->enqueue(paddingBuffer, numPaddingBytes);

    // Set the RTP padding bit:
    unsigned rtpHdr = fOutBuf->extractWord(0);
    rtpHdr |= 0x20000000;
    fOutBuf->insertWord(rtpHdr, 0);
  }
}

Boolean MultiFramedRTPSink::continuePlaying() {
  DEBUG_LOG(INF, "MultiFramedRTPSink::continuePlaying");
  // Send the first packet.
  // (This will also schedule any future sends.)
  buildAndSendPacket(True);
  return True;
}

void MultiFramedRTPSink::stopPlaying() {
  fOutBuf->resetPacketStart();
  fOutBuf->resetOffset();
  fOutBuf->resetOverflowData();

  // Then call the default "stopPlaying()" function:
  MediaSink::stopPlaying();
}

void MultiFramedRTPSink::buildAndSendPacket(Boolean isFirstPacket) {
  DEBUG_LOG(INF, "MultiFramedRTPSink::buildAndSendPacket");
  fIsFirstPacket = isFirstPacket;

  // Set up the RTP header:
  unsigned rtpHdr = 0x80000000; // RTP version 2
  rtpHdr |= (fRTPPayloadType<<16); // 有效载荷类型 
  rtpHdr |= fSeqNo; // sequence number 每发送一个RTP信息包顺序号就加1
  fOutBuf->enqueueWord(rtpHdr);

  // Note where the RTP timestamp will go.
  // (We can't fill this in until we start packing payload frames.)
  // 反映RTP数据信息包中第一个字节的采样时刻，所以这里暂时不填
  fTimestampPosition = fOutBuf->curPacketSize(); // == 4
  fOutBuf->skipBytes(4); // leave a hole for the timestamp, curPacketSize == 8

  //同步源标识符,标识RTP信息包流的起源，在RTP会话或者期间的每个信息包流都有一个清楚的SSRC。
  //SSRC不是发送端的IP地址，而是在新的信息包流开始时源端随机分配的一个号码。  
  fOutBuf->enqueueWord(SSRC()); // curPacketSize == 12

  // Allow for a special, payload-format-specific header following the
  // 空出RTP header:
  fSpecialHeaderPosition = fOutBuf->curPacketSize(); // == 12
  fSpecialHeaderSize = specialHeaderSize(); // == 0
  fOutBuf->skipBytes(fSpecialHeaderSize);

  // Begin packing as many (complete) frames into the packet as we can:
  fTotalFrameSpecificHeaderSizes = 0;
  fNoFramesLeft = False;
  fNumFramesUsedSoFar = 0;
  packFrame();
}

void MultiFramedRTPSink::packFrame() {
  DEBUG_LOG(INF, "MultiFramedRTPSink::packFrame, fOverflowDataSize=%u",fOutBuf->overflowDataSize());
  // Get the next frame.

  // First, see if we have an overflow frame that was too big for the last pkt
  if (fOutBuf->haveOverflowData()) {
    // Use this frame before reading a new one from the source
    unsigned frameSize = fOutBuf->overflowDataSize();
    struct timeval presentationTime = fOutBuf->overflowPresentationTime();
    unsigned durationInMicroseconds = fOutBuf->overflowDurationInMicroseconds();
    fOutBuf->useOverflowData();

    afterGettingFrame1(frameSize, 0, presentationTime, durationInMicroseconds);
  } else {
    // Normal case: we need to read a new frame from the source
    if (fSource == NULL) return;
    //该帧特有的头信息，默认为空
    fCurFrameSpecificHeaderPosition = fOutBuf->curPacketSize(); // == 12
    fCurFrameSpecificHeaderSize = frameSpecificHeaderSize();// == 0
    fOutBuf->skipBytes(fCurFrameSpecificHeaderSize);
    fTotalFrameSpecificHeaderSizes += fCurFrameSpecificHeaderSize;
    //从fSource中获取新的一帧
    DEBUG_LOG(INF, 
      "getNextFrame from FramedSource to RTPSink::fOutBuf(head=%p, cur=%p,max=%u)",
      fOutBuf->packet(), fOutBuf->curPtr(), fOutBuf->totalBytesAvailable());
    
    fSource->getNextFrame(fOutBuf->curPtr(), fOutBuf->totalBytesAvailable(),
			  afterGettingFrame, this, ourHandleClosure, this);
  }
}

void MultiFramedRTPSink
::afterGettingFrame(void* clientData, unsigned numBytesRead,
		    unsigned numTruncatedBytes,
		    struct timeval presentationTime,
		    unsigned durationInMicroseconds) {
  DEBUG_LOG(INF, "AfterGettingFrame:");
  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
  sink->afterGettingFrame1(numBytesRead, numTruncatedBytes,
			   presentationTime, durationInMicroseconds);
}

void MultiFramedRTPSink
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
		     struct timeval presentationTime,
		     unsigned durationInMicroseconds) {
  if (fIsFirstPacket) {
    // Record the fact that we're starting to play now:
    gettimeofday(&fNextSendTime, NULL);
  }

  //是否有数据被截断
  if (numTruncatedBytes > 0) {
    unsigned const bufferSize = fOutBuf->totalBytesAvailable();
    unsigned newMaxSize = frameSize + numTruncatedBytes;
    envir() << "MultiFramedRTPSink::afterGettingFrame1(): The input frame data was too large for our buffer size ("
	    << bufferSize << ").  "
	    << numTruncatedBytes << " bytes of trailing data was dropped!  Correct this by increasing \"OutPacketBuffer::maxSize\" to at least "
	    << newMaxSize << ", *before* creating this 'RTPSink'.  (Current value is "
	    << OutPacketBuffer::maxSize << ".)\n";
  }
  unsigned curFragmentationOffset = fCurFragmentationOffset;
  unsigned numFrameBytesToUse = frameSize;
  unsigned overflowBytes = 0;//溢出了，被抛弃的数据大小

  // If we have already packed one or more frames into this packet,
  // check whether this new frame is eligible合格的 to be packed after them.
  // (This is independent of whether the packet has enough room for this
  // new frame; that check comes later.)
  // 判断是否要把这一帧放入封包
  if (fNumFramesUsedSoFar > 0) {//已经把一些帧打进封包
    if ((fPreviousFrameEndedFragmentation//前面一帧已经把封包撑满
	 && !allowOtherFramesAfterLastFragment())//恒True
	|| !frameCanAppearAfterPacketStart(fOutBuf->curPtr(), frameSize)) {//恒false
      // 不再将这一帧放入封包，Save away this frame for next time:
      numFrameBytesToUse = 0;
      fOutBuf->setOverflowData(fOutBuf->curPacketSize(), frameSize,
			       presentationTime, durationInMicroseconds);
    }
  }
  fPreviousFrameEndedFragmentation = False;

  //要放入封包的这一帧的大小
  if (numFrameBytesToUse > 0) {
    // 检查这一帧是否会使封包溢出。Check whether this frame overflows the packet
    if (fOutBuf->wouldOverflow(frameSize)) {
      // Don't use this frame now; instead, save it as overflow data, and
      // send it in the next packet instead.  However, if the frame is too
      // big to fit in a packet by itself, then we need to fragment分割 it (and
      // use some of it in this packet, if the payload format permits this.)
      // 如果一帧的大小比一个封包的大小还大，则需要分割帧，只取其中的一部分
      if (isTooBigForAPacket(frameSize)
          && (fNumFramesUsedSoFar == 0 || allowFragmentationAfterStart())) {
        // 分割这一帧，We need to fragment this frame, and use some of it now:
        overflowBytes = computeOverflowForNewFrame(frameSize);
        numFrameBytesToUse -= overflowBytes;
        fCurFragmentationOffset += numFrameBytesToUse;
      } else {
        // 抛弃这一帧，We don't use any of this frame now:
        overflowBytes = frameSize;
        numFrameBytesToUse = 0;
      }
      fOutBuf->setOverflowData(fOutBuf->curPacketSize() + numFrameBytesToUse,
          overflowBytes, presentationTime,
          durationInMicroseconds);
    } else if (fCurFragmentationOffset > 0) {
      // This is the last fragment of a frame that was fragmented over
      // more than one packet.  Do any special handling for this case:
      fCurFragmentationOffset = 0;
      fPreviousFrameEndedFragmentation = True;// 封包的最后一帧
    }
  }

  if (numFrameBytesToUse == 0) {
    // 发送!! Send our packet now, because we have filled it up:
    DEBUG_LOG(INF, "Alreadly complete to fill packet up, send packet now");
    sendPacketIfNecessary();
  } else {
    // Use this frame in our outgoing packet:
    unsigned char* frameStart = fOutBuf->curPtr();
    fOutBuf->increment(numFrameBytesToUse);
        // do this now, in case "doSpecialFrameHandling()" calls "setFramePadding()" to append padding bytes

    // Here's where any payload format specific processing gets done:
    doSpecialFrameHandling(curFragmentationOffset, frameStart,
			   numFrameBytesToUse, presentationTime,
			   overflowBytes);

    ++fNumFramesUsedSoFar;

    // Update the time at which the next packet should be sent, based
    // on the duration of the frame that we just packed into it.
    // However, if this frame has overflow data remaining, then don't
    // count its duration yet.
    if (overflowBytes == 0) {
      fNextSendTime.tv_usec += durationInMicroseconds;
      fNextSendTime.tv_sec += fNextSendTime.tv_usec/1000000;
      fNextSendTime.tv_usec %= 1000000;
    }

    // Send our packet now if (i) it's already at our preferred size, or
    // (ii) (heuristic) another frame of the same size as the one we just
    //      read would overflow the packet, or
    // (iii) it contains the last fragment of a fragmented frame, and we
    //      don't allow anything else to follow this or
    // (iv) one frame per packet is allowed:
    if (fOutBuf->isPreferredSize()
        || fOutBuf->wouldOverflow(numFrameBytesToUse)
        || (fPreviousFrameEndedFragmentation &&
            !allowOtherFramesAfterLastFragment())
        || !frameCanAppearAfterPacketStart(fOutBuf->curPtr() - frameSize,
					   frameSize) ) {
      // The packet is ready to be sent now
      DEBUG_LOG(INF, "Packet size is prefect, send packet now");
      sendPacketIfNecessary();
    } else {
      // There's room for more frames; try getting another:
      packFrame();
    }
  }
}

static unsigned const rtpHeaderSize = 12;

Boolean MultiFramedRTPSink::isTooBigForAPacket(unsigned numBytes) const {
  // Check whether a 'numBytes'-byte frame - together with a RTP header and
  // (possible) special headers - would be too big for an output packet:
  // (Later allow for RTP extension header!) #####
  numBytes += rtpHeaderSize + specialHeaderSize() + frameSpecificHeaderSize();
  return fOutBuf->isTooBigForAPacket(numBytes);
}

void MultiFramedRTPSink::sendPacketIfNecessary() {
  if (fNumFramesUsedSoFar > 0) {
    // Send the packet:
#ifdef TEST_LOSS
    if ((our_random()%10) != 0) // simulate 10% packet loss #####
#endif
    if(getenv("HEX") != NULL)
    {
      DEBUG_HEX(fOutBuf->packet(),fOutBuf->curPacketSize());
    }
    DEBUG_LOG(INF, "Send packet(fOutBuf), head = start = %p, size = %4u, data = %s", 
      fOutBuf->packet(), fOutBuf->curPacketSize(), binToHex(fOutBuf->packet(), 16));
    fRTPInterface.sendPacket(fOutBuf->packet(), fOutBuf->curPacketSize());
    ++fPacketCount;
    fTotalOctetCount += fOutBuf->curPacketSize();
    fOctetCount += fOutBuf->curPacketSize()
      - rtpHeaderSize - fSpecialHeaderSize - fTotalFrameSpecificHeaderSizes;

    ++fSeqNo; // for next time
  }

  if (fOutBuf->haveOverflowData()
      && fOutBuf->totalBytesAvailable() > fOutBuf->totalBufferSize()/2) {
    // Efficiency hack: Reset the packet start pointer to just in front of
    // the overflow data (allowing for the RTP header and special headers),
    // so that we probably don't have to "memmove()" the overflow data
    // into place when building the next packet:
    unsigned newPacketStart = fOutBuf->curPacketSize()
      - (rtpHeaderSize + fSpecialHeaderSize + frameSpecificHeaderSize());
    fOutBuf->adjustPacketStart(newPacketStart);
  } else {
    // Normal case: Reset the packet start pointer back to the start:
    fOutBuf->resetPacketStart();
  }
  fOutBuf->resetOffset();
  fNumFramesUsedSoFar = 0;

  //是否已经全部发送完毕
  if (fNoFramesLeft) {
    // We're done:
    DEBUG_LOG(INF, "No frame left");
    onSourceClosure(this);
  } else {
    // We have more frames left to send.  Figure out when the next frame
    // is due to start playing, then make sure that we wait this long before
    // sending the next packet.
    // 计算出下一帧什么时候播放
    struct timeval timeNow;
    gettimeofday(&timeNow, NULL);
    int uSecondsToGo;
    if (fNextSendTime.tv_sec < timeNow.tv_sec
	  || (fNextSendTime.tv_sec == timeNow.tv_sec && fNextSendTime.tv_usec < timeNow.tv_usec)) 
    {
	    //当前时间大于下一帧时间，立即发送，无需等待
      uSecondsToGo = 0; // prevents integer underflow if too far behind
    } 
    else 
    { 
      //当前时间小于下一帧时间，需等待uSecondsToGo微秒
      uSecondsToGo = (fNextSendTime.tv_sec - timeNow.tv_sec)*1000000 + (fNextSendTime.tv_usec - timeNow.tv_usec);
    }

    // 延迟一定时间后再组装封包，Delay this amount of time:
    DEBUG_LOG(INF, "Delay %dus to send next packet", uSecondsToGo);
    nextTask() = envir().taskScheduler().scheduleDelayedTask(uSecondsToGo,
						(TaskFunc*)sendNext, this);
  }
}

// The following is called after each delay between packet sends:
void MultiFramedRTPSink::sendNext(void* firstArg) {
  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)firstArg;
  sink->buildAndSendPacket(False);
}

void MultiFramedRTPSink::ourHandleClosure(void* clientData) {
  MultiFramedRTPSink* sink = (MultiFramedRTPSink*)clientData;
  // There are no frames left, but we may have a partially built packet
  //  to send
  sink->fNoFramesLeft = True;
  DEBUG_LOG(INF, "Handle closure, send packet");
  sink->sendPacketIfNecessary();
}
