/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C)  Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'.
 *
 *---------------------------------------------------------------------------
 * File        : dm_upnpigd.c
 *
 * Created     : 30/05/2017
 * Author      : Ying ZENG
 *
 *---------------------------------------------------------------------------
 *
 *  TODO :
 *
 *---------------------------------------------------------------------------
 */

/**
 * @file dm_upnpigd.c
 *
 * @brief upnpigd threads implementation
 *
 **/


// --------------------------------------------------------------------
// System headers
// --------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
/* for IPPROTO_TCP / IPPROTO_UDP */
#include <netinet/in.h>
#include <ctype.h>
// --------------------------------------------------------------------
// lib UPnP IGD's headers
// --------------------------------------------------------------------
#include "miniwget.h"
#include "miniupnpc.h"
#include "upnpcommands.h"
#include "upnperrors.h"
#include "miniupnpcstrings.h"
#include "dm_upnpigd.h"

// --------------------------------------------------------------------
// DM_ENGINE's header
// --------------------------------------------------------------------
#include "DM_ENG_RPCInterface.h"  		/* DM_Engine module definition    */
#include "DM_GlobalDefs.h"			/* */
#include "DM_ENG_Parameter.h"			/* */
#include "DM_ENG_Common.h"			/* */
#include "dm_com_utils.h"			/* */
#include "DM_COM_ConnectionSecurity.h"		/* */
#include "CMN_Trace.h"

// --------------------------------------------------------------------
// GLOBAL VALUES
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// New definitions
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// FUNCTIONS defined in the file
// --------------------------------------------------------------------

/**
 * @brief the dm_upnpigd thread
 *
 *
 * @param none
 *
 * @return pthread_exit
 *
 */

void*
DM_IGD_upnpigdThread()
{

}
