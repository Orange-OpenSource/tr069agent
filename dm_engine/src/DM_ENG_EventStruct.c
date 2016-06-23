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
 * File        : DM_ENG_EventStruct.c
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
 * @file DM_ENG_EventStruct.c
 *
 * @brief Definition of the event which must be delivered to the ACS
 *
 **/

#include "DM_ENG_EventStruct.h"
#include "DM_ENG_Common.h"

const char* DM_ENG_EVENT_BOOTSTRAP            = "0 BOOTSTRAP";
const char* DM_ENG_EVENT_BOOT                 = "1 BOOT";
const char* DM_ENG_EVENT_PERIODIC             = "2 PERIODIC";
const char* DM_ENG_EVENT_SCHEDULED            = "3 SCHEDULED";
const char* DM_ENG_EVENT_VALUE_CHANGE         = "4 VALUE CHANGE";
const char* DM_ENG_EVENT_KICKED               = "5 KICKED";
const char* DM_ENG_EVENT_CONNECTION_REQUEST   = "6 CONNECTION REQUEST";
const char* DM_ENG_EVENT_TRANSFER_COMPLETE    = "7 TRANSFER COMPLETE";
const char* DM_ENG_EVENT_DIAGNOSTICS_COMPLETE = "8 DIAGNOSTICS COMPLETE";
const char* DM_ENG_EVENT_REQUEST_DOWNLOAD     = "9 REQUEST DOWNLOAD";
const char* DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE = "10 AUTONOMOUS TRANSFER COMPLETE";
const char* DM_ENG_EVENT_M_REBOOT             = "M Reboot";
const char* DM_ENG_EVENT_M_SCHEDULE_INFORM    = "M ScheduleInform";
const char* DM_ENG_EVENT_M_DOWNLOAD           = "M Download";
const char* DM_ENG_EVENT_M_UPLOAD             = "M Upload";

static const char* _internEventCode(const char* ec)
{
   return ( (strcmp(ec, DM_ENG_EVENT_BOOTSTRAP)==0) ? DM_ENG_EVENT_BOOTSTRAP
          : (strcmp(ec, DM_ENG_EVENT_BOOT)==0) ? DM_ENG_EVENT_BOOT
          : (strcmp(ec, DM_ENG_EVENT_PERIODIC)==0) ? DM_ENG_EVENT_PERIODIC
          : (strcmp(ec, DM_ENG_EVENT_SCHEDULED)==0) ? DM_ENG_EVENT_SCHEDULED
          : (strcmp(ec, DM_ENG_EVENT_VALUE_CHANGE)==0) ? DM_ENG_EVENT_VALUE_CHANGE
          : (strcmp(ec, DM_ENG_EVENT_KICKED)==0) ? DM_ENG_EVENT_KICKED
          : (strcmp(ec, DM_ENG_EVENT_CONNECTION_REQUEST)==0) ? DM_ENG_EVENT_CONNECTION_REQUEST
          : (strcmp(ec, DM_ENG_EVENT_TRANSFER_COMPLETE)==0) ? DM_ENG_EVENT_TRANSFER_COMPLETE
          : (strcmp(ec, DM_ENG_EVENT_DIAGNOSTICS_COMPLETE)==0) ? DM_ENG_EVENT_DIAGNOSTICS_COMPLETE
          : (strcmp(ec, DM_ENG_EVENT_REQUEST_DOWNLOAD)==0) ? DM_ENG_EVENT_REQUEST_DOWNLOAD
          : (strcmp(ec, DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE)==0) ? DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE
          : (strcmp(ec, DM_ENG_EVENT_M_REBOOT)==0) ? DM_ENG_EVENT_M_REBOOT
          : (strcmp(ec, DM_ENG_EVENT_M_SCHEDULE_INFORM)==0) ? DM_ENG_EVENT_M_SCHEDULE_INFORM
          : (strcmp(ec, DM_ENG_EVENT_M_DOWNLOAD)==0) ? DM_ENG_EVENT_M_DOWNLOAD
          : (strcmp(ec, DM_ENG_EVENT_M_UPLOAD)==0) ? DM_ENG_EVENT_M_UPLOAD : strdup(ec) );
}

static bool _isInternEventCode(const char* ec)
{
   return (  (ec == DM_ENG_EVENT_BOOTSTRAP)
          || (ec == DM_ENG_EVENT_BOOT)
          || (ec == DM_ENG_EVENT_PERIODIC)
          || (ec == DM_ENG_EVENT_SCHEDULED)
          || (ec == DM_ENG_EVENT_VALUE_CHANGE)
          || (ec == DM_ENG_EVENT_KICKED)
          || (ec == DM_ENG_EVENT_CONNECTION_REQUEST)
          || (ec == DM_ENG_EVENT_TRANSFER_COMPLETE)
          || (ec == DM_ENG_EVENT_DIAGNOSTICS_COMPLETE)
          || (ec == DM_ENG_EVENT_REQUEST_DOWNLOAD)
          || (ec == DM_ENG_EVENT_AUTONOMOUS_TRANSFER_COMPLETE)
          || (ec == DM_ENG_EVENT_M_REBOOT)
          || (ec == DM_ENG_EVENT_M_SCHEDULE_INFORM)
          || (ec == DM_ENG_EVENT_M_DOWNLOAD)
          || (ec == DM_ENG_EVENT_M_UPLOAD) );
}

/**
 * Creates à new event struct
 * 
 * @param ec An event code
 * @param ck A command key
 * 
 * @return A pointer to the struct newly created
 */
DM_ENG_EventStruct* DM_ENG_newEventStruct(const char* ec, char* ck)
{
   DM_ENG_EventStruct* res = (DM_ENG_EventStruct*) malloc(sizeof(DM_ENG_EventStruct));
   res->eventCode = _internEventCode(ec);
   res->commandKey = (ck==NULL ? NULL : strdup(ck));
   res->next = NULL;
   return res;
}

/**
 * Deletes the struct
 * 
 * @param event An event struct
 */
void DM_ENG_deleteEventStruct(DM_ENG_EventStruct* event)
{
   if (!_isInternEventCode(event->eventCode)) { free((char*)event->eventCode); }
   free(event->commandKey);
   free(event);
}

/**
 * Deletes all the struct in the list
 * 
 * @param pEvent A pointer to the firt struct element in the list
 */
void DM_ENG_deleteAllEventStruct(DM_ENG_EventStruct** pEvent)
{
   DM_ENG_EventStruct* event = *pEvent;
   while (event != NULL)
   {
      DM_ENG_EventStruct* ev = event;
      event = event->next;
      DM_ENG_deleteEventStruct(ev);
   }
   *pEvent = NULL;
}

/**
 * Allocates memory for an array of EventStruct 
 * 
 * @param size Size of the array
 * 
 * @return A pointer to the array newly created
 */
DM_ENG_EventStruct** DM_ENG_newTabEventStruct(int size)
{
   return (DM_ENG_EventStruct**)calloc(size+1, sizeof(DM_ENG_EventStruct*));
}

/**
 * Deletes an array of EventStruct
 * 
 * @param tEvent The array to delete
 */
void DM_ENG_deleteTabEventStruct(DM_ENG_EventStruct* tEvent[])
{
   int i=0;
   while (tEvent[i] != NULL) { DM_ENG_deleteEventStruct(tEvent[i++]); }
   free(tEvent);
}

/**
 * Adds an element to the list of EventStruct
 * 
 * @param pEvent Pointer to the first element
 * @param newEvt EventStruct to add
 */
void DM_ENG_addEventStruct(DM_ENG_EventStruct** pEvent, DM_ENG_EventStruct* newEvt)
{
   DM_ENG_EventStruct* event = *pEvent;
   if (event==NULL)
   {
      *pEvent = newEvt;
   }
   else
   {
      while (event->next != NULL) { event = event->next; }
      event->next = newEvt;
   }
}
