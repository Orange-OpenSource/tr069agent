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
 * File        : DM_ENG_DataModelConfiguration.c
 *
 * Created     : 09/03/2011
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
 * @file DM_ENG_DataModelConfiguration.c
 *
 * @brief The 
 * 
 **/

#include "DM_ENG_DataModelConfiguration.h"
#include "DM_ENG_ParameterData.h"
#include "DM_ENG_EntityType.h"
#include "DM_ENG_Global.h"
#include "DM_COM_GenericDomXmlParserInterface.h"

#define DM_ENG_SYNTAX_ERROR_IN_DATA_MODEL_EXTENSION_FILE (9040)
#define DM_ENG_PARAMETER_ALREADY_DEFINED                 (9041)
#define DM_ENG_UNDEFINED_PARAMETER                       (9042)
#define DM_ENG_CONFIG_KEY_MISSING                        (9043)

#define _ROOT_NAME                "DataModelConfiguration"
#define _DEFINE_COMMAND           "DefineParameters"
#define _UNDEFINE_COMMAND         "UndefineParameters"
#define _OVERWRITE_TAG            "Overwrite"
#define _CONFIG_KEY_TAG           "ConfigKey"
#define _PARAMETER_LIST_TAG       "ParameterList"
#define _PARAMETER_DEF_STRUCT_TAG "ParameterDefinitionStruct"
#define _NAME_TAG                 "Name"
#define _PATTERN_TAG              "Pattern"
#define _TYPE_TAG                 "Type"
#define _WRITABLE_TAG             "Writable"
#define _PERSISTENCE_TAG          "Persistence"
#define _GROUPED_DATA_TAG         "GroupedData"
#define _SOURCE_TAG               "Source"
#define _DEFINITION_TAG           "Definition"
#define _DEFAULT_TAG              "Default"
#define _NOTIFICATION_TAG         "Notification"
#define _ACCESS_LIST_TAG          "AccessList"
#define _PARAMETER_NAMES_TAG      "ParameterNames"

static const char* _PAT_FILE_PREFIX = "DMConf";
static const char* _PAT_FILE_SUFFIX = ".xml";

// Patterns spécifiques codés en dur
static const char* _STAT_VAR_PATTERN       = "StatVar";
static const char* _CUMUL_STAT_VAR_PATTERN = "CumulativeStatVar";
static const char* _TRIGGER_PATTERN        = "Trigger";
static const char* _RECURSIVE_PATTERN      = "Recursive";

static int _processCommand(GenericXmlNodePtr cmdNode, const char* rootName, char* configKey);

static void _processPattern(const char* rootName, const char* pattern, char* configKey)
{
   char* fileName = (char*)malloc(strlen(pattern)+strlen(_PAT_FILE_PREFIX)+strlen(_PAT_FILE_SUFFIX)+1);
   strcpy(fileName, _PAT_FILE_PREFIX);
   strcat(fileName, pattern);
   strcat(fileName, _PAT_FILE_SUFFIX);
   GenericXmlDocumentPtr xml = xmlFileToXmlDocument(fileName);
   free(fileName);
   if (xml != NULL)
   {
      GenericXmlNodePtr cmdNode = xmlGetFirstChildNodeOfDocument(xml);
      if (cmdNode != NULL) // Sinon c'est une erreur
      {
         _processCommand(cmdNode, rootName, configKey);
      }
      xmlDocumentFree(xml);      
   }
}

static int _processDefinition(GenericXmlNodePtr nodeDef, const char* rootName, bool overwrite, char* configKey)
{
   int res = DM_ENG_SYNTAX_ERROR_IN_DATA_MODEL_EXTENSION_FILE;
   char* nameStruct = NULL;
   if (xmlGetNodeParameters(nodeDef, &nameStruct, NULL) && (strcmp(nameStruct, _PARAMETER_DEF_STRUCT_TAG) == 0))
   {
      GenericXmlNodeListPtr listAttr = xmlGetChildNodesList(nodeDef);
      if (listAttr != NULL)
      {
         char* paramName = NULL;
         char* pattern = NULL;
         DM_ENG_ParameterType type = DM_ENG_ParameterType_UNDEFINED;
         int minValue = 0;
         int maxValue = 0;
         bool writable = false;
         bool writableDefined = false;
         unsigned int persistence = UINT_MAX;
         bool groupedData = false;
         bool groupedDataDefined = false;
         char* definition = NULL;
         bool definitionSpecified = false;
         char* defaultValue = NULL;
         DM_ENG_NotificationMode notification = DM_ENG_NotificationMode_UNDEFINED;
         char* accessList = NULL;
         unsigned int nbAttr = xmlGetNodesListLength(listAttr);
         unsigned int k;
         for (k=0; k<nbAttr; k++)
         {
            GenericXmlNodePtr nodeAttr = xmlGetNodeFromNodesList(listAttr, k);
            char* name = NULL;
            char* value = NULL;
            if (xmlGetNodeParameters(nodeAttr, &name, &value))
            {
               if (strcmp(name, _NAME_TAG) == 0)
               {
                  paramName = value;
               }
               else if (strcmp(name, _PATTERN_TAG) == 0)
               {
                  pattern = value;
               }
               else if (strcmp(name, _TYPE_TAG) == 0)
               {
                  type = DM_ENG_stringToType(value, &minValue, &maxValue);
               }
               else if (strcmp(name, _WRITABLE_TAG) == 0)
               {
                  DM_ENG_stringToBool(value, &writable);
                  writableDefined = true;
               }
               else if (strcmp(name, _PERSISTENCE_TAG) == 0)
               {
                  DM_ENG_stringToUint(value, &persistence);
               }
               else if (strcmp(name, _GROUPED_DATA_TAG) == 0)
               {
                  DM_ENG_stringToBool(value, &groupedData);
                  groupedDataDefined = true;
               }
               else if (strcmp(name, _SOURCE_TAG) == 0)
               {
                  definition = value;
               }
               else if (strcmp(name, _DEFINITION_TAG) == 0) // Vérifier que les parties Source et Definition s'excluent mutuellement ??
               {
                  definition = value;
                  definitionSpecified = true;
               }
               else if (strcmp(name, _DEFAULT_TAG) == 0)
               {
                  defaultValue = value;
               }
               else if (strcmp(name, _NOTIFICATION_TAG) == 0)
               {
                  DM_ENG_stringToUint(value, &notification);
               }
               else if (strcmp(name, _ACCESS_LIST_TAG) == 0)
               {
                  accessList = value;
               }
            }
         }

         if (paramName != NULL)
         {
            res = 0;
            char* longName = NULL;
            char* nodeName = NULL;
            int recursiveCase = 0;
            DM_ENG_Parameter* prm = NULL;
            char* wildcard = strchr(paramName, '$');
            char* foundParamName = NULL;
            char* def2 = definition;
            char* decConfigKey = NULL;
            if (configKey != NULL)
            {
               decConfigKey = DM_ENG_decodeValue(configKey); // Attention, NULL si rien à décoder !
               if (decConfigKey == NULL) { decConfigKey = configKey; }
            }

            // calcule le longName sauf dans le cas du $1 où il sera calculé à chaque itération
            if (rootName == NULL)
            {
               longName = strdup(paramName);
            }
            else if ((wildcard != NULL) && (wildcard[1] == '0'))
            {
               longName = (char*)malloc(strlen(rootName)+strlen(paramName));
               strcpy(longName, paramName);
               strcpy(longName+(wildcard-paramName), rootName);
               strcat(longName, wildcard+2);
            }
            else if ((wildcard != NULL) && (wildcard[1] == '1'))
            {
               recursiveCase = 2;
               nodeName = (char*)rootName;
            }
            else
            {
               longName = DM_ENG_Parameter_getLongName(paramName, rootName);
            }

            if ((recursiveCase == 0) && (pattern != NULL) && (strcmp(pattern, _RECURSIVE_PATTERN) == 0) && DM_ENG_Parameter_isNode(longName))
            {
               recursiveCase = 1;
               nodeName = longName;
            }

            // calcul de def2 où $0 est remplacé par le nom court du rootName
            if ((rootName != NULL) && (definition != NULL))
            {
               char* wc = strstr(definition, "$0");
               if (wc != NULL)
               {
                  int k = strlen(rootName);
                  while ((k>0) && (rootName[k] != '.')) { k--; }
                  if ((rootName[k] == '.') && (rootName[k+1] != '\0')) { k++; } 
                  def2 = (char*)malloc(strlen(definition)+strlen(rootName+k));
                  strcpy(def2, definition);
                  strcpy(def2+(wc-definition), rootName+k);
                  strcat(def2, wc+2);
               }
            }

            do
            {
               char* def3 = def2; // calcul de def3 où $1 est remplacé par le nom trouvé à chq itération

               // calcul du foundParamName sur lequel on itère et du longName correspondant (qui est différent dans le cas d'un $1)
               // + remplace l'éventuel $1 dans def2 pour donner def3
               if (recursiveCase != 0)
               {
                  if (foundParamName == NULL)
                  {
                     foundParamName = DM_ENG_ParameterData_getFirstName();
                  }
                  else
                  {
                     DM_ENG_ParameterData_resumeParameterLoopFrom(foundParamName);
                     free(foundParamName);
                     foundParamName = DM_ENG_ParameterData_getNextName();
                  }
                  while (foundParamName != NULL)
                  {
                     if ((strncmp(foundParamName, nodeName, strlen(nodeName)) == 0) && (strchr(foundParamName, '!') == NULL))
                     {
                        if (recursiveCase == 1) break;
                        prm = DM_ENG_ParameterData_getCurrent();
                        if ((!prm->writable) && ((prm->immediateChanges == 1) || (prm->immediateChanges == 2))) break;
                     }
                     foundParamName = DM_ENG_ParameterData_getNextName();
                  }

                  if (foundParamName == NULL) break;

                  foundParamName = strdup(foundParamName);
                  if (recursiveCase == 1)
                  {
                     longName = foundParamName;
                  }
                  else
                  {
                     longName = (char*)malloc(strlen(foundParamName)+strlen(paramName));
                     strcpy(longName, rootName);
                     strncat(longName, paramName, (wildcard-paramName));
                     strcat(longName, foundParamName+strlen(rootName));
                     strcat(longName, wildcard+2);

                     if (def2 != NULL)
                     {
                        char* wc = strstr(def2, "$1");
                        if (wc != NULL)
                        {
                           def3 = (char*)malloc(strlen(def2)+strlen(foundParamName)-strlen(rootName));
                           strcpy(def3, def2);
                           strcpy(def3+(wc-def2), foundParamName+strlen(rootName));
                           strcat(def3, wc+2);
                        }
                     }
                  }
               }

               prm = DM_ENG_ParameterData_getParameter(longName);
               if ((prm != NULL) && !overwrite)
               {
                  res = DM_ENG_PARAMETER_ALREADY_DEFINED;
               }
               else // on crée un nouveau paramètre s'il n'en existe pas déjà
                    // on recopie les champs s'ils sont spécifiés
               {
                  DM_ENG_Parameter* def = prm;
                  char* def4 = DM_ENG_decodeValue(def3); // def4=def3 décodé ou NULL si rien à décoder !
                  char* def5 = (def4 != NULL ? def4 : def3); // def5 definition à affecter
                  if (def == NULL)
                  {
                     def = DM_ENG_newDefinedParameter(longName, type, def5, false);
                     def->minValue = minValue;
                     def->maxValue = maxValue;
                     if (decConfigKey != NULL) { def->configKey = strdup(decConfigKey); }
                  }
                  else
                  {
                     if (type != DM_ENG_ParameterType_UNDEFINED)
                     {
                        def->type = type;
                        def->minValue = minValue;
                        def->maxValue = maxValue;
                     }
                     if (def5 != NULL)
                     {
                        if ((def->definition != NULL) && (def->definition != DM_ENG_EMPTY)) { free(def->definition); }
                        def->definition = (*def5 == '\0' ? (char*)DM_ENG_EMPTY : strdup(def5));
                     }
                     DM_ENG_Parameter_addConfigKey(def, decConfigKey);
                  }
                  DM_ENG_FREE(def4);

                  if (groupedDataDefined)
                  {
                     def->loadingMode = (groupedData ? 1 : 0);
                  }

                  if (persistence == UINT_MAX) // non spécifié
                  {
                     if (definitionSpecified) // par défaut : DM_ONLY, mais si une definition est spécifiée : COMPUTED
                     {
                        def->storageMode = DM_ENG_StorageMode_COMPUTED;
                     }
                  }
                  else if (persistence <= 3)
                  {
                     def->storageMode = persistence;
                  }
                  else
                  {
                     def->loadingMode |= 2;
                     def->storageMode = persistence - 3; // 4 -> SYSTEM, 5 -> MIXED
                  } 

                  if (writableDefined)
                  {
                     def->writable = writable;
                     if (writable) { def->immediateChanges = 1; } // par défaut, pour les param définis en écriture
                  }

                  if (defaultValue != NULL)
                  {
                     if ((def->value != NULL) && (def->value != DM_ENG_EMPTY)) { free(def->value); }
                     def->value = DM_ENG_decodeValue(defaultValue); // Attention, NULL si rien à décoder !
                     if (def->value == NULL) { def->value = (*defaultValue == '\0' ? (char*)DM_ENG_EMPTY : strdup(defaultValue)); }
                  }

                  if (notification != DM_ENG_NotificationMode_UNDEFINED) { def->notification = notification; }
   
                  if (accessList != NULL)
                  {
                     if ((def->accessList != NULL) && (def->accessList != (char**)DM_ENG_DEFAULT_ACCESS_LIST)) { DM_ENG_deleteTabString(def->accessList); }   
                     def->accessList = DM_ENG_splitCommaSeparatedList((char*)accessList);
                  }

                  if (pattern != NULL)
                  {
                     if ((strcmp(pattern, _STAT_VAR_PATTERN) == 0) || (strcmp(pattern, _CUMUL_STAT_VAR_PATTERN) == 0)) 
                     {
                        if (!def->writable)
                        {
                           def->immediateChanges = (strcmp(pattern, _STAT_VAR_PATTERN) == 0 ? 1 : 2); // signature des variables de stat
                        }
                        pattern = NULL; // pattern appliqué, rien de plus à faire
                     }
                     else if (strcmp(pattern, _TRIGGER_PATTERN) == 0)
                     {
                        if (!def->writable)
                        {
                           def->immediateChanges = 3; // signature des triggers
                           if (defaultValue != NULL)
                           {
                              if ((def->backValue != NULL) && (def->backValue != DM_ENG_EMPTY)) { free(def->backValue); }
                              def->backValue = def->value;
                              if ((def->value != NULL) && (def->value != DM_ENG_EMPTY))
                              {
                                 def->value = strdup(def->backValue);
                                 char* separ = strchr(def->value, '/');
                                 if (separ != NULL) { *separ= '\0'; }
                                 // Cas "Default/Trigger" : valeur initiale coupée à "Default"
                              }
                           }
                        }
                        pattern = NULL; // pattern appliqué, rien de plus à faire
                     }
                  }

                  // Vérification de la présence des noeuds parents et création sinon
                  unsigned int k;
                  for (k=strlen(DM_ENG_PARAMETER_PREFIX); k<strlen(longName)-1; k++)
                  {
                     if (longName[k] == '.')
                     {
                        char ch = longName[k+1];
                        longName[k+1] = '\0';
                        DM_ENG_Parameter* node = DM_ENG_ParameterData_getParameter(longName);
                        if (node == NULL)
                        {
                           node = DM_ENG_newDefinedParameter(longName, DM_ENG_ParameterType_ANY, NULL, false);
                           if (decConfigKey != NULL) { node->configKey = strdup(decConfigKey); }
                           DM_ENG_ParameterData_addParameter(node);
                        }
                        else if (decConfigKey != NULL)
                        {
                           DM_ENG_Parameter_addConfigKey(node, decConfigKey);
                        }
                        longName[k+1] = ch;
                     }
                  }

                  if (prm == NULL)
                  {
                     DM_ENG_ParameterData_addParameter(def);
                  }
                  else
                  {
                     DM_ENG_Parameter_setDataChanged(true);
                  }
   
                  if (pattern != NULL)
                  {
                     _processPattern(longName, pattern, configKey);
                  }
               }
               if (longName != foundParamName) { free(longName); }
               if (def3 != def2) { free(def3); }
            }
            while ((res == 0) && (foundParamName != NULL));

            if (def2 != definition) { free(def2); }
            if ((nodeName != NULL) && (nodeName != rootName)) { free(nodeName); } 
            if (decConfigKey != configKey) { free(decConfigKey); }
         }
         xmlFreeNodesList( listAttr );
      }
   }
   return res;
}

static int _processUndef(GenericXmlNodePtr nodeName, const char* rootName, char* configKey)
{
   int res = DM_ENG_SYNTAX_ERROR_IN_DATA_MODEL_EXTENSION_FILE;
   char* nameTag = NULL;
   char* paramName = NULL;
   if (xmlGetNodeParameters(nodeName, &nameTag, &paramName) && (strcmp(nameTag, _NAME_TAG) == 0) && (paramName != NULL))
   {
      char* longName = paramName;
      if (rootName != NULL)
      {
         longName = (char*)malloc(strlen(rootName)+strlen(paramName)+1);
         strcpy(longName, rootName);
         strcat(longName, paramName);
      }
      if (DM_ENG_ParameterData_deleteParameters(longName, configKey))
      {
         res = 0;
      }
      else
      {
         res = DM_ENG_UNDEFINED_PARAMETER;
      }
      if (rootName != NULL)
      {
         free(longName);
      }
   }
   return res;
}

static int _processCommand(GenericXmlNodePtr cmdNode, const char* rootName, char* configKey)
{
   int res = DM_ENG_SYNTAX_ERROR_IN_DATA_MODEL_EXTENSION_FILE;
   char* cmdName = NULL;
   if (xmlGetNodeParameters(cmdNode, &cmdName, NULL))
   {
      if (strcmp(cmdName, _DEFINE_COMMAND) == 0)
      {
         GenericXmlNodeListPtr list = xmlGetChildNodesList(cmdNode);
         if (list != NULL) // Sinon c'est une erreur
         {
            bool overwrite = false;
            GenericXmlNodeListPtr listDef = NULL;
            unsigned int nbNodes = xmlGetNodesListLength(list);
            unsigned int i;
            for (i=0; i<nbNodes; i++)
            {
               GenericXmlNodePtr node = xmlGetNodeFromNodesList(list, i);
               char* name = NULL;
               char* value = NULL;
               if (xmlGetNodeParameters(node, &name, &value))
               {
                  if (strcmp(name, _OVERWRITE_TAG) == 0)
                  {
                     DM_ENG_stringToBool(value, &overwrite);
                  }
                  else if (strcmp(name, _CONFIG_KEY_TAG) == 0)
                  {
                     configKey = value;
                  }
                  else if (strcmp(name, _PARAMETER_LIST_TAG) == 0)
                  {
                     listDef = xmlGetChildNodesList(node);
                  }
               }
            }
            if (configKey == NULL)
            {
               res = DM_ENG_CONFIG_KEY_MISSING;
            }
            else if (listDef != NULL)
            {
               unsigned int nbDef = xmlGetNodesListLength(listDef);
               unsigned int j;
               for (j=0; j<nbDef; j++)
               {
                  GenericXmlNodePtr nodeDef = xmlGetNodeFromNodesList(listDef, j);
                  res = _processDefinition(nodeDef, rootName, overwrite, configKey); // Ne peut être NULL (?)
                  if (res != 0) break;
               }
               xmlFreeNodesList( listDef );
            }
            xmlFreeNodesList( list );
         }
      }
      else if (strcmp(cmdName, _UNDEFINE_COMMAND) == 0)
      {
         GenericXmlNodeListPtr list = xmlGetChildNodesList(cmdNode);
         if (list != NULL) // Sinon c'est une erreur
         {
            bool overwrite = false;
            GenericXmlNodeListPtr listNames = NULL;
            unsigned int nbNodes = xmlGetNodesListLength(list);
            unsigned int i;
            for (i=0; i<nbNodes; i++)
            {
               GenericXmlNodePtr node = xmlGetNodeFromNodesList(list, i);
               char* name = NULL;
               char* value = NULL;
               if (xmlGetNodeParameters(node, &name, &value))
               {
                  if (strcmp(name, _OVERWRITE_TAG) == 0)
                  {
                     DM_ENG_stringToBool(value, &overwrite);
                  }
                  else if (strcmp(name, _CONFIG_KEY_TAG) == 0)
                  {
                     configKey = value;
                  }
                  else if (strcmp(name, _PARAMETER_NAMES_TAG) == 0)
                  {
                     listNames = xmlGetChildNodesList(node);
                  }
               }
            }
            if (listNames != NULL)
            {
               unsigned int nbNames = xmlGetNodesListLength(listNames);
               unsigned int j;
               for (j=0; j<nbNames; j++)
               {
                  GenericXmlNodePtr nodeName = xmlGetNodeFromNodesList(listNames, j);
                  res = _processUndef(nodeName, rootName, configKey); // Ne peut être NULL (?)
                  if (overwrite && (res == DM_ENG_UNDEFINED_PARAMETER)) { res = 0; }
                  if (res != 0) break;
               }
               xmlFreeNodesList( listNames );
            }
            xmlFreeNodesList( list );
         }
      }
   }

   return res;
}

int DM_ENG_DataModelConfiguration_processConfig(const char* fileName)
{
   int res = DM_ENG_SYNTAX_ERROR_IN_DATA_MODEL_EXTENSION_FILE;
   GenericXmlDocumentPtr xml = xmlFileToXmlDocument(fileName);
   if (xml != NULL)
   {
      GenericXmlNodePtr root = xmlGetFirstChildNodeOfDocument(xml);
      char* rootName = NULL;
      GenericXmlNodeListPtr list = NULL;
      unsigned int nbNodes = 0;
      if (root != NULL) // Sinon c'est une erreur
      {
         if (xmlGetNodeParameters(root, &rootName, NULL))
         {
            if (strcmp(rootName, _ROOT_NAME) == 0)
            {
               list = xmlGetChildNodesList(root);
               if (list != NULL) // Sinon c'est une erreur
               {
                  nbNodes = xmlGetNodesListLength(list);
               }
            }
         }
      }

      if (nbNodes > 0) // Sinon c'est une erreur
      {
         unsigned int i;
         for (i=0; i<nbNodes; i++)
         {
            GenericXmlNodePtr node = xmlGetNodeFromNodesList(list, i);
            res = _processCommand(node, NULL, NULL);
            if (res != 0) break;
         }
      }
      xmlFreeNodesList(list);
      xmlDocumentFree(xml);
      if (res == 0)
      {
         DM_ENG_ParameterData_sync();
      }
      else
      {
         DM_ENG_ParameterData_restore();
      }
   }
   return res;
}
