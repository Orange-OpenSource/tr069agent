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
 * File        : dm_main.h
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
 * @file dm_main.h
 *
 * @brief 
 *
 **/

 

#ifndef _DM_MAIN_H_
#define _DM_MAIN_H_

#include "DM_ENG_Common.h"

/**
 * @brief Function which display the usage of this binary
 *
 * @param  pBinary Name of the binary called
 *
 * @return none
 *
 */
void
DM_ShowUsage(IN const char *pBinaryName);

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file doesn't exist else return 0
 *
 */
int
DM_CheckFileExist(IN const char * fileName);

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file was already existing of it a problem occured during the creation process ; else return 0
 *
 */
int
DM_CreateLockFile(IN const char *pPidfile,
                  IN const int   nPIDProcess);

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file haven't been found or if a problem occured during the removing step ; else return 0
 *
 */
int
DM_RemoveLockFile(IN const char *pPidfile);

/*
 * To stop the process
 */
void
DM_Agent_Stop(int nSig);

#endif /* _DM_MAIN_H_ */
