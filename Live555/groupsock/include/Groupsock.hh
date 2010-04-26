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
// "mTunnel" multicast access service
// Copyright (c) 1996-2009 Live Networks, Inc.  All rights reserved.
// 'Group sockets'
// C++ header

#ifndef _GROUPSOCK_HH
#define _GROUPSOCK_HH

#ifndef _GROUPSOCK_VERSION_HH
#include "groupsock_version.hh"
#endif

#ifndef _NET_INTERFACE_HH
#include "NetInterface.hh"
#endif

#ifndef _GROUPEID_HH
#include "GroupEId.hh"
#endif

// An "OutputSocket" is (by default) used only to send packets.
// No packets are received on it (unless a subclass arranges this)
// 用来发送数据，不会接收数据(除非子类来实现)
class OutputSocket: public Socket {
public:
  OutputSocket(UsageEnvironment& env);
  virtual ~OutputSocket();

  //发送数据.address:IPv4地址，port:端口号
  Boolean write(netAddressBits address, Port port, u_int8_t ttl,
		unsigned char* buffer, unsigned bufferSize);

protected:
  OutputSocket(UsageEnvironment& env, Port port);

  portNumBits sourcePortNum() const {return fSourcePort.num();}

private: // 接收数据 redefined virtual function
  virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
			     unsigned& bytesRead,
			     struct sockaddr_in& fromAddress);

private:
  Port fSourcePort;
  u_int8_t fLastSentTTL;
};

class destRecord {
public:
  destRecord(struct in_addr const& addr, Port const& port, u_int8_t ttl,
	     destRecord* next);
  virtual ~destRecord();

public:
  destRecord* fNext;
  GroupEId fGroupEId;
  Port fPort;
};

// A "Groupsock" is used to both send and receive packets.
// As the name suggests, it was originally designed to send/receive
// multicast, but it can send/receive unicast as well.
// 发送、接收数据。起初是为多播而设计，现在也可用来单播
class Groupsock: public OutputSocket {
public:
  Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
	    Port port, u_int8_t ttl);
      // used for a 'source-independent multicast' group
  Groupsock(UsageEnvironment& env, struct in_addr const& groupAddr,
	    struct in_addr const& sourceFilterAddr,
	    Port port);
      // used for a 'source-specific multicast' group
  virtual ~Groupsock();

  void changeDestinationParameters(struct in_addr const& newDestAddr,
				   Port newDestPort, int newDestTTL);
      // By default, the destination address, port and ttl for
      // outgoing packets are those that were specified in
      // the constructor.  This works OK for multicast sockets,
      // but for unicast we usually want the destination port
      // number, at least, to be different from the source port.
      // (If a parameter is 0 (or ~0 for ttl), then no change made.)
      // 在单播中，目的地址、端口、TTL要与原来的不同。多播中无所谓

  // As a special case, we also allow multiple destinations (addresses & ports)
  // (This can be used to implement multi-unicast.)
  // 允许有多个目的地址。可用于多个单播
  void addDestination(struct in_addr const& addr, Port const& port);
  void removeDestination(struct in_addr const& addr, Port const& port);
  void removeAllDestinations();

  struct in_addr const& groupAddress() const {
    return fIncomingGroupEId.groupAddress();
  }
  struct in_addr const& sourceFilterAddress() const {
    return fIncomingGroupEId.sourceFilterAddress();
  }

  Boolean isSSM() const {
    return fIncomingGroupEId.isSSM();
  }

  u_int8_t ttl() const { return fTTL; }

  // 发送多播数据.不接受
  void multicastSendOnly(); // send, but don't receive any multicast packets

  Boolean output(UsageEnvironment& env, u_int8_t ttl,
		 unsigned char* buffer, unsigned bufferSize,
		 DirectedNetInterface* interfaceNotToFwdBackTo = NULL);

  DirectedNetInterfaceSet& members() { return fMembers; }

  Boolean deleteIfNoMembers;
  Boolean isSlave; // for tunneling

  // 统计数据
  static NetInterfaceTrafficStats statsIncoming;
  static NetInterfaceTrafficStats statsOutgoing;
  static NetInterfaceTrafficStats statsRelayedIncoming;
  static NetInterfaceTrafficStats statsRelayedOutgoing;
  NetInterfaceTrafficStats statsGroupIncoming; // *not* static
  NetInterfaceTrafficStats statsGroupOutgoing; // *not* static
  NetInterfaceTrafficStats statsGroupRelayedIncoming; // *not* static
  NetInterfaceTrafficStats statsGroupRelayedOutgoing; // *not* static

  // 检查fromAddress是否是本机地址
  Boolean wasLoopedBackFromUs(UsageEnvironment& env,
			      struct sockaddr_in& fromAddress);

public: // redefined virtual functions
  virtual Boolean handleRead(unsigned char* buffer, unsigned bufferMaxSize,
			     unsigned& bytesRead,
			     struct sockaddr_in& fromAddress);

private:
  int outputToAllMembersExcept(DirectedNetInterface* exceptInterface,
			       u_int8_t ttlToFwd,
			       unsigned char* data, unsigned size,
			       netAddressBits sourceAddr);

private:
  GroupEId fIncomingGroupEId;
  destRecord* fDests;
  u_int8_t fTTL;
  DirectedNetInterfaceSet fMembers;
};

UsageEnvironment& operator<<(UsageEnvironment& s, const Groupsock& g);

// A data structure for looking up a 'groupsock'
// by (multicast address, port), or by socket number
// 存放groupsock的hash表。以多播地址和端口号为key
class GroupsockLookupTable {
public:
  Groupsock* Fetch(UsageEnvironment& env, netAddressBits groupAddress,
		   Port port, u_int8_t ttl, Boolean& isNew);
      // Creates a new Groupsock if none already exists
  Groupsock* Fetch(UsageEnvironment& env, netAddressBits groupAddress,
		   netAddressBits sourceFilterAddr,
		   Port port, Boolean& isNew);
      // Creates a new Groupsock if none already exists
  Groupsock* Lookup(netAddressBits groupAddress, Port port);
      // Returns NULL if none already exists
  Groupsock* Lookup(netAddressBits groupAddress,
		    netAddressBits sourceFilterAddr,
		    Port port);
      // Returns NULL if none already exists
  Groupsock* Lookup(UsageEnvironment& env, int sock);
      // Returns NULL if none already exists
  Boolean Remove(Groupsock const* groupsock);

  // Used to iterate through the groupsocks in the table
  class Iterator {
  public:
    Iterator(GroupsockLookupTable& groupsocks);

    Groupsock* next(); // NULL iff none

  private:
    AddressPortLookupTable::Iterator fIter;
  };

private:
  Groupsock* AddNew(UsageEnvironment& env,
		    netAddressBits groupAddress,
		    netAddressBits sourceFilterAddress,
		    Port port, u_int8_t ttl);

private:
  friend class Iterator;
  AddressPortLookupTable fTable;
};

#endif
