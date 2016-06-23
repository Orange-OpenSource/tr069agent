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
 * File        : DM_ENG_DownloadRequest.c
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
 * @file DM_ENG_DownloadRequest.c
 *
 * @brief The ArgStruct is a name-value pair. The DownloadRequest represents a download request from the CPE
 *
 */

#include "DM_ENG_DownloadRequest.h"
#include "DM_ENG_Common.h"

/**
 * Constructs a new ArgStruct object.
 * 
 * @param name Name
 * @param val Value
 * 
 * @return A pointer to the new ArgStruct object
 */
DM_ENG_ArgStruct* DM_ENG_newArgStruct(const char* name, char* val)
{
   DM_ENG_ArgStruct* res = (DM_ENG_ArgStruct*) malloc(sizeof(DM_ENG_ArgStruct));
   res->name = strdup(name);
   res->value = (val==NULL ? NULL : strdup(val));
   res->next = NULL;
   return res;
}

/**
 * Deletes the ArgStruct object.
 * 
 * @param as A pointer to a ArgStruct object
 * 
 */
void DM_ENG_deleteArgStruct(DM_ENG_ArgStruct* as)
{
   free((char*)as->name);
   if (as->value != NULL) { free(as->value); }
   free(as);
}

/**
 * Appends a new ArgStruct object to the list 
 * 
 * @param pAs Pointer to the first pointer of the list.
 * @param newAs A pointer to a ArgStruct object
 * 
 */
void DM_ENG_addArgStruct(DM_ENG_ArgStruct** pAs, DM_ENG_ArgStruct* newAs)
{
   DM_ENG_ArgStruct* as = *pAs;
   if (as==NULL)
   {
      *pAs=newAs;
   }
   else
   {
      while (as->next != NULL) { as = as->next; }
      as->next = newAs;
   }
}

/**
 * Deletes all the elements of the list 
 * 
 * @param pAs Pointer to the first pointer of the list.
 * 
 */
void DM_ENG_deleteAllArgStruct(DM_ENG_ArgStruct** pAs)
{
   DM_ENG_ArgStruct* as = *pAs;
   while (as != NULL)
   {
      DM_ENG_ArgStruct* pv = as;
      as = as->next;
      DM_ENG_deleteArgStruct(pv);
   }
   *pAs = NULL;
}

/**
 * Allocates space for a null terminated array which may contains ArgStruct pointer elements.
 * 
 * @param size Maximum number of array elements
 * 
 * @return An array pointer
 */
DM_ENG_ArgStruct** DM_ENG_newTabArgStruct(int size)
{
   return (DM_ENG_ArgStruct**)calloc(size+1, sizeof(DM_ENG_ArgStruct*));
}

/**
 * Copies an array of ArgStruct pointer
 * 
 * @param tAs An array pointer
 * 
 * @return An array pointer
 */
DM_ENG_ArgStruct** DM_ENG_copyTabArgStruct(DM_ENG_ArgStruct* tAs[])
{
   int size = 0;
   while (tAs[size] != NULL) { size++; }
   DM_ENG_ArgStruct** newTab = (DM_ENG_ArgStruct**)calloc(size+1, sizeof(DM_ENG_ArgStruct*));
   int i=0;
   while (tAs[i] != NULL) { newTab[i] = DM_ENG_newArgStruct(tAs[i]->name, tAs[i]->value); i++; }
   return newTab; 
}

/**
 * Releases the space allocated for an array of ArgStruct pointer
 * 
 * @param tAs An array pointer
 */
void DM_ENG_deleteTabArgStruct(DM_ENG_ArgStruct* tAs[])
{
   int i=0;
   while (tAs[i] != NULL) { DM_ENG_deleteArgStruct(tAs[i++]); }
   free(tAs);
}

/**
 * Constructs a new download request object.
 * 
 * @param fileType The file type
 * @param args Name-value pairs
 * 
 * @return A pointer to the new ArgStruct object
 */
DM_ENG_DownloadRequest* DM_ENG_newDownloadRequest(const char* fileType, DM_ENG_ArgStruct* args[])
{
   DM_ENG_DownloadRequest* res = (DM_ENG_DownloadRequest*) malloc(sizeof(DM_ENG_DownloadRequest));
   res->fileType = strdup(fileType);
   res->args = (args==NULL ? NULL : DM_ENG_copyTabArgStruct(args));
   res->next = NULL;
   return res;
}

/**
 * Deletes the download request object.
 * 
 * @param as A pointer to a DownloadRequest object
 * 
 */
void DM_ENG_deleteDownloadRequest(DM_ENG_DownloadRequest* dr)
{
   free((char*)dr->fileType);
   if (dr->args != NULL) { DM_ENG_deleteTabArgStruct(dr->args); }
   free(dr);
}

/**
 * Appends a new DownloadRequest object to the list 
 * 
 * @param pDr Pointer to the first pointer of the list.
 * @param newDr A pointer to a DownloadRequest object
 * 
 */
void DM_ENG_addDownloadRequest(DM_ENG_DownloadRequest** pDr, DM_ENG_DownloadRequest* newDr)
{
   DM_ENG_DownloadRequest* dr = *pDr;
   if (dr==NULL)
   {
      *pDr=newDr;
   }
   else
   {
      while (dr->next != NULL) { dr = dr->next; }
      dr->next = newDr;
   }
}
