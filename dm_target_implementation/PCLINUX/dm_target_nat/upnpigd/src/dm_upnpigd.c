/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'.
 *
 *---------------------------------------------------------------------------
 *
 */


// --------------------------------------------------------------------
// System headers
// --------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <winsock2.h>
#define snprintf _snprintf
#else
/* for IPPROTO_TCP / IPPROTO_UDP */
#include <netinet/in.h>
#endif
#include <ctype.h>

// --------------------------------------------------------------------
// lib IGD's headers
// --------------------------------------------------------------------
#include "miniwget.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"
#include "miniupnpcstrings.h"


void*
DM_IGD_upnpigdThread()
{

}
