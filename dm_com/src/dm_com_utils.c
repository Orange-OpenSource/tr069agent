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
 * File        : dm_com_utils.c
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
 * @file dm_com_utils.c
 *
 * @brief dm_com utilities
 *
 *
 */

#include "dm_com_utils.h"
#include "CMN_Trace.h"
#include <stdlib.h>


/**
 * @brief Private routine used to generate a random stream
 *
 * @param A pointer on the stream to generate (the memory must be 
 *        allocated by the calling routine)
          The size of the random stream to generate
 *
 * @Return TRUE if the stream is generated
 *         FALSE otherwise
 *
 */
bool
_generateRandomString(char * strToGenerate,
                      int    strSizeToGenerate){

  static int randomInitCounter = 0;
  int r, x;
  char * tmpStr;
  char _dico[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
  
  // Check parameter
  if(NULL == strToGenerate) {
    EXEC_ERROR( "Invalid Parameters" );
    return false;
  }
   
  memset((void *) strToGenerate, 0x00, strSizeToGenerate);
  tmpStr = strToGenerate;
  
  srand(time (NULL) + randomInitCounter++);
  
  for(x=0 ; x < strSizeToGenerate ; x++){
    r = (rand() % strlen(_dico));
    sprintf(tmpStr, "%c", _dico[r]);
    tmpStr++;
  }  
  
  // Add \0 at the end
  memcpy((void *) tmpStr, "\0", 1);
  
  DBG("GENERATED RANDOM STRING:%s", strToGenerate);	
  
	return true;      
}
