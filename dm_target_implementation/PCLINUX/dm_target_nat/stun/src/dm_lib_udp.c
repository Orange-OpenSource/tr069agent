#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>


#ifdef WIN32

#include <winsock2.h>
#include <stdlib.h>
#include <io.h>

#else

#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#endif

#include "DM_GlobalDefs.h"
#include "DM_ENG_Parameter.h"
#include "DM_ENG_Common.h"
#include "dm_com_utils.h"
#include "CMN_Trace.h"

#include "dm_lib_udp.h"

Socket
openPort (unsigned short port, unsigned int interfaceIp)
{
  Socket fd;

  DBG ("Opening #%u port on Ox%x interface...", port, htonl (interfaceIp));

  fd = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == INVALID_SOCKET)
  {
    EXEC_ERROR("Could not create a UDP socket: %d", errno);
    return INVALID_SOCKET;
  }

  struct sockaddr_in addr;
  memset ((char *) &(addr), 0, sizeof ((addr)));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_ANY);
  addr.sin_port = htons (port);

  if ((interfaceIp != 0) && (interfaceIp != 0x100007f))
  {
    addr.sin_addr.s_addr = htonl (interfaceIp);
  }

  if (bind (fd, (struct sockaddr *) &addr, sizeof (addr)) != 0)
  {
    int e = errno;

    switch (e)
    {
    case 0:
      {
        EXEC_ERROR("Could not bind socket");
        return INVALID_SOCKET;
      }
    case EADDRINUSE:
      {
        EXEC_ERROR("Port %d for receiving UDP is in use", port);
        return INVALID_SOCKET;
      }
      break;
    case EADDRNOTAVAIL:
      {
        EXEC_ERROR("Cannot assign requested address");
        return INVALID_SOCKET;
      }
      break;
    default:
      {
        EXEC_ERROR("Could not bind UDP receive port Error= %d %s", e,
               strerror (e));
        return INVALID_SOCKET;
      }
      break;
    }
  }
  DBG ("Opened port %d with fd %d", port, fd);

  assert (fd != INVALID_SOCKET);

  return fd;
}


bool
getMessage (Socket fd, char *buf, int *len,
            unsigned int *srcIp, unsigned short *srcPort)
{

  assert (fd != INVALID_SOCKET);

  int originalSize = *len;
  assert (originalSize > 0);

  struct sockaddr_in from;
  int fromLen = sizeof (from);

  *len = recvfrom (fd, buf, originalSize, 0, (struct sockaddr *) &from,
                   (socklen_t *) & fromLen);

  DBG ("Message received = '%s' (%d) ", buf, *len);

  if (*len == SOCKET_ERROR)
  {
    int err = errno;

    switch (err)
    {
    case ENOTSOCK:
      EXEC_ERROR("Error fd not a socket");
      break;
    case ECONNRESET:
      EXEC_ERROR("Error connection reset - host not reachable");
      break;
    default:
      EXEC_ERROR("Socket Error= %d", err);
    }
    return false;
  }

  if (*len < 0)
  {
    EXEC_ERROR("socket closed? negative len");
    return false;
  }

  if (*len == 0)
  {
    EXEC_ERROR("socket closed? zero len");
    return false;
  }

  *srcPort = ntohs (from.sin_port);
  *srcIp = ntohl (from.sin_addr.s_addr);

  if ((*len) + 1 >= originalSize)
  {
    EXEC_ERROR("Received a message that was too large");
    return false;
  }
  buf[*len] = 0;
  return true;
}


bool
sendMessage(Socket fd, char *buf, int l,
             unsigned int dstIp, unsigned short dstPort)
{
  /*char Buffer[255];
  bzero (Buffer, sizeof (Buffer));
  memcpy (Buffer, buf, l);*/

  assert (fd != INVALID_SOCKET);

  DBG ("Message to send (%d) : ", l);

  int s;
  if (dstPort == 0)
  {
    // sending on a connected port 
    assert (dstIp == 0);

    s = send (fd, buf, l, 0);
  }
  else
  {
    assert (dstIp != 0);
    assert (dstPort != 0);

    struct sockaddr_in to;
    int toLen = sizeof (to);
    memset (&to, 0, toLen);

    to.sin_family = AF_INET;
    to.sin_port = htons (dstPort);
    to.sin_addr.s_addr = htonl (dstIp);
    s = sendto (fd, buf, l, 0, (struct sockaddr *) &to, toLen);

  }

  if (s == SOCKET_ERROR)
  {
    int e = errno;
    switch (e)
    {
    case ECONNREFUSED:
    {
      EXEC_ERROR("err ECONNREFUSED in send");
    }
    break;
    case EHOSTDOWN:
    {
      EXEC_ERROR("err EHOSTDOWN in send");
    }
    break;
    case EHOSTUNREACH:
    {
      EXEC_ERROR("err EHOSTUNREACH in send");
    }
    break;
    case EAFNOSUPPORT:
      {
        EXEC_ERROR("err EAFNOSUPPORT in send");
      }
      break;
    default:
      {
        EXEC_ERROR("err %d %s in send", e, strerror (e));
      }
    }
    return false;
  }

  if (s == 0)
  {
    WARN ("no data sent in send");
    return false;
  }

  if (s != l)
  {
    WARN ("only %d out of %d bytes sent", s, l);
    return false;
  }

  return true;
}


void
initNetwork ()
{
#ifdef WIN32
  WORD wVersionRequested = MAKEWORD (2, 2);
  WSADATA wsaData;
  int err;

  err = WSAStartup (wVersionRequested, &wsaData);
  if (err != 0)
  {
    // could not find a usable WinSock DLL
    EXEC_ERROR("Could not load winsock");
    assert (0);                 // is this is failing, try a different version that 2.2, 1.0 or later will likely work 
    exit (1);
  }

  /* Confirm that the WinSock DLL supports 2.2. */
  /* Note that if the DLL supports versions greater    */
  /* than 2.2 in addition to 2.2, it will still return */
  /* 2.2 in wVersion since that is the version we      */
  /* requested.                                        */

  if (LOBYTE (wsaData.wVersion) != 2 || HIBYTE (wsaData.wVersion) != 2)
  {
    /* Tell the user that we could not find a usable */
    /* WinSock DLL.                                  */
    WSACleanup ();
    prinf ("Bad winsock verion\n");
    assert (0);                 // is this is failing, try a different version that 2.2, 1.0 or later will likely work 
    exit (1);
  }
#endif
}


/* ====================================================================
 * The Vovida Software License, Version 1.0 
 * 
 * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * 3. The names "VOCAL", "Vovida Open Communication Application Library",
 *    and "Vovida Open Communication Application Library (VOCAL)" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact vocal@vovida.org.
 *
 * 4. Products derived from this software may not be called "VOCAL", nor
 *    may "VOCAL" appear in their name, without prior written
 *    permission of Vovida Networks, Inc.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
 * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
 * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * 
 * ====================================================================
 * 
 * This software consists of voluntary contributions made by Vovida
 * Networks, Inc. and many individuals on behalf of Vovida Networks,
 * Inc.  For more information on Vovida Networks, Inc., please see
 * <http://www.vovida.org/>.
 *
 */

/* 
 * Contributions: 25/01/2009
 * Author       : Orange
 * Update the udp and stun source code for the static stun library
 * and adaptation of the trace system
 */

// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . +))
// indent-tabs-mode:nil
// End:
