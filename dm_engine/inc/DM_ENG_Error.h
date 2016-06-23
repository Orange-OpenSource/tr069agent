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
 * File        : DM_ENG_Error.h
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
 * @file DM_ENG_Error.h
 *
 * @brief Error codes 9000 to 9013 as defined in TR-069.
 *
 **/

#ifndef _DM_ENG_ERROR_H_
#define _DM_ENG_ERROR_H_

#define DM_ENG_METHOD_NOT_SUPPORTED          (9000)
#define DM_ENG_REQUEST_DENIED                (9001)
#define DM_ENG_INTERNAL_ERROR                (9002)
#define DM_ENG_INVALID_ARGUMENTS             (9003)
#define DM_ENG_RESOURCES_EXCEEDED            (9004)
#define DM_ENG_INVALID_PARAMETER_NAME        (9005)
#define DM_ENG_INVALID_PARAMETER_TYPE        (9006)
#define DM_ENG_INVALID_PARAMETER_VALUE       (9007)
#define DM_ENG_READ_ONLY_PARAMETER           (9008)
#define DM_ENG_NOTIFICATION_REQUEST_REJECTED (9009)
#define DM_ENG_DOWNLOAD_FAILURE              (9010)
#define DM_ENG_UPLOAD_FAILURE                (9011)
#define DM_ENG_AUTHENTICATION_FAILURE        (9012)
#define DM_ENG_UNSUPPORTED_PROTOCOL          (9013)

#endif
