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
 * File        : DM_ENG_SystemNotificationsStruct.c
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
 * @file DM_ENG_SystemNotificationsStruct.c
 *
 * @brief 
 *
 **/


#include "DM_ENG_SystemNotificationStruct.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_TransferRequest.h"
#include "DM_ENG_SampleDataStruct.h"
#include "DM_ENG_Common.h"

DM_ENG_SystemNotificationStruct* DM_ENG_newSystemNotificationStruct(DM_ENG_NotificationType t, void* notif)
{
   DM_ENG_SystemNotificationStruct* res = (DM_ENG_SystemNotificationStruct*) malloc(sizeof(DM_ENG_SystemNotificationStruct));
   res->type = t;
   res->notification = notif;
   res->next = NULL;
   return res;
}

/*
 * Ne supprime pas l'attribut notification !
 */
void DM_ENG_deleteSystemNotificationStruct(DM_ENG_SystemNotificationStruct* notifStruct)
{
   free(notifStruct);
}

void DM_ENG_addSystemNotificationStruct(DM_ENG_SystemNotificationStruct** pNotifStruct, DM_ENG_SystemNotificationStruct* newNotifStruct)
{
   DM_ENG_SystemNotificationStruct* notif = *pNotifStruct;
   if (notif==NULL)
   {
      *pNotifStruct = newNotifStruct;
   }
   else
   {
      while (notif->next != NULL) { notif = notif->next; }
      notif->next = newNotifStruct;
   }
}

/*
 * Supprime également les attributs notification
 */
void DM_ENG_deleteAllSystemNotificationStruct(DM_ENG_SystemNotificationStruct** pNotifStruct)
{
   DM_ENG_SystemNotificationStruct* notif = *pNotifStruct;
   while (notif != NULL)
   {
      DM_ENG_SystemNotificationStruct* nt = notif;
      notif = notif->next;
      switch (notif->type)
      {
         case DM_ENG_PATH_CHANGE : free(notif->notification); break;
         case DM_ENG_DATA_VALUE_CHANGE : DM_ENG_deleteParameterValueStruct((DM_ENG_ParameterValueStruct*)notif->notification); break;
         case DM_ENG_VALUES_UPDATED : DM_ENG_deleteTabParameterValueStruct((DM_ENG_ParameterValueStruct**)notif->notification); break;
         case DM_ENG_TRANSFER_COMPLETE : DM_ENG_deleteTransferRequest((DM_ENG_TransferRequest*)notif->notification); break;
         case DM_ENG_SAMPLE_DATA : DM_ENG_deleteSampleDataStruct((DM_ENG_SampleDataStruct*)notif->notification); break;
         default : break;
      }
      free(nt);
   }
   *pNotifStruct = NULL;
}
