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
 * File        : DM_COM_GenericDomXmlParserInterface.h
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
 * @file GenericDomXmlParserInterface.h
 *
 * @brief Defines the generic XML API
 *
 *
 */
 
#ifndef _GENERIC_DOM_XML_PARSER_INTERFACE_H
#define _GENERIC_DOM_XML_PARSER_INTERFACE_H

#include "DM_ENG_Common.h"

typedef void * GenericXmlNodePtr;      // Generic Pointer on XML Node
typedef void * GenericXmlDocumentPtr;  // Generic Pointer on XML Document
typedef void * GenericXmlNodeListPtr;  // Generic Pointer on XML Node List


/*
* @brief Function used to convert an XML text buffer into XML DOM representation
*
* @param the xmlStringBuffer
*
* @return The function returns a pointer on the XML doucment or null on error.
*
*/
GenericXmlDocumentPtr
xmlStringBufferToXmlDocument(IN char * xmlStringBuffer);

/*
* @brief Function used to convert an XML file into XML DOM representation
*
* @param the file name
*
* @return The function returns a pointer on the XML doucment or null on error.
*
*/
GenericXmlDocumentPtr
xmlFileToXmlDocument(IN const char* fileName);

/*
* @brief Function used to convert an XML text buffer into XML DOM representation
*
* @param the xmlDocument
*
* @return The function returns a pointer on the XML doucment or null on error.
*
*/
char *
xmlDocumentToStringBuffer(IN GenericXmlDocumentPtr xmlDocumentPtr);

/*
* @brief Function used to free an xml document
*
* @param the xmlDocument
*
* @return void
*
*/
void
xmlDocumentFree(GenericXmlDocumentPtr xmlDocPtr);

/*
* @brief Function used to retrieve the first child node of the given document
*
* @param GenericXmlDocumentPtr xml doucment
*
* @return The function returns a pointer on the XML child node (GenericXmlNode)
*         or NULL on error or if no child node exist.
*
*/
GenericXmlNodePtr
xmlGetFirstChildNodeOfDocument(IN GenericXmlDocumentPtr xmlDocument);

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
                           IN char *                nodeNamePtr);

/*
* @brief Function used to retrieve the value of the node (string type)
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The node to retrieves paramters
*        OUT  The node name (if xmlNodeNameStr is not NULL)
*        OUT  The node value (if xmlNodeValueStr is not NULL)
*
* @return The function returns a pointer on the XML child node (GenericXmlNode)
*         or NULL on error or if no child node exist.
*
*/
bool
xmlGetNodeParameters(IN  GenericXmlNodePtr xmlNodePtr,
                     OUT char **           xmlNodeNameStr,
                     OUT char **           xmlNodeValueStr);


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
		       IN const char *          nodeValueStr);


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
                   IN const char *          nodeAttributTypeStr);


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
                    char *                   attributNameStr);

/*
* @brief This function returns the list of the child nodes 
*
* @param IN   GenericXmlNodePtr xmlNodePtr // The parent node of the clids nodes to retrieve
*
* @return A pointer on the child node list (or NULL if no child node or error)
*
*/
GenericXmlNodeListPtr
xmlGetChildNodesList(IN GenericXmlNodePtr     xmlParentNodePt);



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
xmlGetNodesListWithTagName(IN GenericXmlNodePtr     xmlParentNodePt,
                           IN char *                tagName);


/*
* @brief This function returns the number of xml node in the node list
*
* @param IN   The node list
*
* @return The number of nodes in the list
*
*/			   
unsigned int
xmlGetNodesListLength(GenericXmlNodeListPtr xmlNodeList);


/*
* @brief This function returns the number of child nodes of the parent node
*
* @param IN   The parent node
*
* @return The number of child nodes
*
*/			   
unsigned int
xmlGetNumberOfChildNodes(IN GenericXmlNodePtr     xmlParentNodePt);

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
                        unsigned int          entry);

						
/*
* @brief This function free the node list
*
* @param IN   The node list
*
* @return void
*
*/			
void
xmlFreeNodesList(GenericXmlNodeListPtr xmlNodeList);


/*
* @brief This function is used to return the main node of a document
*
* @param IN   The xml document
*
* @return The document node
*
*/	
GenericXmlNodePtr
xmlDocumentToNode(IN GenericXmlDocumentPtr xmlDocPtr);


/*
* @brief This function is used to return the document from a node entry.
*
* @param IN   The xml node
*
* @return The document
*
*/	
GenericXmlDocumentPtr
xmlNodeToDocument(IN GenericXmlNodePtr xmlNodePtr);


#endif /* _GENERIC_DOM_XML_PARSER_INTERFACE_H */
 
 

