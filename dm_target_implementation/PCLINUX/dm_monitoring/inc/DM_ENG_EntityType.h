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
 * File        : DM_ENG_EntityType.h
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
 * @file DM_ENG_EntityType.h
 *
 * @brief An entity means any element capable of interacting with the DM Agent. The defined type of entities are 
 * ACS, Selface App and Subscriber.
 *
 **/

#ifndef _DM_ENG_ENTITY_TYPE_H_
#define _DM_ENG_ENTITY_TYPE_H_

#include "DM_ENG_Common.h"

/**
 * Types of entities
 */
typedef enum _DM_ENG_EntityType
{
   DM_ENG_EntityType_ACS = 0,
   DM_ENG_EntityType_SELFCARE_APP,
   DM_ENG_EntityType_SYSTEM,
   DM_ENG_EntityType_SUBSCRIBER,
   DM_ENG_EntityType_ANY

} DM_ENG_EntityType;

extern const char* DM_ENG_DEFAULT_ACCESS_LIST[];

bool DM_ENG_EntityType_belongsTo(DM_ENG_EntityType entity, char* accessList[]);
int DM_ENG_EntityType_getEntityNumber(const char* entityName);
const char* DM_ENG_EntityType_getEntityName(DM_ENG_EntityType entity);

#endif
