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
 * File        : DM_COM_ConnectionSecurity.c
 *
 * Created     : 29/12/2008
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */


// The total number of connection requested by ACS
//unsigned int numberOfConnectionRequest = 0;

// Array used to track the connection request frequency

//time_t connectionFreqArray[MAXCONNECTIONREQUESTARRAYSIZE];
//#include <stdio.h>
#include <sys/time.h> /* gettimeofday */

#include "DM_GlobalDefs.h"
#include "CMN_Trace.h"
#include "DM_ENG_RPCInterface.h"  /* DM_Engine module definition    */
#include "DM_COM_GenericHttpServerInterface.h"
#include "DM_COM_ConnectionSecurity.h"

/**
 * @brief This routine is used to initialize the 
 *        connection request frequency array.
 *        This array is used to limit the number
 *        of connection request per minute.
 *
 * @param 
 *
 * @return Void
 *
 */
void
    initConnectionFreqArray(time_t *connectionFreqArray){
  int i;
  for(i=0; i < MAXCONNECTIONREQUESTARRAYSIZE; i++) {
    connectionFreqArray[i]=0;
  }
}



/**
 * @brief This routine is used to update the
 *        connection request frequency array.
 *        A new entry is added with the current time
 *        (and the last entry is lost).
 *
 * @param 
 *
 * @return Void
 *
 */
void
    newConnectionRequestAllowed(time_t *connectionFreqArray){
  struct timeval t;
  time_t curtime;
  int    i;
  
  gettimeofday( &t, NULL );
  curtime = t.tv_sec;
  
  // Move the array entry to array entry + 1
  for(i = (MAXCONNECTIONREQUESTARRAYSIZE-1); i > 0 ; i--) {
    connectionFreqArray[i]=connectionFreqArray[i-1];
  } 
  
  // Set the new entry
  connectionFreqArray[0] = curtime;

}



/**
 * @brief This routine is used to check
 *        the connection acceptance policy
 *
 * @param 
 *
 * @return Void
 *
 */
bool
    maxConnectionPerPeriodReached(time_t *connectionFreqArray){
  unsigned int             numberOfRequestDuringThePreviousPeriod = 0;
  int                      i;
  struct                   timeval t;
  time_t                   curtime;
  bool                     maxConnectionPerPeriodReachedFlag = TRUE;
  unsigned int             maxConnectionRequest    = 0;
  unsigned int             periodConnectionRequest   = 0;  
  char                   * pParameterName[3];
  DM_ENG_ParameterValueStruct ** pResultGet = NULL;
  
      
  // Retrieve the number of connection allowed in the period and the period duration in seconds.
  pParameterName[0] = DM_TR106_MAXCONNECTIONREQUEST;
  pParameterName[1] = DM_TR106_MAXCONNECTIONFREQ;
  pParameterName[2] = NULL;
  DBG( "Requesting informations from to Dm_Engine server..." );
  if ( DM_ENG_GetParameterValues( DM_ENG_EntityType_ACS, pParameterName, &pResultGet ) == 0 ) {
    if((NULL == pResultGet[0]->value) || 
       (NULL == pResultGet[1]->value)) {
      EXEC_ERROR("Can not retrieve Max Connection Request / Freq. Set default value.");
      maxConnectionRequest    = DEFAULT_MAX_CONNECTIONREQUEST;
      periodConnectionRequest = DEFAULT_FREQ_CONNECTION_REQUEST;       
    } else {
      DM_ENG_stringToUint(pResultGet[0]->value, &maxConnectionRequest);
      DM_ENG_stringToUint(pResultGet[1]->value, &periodConnectionRequest);
      DBG("maxConnectionRequest = %d, periodConnectionRequest = %d", maxConnectionRequest, periodConnectionRequest);
      
      // Check the maximum array size is not reached
      if(maxConnectionRequest >= MAXCONNECTIONREQUESTARRAYSIZE) {
        DBG("Exceeds the array size (%d). Set default value", MAXCONNECTIONREQUESTARRAYSIZE);
        maxConnectionRequest = DEFAULT_MAX_CONNECTIONREQUEST;
      }
    }
    DM_ENG_deleteTabParameterValueStruct(pResultGet);
  } else {
    EXEC_ERROR("Can not retrieve max and freq connection request - Set default value");
    maxConnectionRequest    = DEFAULT_MAX_CONNECTIONREQUEST;
    periodConnectionRequest = DEFAULT_FREQ_CONNECTION_REQUEST;
  }
  gettimeofday( &t, NULL );
  curtime = t.tv_sec;  

  // count the number of connection request allowed during the previous period
  for(i=0; i < MAXCONNECTIONREQUESTARRAYSIZE ; i++) {
    // Check if the connectionFreqArray is within the last periodConnectionRequest seconds.
    if((long int)(curtime - connectionFreqArray[i]) < (long int) periodConnectionRequest) {
      numberOfRequestDuringThePreviousPeriod++;
    }
  }
  
  DBG( "numberOfRequestDuringThePreviousPeriod = %d", numberOfRequestDuringThePreviousPeriod );
  DBG( "periodConnectionRequest = %d", periodConnectionRequest );
  DBG( "maxConnectionRequestArraySize = %d", MAXCONNECTIONREQUESTARRAYSIZE );
  if(numberOfRequestDuringThePreviousPeriod <  maxConnectionRequest) {
    // The maximum number of connection per period is not reached
    // Make sure the number freq does not exceeds the array size
    if(numberOfRequestDuringThePreviousPeriod == MAXCONNECTIONREQUESTARRAYSIZE) {
      // Exceeds the array size. Should never arrive...
      WARN( "The maximum number of connections per period is reached (Max array size reached)" );
    } else {
      maxConnectionPerPeriodReachedFlag = FALSE;
    }
    DBG( "The maximum number of connections per period is NOT reached" );
  } else {
    WARN( "The maximum number of connections per period is reached" );
  }
  
  return maxConnectionPerPeriodReachedFlag;
}

