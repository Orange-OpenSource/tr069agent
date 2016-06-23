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
 * File        : dm_com_acs_errorcode.h
 *
 * Created     : 2008/06/05
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
 * @file dm_acs_errorcode.h
 *
 * @brief Definition of the return code of the ACS server
 *
 */

#ifndef _DM_ACS_ERRORCODES_
#define _DM_ACS_ERRORCODES_

/**
 * @brief Definition of the ACS return code
 */
#define		ACS_ERROR_METHOD_NOT_SUPPORTED		(8000)
#define		ACS_ERROR_REQUEST_DENIED		      (8001)
#define		ACS_ERROR_INTERNAL_ERROR		      (8002)
#define		ACS_ERROR_INVALID_ARGUMENTS		    (8003)
#define		ACS_ERROR_RESSOURCES_EXCEEDED		  (8004)
#define		ACS_ERROR_RETRY_REQUEST			      (8005)

#endif /* _DM_ACS_ERRORCODES_ */
