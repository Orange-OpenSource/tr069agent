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
 * File        : DM_IPC_DBusInterface.c
 *
 * Created     : 11/07/2011
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
 * @file DM_IPC_DBusInterface.h
 *
 * @brief DBus Interface
 *
 */

#include <dbus/dbus.h>

#include "DM_IPC_DBusInterface.h"
#include "DM_ENG_RPCInterface.h"
#include "DM_ENG_ParameterBackInterface.h"
#include "DM_CMN_Thread.h"
#include "CMN_Trace.h"

#define DM_DBUS_SERVICE_NAME "fr.francetelecom.TR069Agent.DmEngine"
#define DM_DBUS_OBJECT_NAME "/fr/francetelecom/TR069Agent/DmEngine/RPCInterface"
#define DM_DBUS_INTERFACE_NAME "fr.francetelecom.TR069Agent.DmEngine.RPCInterface"

static char* _EMPTY_STRING = "";

static DM_CMN_ThreadId_t _dbusListenerId = 0;

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

static void _replyToGetParameterValues(DBusMessage* msg, DBusConnection* conn)
{
   int res = 0;

   DBusMessage* reply;
   DBusMessageIter args;
   dbus_uint32_t serial = 0;

   dbus_uint32_t nbParam = 0;
   char** parameterNames = NULL;
   DM_ENG_ParameterValueStruct** parameterListResult = NULL;

   DBG("_replyToGetParameterValues - Begin");

   // read the arguments
   res = _getMessageArg(msg, &args, DBUS_TYPE_UINT32, &nbParam);
   if (res != 0)
   {
      res = 9002;
   }
   else
   {
      parameterNames = (char**) calloc(nbParam+1, sizeof(char*));
      unsigned int i = 0;
      for (i=0; i<nbParam; i++)
      {
         char* name = NULL;
         res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &name);
         if ((res != 0) || (name == NULL))
         {
            res = 9002;
            break;
         }
         parameterNames[i] = strdup(name);
      }
      
      if (res == 0)
      {
         res = DM_ENG_GetParameterValues(DM_ENG_EntityType_SUBSCRIBER, parameterNames, &parameterListResult);
      }
      DM_ENG_deleteTabString(parameterNames);
   }

   // create a reply from the message
   reply = dbus_message_new_method_return(msg);

   // add the arguments to the reply
   dbus_message_iter_init_append(reply, &args);
   dbus_uint32_t code = res;
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &code))
   {
      res = 1;
   }
   else
   {
      if (res == 0)
      {
         dbus_uint32_t nbResults = DM_ENG_tablen((void**)parameterListResult);
         if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &nbResults))
         {
            res = 1;
         }
         else
         {
            int i;
            for (i=0; (res == 0) && (parameterListResult[i]!=NULL); i++)
            {
               if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parameterListResult[i]->parameterName))
               {
                  res = 1;
               }
               else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &parameterListResult[i]->type))
               {
                  res = 1;
               }
               else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, (parameterListResult[i]->value == NULL ? &_EMPTY_STRING : &parameterListResult[i]->value)))
               {
                  res = 1;
               }
            }
         }
      }
      // send the reply && flush the connection
      if (!dbus_connection_send(conn, reply, &serial))
      {
         res = 1;
      }
      else
      {
         dbus_connection_flush(conn);
      }
   }
   if (res == 1)
   {
      EXEC_ERROR("Out Of Memory!"); 
   }   

   // free the reply
   dbus_message_unref(reply);

   if (parameterListResult != NULL) { DM_ENG_deleteTabParameterValueStruct(parameterListResult); }

   DBG("_replyToGetParameterValues - End");
}

static void _replyToSetParameterValues(DBusMessage* msg, DBusConnection* conn)
{
   int res = 0;

   DBusMessage* reply;
   DBusMessageIter args;
   dbus_uint32_t serial = 0;

   // Paramètres en entrée
   dbus_uint32_t nbParam = 0;
   DM_ENG_ParameterValueStruct** parameterList = NULL;
   char* parameterKey = NULL;
   // Paramètres en sortie
   DM_ENG_ParameterStatus status;
   DM_ENG_SetParameterValuesFault** faults = NULL;

   DBG("_replyToSetParameterValues - Begin");

   // read the arguments
   res = _getMessageArg(msg, &args, DBUS_TYPE_UINT32, &nbParam);
   if (res == 0)
   {
      parameterList = (DM_ENG_ParameterValueStruct**) calloc(nbParam+1, sizeof(DM_ENG_ParameterValueStruct*));
      unsigned int i = 0;
      for (i=0; (res == 0) && (i<nbParam); i++)
      {
         char* name = NULL;
         dbus_uint32_t type;
         char* value = NULL;
         res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &name);
         if (res == 0)
         {
            res = _getMessageArg(NULL, &args, DBUS_TYPE_UINT32, &type);
            if (res == 0)
            {
               res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &value);
               if (res == 0)
               {
                  parameterList[i] = DM_ENG_newParameterValueStruct(name, type, value);
               }
            }
         }
      }

      if (res == 0)
      {
         res = _getMessageArg(NULL, &args, DBUS_TYPE_STRING, &parameterKey);
         if (res == 0)
         {
            res = DM_ENG_SetParameterValues(DM_ENG_EntityType_SUBSCRIBER, parameterList, parameterKey, &status, &faults);
         }
      }
      DM_ENG_deleteTabParameterValueStruct(parameterList);
   }
   if (res == 1)
   {
      res = 9002;
   }

   // create a reply from the message
   reply = dbus_message_new_method_return(msg);

   // add the arguments to the reply
   dbus_message_iter_init_append(reply, &args);
   dbus_uint32_t code = res;
   if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &code))
   {
      res = 1;
   }
   else
   {
      if (res == 0)
      {
         dbus_uint32_t uStatus = status;
         if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &uStatus))
         {
            res = 1;
         }
      }
      else if (res == 9003)
      {
         dbus_uint32_t nbFaults = DM_ENG_tablen((void**)faults);
         if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &nbFaults))
         {
            res = 1;
         }
         else
         {
            int i;
            for (i=0; (res == 0) && (faults[i]!=NULL); i++)
            {
               if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &faults[i]->parameterName))
               {
                  res = 1;
               }
               else if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &faults[i]->faultCode))
               {
                  res = 1;
               }
            }
         }
      }
      // send the reply && flush the connection
      if (!dbus_connection_send(conn, reply, &serial))
      {
         res = 1;
      }
      else
      {
         dbus_connection_flush(conn);
      }
   }
   if (res == 1)
   {
      EXEC_ERROR("Out Of Memory!"); 
   }   

   // free the reply
   dbus_message_unref(reply);

   if (faults != NULL) { DM_ENG_deleteTabSetParameterValuesFault(faults); }

   DBG("_replyToSetParameterValues - End");
}

static void _replyToVendorSpecificEvent(DBusMessage* msg, DBusConnection* conn)
{
   int res = 0;

   DBusMessage* reply;
   DBusMessageIter args;
   dbus_uint32_t serial = 0;

   char* OUI = NULL;
   char* event = NULL;

   DBG("_replyToVendorSpecificEvent - Begin");

   // read the arguments and call back vendorSpecificEvent
   if ((_getMessageArg(msg, &args, DBUS_TYPE_STRING, &OUI) == 0) && (_getMessageArg(NULL, &args, DBUS_TYPE_STRING, &event) == 0))
   {
      DM_ENG_ParameterManager_vendorSpecificEvent(OUI, event);
   }
   else
   {
      res = 9002;
   }

   // create a reply from the message
   reply = dbus_message_new_method_return(msg);

   // add the arguments to the reply
   dbus_message_iter_init_append(reply, &args);
   dbus_uint32_t code = res;
   // append code && send the reply && flush the connection
   if (dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &code) && dbus_connection_send(conn, reply, &serial))
   {
      dbus_connection_flush(conn);
   }
   else
   {
      EXEC_ERROR("Out Of Memory!"); 
   }

   // free the reply
   dbus_message_unref(reply);

   DBG("_replyToVendorSpecificEvent - End");
}

static bool _listening = true;

/**
 * Server that exposes a method call and waits for it to be called
 */
static void *_listen(void *data UNUSED)
{
   DBusMessage* msg;
   DBusConnection* conn;
   DBusError err;
   int ret;

   DBG("Listening for method calls");

   // initialise the error
   dbus_error_init(&err);

   // connect to the bus and check for errors
   conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
   if (dbus_error_is_set(&err)) { 
      EXEC_ERROR("Connection Error (%s)", err.message); 
      dbus_error_free(&err); 
   }
   if (NULL == conn) {
      EXEC_ERROR("Connection Null"); 
      return 0; 
   }

   // request our name on the bus and check for errors
   ret = dbus_bus_request_name(conn, DM_DBUS_SERVICE_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
   if (dbus_error_is_set(&err)) {
      EXEC_ERROR("Name Error (%s)", err.message); 
      dbus_error_free(&err);
   }
   if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) { 
      EXEC_ERROR("Not Primary Owner (%d)", ret);
      return 0;
   }

   // loop, testing for new messages
   while (_listening)
   {
      // non blocking read of the next available message
      dbus_connection_read_write(conn, 0);
      msg = dbus_connection_pop_message(conn);

      // loop again if we haven't got a message
      if (NULL == msg) { 
         sleep(1); 
         continue; 
      }
      
      // check this is a method call for the right interface & method
      if (dbus_message_is_method_call(msg, DM_DBUS_INTERFACE_NAME, "GetParameterValues"))
      { 
         _replyToGetParameterValues(msg, conn);
      }
      else if (dbus_message_is_method_call(msg, DM_DBUS_INTERFACE_NAME, "SetParameterValues"))
      { 
         _replyToSetParameterValues(msg, conn);
      }      
      else if (dbus_message_is_method_call(msg, DM_DBUS_INTERFACE_NAME, "VendorSpecificEvent"))
      { 
         _replyToVendorSpecificEvent(msg, conn);
      }      

      // free the message
      dbus_message_unref(msg);
   }

   // close the connection
   dbus_connection_close(conn);

   return 0;
}

/**
 * Init the DBus Connection and starts the message listener
 */
int DM_IPC_InitDBusConnection()
{
   _listening = true;
   DM_CMN_Thread_create(_listen, NULL, true, &_dbusListenerId);
   return 0;
}

/**
 * Stops the listener ans closes the DBus Connection 
 */
int DM_IPC_CloseDBusConnection()
{
   _listening = false;
   DM_CMN_Thread_join(_dbusListenerId);
   return 0;
}
