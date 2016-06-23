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
 * File        : dm_com_digest.c
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
 * @file dm_com_digest.c
 *
 *
 * This file performs the ACS Digest Autkentication
 *
 */

#include "dm_com_digest.h"
#include "DM_ENG_Global.h"
#include "CMN_Trace.h"

#define requestDigestAuthStr "HTTP/1.1 401 Unauthorized\nWWW-Authenticate: Digest\n   realm=\"liveboxrealm\",\n   qop=\"auth\",\n   nonce=\"%s\",\n   opaque=\"%s\"\n"

#define NONCESIZE  (34)
#define OPAQUESIZE (32)
static char randomNonceStr[NONCESIZE + 1];  // Max(NONCESIZE, OPAQUESIZE) + 1
static char randomOpaqueStr[NONCESIZE + 1]; // Max(NONCESIZE, OPAQUESIZE) + 1

extern char * g_randomCpeUrl;

/**
 * @brief Private routine used to send an authentication
 *        request tp the ACS.
 *
 * @param The socket aon wich the message is sent
 *
 * @Return OK DM_OK or ERROR
 *
 */
DMRET
_sendAuthenticationRequest(int nSocksExp)
{
  char requestDigestMsg[SIZE_HTTP_MSG]; // No need to require a very large buffer.
  DMRET nRet = DM_ERR;
  
  DBG( "Send Authentication Digest Request to the ACS" );
  
  memset((void *)requestDigestMsg, 0x00, SIZE_HTTP_MSG);
   
  memset((void *) &randomNonceStr[0],  '\0', sizeof(randomNonceStr));
  memset((void *) &randomOpaqueStr[0], '\0', sizeof(randomOpaqueStr));
  
  // Generate a random nonce and opaque string
  if((!_generateRandomString(randomNonceStr , NONCESIZE)) ||
     (!_generateRandomString(randomOpaqueStr , OPAQUESIZE))) {
     EXEC_ERROR("CAN NOT GENERATE RANDOM STRING");
     return nRet;
  }

  snprintf(requestDigestMsg, SIZE_HTTP_MSG, requestDigestAuthStr, randomNonceStr, randomOpaqueStr);
  DBG( "Request Digest Authentication Message:\n%s", requestDigestMsg );

  // Send the message to the ACS and Wait for the response.
#ifndef WIN32
  if(-1 ==  send( nSocksExp, requestDigestMsg, strlen(requestDigestMsg), MSG_DONTWAIT )){
    EXEC_ERROR("Error sending Authentication Request message on socket!!!");
  } else {
    nRet = DM_OK;
  }
#else
  if(-1 ==  send( nSocksExp, requestDigestMsg, strlen(requestDigestMsg), 0 )){
    EXEC_ERROR("Error sending Authentication Request message on socket!!!");
  } else {
    nRet = DM_OK;
  }
#endif

  return nRet; 
}

/**
 * @brief Private routine used to check if all
 *        the digest authentication data are in the message
 *
 * @param A pointer on the message to analyse
 *
 * @Return TRUE if all the digest authentication data are in the message
 *         FALSE otherwise
 *
 */
bool 
_checkDigestAuthMessageContent(const char * _str)
{
  // Make sure "Authorization:", "Digest username=", "realm=", "nonce=", 
  // "uri=", "response=", "opaque=", "qop=", "nc" and "cnonce" are in the message

  if((NULL == strstr(_str , AuthorizationToken))  ||
     (NULL == strstr(_str , DigestUsernameToken)) ||
     (NULL == strstr(_str , realmToken))          ||
     (NULL == strstr(_str , nonceToken))          ||
     (NULL == strstr(_str , uriToken))            ||
     (NULL == strstr(_str , responseToken))       ||
     (NULL == strstr(_str , opaqueToken))         ||
     (NULL == strstr(_str , qopToken))            ||
     (NULL == strstr(_str , ncToken))             ||
     (NULL == strstr(_str , cnonceToken))) {
       DBG("No Digest Authentication Data found (or partial)");
       return false;
     }

   DBG("Digest Authentication Data found");
   return true;
}

/**
 * @brief Private routine used to get the string betwen quote ("The stream to retrieve")
 *
 * @param IN: A pointer on the message to analyse. The string to retrieve is the next string between "".
 *        OUT: The requested string. The memory must be allocated by the calling routine.
 *
 * @Return TRUE if everything if fine
 *         FALSE otherwise.
 *
 */
static bool _getQuotedString(const char* _inTokenStr, char* _outQuotedStr)
{
  char * firstTokenStr  = NULL;
  char * secondTokenStr = NULL;
  char   tmpQuotedStr[MAXTOKENSIZE];
  int    i = 0;

  memset((void*) tmpQuotedStr, 0x00, MAXTOKENSIZE);

  // Get the string between the first '"' and the second '"'.
  // Find the first '"'
  firstTokenStr = strchr(_inTokenStr, '"');
  if(NULL == firstTokenStr) {
    // DBG("Can not find the second '"'\n");
    return false;
  } 
  // Increment firstTokenStr to go to the next charactere
  firstTokenStr++;

  // Find the second '"'
  secondTokenStr = strchr(firstTokenStr, '"');
  if(NULL == secondTokenStr) {
    // DBG("Can not find the first '"'\n");
    return false;
  } 

  while(firstTokenStr != secondTokenStr) {
    tmpQuotedStr[i] = *firstTokenStr;
    firstTokenStr++;
    i++;
    if(i >= MAXTOKENSIZE) {
      // Error, MAXTOKENSIZE reached
      return false;
    }
  }

  strncpy(_outQuotedStr, &tmpQuotedStr[0], MAXTOKENSIZE);

  return true;
}

/**
 * @brief Private routine used to get the string betwen = and , (=The stream to retrieve,)
 *
 * @param IN: A pointer on the message to analyse. The string to retrieve is the next string between =...,
 *        OUT: The requested string. The memory must be allocated by the calling routine.
 *
 * @Return TRUE if everything if fine
 *         FALSE otherwise.
 *
 */
static bool _getNonQuotedString(const char* _inTokenStr, char* _outNonQuotedStr)
{
  char * firstTokenStr  = NULL;
  char * secondTokenStr = NULL;
  char   tmpQuotedStr[MAXTOKENSIZE];
  int    i = 0;

  memset((void*) tmpQuotedStr, 0x00, MAXTOKENSIZE);

  // Get the string between '=' and ','.
  // Find the first '='
  firstTokenStr = strchr(_inTokenStr, '=');
  if(NULL == firstTokenStr) {
    // DBG("Can not find the second '='\n");
    return false;
  } 
  // Increment firstTokenStr to go to the next charactere
  firstTokenStr++;

  // Find the second token ','
  secondTokenStr = strchr(firstTokenStr, ',');
  if(NULL == secondTokenStr) {
    // DBG("Can not find the first ','\n");
    return false;
  }

  while(firstTokenStr != secondTokenStr) {
    tmpQuotedStr[i] = *firstTokenStr;
    firstTokenStr++;
    i++;
    if(i >= MAXTOKENSIZE) {
      // Error, MAXTOKENSIZE reached
      return false;
    }
  }

  strncpy(_outNonQuotedStr, &tmpQuotedStr[0], MAXTOKENSIZE);

  return true;
}

/**
 * @brief Private routine used to get all the Digest Authentication Data
 *
 * @param 
 *
 * @Return TRUE if everything if fine
 *         FALSE otherwise.
 *
 */
static bool _getDigestMessageToken(
  const char * _str,
  char * digestUsernameTokenStr,
  char * realmTokenStr,
  char * nonceTokenStr,
  char * uriTokenStr,
  char * responseTokenStr,
  char * opaqueTokenStr,
  char * qopTokenStr,
  char * ncTokenStr,
  char * cnonceTokenStr) {

  char * tmpStr = NULL;

  DBG("HTTP Message to analyse:\n%s", _str);

  // ---------- Retrieve the digestUsernameTokenStr ----------
  tmpStr = strstr(_str , DigestUsernameToken);
  if(!_getQuotedString(tmpStr, digestUsernameTokenStr)) {
    DBG("Can not retrieve the DigestUsername");
    return false;
  }
  DBG("digestUsernameTokenStr=%s", digestUsernameTokenStr);

  // ---------- Retrieve the realmTokenStr ----------
  tmpStr = strstr(_str , realmToken);
  if(!_getQuotedString(tmpStr, realmTokenStr)) {
    DBG("Can not retrieve the realmTokenStr");
    return false;
  }
  DBG("realmTokenStr=%s", realmTokenStr);

  // ---------- Retrieve the nonceTokenStr ----------
  tmpStr = strstr(_str , nonceToken);
  if(!_getQuotedString(tmpStr, nonceTokenStr)) {
    DBG("Can not retrieve the nonceTokenStr");
    return false;
  }
  DBG("nonceTokenStr=%s", nonceTokenStr);

  // ---------- Retrieve the uriTokenStr ----------
  tmpStr = strstr(_str , uriToken);
  if(!_getQuotedString(tmpStr, uriTokenStr)) {
    DBG("Can not retrieve the uriTokenStr");
    return false;
  }
  DBG("uriTokenStr=%s", uriTokenStr);

  // ---------- Retrieve the responseTokenStr ----------
  tmpStr = strstr(_str , responseToken);
  if(!_getQuotedString(tmpStr, responseTokenStr)) {
    DBG("Can not retrieve the responseTokenStr");
    return false;
  }
  DBG("responseTokenStr=%s", responseTokenStr);

  // ---------- Retrieve the opaqueTokenStr ----------
  tmpStr = strstr(_str , opaqueToken);
  if(!_getQuotedString(tmpStr, opaqueTokenStr)) {
    DBG("Can not retrieve the opaqueTokenStr");
    return false;
  }
  DBG("opaqueTokenStr=%s", opaqueTokenStr);

  // ---------- Retrieve the qopTokenStr ----------
  tmpStr = strstr(_str , qopToken);
  if(!_getNonQuotedString(tmpStr, qopTokenStr)) {
    DBG("Can not retrieve the qopTokenStr");
    return false;
  }
  DBG("qopTokenStr=%s", qopTokenStr);

  // ---------- Retrieve the ncTokenStr ----------
  tmpStr = strstr(_str , ncToken);
  if(!_getNonQuotedString(tmpStr, ncTokenStr)) {
    DBG("Can not retrieve the ncTokenStr");
    return false;
  }
  DBG("ncTokenStr=%s", ncTokenStr);

  // ---------- Retrieve the cnonceTokenStr ----------
  tmpStr = strstr(_str , cnonceToken);
  if(!_getQuotedString(tmpStr, cnonceTokenStr)) {
    DBG("Can not retrieve the cnonceTokenStr");
    return false;
  }
  DBG("cnonceTokenStr=%s", cnonceTokenStr);
  
  return true;
}


/**
 * @brief Private routine used to check if the Digest Authentication message can be accepted or rejected.
 *
 * @param 
 *
 * @Return TRUE if everything if fine
 *         FALSE otherwise.
 *
 */
bool
performClientDigestAuthentication(const char * _str){

  bool nRet = false;

  DBG("DIGEST AUTH - Str:\n%s", _str);

  unsigned char signature[SIGNATURE_SIZE];

  char digestUsernameTokenStr[MAXTOKENSIZE];
  char realmTokenStr[MAXTOKENSIZE];
  char nonceTokenStr[MAXTOKENSIZE];
  char uriTokenStr[MAXTOKENSIZE];
  char responseTokenStr[MAXTOKENSIZE];
  char opaqueTokenStr[MAXTOKENSIZE];
  char qopTokenStr[MAXTOKENSIZE];
  char ncTokenStr[MAXTOKENSIZE];
  char cnonceTokenStr[MAXTOKENSIZE];
  char * httpMethodPtr = NULL;
  char pBufferTemp[SIZE_HTTP_MSG]; // No need to require a very large buffer.

  // Below variables used to compute the response
  MD5_CTX ha1Ctx;
  MD5_CTX ha2Ctx;
  MD5_CTX responseCtx;
  char ha1Str[HA1_MAXTOKENSIZE];
  char ha2Str[HA2_MAXTOKENSIZE];
  char totalStr[TOTAL_MAXTOKENSIZE];
  char ha1StrResult[MAXTOKENSIZE];
  char ha2StrResult[MAXTOKENSIZE];
  char totalStrResult[MAXTOKENSIZE];

  memset((void*) digestUsernameTokenStr, 0x00, MAXTOKENSIZE);
  memset((void*) realmTokenStr,          0x00, MAXTOKENSIZE);
  memset((void*) nonceTokenStr,          0x00, MAXTOKENSIZE);
  memset((void*) uriTokenStr,            0x00, MAXTOKENSIZE);
  memset((void*) responseTokenStr,       0x00, MAXTOKENSIZE);
  memset((void*) opaqueTokenStr,         0x00, MAXTOKENSIZE);
  memset((void*) qopTokenStr,            0x00, MAXTOKENSIZE);
  memset((void*) ncTokenStr,             0x00, MAXTOKENSIZE);
  memset((void*) cnonceTokenStr,         0x00, MAXTOKENSIZE);
  memset((void*) pBufferTemp,            0x00, SIZE_HTTP_MSG);  

  memset((void *) ha1Str,                0x00, HA1_MAXTOKENSIZE);
  memset((void *) ha2Str,                0x00, HA2_MAXTOKENSIZE);
  memset((void *) totalStr,              0x00, TOTAL_MAXTOKENSIZE);

  // Get all the digest data from the message
  if(!_getDigestMessageToken(_str,
                             digestUsernameTokenStr,
                             realmTokenStr,
                             nonceTokenStr,
                             uriTokenStr,
                             responseTokenStr,
                             opaqueTokenStr,
                             qopTokenStr,
                             ncTokenStr,
                             cnonceTokenStr)) {
    EXEC_ERROR("Can not retrieve all the HTTP Digest Tokens");
    return false;
	}

	// Make sure the nonce and opaque str are identical to 
	// the strings sent in the request authentication demand message
	// Note: These strings are generated randomly
	if((0 != strcmp(randomNonceStr, nonceTokenStr))   ||
	   (0 != strcmp(randomOpaqueStr, opaqueTokenStr))) {
    EXEC_ERROR("Authentication request does not match the generated Authentication demand message");
    EXEC_ERROR("Nonce and opaue strings are diferent from the generated ones.");
    return false;	   
	} else {
	  DBG("HTTP Digest MD5 Auth - Nonce and Opaque are valid.");
	}

	// Make sure the requested URL corrspond to the randomly choosen one.
	if(NULL == strstr(uriTokenStr, g_randomCpeUrl)) {
	  WARN("--------- The requested and choosen URL are not equal");
	  WARN("--------- Requested: %s, randomly Choosen by CPE: %s", uriTokenStr, g_randomCpeUrl);
	  WARN("Must return FALSE but for test purpose and while DM_ENGINE does not use g_randomCpeUrl continue...");
	  // return FALSE;
  } else {
    DBG("HTTP Digest MD5 Auth - CPE URL is Valid.");
  }

  char* parameterNames[] = { (char*)DM_TR106_CONNECTIONREQUESTUSERNAME, (char*)DM_TR106_CONNECTIONREQUESTPASSWORD, NULL };
  DM_ENG_ParameterValueStruct** result = NULL;

  char* cpeUsername = NULL;
  char* cpeConnectionRequestPassword = NULL;

  if ((DM_ENG_GetParameterValues( DM_ENG_EntityType_ANY, parameterNames, &result ) == 0)
   && (result != NULL) && (result[0] != NULL) && (result[1] != NULL)) // luxe de conditions normalement inutiles
  {
     cpeUsername = result[0]->value;
     cpeConnectionRequestPassword = result[1]->value;
  }

  if ((cpeUsername == NULL) || (cpeConnectionRequestPassword == NULL))
  {
    EXEC_ERROR("Can not retrieve the CPE Connection Request Username and Password");
    return false;  
  }

  DBG("CPE username: %s, password: %s", cpeUsername, cpeConnectionRequestPassword);

  // Build ha1Str (username:realm:password)
  strncpy(ha1Str, cpeUsername,                  HA1_MAXTOKENSIZE);
  strncat(ha1Str, ":",                          HA1_MAXTOKENSIZE - strlen(ha1Str));
  strncat(ha1Str, realmTokenStr,                HA1_MAXTOKENSIZE - strlen(ha1Str));
  strncat(ha1Str, ":",                          HA1_MAXTOKENSIZE - strlen(ha1Str));
  strncat(ha1Str, cpeConnectionRequestPassword, HA1_MAXTOKENSIZE - strlen(ha1Str));

  DM_ENG_deleteTabParameterValueStruct(result);

	// Compute the response.
  DBG("DIGEST AUTH - ha1Str:%s", ha1Str);

  MD5_Init (&ha1Ctx);  
  MD5_Update (&ha1Ctx, (unsigned char *) ha1Str, strlen(ha1Str));
  memset((void *) signature, 0x00, SIGNATURE_SIZE);
  MD5_Final (signature, &ha1Ctx);
  getStringResult(signature, ha1StrResult);
  DBG("DIGEST AUTH - MD5(ha1Str):%s", ha1StrResult);

  // Build ha2Str (HTTPCommand:uri)
  // Recopy the buffer into a temp one to use strtok
  strncpy( pBufferTemp, _str, SIZE_HTTP_MSG );
  httpMethodPtr   = strtok( pBufferTemp, " " );
  strncpy(ha2Str, httpMethodPtr, HA2_MAXTOKENSIZE);
  strncat(ha2Str, ":",           HA2_MAXTOKENSIZE - strlen(ha2Str));
  strncat(ha2Str, uriTokenStr,   HA2_MAXTOKENSIZE - strlen(ha2Str));

  DBG("DIGEST AUTH - ha2Str:%s\n", ha2Str);

  MD5_Init (&ha2Ctx);  
  MD5_Update (&ha2Ctx, (unsigned char *) ha2Str, strlen(ha2Str));
  memset((void *) signature, 0x00, SIGNATURE_SIZE);
  MD5_Final (signature, &ha2Ctx);
  getStringResult(signature, ha2StrResult);
  DBG("DIGEST AUTH - MD5(ha2Str):%s\n", ha2StrResult);

  // Build totalStr (ha1:noncevalue:nc-value:cnonce-value:qop:ha2)
  strncpy(totalStr, ha1StrResult,   TOTAL_MAXTOKENSIZE);
  strncat(totalStr, ":",            TOTAL_MAXTOKENSIZE - strlen(totalStr) );
  strncat(totalStr, nonceTokenStr,  TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ":",            TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ncTokenStr,     TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ":",            TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, cnonceTokenStr, TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ":",            TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, qopTokenStr,    TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ":",            TOTAL_MAXTOKENSIZE - strlen(totalStr));
  strncat(totalStr, ha2StrResult,   TOTAL_MAXTOKENSIZE - strlen(totalStr));

  DBG("DIGEST AUTH - totalStr:%s\n", totalStr);

  MD5_Init (&responseCtx);
  MD5_Update (&responseCtx, (unsigned char *) totalStr, strlen(totalStr));
  memset((void *) signature, 0x00, SIGNATURE_SIZE);
  MD5_Final (signature, &responseCtx);
  getStringResult(signature, totalStrResult);

  // Compare the computed result and the transmitted one.
  if(0 == strcmp(totalStrResult, responseTokenStr)) {
    INFO("DIGEST AUTH - AUTHENTICATION OK");
    nRet = true;
  } else {
    WARN("DIGEST AUTH - AUTHENTICATION NOK");
    WARN("DIGEST AUTH - Computed Result:%s", totalStrResult);
    WARN("DIGEST AUTH - Transmitted Result:%s", responseTokenStr);
  }

  return nRet;
}
