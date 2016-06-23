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
 * File        : DM_IPC_RPCInterface.c
 *
 * Created     : 12/07/2011
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
 * @file DM_IPC_RPCInterface.h
 *
 * @brief RPC Interface via DBus
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>

#include "DM_IPC_RPCInterface.h"
#include "DM_ENG_Common.h"

#define EXEC_ERROR( ... ) fprintf( stderr, __VA_ARGS__);
#define DBG( ... ) fprintf( stdout, __VA_ARGS__);

#define DM_REMOTE_SERVICE_NAME "fr.francetelecom.TR069Agent.DmEngine"
#define DM_REMOTE_OBJECT_NAME "/fr/francetelecom/TR069Agent/DmEngine/RPCInterface"
#define DM_REMOTE_INTERFACE_NAME "fr.francetelecom.TR069Agent.DmEngine.RPCInterface"

// Timeout for reply (must be > 30000) (-1 is default timeout)
#define _REPLY_TIMEOUT 40000

static int _getMessageArg(DBusMessage* pMsg, DBusMessageIter* pArgs, int type, void* pVal)
{
   int res = 0;
   if (pMsg != NULL) // lecture du 1er arg
   {
      if (!dbus_message_iter_init(pMsg, pArgs))
      {
         EXEC_ERROR("Message has no arguments !\n");
         res = 1;
      }
   }
   else if (!dbus_message_iter_next(pArgs)) // lecture du prochain arg
   {
      EXEC_ERROR("Message has too few arguments !\n");
      res = 1;
   }

   if (res == 0)
   {
      if (dbus_message_iter_get_arg_type(pArgs) != type)
      {
         EXEC_ERROR("Argument type is not : %d !\n", type);
         res = 1;
      } 
      else
      {
         dbus_message_iter_get_basic(pArgs, pVal);
      }
   }

   return res;
}

static DBusConnection* _connection = NULL;

/**
 * Initiates the DBUS Connection
 *
 * @return 0 if OK, 9800 if the connection can not be initiated
 */
int DM_IPC_InitConnection()
{
   int res = 0;

   DBusError err;

   DBG("DM_IPC_InitConnection - Begin\n");

   // initialiset the errors
   dbus_error_init(&err);

   // connect to the system bus and check for errors
   _connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
   if (dbus_error_is_set(&err))
   {
      EXEC_ERROR("Connection Error (%s)\n", err.message);
      dbus_error_free(&err);
   }
   if (NULL == _connection)
   {
      return 9800;
   }

   DBG("DM_IPC_InitConnection - End\n");
   return res;
}

/**
 * Closes the DBUS Connection
 */
void DM_IPC_CloseConnection()
{
   DBG("DM_IPC_CloseConnection - Begin\n");
   dbus_connection_close(_connection);
   DBG("DM_IPC_CloseConnection - End\n");
}

/**
 * This function may be used by the subscriber to modify the value of one or more CPE Parameters.
 *
 * @param parameterList Array of name-value pairs as specified. For each name-value pair, the CPE is instructed
 * to set the Parameter specified by the name to the corresponding value.
 * 
 * @param parameterKey The value to set the ParameterKey parameter. This MAY be used by the server to identify
 * Parameter updates, or left empty.
 * 
 * @param pStatus Pointer to get the status defined as follows :
 * - DM_ENG_ParameterStatus_APPLIED (=0) : Parameter changes have been validated and applied.
 * - DM_ENG_ParameterStatus_READY_TO_APPLY (=1) : Parameter changes have been validated and committed, but not yet applied
 * (for example, if a reboot is required before the new values are applied).
 * - DM_ENG_ParameterStatus_UNDEFINED : Undefined status (for example, parameterList is empty).
 *
 *  Note : The result is DM_ENG_ParameterStatus_APPLIED if all the parameter changes are APPLIED.
 * The result is DM_ENG_ParameterStatus_READY_TO_APPLY if one or more parameter changes is READY_TO_APPLY.
 * 
 * @param pFaults Pointer to get the faults struct if any, as specified in TR-069
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069, or 9800 for DBUS Error
 */
int DM_IPC_SetParameterValues(DM_ENG_ParameterValueStruct* parameterList[], char* parameterKey, OUT DM_ENG_ParameterStatus* pStatus, OUT DM_ENG_SetParameterValuesFault** pFaults[])
{
   int res = 0;

   DBusMessage* msg;
   DBusMessageIter args;
   DBusPendingCall* pending = NULL;

   dbus_uint32_t errcode;

   DBG("DM_IPC_SetParameterValues - Begin\n");

   // create a new method call and check for errors
   msg = dbus_message_new_method_call(DM_REMOTE_SERVICE_NAME,   // target for the method call
                                      DM_REMOTE_OBJECT_NAME,    // object to call on
                                      DM_REMOTE_INTERFACE_NAME, // interface to call on
                                      "SetParameterValues"); // method name
   if (NULL == msg)
   { 
      EXEC_ERROR("Message Null\n");
      return 9800;
   }

   dbus_uint32_t nbParam = DM_ENG_tablen((void**)parameterList);
   // append arguments
   dbus_message_iter_init_append(msg, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &nbParam))
   {
      res = 2;
   }

   int i;
   for (i=0; (res==0) && (parameterList[i]!=NULL); i++)
   {
      const char* name = parameterList[i]->parameterName;
      dbus_uint32_t type = parameterList[i]->type;
      char* val = parameterList[i]->value;
      if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name))
      {
         res = 2;
      }
      else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &type))
      {
         res = 2;
      }
      else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &val))
      {
         res = 2;
      }
   }

   if ((res == 0) && !dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parameterKey))
   {
      res = 2;
   }

   if (res == 0)
   {
      // send message and get a handle for a reply
      if (!dbus_connection_send_with_reply(_connection, msg, &pending, _REPLY_TIMEOUT))
      {
         res = 2;
      }
      else
      {
         if (NULL == pending)
         {
            EXEC_ERROR("Pending Call Null\n"); 
            res = 1; 
         }
         else
         {
            dbus_connection_flush(_connection);
            DBG("Request Sent\n");
         }
      }
   }

   // free message
   dbus_message_unref(msg);

   if (res != 0)
   {
      if (res == 2) { EXEC_ERROR("Out Of Memory !\n"); }
      return 9800;
   }

   // block until we receive a reply
   dbus_pending_call_block(pending);

   // get the reply message
   msg = dbus_pending_call_steal_reply(pending);

   // free the pending message handle
   dbus_pending_call_unref(pending);

   if (NULL == msg)
   {
      EXEC_ERROR("Reply Null\n");
      return 9800;
   }

   // read the parameters
   res = _getMessageArg(msg, &args, DBUS_TYPE_UINT32, &errcode);
   if (res == 0)
   {
      res = errcode;
      DBG("SetParameterValuesResponse returns %d\n", res);
   }

   if (res == 0)
   {
      dbus_uint32_t status = 0;
      res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &status);
      *pStatus = status;
   }
   else if (res == 9003)
   {
      dbus_uint32_t nbFaults = 0; 
      res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &nbFaults);
      if (res == 0)
      {
         *pFaults = DM_ENG_newTabSetParameterValuesFault(nbFaults);
         unsigned int i=0;
         for (i=0; (res == 0) && (i<nbFaults); i++)
         {
            char* name = NULL;
            dbus_uint32_t code = 0;
            res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &name);
            if (res == 0) { res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &code); }
            if (res == 0) { (*pFaults)[i] = DM_ENG_newSetParameterValuesFault(name, code); }
         }
         if (res != 0)
         {
            DM_ENG_deleteTabSetParameterValuesFault(*pFaults);
            *pFaults = NULL;
         }
      }
   }
   if (res == 0)
   {
      DBG("Got SetParameterValuesResponse\n");
   }
   else
   {
      EXEC_ERROR("Error in SetParameterValuesResponse\n");
   }

   // free reply and close connection
   dbus_message_unref(msg);

   DBG("DM_IPC_SetParameterValues - End\n");

   return ((res == 0) || (res >= 9000) ? res : 9800);
}

/**
 * This function may be used by the subscriber to obtain the value of one or more CPE parameters.
 * 
 * @param parameterNames Array of strings, each representing the name of the requested parameter.
 * @param Pointer to the array of the ParameterValueStruct.
 * 
 * @return 0 if OK, else a fault code (9001, ...) as specified in TR-069, or 9800 for DBUS Error
 */
int DM_IPC_GetParameterValues(char* parameterNames[], OUT DM_ENG_ParameterValueStruct** pResult[])
{
   int res = 0;

   DBusMessage* msg;
   DBusMessageIter args;
   DBusPendingCall* pending = NULL;

   dbus_uint32_t errcode;

   DBG("DM_IPC_GetParameterValues - Begin\n");

   // create a new method call and check for errors
   msg = dbus_message_new_method_call(DM_REMOTE_SERVICE_NAME,   // target for the method call
                                      DM_REMOTE_OBJECT_NAME,    // object to call on
                                      DM_REMOTE_INTERFACE_NAME, // interface to call on
                                      "GetParameterValues"); // method name
   if (NULL == msg)
   { 
      EXEC_ERROR("Message Null\n");
      return 9800;
   }

   dbus_uint32_t nbParam = DM_ENG_tablen((void**)parameterNames);
   // append arguments
   dbus_message_iter_init_append(msg, &args);
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &nbParam))
   {
      res = 2;
   }

   int i;
   for (i=0; (res==0) && (parameterNames[i]!=NULL); i++)
   {
      if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parameterNames[i]))
      {
         res = 2;
      }
   }

   if (res == 0)
   {
      // send message and get a handle for a reply
      if (!dbus_connection_send_with_reply(_connection, msg, &pending, _REPLY_TIMEOUT))
      {
         res = 2;
      }
      else
      {
         if (NULL == pending)
         { 
            EXEC_ERROR("Pending Call Null\n"); 
            res = 1; 
         }
         else
         {
            dbus_connection_flush(_connection);
            DBG("Request Sent\n");
         }
      }
   }

   // free message
   dbus_message_unref(msg);

   if (res != 0)
   {
      if (res == 2) { EXEC_ERROR("Out Of Memory !\n"); }
      return 9800;
   }

   // block until we recieve a reply
   dbus_pending_call_block(pending);

   // get the reply message
   msg = dbus_pending_call_steal_reply(pending);

   // free the pending message handle
   dbus_pending_call_unref(pending);

   if (NULL == msg)
   {
      EXEC_ERROR("Reply Null\n");
      return 9800;
   }

   // read the parameters
   res = _getMessageArg(msg, &args, DBUS_TYPE_UINT32, &errcode);
   if (res == 0)
   {
      res = errcode;
      DBG("GetParameterValuesResponse returns %d\n", res);
   }

   if (res == 0)
   {
      dbus_uint32_t nbResults = 0; 
      res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &nbResults);
      if (res == 0)
      {
         *pResult = DM_ENG_newTabParameterValueStruct(nbResults);
         unsigned int i=0;
         for (i=0; (res == 0) && (i<nbResults); i++)
         {
            char* name = NULL;
            dbus_uint32_t type = 0;
            char* val = NULL;
            res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &name);
            if (res == 0) { res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &type); }
            if (res == 0) { res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &val); }
            if (res == 0) { (*pResult)[i] = DM_ENG_newParameterValueStruct(name, type, val); }
         }
         if (res == 0)
         {
            DBG("Got GetParameterValuesResponse\n");
         }
         else
         {
            EXEC_ERROR("Error in GetParameterValuesResponse\n");
            DM_ENG_deleteTabParameterValueStruct(*pResult);
            *pResult = NULL;
         }
      }
   }

   // free reply and close connection
   dbus_message_unref(msg);

   DBG("DM_IPC_GetParameterValues - End\n");

   return ((res == 0) || (res >= 9000) ? res : 9800);
}

/**
 * Notify the DM Engine that a vendor specific event occurs.
 * 
 * @param OUI Organizationally Unique Identifier
 * @param event Name of an event
 * 
 * @return 0 if OK, or 9800 for DBUS Error
 */
int DM_IPC_VendorSpecificEvent(const char* OUI, const char* event)
{
   int res = 0;

   DBusMessage* msg;
   DBusMessageIter args;
   DBusPendingCall* pending = NULL;

   dbus_uint32_t errcode;

   DBG("DM_IPC_VendorSpecificEvent - Begin\n");

   // create a new method call and check for errors
   msg = dbus_message_new_method_call(DM_REMOTE_SERVICE_NAME,   // target for the method call
                                      DM_REMOTE_OBJECT_NAME,    // object to call on
                                      DM_REMOTE_INTERFACE_NAME, // interface to call on
                                      "VendorSpecificEvent"); // method name
   if (NULL == msg)
   { 
      EXEC_ERROR("Message Null\n");
      return 9800;
   }

   // append arguments
   dbus_message_iter_init_append(msg, &args);
   if (dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &OUI) && dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &event))
   {
      // send message and get a handle for a reply
      if (!dbus_connection_send_with_reply(_connection, msg, &pending, _REPLY_TIMEOUT))
      {
         res = 2;
      }
      else
      {
         if (NULL == pending)
         {
            EXEC_ERROR("Pending Call Null\n"); 
            res = 1; 
         }
         else
         {
            dbus_connection_flush(_connection);
            DBG("Vendor Specific Event Sent\n");
         }
      }
   }
   else
   {
      res = 2;
   }

   // free message
   dbus_message_unref(msg);

   if (res != 0)
   {
      if (res == 2) { EXEC_ERROR("Out Of Memory !\n"); }
      return 9800;
   }

   // block until we recieve a reply
   dbus_pending_call_block(pending);

   // get the reply message
   msg = dbus_pending_call_steal_reply(pending);

   // free the pending message handle
   dbus_pending_call_unref(pending);

   if (NULL == msg)
   {
      EXEC_ERROR("Reply Null\n");
      return 9800;
   }

   // read the parameters
   res = _getMessageArg(msg, &args, DBUS_TYPE_UINT32, &errcode);
   if (res == 0)
   {
      res = errcode;
      DBG("VendorSpecificEvent returns %d\n", res);
   }

   // free reply and close connection
   dbus_message_unref(msg);

   DBG("DM_IPC_VendorSpecificEvent - End\n");

   return ((res == 0) || (res >= 9000) ? res : 9800);   
}
