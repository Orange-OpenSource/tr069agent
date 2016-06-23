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
 * File        : DM_ENG_DeviceIdStruct.h
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
 * @file DM_ENG_DeviceIdStruct.h
 *
 * @brief Structure that uniquely identifies the CPE. Used in Inform Messages.
 *
 **/

#ifndef _DM_ENG_DEVICE_ID_STRUCT_H_
#define _DM_ENG_DEVICE_ID_STRUCT_H_

/**
 * A structure that uniquely identifies the CPE. Used in Inform Messages.
 */
typedef struct DM_ENG_DeviceIdStruct
{
   /** The manufacturer of the CPE (human readable string). */
   char* manufacturer;
   /** Organizationally unique identifier of the device manufacturer.*/
	char* OUI;
   /** Identifier of the class of product. */
	char* productClass;
   /** Serial number of the CPE. */
	char* serialNumber;
} __attribute((packed)) DM_ENG_DeviceIdStruct;

DM_ENG_DeviceIdStruct* DM_ENG_newDeviceIdStruct(char* mn, char* oui, char* pc, char* sn);
void DM_ENG_deleteDeviceIdStruct(DM_ENG_DeviceIdStruct* id);

#endif
