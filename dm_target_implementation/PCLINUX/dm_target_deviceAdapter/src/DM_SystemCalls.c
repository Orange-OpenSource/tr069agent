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
 * File        : DM_SystemCalls.c
 *
 * Created     : 22/05/2008
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file DM_SystemCalls
 *
 * @brief 
 *
 **/

#include "DM_SystemCalls.h"
#include "DM_ENG_Error.h"
#include "CMN_Trace.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#endif

extern char* ETH_INTERFACE;

static char* ipAddress = NULL;
//static const char* DOWNLOAD_WEB_DIR = "../Download/";
static const char* LOG_DIR = "/var/log/adm/";

static struct sysinfo _info;
static bool _infoLoaded = false;

void DM_SystemCalls_reboot(bool factoryReset UNUSED)
{
}

/**
 * Returns the IP Address.
 *
 * @param objName A partial path name.
 *
 * @return Returns the IP Address or NULL if not found.
 */
char* DM_SystemCalls_getIPAddress()
{
   if (ipAddress == NULL)
   {
#ifdef WIN32
      FILE *pp;
      char buf[200];

      if ((pp = popen("ipconfig", "r")) == NULL)
      {
         perror("popen");
         exit(1);
      }
      while (fgets(buf, sizeof(buf), pp))
      {
         if (strstr(buf, "Adresse IP")!=NULL)
         {
            int last = strlen(buf)-1;
            while((buf[last]=='\n') || (buf[last]=='\r')) { buf[last--] = '\0'; }
            ipAddress = strdup(strstr(buf, ": ") + 2);
            break;
         }
   //    fputs(buf, stdout);
      }
      pclose(pp);

      if (ipAddress == NULL)
      {
         WARN("IP Address not found");
      }
      else
      {
         DBG("Retrieving IP Address : %s", ipAddress);
      }      
#else
      struct ifaddrs *myaddrs = NULL, *ifa = NULL;
      struct sockaddr_in *s4 = NULL;
      int  status;
      /* buf must be big enough for an IPv6 address (e.g. 3ffe:2fa0:1010:ca22:020a:95ff:fe8a:1cf8) */
      char buf[64];
      memset((void *) buf,  0x00, sizeof(buf));

      status = getifaddrs(&myaddrs);
      if (status == 0)
      {
         for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
         {
            if ( (ifa->ifa_addr != NULL)
               && ((ifa->ifa_flags & IFF_UP) != 0)
               && (ifa->ifa_addr->sa_family == AF_INET) )
            {
               s4 = (struct sockaddr_in *)(ifa->ifa_addr);
               if ( (inet_ntop(ifa->ifa_addr->sa_family, (void *)&(s4->sin_addr), buf, sizeof(buf)) != NULL)
                  && (strcmp(ifa->ifa_name, ETH_INTERFACE)==0) )
               {
                  ipAddress = strdup(buf);
                  break;
               }
            }
         }
      }
      if (ipAddress == NULL)
      {
         WARN("IP Address not found. Is '%s' the good interface ?", ETH_INTERFACE);
      }
      else
      {
         DBG("Retrieving IP Address : %s", ipAddress);
      }      

      freeifaddrs(myaddrs);
#endif
   }
   return ipAddress;
}

void DM_SystemCalls_freeSessionData()
{
   _infoLoaded = false;
}

char* _loadSystemResponse(const char* data)
{
   char* res = NULL;
   FILE *pp;
   char buf[200];

   if ((pp = popen(data, "r")) == NULL)
   {
      perror("popen");
      return NULL;
   }
   if (fgets(buf, sizeof(buf), pp))
   {
      int len = strlen(buf);
      if ((len>0) && (buf[len-1] == '\n')) {  buf[len-1] = '\0'; }
      res = strdup(buf);
   }
   pclose(pp);
   return res;
}

char* DM_SystemCalls_getSystemData(const char* name, const char* data)
{
   char* sVal = NULL;
#ifndef WIN32
   if (strcmp(name, "DeviceInfo.UpTime") == 0)
   {
      if (!_infoLoaded) { _infoLoaded = (sysinfo(&_info) == 0); }
      if (_infoLoaded) { sVal = DM_ENG_longToString(_info.uptime); }
   }
   else if ((data != NULL) && (*data != '\0'))
   {
      sVal = _loadSystemResponse(data);
   }
#endif
   return sVal;
}


bool DM_SystemCalls_getSystemObjectData(const char* objName UNUSED, OUT DM_ENG_ParameterValueStruct** pParamVal[] UNUSED)
{
   // TO BE COMPLETED
   return false;
}

bool DM_SystemCalls_setSystemData(const char* name UNUSED, const char* value UNUSED)
{
   return true;
}

/**
 * Performs a ping test.
 * 
 * @param host
 * @param numberOfRepetitions
 * @param timeout
 * @param dataSize
 * @param dscp
 * @param pFoundHost
 * @param pRecus
 * @param pPerdus
 * @param pMoy
 * @param pMin
 * @param pMax
 *
 * @return Returns O if OK or an error code otherwise
 */
int DM_SystemCalls_ping(char* host, int numberOfRepetitions, long timeout, unsigned int dataSize, unsigned int dscp UNUSED,
                        bool* pFoundHost, int* pRecus, int* pPerdus, int* pMoy, int* pMin, int* pMax)
{
   FILE* pp;
   char buf[200];
   int toSec = timeout / 1000; // timeout en secondes pour la commande ping
   sprintf(buf, "ping -c %d -s %d -w %d %s", numberOfRepetitions, dataSize, toSec, host);
   DBG("Performing %s", buf);

   if ((pp = popen(buf, "r")) == NULL)
   {
      perror("popen");
      return DM_ENG_INTERNAL_ERROR;
   }
   while (fgets(buf, sizeof(buf), pp))
   {
      char* token[4];
      int len = strlen(buf);
      int c = 0; // index caractère
      int i = 0; // index parametre
      char* rttPrefix = "rtt min/avg/max/mdev = ";
      if (strncmp(buf, rttPrefix, strlen(rttPrefix))==0)
      {
         c = strlen(rttPrefix);
      }
      while ((c<=len) && (i<4))
      {
         token[i++] = buf + c;
         while ((c<len) && (buf[c] != ',') && (buf[c] != '/')) { c++; }
         buf[c++] = '\0';
      }
      if (i<3) { continue; }
      if (strstr(buf, "packets")!=NULL)
      {
         *pRecus = atoi(token[1]);
         *pPerdus = atoi(token[0]) - *pRecus;
      }
      else
      {
         *pFoundHost = true;
         *pMin = (int)(atof(token[0])+0.5);
         *pMax = (int)(atof(token[2])+0.5);
         *pMoy = (int)(atof(token[1])+0.5);
      }
   }
   pclose(pp);
   return 0;
}

/**
 * Performs a traceroute test.
 * 
 * @param host
 * @param timeout
 * @param dataBlockSize
 * @param maxHopCount
 * @param pFoundHost
 * @param pResponseTime
 * @param pRouteHops
 *
 * @return Returns O if OK or an error code otherwise
 */
int DM_SystemCalls_traceroute(char* host, long timeout, int dataBlockSize, int maxHopCount,
                              bool* pFoundHost, int* pResponseTime, char** pRouteHops[])
{
   FILE* pp;
   char buf[200];
   int toSec = timeout / 1000; // timeout en secondes pour la commande traceroute
   double moy = 0;
   sprintf(buf, "traceroute -w %d -m %d %s %d", toSec, maxHopCount, host, dataBlockSize);
   DBG("Performing %s", buf);

   if ((pp = popen(buf, "r")) == NULL)
   {
      perror("popen");
      return DM_ENG_INTERNAL_ERROR;
   }
   char** routeHops = (char**)calloc(maxHopCount+1, sizeof(char*));
   int nbHops = 0;
   bool foundHost = false;
   while (fgets(buf, sizeof(buf), pp))
   {
      foundHost = false;
      moy = 0;
      char* token[10];
      int len = strlen(buf);
      int c = 0; // index caractère
      int i = 0; // index parametre
      char* traceroutePrefix = "traceroute";
      if (strncmp(buf, traceroutePrefix, strlen(traceroutePrefix))==0) { continue; }
      while ((c<len) && (i<10))
      {
         while ((c<len) && (buf[c] == ' ')) { c++; } // on passe les blancs
         token[i++] = buf + c;
         while ((c<len) && (buf[c] != ' ')) { c++; } // on passe tous les car non blancs
         buf[c++] = '\0';
      }
      routeHops[nbHops++] = strdup(token[1]);
      if (i<9) { continue; }

      double t1 = atof(token[3]); // ou strtod( token[3], &end );
      if (t1 == 0) continue; // ds ce cas, token[3] n'est pas numérique (sinon nécessairement > 0) car pas de réponse de la gateway (token "*")
      double t2 = atof(token[5]);
      if (t2 == 0) continue;
      double t3 = atof(token[7]);
      if (t3 == 0) continue;

      foundHost = true;
      moy = (t1+t2+t3) / 3;
   }
   *pFoundHost = foundHost;
   if (foundHost)
   {
      *pResponseTime = (int)(moy+0.5);
      *pRouteHops = routeHops;
   }
   else
   {
      int i;
      for (i=0; routeHops[i] != NULL; i++) { free(routeHops[i]); }
      free(routeHops);
   }
   pclose(pp);
   return 0;
}

/**
 * Performs a file download.
 * 
 * @param fileType
 * @param url
 * @param username
 * @param password
 * @param fileSize
 * @param targetFileName
 *
 * @return Returns O if OK or an error code otherwise
 */
int DM_SystemCalls_download(char* fileType, char* url, char* username, char* password, unsigned int fileSize UNUSED, char* targetFileName)
{
   char buf[200];
   int  nType;
   char *pStrPath[] = { "/tmp/adm/", "/root/OpenSTB/browser/demo/mosaic/", "" };
   
   // --> downloadExecDM_SystemCalls_download: type = '2' DM_SystemCalls_download: cmd = 'wget -o /var/log/adm/log.txt --ftp-user=cwmp --ftp-password=cwmp -O mosaic_jb.htm ftp://192.168.1.5/mosaic_jb.htm'

   nType = atoi( fileType );
   printf( "\nDM_SystemCalls_download: type = '%d' - '%s' ", nType, pStrPath[nType-1] );
   if ( targetFileName == NULL )
   {
      printf( "\nTargetFileName undefined " );
      sprintf( buf,
          "wget -o %slog.txt --ftp-user=%s --ftp-password=%s -P %s %s",
          LOG_DIR,
          username,
          password,
          pStrPath[nType-1],
          url );
   } else   {
      printf( "\nTargetFileName well defined " );
      sprintf( buf,
          "wget -o %slog.txt --ftp-user=%s --ftp-password=%s -O %s%s %s",
          LOG_DIR,
          username,
          password,
          pStrPath[nType-1],
          targetFileName,
          url );
   }
   printf( "\nDM_SystemCalls_download: cmd = '%s' ", buf );
   system( buf );
   
   return 0;
}
