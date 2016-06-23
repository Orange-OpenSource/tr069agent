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
 * File        : DM_ENG_EntityType.c
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
 * @file DM_ENG_EntityType.c
 *
 * @brief An entity means any element capable of interacting with the DM Agent. The defined type of entities are 
 * ACS, Selface App and Subscriber.
 *
 **/

#include "DM_ENG_EntityType.h"
#include <ctype.h>

const char* DM_ENG_DEFAULT_ENTITIES[] = { "ACS", "SelfcareApp", "System", "Subscriber", "Any" };
static const char** _entities = DM_ENG_DEFAULT_ENTITIES;
static int _nbEntities = 5;

/**
 * Default access list : all defined entities.
 * N.B. Implicitly, ACS always belongs to the access list.
 *
 * Liste de toutes les entités définies ci-dessus séparées par des "," (?)
 * "ACS" et identiquement "SelfcareApp" sont implicites
 */
const char* DM_ENG_DEFAULT_ACCESS_LIST[] = { "Subscriber", NULL };

/**
 * Vérifie que le nom de l'entité est dans la liste
 */
bool DM_ENG_EntityType_belongsTo(DM_ENG_EntityType entity, char* accessList[])
{
   if (entity <= 2) return true; // appartenance implicite de "ACS", "SelfcareApp" et "System"
   if (((int)entity >= _nbEntities) || (accessList==NULL)) return false;

   int i;
   for (i=0; accessList[i]!=NULL; i++)
   {
      if (strcmp(accessList[i], _entities[entity]) == 0) return true;
   }
   return false;
}

/**
 * Get the entity number. Il the entityName is allready known the defined entity number is returned.
 * Otherwise, if entityName is a valid name, a new entity is defined and the new index is returned.
 */
int DM_ENG_EntityType_getEntityNumber(const char* entityName)
{
   int res = -1;
   int i;
   for (i=0; i<_nbEntities; i++)
   {
      if (strcmp(entityName, _entities[i]) == 0)
      {
         res = i;
         break;
      }
   }
   if (res == -1)
   {
      const char* s = entityName;
      while ((*s!='\0') && !isspace(*s) && (*s!=',') && (*s!=';')) { s++; }
      if (*s=='\0') // Le nom est valide, on l'ajoute
      {
         const char** newList = (const char**)calloc(_nbEntities+1, sizeof(const char*));
         for (i=0; i<_nbEntities; i++)
         {
            newList[i] = _entities[i];
         }
         res = _nbEntities;
         newList[res] = strdup(entityName);
         if (_entities != DM_ENG_DEFAULT_ENTITIES) { free(_entities); }
         _entities = newList;
         _nbEntities++;
      }
   }
   return res;
}

/**
 * Returns the name corresponding to the entity number
 */
const char* DM_ENG_EntityType_getEntityName(DM_ENG_EntityType entity)
{
   return ((int)entity<_nbEntities ? _entities[entity] : NULL);
}
