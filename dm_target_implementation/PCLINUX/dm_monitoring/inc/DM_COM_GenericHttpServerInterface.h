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
 * File        : DM_COM_GenericHttpServerInterface.h
 *
 * Created     : 2008/06/05
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
 * @file GenericHttpServerInterface.h
 *
 * @brief Generic Http Server Ineterface Declaration
 *
 */

#ifndef _GENERIC_HTTP_SERVER_INTERFACE_H
#define _GENERIC_HTTP_SERVER_INTERFACE_H

#include "dm_com.h"  /* global definition 	  */

#define	NET_INTERFACE       (INADDR_ANY)
#define	NET_FAMILY          (AF_INET)
#define	NET_TYPE_SOCKET     (SOCK_STREAM)
#define	NET_TYPE_PROTOCOLE  (0)
#define SIZE_HTTP_MSG       (4096)
#define SIZE_HTTP_DATE      (128)

#define CONNECTION_REQUESTION_PROTOCOL  "HTTP/1.1"
#define HTTP_VERSION                    "HTTP/"
#define CONNECTION_REQUESTION_METHOD    "GET"

#define SIZE_PATH          (256)
#define SIZE_USERNAME      (256)
#define SIZE_PASSWORD      (256)
#define	HTTP_RESPONSE      "HTTP/1.1 %s %s\n"
#define HTTP_DATETIME      "Date: %a, %d %b %Y %H:%M:%S GMT\n"
#define HTTP_LENGTH        "Content-Length: 0"
#define HTTP_CODE_OK       "200"
#define HTTP_STRING_OK     "OK"
#define HTTP_CODE_SVR_BUSY "503"
#define HTTP_STRING_SVR_BUSY "Service Unvailable"

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN (256)
#endif

#define MAX_BACKLOG_CONNECTION (5) // The maximum number of backlog connection on the CPE HTTP Server socket

#define DEFAULT_MAX_CONNECTIONREQUEST      (50)    // The Maximum number of connection request during the period time
#define DEFAULT_FREQ_CONNECTION_REQUEST    (3600)  // The period duration in seconds (Default 50 connections allowed per hour)

/**
 * Definition of the HTTP Return Codes 
 * Comform to the RFC 2616 - Hypertext Transfer Protocol - HTTP/1.1
 */
#ifndef HTTP_RC
#define HTTP_RC
// 1XX --------------------------------------------
// 100 : continue
#define HTTP_CONTINUE        (100)
// 101 : switching protocols
#define HTTP_SWITCH_PROTOCOL (101)
// 2XX --------------------------------------------
// 200 : ok
#define HTTP_OK         (200)
// 201 : created
#define HTTP_CREATED    (201)
// 202 : accepted
#define HTTP_ACCEPTED    (202)
// 203 : non-authoritative information
#define HTTP_NON_AUTHORITATIVE_INFORMATION	(203)
// 204 : no content
#define HTTP_NO_CONTENT         (204)
// 205 : reset content
#define HTTP_RESET_CONTENT      (205)
// 206 : partial content
#define HTTP_PARTIAL_CONTENT	  (206)
// 3XX --------------------------------------------
// 300 : multiple choice
#define HTTP_MULTIPLE_CHOICE	  (300)
// 301 : moved permanently
#define HTTP_MOVED_PERMANENTLY	(301)
// 302 : moved temporarily
#define HTTP_MOVED_TEMPORARILY1	(302)
// 303 : see other / method 
#define HTTP_SEE_OTHER_METHOD	  (303)
// 304 : not modified
#define HTTP_NOT_MODIFIED	      (304)
// 305 : use proxy 
#define HTTP_USE_PROXY          (305)
// 306 : unused 
#define HTTP_UNUSED             (306)
// 307 : moved temporarily
#define HTTP_MOVED_TEMPORARILY2	(307)
// 4XX --------------------------------------------
// 400 : bad request
#define HTTP_BAD_REQUEST	  (400)
// 401 : unauthorized
#define	HTTP_UNAUTHORIZED	  (401)
// 402 : payment required
#define HTTP_PAYMENT_REQUIRED	  (402)
// 403 : forbidden
#define HTTP_FORBIDDEN    (403)
// 404	not found
#define HTTP_NOT_FOUND    (404)
// 405 : method not allowed (about MIME type)
#define HTTP_METHOD_NOT_ALLOWED	  (405)
// 406 : not acceptable
#define HTTP_NOT_ACCEPTABLE	  (406)
// 407 : proxy authentication required
#define HTTP_PROXY_AUTHENTIFICATION_REQUIRED	(407)
// 408 : timeout	
#define HTTP_TIMEOUT    (408)
// 410 : gone
#define HTTP_GONE    (410)
// 411 : length required
#define HTTP_LENGTH_REQUIRED	  (411)
// 413 : too large
#define HTTP_TOO_LARGE    (413)
// 414 : URI to large 
#define HTTP_URI_TOO_LARGE	  (414)
// 415	unsupported media type
#define	HTTP_UNSUPPORTED_MEDIA_TYPE  (415)
// 5XX --------------------------------------------
// 500 : internal server error (hardware or sotware)
#define HTTP_INTERNAL_SERVER_ERROR  (500)
// 501 : not implemented 
#define HTTP_NOT_IMPLEMENTED	  (501)
// 502 : bad gateway
#define HTTP_BAD_GATEWAY	  (502)
// 503 : service unavailable
#define HTTP_SERVICE_UNAVAILABLE	  (503)
// 504 : gateway timeout (proxy or gateway)
#define HTTP_GATEWAY_TIMEOUT	  (504)
// 505 : version not supported
#define HTTP_VERSION_NOT_SUPPORTED  (505)
#endif




/**
 * @brief Launch the http server an infinite loop
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
void*
DM_HttpServer_Start(void* data);



#endif /* _GENERIC_HTTP_SERVER_INTERFACE_H */
