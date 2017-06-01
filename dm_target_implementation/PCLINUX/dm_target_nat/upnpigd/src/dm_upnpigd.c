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
#include <sys/time.h>				/* */
#include <arpa/inet.h>				/* For inet_aton            */
#include <assert.h>				/* */
#include <stdbool.h>				/* Need for the bool type   */
#include <unistd.h>       /*for sleep function*/
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
// variables for upnpDiscoverIGD
const char * multicastif = 0;
const char * minissdpdpath = 0;
int ipv6 = 0;
unsigned char ttl = 2;
int error = 0;
//struct for Discover results
struct UPNPDev * devlist = 0;
struct UPNPDev * dev;
// variables for UPNP_GetValidIGD()
char lanaddr[40] = "unset";	/* ip address on the LAN */
char * remoteHost = NULL;
char extPort[6] = "7547";
// char extPort[6] = DM_ENG_intToString(extPort_int);
char inPort[6] = "50805";
char * proto = "TCP";
char leaseDuration[16]= "3600";

int i,ret;
// --------------------------------------------------------------------
// New definitions
// --------------------------------------------------------------------
#define IGD_SLEEP_TIME_ON_LeaseDuration (60) /*Default value 3600*/

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
  do{
    DBG("Threadupnpigd Test!");

    devlist = upnpDiscoverIGD(2000, multicastif, minissdpdpath,
                             0/*localport*/, ipv6, ttl, &error);
    if (devlist){
      for(dev = devlist, i = 1; dev != NULL; dev = dev->pNext, i++) {
        struct UPNPUrls urls;
        struct IGDdatas data;
      	int state = UPNP_GetValidIGD(dev, &urls, &data, lanaddr, sizeof(lanaddr));
        ret = UPNP_AddPortMapping(urls.controlURL /*controlURL*/, data.first.servicetype /*servicetype*/,
                             extPort /*extPort*/,
                             inPort /*inPort*/,
                             lanaddr /*intClient*/,
                             "WANConnection:1 addportmapping" /*desc*/,
                             proto /*proto UPPERCASE*/,
                             remoteHost /*remoteHost*/,
                             leaseDuration /*leaseDuration*/);
       if (ret == UPNPCOMMAND_SUCCESS){
         DBG("Re-active port mapping success!!! external port actived is %s", extPort);
       } else{
         DBG("Re-active port mapping not success!!! Error code %d, %s", ret, strupnperror(ret));
       }
       sleep(IGD_SLEEP_TIME_ON_LeaseDuration); //seconds
      }
    }
  }
  while (true);
}
