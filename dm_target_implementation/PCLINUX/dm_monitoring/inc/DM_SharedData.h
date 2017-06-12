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
 * File        : DM_SharedData.h
 *
 * Created     : 21/10/2010
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
 * @file DM_SharedData.h
 *
 * @brief 
 *
 **/

#ifndef DM_SHAREDDATA_H_
#define DM_SHAREDDATA_H_

#include <unistd.h>

typedef struct _DM_SharedData
{
   int flag;

   time_t timestamp;
   int continuity;
   unsigned int packetsReceived;
   unsigned int packetsLost;

} __attribute((packed)) DM_SharedData;


/***************************************************************************************************
 *  LIGHT MUTEX between 2 threads or processes (using shared memory) : 1 which has priority and 2
 **************************************************************************************************/

#ifndef LIGHT_LOCK_INIT
#define LIGHT_LOCK_INIT(mutex) do { mutex = 0; } while(0)
#endif

#ifndef LIGHT_LOCK_1
#define LIGHT_LOCK_1(mutex) do { mutex |= 1; while (mutex != 1) { usleep(50000); } } while(0)
#endif

#ifndef LIGHT_UNLOCK_1
#define LIGHT_UNLOCK_1(mutex) do { mutex = 0; } while(0)
#endif

#ifndef LIGHT_LOCK_2
#define LIGHT_LOCK_2(mutex) do { while ((mutex |= 2) != 2) { mutex &= 1; usleep(100000); } } while(0)
#endif

#ifndef LIGHT_UNLOCK_2
#define LIGHT_UNLOCK_2(mutex) do { mutex &= 1; } while(0)
#endif

#endif /*DM_SHAREDDATA_H_*/
