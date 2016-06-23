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
 * File        : DM_ENG_SystemNotificationsStruct.h
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
 * @file DM_ENG_SystemNotificationsStruct.h
 *
 * @brief 
 *
 **/

#ifndef _DM_ENG_SYSTEM_NOTIFICATION_STRUCT_H_
#define _DM_ENG_SYSTEM_NOTIFICATION_STRUCT_H_

typedef enum _DM_ENG_NotificationType
{
   DM_ENG_NONE,
   DM_ENG_PATH_CHANGE,
   DM_ENG_DATA_VALUE_CHANGE,
   DM_ENG_VALUES_UPDATED,
   DM_ENG_TRANSFER_COMPLETE,
   DM_ENG_REQUEST_DOWNLOAD,
   DM_ENG_VENDOR_SPECIFIC_EVENT,
   DM_ENG_SAMPLE_DATA

} __attribute((packed)) DM_ENG_NotificationType;

typedef struct _SystemNotificationStruct
{
   DM_ENG_NotificationType type;
   void* notification;
   struct _SystemNotificationStruct* next;

} __attribute((packed)) DM_ENG_SystemNotificationStruct;

DM_ENG_SystemNotificationStruct* DM_ENG_newSystemNotificationStruct(DM_ENG_NotificationType t, void* notif);
void DM_ENG_deleteSystemNotificationStruct(DM_ENG_SystemNotificationStruct* notifStruct);
void DM_ENG_addSystemNotificationStruct(DM_ENG_SystemNotificationStruct** pNotifStruct, DM_ENG_SystemNotificationStruct* newNotifStruct);
void DM_ENG_deleteAllSystemNotificationStruct(DM_ENG_SystemNotificationStruct** pNotifStruct);

#endif
