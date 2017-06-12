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
 * File        : DM_ENG_ArgStruct.h
 *
 * Created     : 16/03/2009
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
 * @file DM_ENG_ArgStruct.h
 *
 * @brief The ArgStruct is a name-value pair.
 *
 */

#ifndef _DM_ENG_ARG_STRUCT_H_
#define _DM_ENG_ARG_STRUCT_H_

/**
 * Name-value Structure
 */
typedef struct _DM_ENG_ArgStruct
{
   const char* name;
   char* value;
   struct _DM_ENG_ArgStruct* next;

} __attribute((packed)) DM_ENG_ArgStruct;

DM_ENG_ArgStruct* DM_ENG_newArgStruct(const char* name, char* val);
void DM_ENG_deleteArgStruct(DM_ENG_ArgStruct* as);
void DM_ENG_addArgStruct(DM_ENG_ArgStruct** pAs, DM_ENG_ArgStruct* newAs);
void DM_ENG_deleteAllArgStruct(DM_ENG_ArgStruct** pAs);
DM_ENG_ArgStruct** DM_ENG_newTabArgStruct(int size);
DM_ENG_ArgStruct** DM_ENG_copyTabArgStruct(DM_ENG_ArgStruct* tAs[]);
void DM_ENG_deleteTabArgStruct(DM_ENG_ArgStruct* tAs[]);

#endif
