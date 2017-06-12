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
 * File        : DM_ENG_EventStruct.h
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
 * @file DM_ENG_EventStruct.h
 *
 * @brief Definition of the event which must be delivered to the ACS
 *
 **/
 
#ifndef _DM_ENG_EVENT_STRUCT_H_
#define _DM_ENG_EVENT_STRUCT_H_

extern const char* DM_ENG_EVENT_BOOTSTRAP;
extern const char* DM_ENG_EVENT_BOOT;
extern const char* DM_ENG_EVENT_PERIODIC;
extern const char* DM_ENG_EVENT_SCHEDULED;
extern const char* DM_ENG_EVENT_VALUE_CHANGE;
extern const char* DM_ENG_EVENT_KICKED;
extern const char* DM_ENG_EVENT_CONNECTION_REQUEST;
extern const char* DM_ENG_EVENT_TRANSFER_COMPLETE;
extern const char* DM_ENG_EVENT_DIAGNOSTICS_COMPLETE;
extern const char* DM_ENG_EVENT_REQUEST_DOWNLOAD;
extern const char* DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE;
extern const char* DM_ENG_EVENT_M_REBOOT;
extern const char* DM_ENG_EVENT_M_SCHEDULE_INFORM;
extern const char* DM_ENG_EVENT_M_DOWNLOAD;
extern const char* DM_ENG_EVENT_M_UPLOAD;

/**
 * Event which must be delivered to the ACS
 */
typedef struct _DM_ENG_EventStruct
{
   /** The event code as specified in TR-069 ("0 BOOTSTRAP", "1 BOOT", ... */
   const char* eventCode;
   /** The command key corresponding to the cause of the event */  
   char* commandKey;
   struct _DM_ENG_EventStruct* next;

} __attribute((packed)) DM_ENG_EventStruct;

DM_ENG_EventStruct* DM_ENG_newEventStruct(const char* ec, char* ck);
void DM_ENG_deleteEventStruct(DM_ENG_EventStruct* event);
void DM_ENG_deleteAllEventStruct(DM_ENG_EventStruct** pEvent);
DM_ENG_EventStruct** DM_ENG_newTabEventStruct(int size);
void DM_ENG_deleteTabEventStruct(DM_ENG_EventStruct* tEvent[]);
void DM_ENG_addEventStruct(DM_ENG_EventStruct** pEvent, DM_ENG_EventStruct* newEvt);

#endif
