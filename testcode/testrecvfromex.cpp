/*
   Copyright 2011 John Selbie

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/


#include "commonincludes.h"

#include "stuncore.h"
#include "recvfromex.h"
#include "testrecvfromex.h"
#include "stunsocket.h"


HRESULT CTestRecvFromEx::Run()
{
    HRESULT hr1=S_OK, hr2=S_OK;
    HRESULT hr = S_OK;

    
    hr1 = DoTest(false); // ipv4
    printf("Test result of recvfromex on ipv4.  Result: %s\n", SUCCEEDED(hr1)?"Pass":"Fail");

    hr2 = DoTest(true);  // ipv6
    printf("Test result of recvfromex on ipv6.  Result: %s\n", SUCCEEDED(hr2)?"Pass":"Fail");

    Chk(hr1);
    Chk(hr2);
    
Cleanup:
    return hr;
    
}


// This test validates that the EnablePktInfoOption set on a socket allows us to get at the destination IP address for incoming packets
// This is needed so that we can correctly insert an origin address into responses from the server
// Otherwise, the server doesn't have a way of knowing which interface a packet arrived on when it's listening on INADDR_ANY (all available addresses)
HRESULT CTestRecvFromEx::DoTest(bool fIPV6)
{
    // create a couple of sockets for send/recv
    HRESULT hr = S_OK;
    CSocketAddress addrAny(0,0); // INADDR_ANY, random port
    sockaddr_in6 addrAnyIPV6 = {};
    uint16_t portRecv = 0;
    CRefCountedStunSocket spSocketSend, spSocketRecv;
    fd_set set = {};
    CSocketAddress addrDestForSend;
    CSocketAddress addrDestOnRecv;
    CSocketAddress addrSrcOnRecv;
    
    CSocketAddress addrSrc;
    CSocketAddress addrDst;
    
    char ch = 'x';
    sockaddr_storage addrDummy;
    socklen_t addrlength;
    int ret;
    timeval tv = {};
    
    
    if (fIPV6)
    {
        addrAnyIPV6.sin6_family = AF_INET6;
        addrAny = CSocketAddress(addrAnyIPV6);
    }
    
    
    // create two sockets listening on INADDR_ANY.  One for sending and one for receiving
    ChkA(CStunSocket::Create(addrAny, RolePP, &spSocketSend));
    ChkA(CStunSocket::Create(addrAny, RolePP, &spSocketRecv));
    
    spSocketRecv->EnablePktInfoOption(true);
    
    portRecv = spSocketRecv->GetLocalAddress().GetPort();
    
    // now send to localhost
    if (fIPV6)
    {
        sockaddr_in6 addr6 = {};
        addr6.sin6_family = AF_INET6;
        ::inet_pton(AF_INET6, "::1", &(addr6.sin6_addr));
        addrDestForSend = CSocketAddress(addr6);
    }
    else
    {
        sockaddr_in addr4 = {};
        addr4.sin_family = AF_INET;
        ::inet_pton(AF_INET, "127.0.0.1", &(addr4.sin_addr));
        addrDestForSend = CSocketAddress(addr4);
    }
    addrDestForSend.SetPort(portRecv);
    
    // flush out any residual packets that might be buffered up on recv socket
    ret = -1;
    do
    {
        addrlength = sizeof(addrDummy);
        ret = ::recvfrom(spSocketRecv->GetSocketHandle(), &ch, sizeof(ch), MSG_DONTWAIT, (sockaddr*)&addrDummy, &addrlength);
    } while (ret >= 0);
    
    // now send some data to ourselves
    ret = sendto(spSocketSend->GetSocketHandle(), &ch, sizeof(ch), 0, addrDestForSend.GetSockAddr(), addrDestForSend.GetSockAddrLength());
    ChkIfA(ret <= 0, E_UNEXPECTED);
    
    // now wait for the data to arrive
    FD_ZERO(&set);
    FD_SET(spSocketRecv->GetSocketHandle(), &set);
    tv.tv_sec = 3;
    
    ret = select(spSocketRecv->GetSocketHandle()+1, &set, NULL, NULL, &tv);
    
    ChkIfA(ret <= 0, E_UNEXPECTED);
    
    ret = ::recvfromex(spSocketRecv->GetSocketHandle(), &ch, 1, MSG_DONTWAIT, &addrSrcOnRecv, &addrDestOnRecv);
    
    ChkIfA(ret <= 0, E_UNEXPECTED);    
    
    ChkIfA(addrSrcOnRecv.IsIPAddressZero(), E_UNEXPECTED);
    ChkIfA(addrDestOnRecv.IsIPAddressZero(), E_UNEXPECTED);
    
    
Cleanup:

    return hr;
}

