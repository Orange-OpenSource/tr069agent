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
 * File        : dm_com_digest.h
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
 * @file dm_com_digest.h
 *
 *
 */
#ifndef __DM_COM_DIGEST__
#define __DM_COM_DIGEST__

#include "md5.h"
#include "dm_com.h"


#define AuthorizationToken   "Authorization:"
#define DigestUsernameToken  "Digest username="
#define realmToken           "realm="
#define nonceToken           "nonce="
#define uriToken             "uri="
#define responseToken        "response="
#define opaqueToken          "opaque="
#define qopToken             "qop="
#define ncToken              "nc="
#define cnonceToken          "cnonce="

#define defaultCpeUsername "lbOui-liveBoxProductClass-123456789"

#define MAXTOKENSIZE        (100)
#define HA1_MAXTOKENSIZE    (3*MAXTOKENSIZE)
#define HA2_MAXTOKENSIZE    (2*MAXTOKENSIZE)
#define TOTAL_MAXTOKENSIZE  (6*MAXTOKENSIZE)
#define SIGNATURE_SIZE      (16)


bool 
_checkDigestAuthMessageContent(const char * _str);

bool
performClientDigestAuthentication(const char * _str);

#endif


