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
 * File        : DM_COM_DomXmlParserInterface.c
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
 * @file ixmlDomXmlParserInterface.c
 *
 * @brief Implements the GenericDomXmlParserInterface routines
 *
 *
 */
 

#include "DM_COM_GenericDomXmlParserInterface.h"
#include "ixml.h"
#include "CMN_Trace.h"

/*
* @brief Function used to convert an XML text buffer into XML DOM representation
*
* @param the xmlStringBuffer
*
* @return The function returns a pointer on the XML doucment or null on error.
*
*/
GenericXmlDocumentPtr
xmlStringBufferToXmlDocument(IN char * xmlStringBuffer)
{
  IXML_Document* xmlDocumentPtr = NULL;
  
  if(xmlStringBuffer != NULL) {
    xmlDocumentPtr = ixmlParseBuffer(xmlStringBuffer);
  }

  return ((GenericXmlDocumentPtr) xmlDocumentPtr);
}

/*
* @brief Function used to convert an XML file into XML DOM representation
*
* @param the file name
*
* @return The function returns a pointer on the XML doucment or null on error.
*
*/
GenericXmlDocumentPtr
xmlFileToXmlDocument(IN const char* fileName)
{
  IXML_Document* xmlDocumentPtr = NULL;
  
  if(fileName != NULL) {
    xmlDocumentPtr = ixmlLoadDocument(fileName);
  }

  return ((GenericXmlDocumentPtr) xmlDocumentPtr);
}

/*
* @brief Function used to free an xml document
*
* @param the xmlDocument
*
* @return void
*
*/
void
xmlDocumentFree(GenericXmlDocumentPtr xmlDocPtr)
{
  DBG("xmlDocumentFree - begin");
  if(NULL != xmlDocPtr) {
    ixmlDocument_free((IXML_Document *) xmlDocPtr);
  }
  DBG("xmlDocumentFree - done");
}


/*
* @brief Function used to convert an XML text buffer into XML DOM representation
*
* @param the xmlDocument
*
* @return The function returns a pointer on the XML doucment or null on error.
*         The calling routine must free the allocated memory.
*
*/
char *
xmlDocumentToStringBuffer(IN GenericXmlDocumentPtr xmlDocumentPtr)
{  
  char * docStr = NULL;

  if(NULL != xmlDocumentPtr) {
    docStr = ixmlDocumenttoString((IXML_Document *) xmlDocumentPtr);
  }
  
  return docStr;
  
}


GenericXmlNodePtr
xmlGetFirstChildNodeOfDocument(IN GenericXmlDocumentPtr xmlDocument)
{
  IXML_Node         * firstXmlNodeChildPtr = NULL;
  GenericXmlNodePtr   documentNodePtr      = NULL;
  if(xmlDocument != NULL) {
    documentNodePtr = xmlDocumentToNode(xmlDocument);
    firstXmlNodeChildPtr = ixmlNode_getFirstChild((IXML_Node *) documentNodePtr);
  }
  
  return ((GenericXmlNodePtr ) firstXmlNodeChildPtr); 
}

/*
* @brief Function used to retrieve the first node with the given tag name
*
* @param GenericXmlNodePtr 
*
* @return The function returns a pointer on the XML node (GenericXmlNode)
*         or NULL on error or if no child node exist.
*
*/
GenericXmlNodePtr
xmlGetFirstNodeWithTagName(IN GenericXmlDocumentPtr xmlDocumentPtr,
                           IN char *                nodeNamePtr)
{
  GenericXmlNodeListPtr listPtr = NULL;
  GenericXmlNodePtr     nodePtr = NULL;
  
  // Check parameters
  if((NULL == xmlDocumentPtr) ||
     (NULL == nodeNamePtr)) {
    return NULL;
  }

  listPtr = xmlGetNodesListWithTagName(xmlDocumentToNode(xmlDocumentPtr), (char *) nodeNamePtr);
  nodePtr = xmlGetNodeFromNodesList(listPtr, 0);
  xmlFreeNodesList(listPtr);
  
  return nodePtr;

}

/*
* @brief Function used to retrieve the value of the node (string type)
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The node to retrieves paramters
*        OUT  
*
* @return The function returns a pointer on the XML child node (GenericXmlNode)
*         or NULL on error or if no child node exist.
*
*/
bool
xmlGetNodeParameters(const IN  GenericXmlNodePtr xmlNodePtr,
                     OUT       char **           xmlNodeNameStr,
                     OUT       char **           xmlNodeValueStr)
{

  char * _xmlNodeNameStr      = NULL;
  char * _xmlNodeValueStr     = NULL;

  // Check parameter
  if(NULL == xmlNodePtr) {
    //ERROR
    return false;
  }
  
  if(NULL != xmlNodeNameStr) {
    // Retrieve the Node name
    _xmlNodeNameStr  = (char *) ixmlNode_getNodeName((IXML_Node *) xmlNodePtr);
    * xmlNodeNameStr = _xmlNodeNameStr;
  }

  if(NULL != xmlNodeValueStr) {
    // Retrieve the node value
    _xmlNodeValueStr  = (char *) ixmlNode_getNodeValue(ixmlNode_getFirstChild((IXML_Node *) xmlNodePtr));
    * xmlNodeValueStr = _xmlNodeValueStr;
  }

  return true;
}


/*
* @brief Function used to add a node with a value and attribut. 
*        To create the node with no value, set the value ptr to 
*        null (idem for attribut)
*
* @param IN   GenericXmlDocumentPtr xmlDocPtr         // The XML document
*        IN GenericXmlNodePtr       xmlParentNodePtr, // The XML parent node
*        IN char *                  nodeNameStr,      // The new node name
*        IN char *                  nodeAttributStr,  // The new node attribut (NULL if no attribut)
*        IN char *                  nodeValueStr      // The new node value (NULL if no value)
*
* @return The function returns a pointer on the XML node
*         or NULL on error or if no child node exist.
*
*/
GenericXmlNodePtr
xmlAddNode(IN GenericXmlDocumentPtr xmlDocPtr,
           IN GenericXmlNodePtr     xmlParentNodePtr,
           IN const char *          nodeNameStr,
           IN const char *          nodeAttributNameStr,
           IN const char *          nodeAttributTypeStr,
           IN const char *          nodeValueStr)
{

  IXML_Element * pNewElement = NULL; 
  IXML_Node    * nodeText    = NULL;
  
    
  // Check mandatory parameters
  if((NULL == xmlDocPtr)        ||
     (NULL == xmlParentNodePtr) ||
     (NULL == nodeNameStr)) {
    return NULL;   
  }
  
  // Create a new element
  pNewElement = ixmlDocument_createElement((IXML_Document *) xmlDocPtr, nodeNameStr);

  
  if((NULL != nodeAttributNameStr) && (NULL != nodeAttributTypeStr)) {   
    // Set the node attribut
    ixmlElement_setAttribute(pNewElement, (char *) nodeAttributNameStr, (char *) nodeAttributTypeStr); 
  } 
   
  if(NULL != nodeValueStr) {
    // Add a node with no attribut and value
    nodeText = ixmlDocument_createTextNode((IXML_Document *)xmlDocPtr, nodeValueStr);

    // Assign the value to the new element
    ixmlNode_appendChild( &(pNewElement->n), nodeText);   
  }
    
  ixmlNode_appendChild((IXML_Node *) xmlParentNodePtr, &(pNewElement->n) );
  
  return ((GenericXmlNodePtr) &(pNewElement->n) );
        
}


/*
* @brief Function used to add node's attribut
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The node to add attribut
*        IN   char *            nodeAttributNameStr
*        IN   char *            nodeAttributTypeStr
*
* @return true on success (false otherwise)
*
*/
bool
xmlAddNodeAttribut(IN GenericXmlNodePtr     xmlNodePtr,
                   IN const char *          nodeAttributNameStr,
                   IN const char *          nodeAttributTypeStr)
{
  bool rc = false;
  
  // Check parameters
  if((NULL == xmlNodePtr)          ||
     (NULL == nodeAttributNameStr) || 
     (NULL == nodeAttributTypeStr)) {
    // Invalid
    EXEC_ERROR("Invalid Arguments");
  } else {
    if(IXML_SUCCESS == ixmlElement_setAttribute(/*&ixmlElement,*/ (IXML_Element *) xmlNodePtr, (char *) nodeAttributNameStr, (char *) nodeAttributTypeStr)) {
      rc = true;
    } else {
      EXEC_ERROR("Can not set ixmlElement_setAttribute");
    }
  }
  
  return rc;

}            

/*
* @brief This function returns the attribut value of the attributName for the given node
*
* @param IN   The Node and the attribut name
*
* @return A pointer on the attribut value (or NULL if no child node or error)
*
*/
char *
xmlGetAttributValue(IN GenericXmlNodePtr     xmlNodePtr,
                    char *                   attributNameStr)
{
  IXML_NamedNodeMap * attributListPtr = NULL;
  char              * attrValueToReturnStr = NULL;
  
  // check parameters
  if((NULL == xmlNodePtr) ||
     (NULL == attributNameStr)) {
    return NULL;
  }
  
  attributListPtr = ixmlNode_getAttributes((IXML_Node*)xmlNodePtr);
  while(NULL != attributListPtr) {
    if(0 == strcmp(attributNameStr, attributListPtr->nodeItem->nodeName)) {
      // Retrieve the attribut value
      if(NULL != attributListPtr->nodeItem->nodeValue) {
        attrValueToReturnStr = strdup(attributListPtr->nodeItem->nodeValue);
      }
      // return (char * ) (attributListPtr->nodeItem->nodeValue);
      break;
    }
    attributListPtr = attributListPtr->next;
  }
  
  // Free the map
  ixmlNamedNodeMap_free(attributListPtr);
  
  return attrValueToReturnStr;
}

/*
* @brief This function returns the list of the child nodes 
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The parent node of the clids nodes to retrieve
*
* @return A pointer on the child node list (or NULL if no child node or error)
*
*/
GenericXmlNodeListPtr
xmlGetChildNodesList(IN GenericXmlNodePtr     xmlParentNodePt)
{
  IXML_NodeList * nodeListPtr = NULL;
  
  if(NULL != xmlParentNodePt) {
    nodeListPtr = ixmlNode_getChildNodes((IXML_Node *) xmlParentNodePt);
  }
  
  return ((IXML_NodeList * ) nodeListPtr);
  
}



/*
* @brief This function returns the list of the child nodes with the corresponding tagName
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The parent node of the clids nodes to retrieve
*        IN   the Tag Name
*
* @return A pointer on the child node list (or NULL if no child node or error)
*
*/
GenericXmlNodeListPtr
xmlGetNodesListWithTagName(IN GenericXmlNodePtr     xmlParentNodePtr,
                           IN char *                tagNameStr)
{
  IXML_NodeList * nodeListPtr = NULL;

  if((NULL != xmlParentNodePtr) && (NULL != tagNameStr)) {
     nodeListPtr = ixmlDocument_getElementsByTagName ((IXML_Document *) xmlNodeToDocument(xmlParentNodePtr), tagNameStr);
  }
    
  return ((GenericXmlNodeListPtr ) nodeListPtr);
  
}


/*
* @brief This function returns the number of xml node in the node list
*
* @param IN   The node list
*
* @return The number of nodes in the list
*
*/          
unsigned int
xmlGetNodesListLength(GenericXmlNodeListPtr xmlNodeList)
{
  unsigned int nbNodes = 0;
  
  if(NULL != xmlNodeList) {
    nbNodes = ixmlNodeList_length((IXML_NodeList *)xmlNodeList);
  }
  
  return nbNodes;
  
  
}

/*
* @brief This function returns the number of child nodes of the parent node
*
* @param IN   The parent node
*
* @return The number of child nodes
*
*/          
unsigned int
xmlGetNumberOfChildNodes(IN GenericXmlNodePtr     xmlParentNodePt)
{ 
  unsigned int          nbNodes = 0;
  GenericXmlNodeListPtr listPtr = NULL;
  
  // check parameters
  if(NULL == xmlParentNodePt) {
    return nbNodes;
  }
  
  listPtr = xmlGetChildNodesList(xmlParentNodePt);
  nbNodes = xmlGetNodesListLength(listPtr);
  xmlFreeNodesList(listPtr);
  
  return nbNodes;

}

/*
* @brief This function returns the node corresponding to the given entry (0 based)
*
* @param IN   The node list
*
* @return The node
*
*/
GenericXmlNodePtr
xmlGetNodeFromNodesList(GenericXmlNodeListPtr xmlNodeList,
                        unsigned int          entry)
{
  IXML_Node * nodePtr = NULL;
  
  if(NULL != xmlNodeList) {
    nodePtr = ixmlNodeList_item((IXML_NodeList *) xmlNodeList, entry);
  }

  return ((GenericXmlNodePtr) nodePtr);

}

                  
/*
* @brief This function free the node list
*
* @param IN   The node list
*
* @return void
*
*/       
void
xmlFreeNodesList(GenericXmlNodeListPtr xmlNodeList)
{
  if(NULL != xmlNodeList) {
    ixmlNodeList_free( (IXML_NodeList *) xmlNodeList );
  }
}


/*
* @brief This function is used to return the main node of a document
*
* @param IN   The xml document
*
* @return The document node
*
*/ 
GenericXmlNodePtr
xmlDocumentToNode(IN GenericXmlDocumentPtr xmlDocPtr)
{
  GenericXmlNodePtr xmlNodePtr = NULL;
  
  if(NULL != xmlDocPtr) {
    xmlNodePtr = (GenericXmlNodePtr *) ((IXML_Document *)xmlDocPtr);
  }
  
  return xmlNodePtr;

}


/*
* @brief This function is used to return the document from a node entry.
*
* @param IN   The xml node
*
* @return The document
*
*/ 
GenericXmlDocumentPtr
xmlNodeToDocument(IN GenericXmlNodePtr xmlNodePtr)
{
  GenericXmlDocumentPtr xmlDocPtr = NULL;
  
  if(NULL != xmlNodePtr) {
    xmlDocPtr = (GenericXmlDocumentPtr *) xmlNodePtr;
  }
  
  return xmlDocPtr;

}
