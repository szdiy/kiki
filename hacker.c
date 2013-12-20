/*
* Copyright 2011 Anders Ma (andersma.com). All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. The name of the copyright holder may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WILLIAM TISÄTER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlreader.h>
#include <string.h>

#include "hacker.h"

hacker_t* new_hacker(hacker_list_t *l)
{
	hacker_t *new, *tmp;
	GSList *iter;
/*
	pthread_mutex_lock(&l->mutex);
	for (iter = l->list; iter != NULL; iter = g_slist_next (iter)) {
		tmp = (hacker_t*)iter->data;
		if (strcasecmp(hacker->rfid, tmp->rfid) == 0) {
			pthread_mutex_unlock(&l->mutex);
			return -1;
		}
	}
	pthread_mutex_unlock(&l->mutex);
*/
	new = (hacker_t*)g_malloc(sizeof(hacker_t));
	new->rfid = NULL;
	new->name = NULL;
	new->prof = NULL;
	new->why = NULL;
	new->email = NULL;
	new->mac = NULL;
	new->login = 0;
	new->login_time = NULL;

	pthread_mutex_lock(&l->mutex);
	l->list = g_slist_append(l->list, (gpointer)new);
	pthread_mutex_unlock(&l->mutex);

	return new;
}

hacker_t* search_hacker(hacker_list_t *l, char* rfid)
{
	hacker_t *hacker;
	GSList *iter;

	pthread_mutex_lock(&l->mutex);
	for (iter = l->list; iter != NULL; iter = g_slist_next (iter)) {
		hacker = (hacker_t*)iter->data;
		if (strcasecmp(rfid, hacker->rfid) == 0) {
			pthread_mutex_unlock(&l->mutex);
			return hacker;
		}
	}
	pthread_mutex_unlock(&l->mutex);
	return NULL;
}

static int write_default_hackers_xml()
{
	xmlDocPtr doc = NULL;       /* document pointer */
	xmlNodePtr szdiy_node, hacker_node = NULL;	/* node pointers */
	xmlNodePtr property_node = NULL;	/* node pointers */
	xmlDtdPtr dtd = NULL;       /* DTD pointer */
	
	LIBXML_TEST_VERSION;

	/* 
	 * Creates a new document, a node and set it as a root node
	 */
	doc = xmlNewDoc(BAD_CAST "1.0");
	szdiy_node = xmlNewNode(NULL, BAD_CAST "szdiy");
	xmlDocSetRootElement(doc, szdiy_node);

	/*
	 * Creates a DTD declaration. Isn't mandatory. 
	 */
	dtd = xmlCreateIntSubset(doc, BAD_CAST "Hackerspace", NULL, BAD_CAST "tree2.dtd");

	/*
	 * Hackers data
	 */

	// rfid
	hacker_node = xmlNewChild(szdiy_node, NULL, BAD_CAST "hacker", NULL);
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
						BAD_CAST HACKER_DEF_RFID);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_RFID_NODE);
	// name
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
						BAD_CAST HACKER_DEF_NAME);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_NAME_NODE);
	// profession
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
						BAD_CAST HACKER_DEF_PROF);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_PROF_NODE);
	// why coming?
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
						BAD_CAST HACKER_DEF_WHY);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_WHY_NODE);
	// email
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST HACKER_DEF_EMAIL);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_EMAIL_NODE);
	// mac address
	property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
						BAD_CAST HACKER_DEF_MAC);
	xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_MAC_NODE);
	
	/* 
	 * Dumping document to stdio or file
	 */
	xmlSaveFormatFileEnc(HACKERS_XML_FILENAME, doc, "UTF-8", 1);

	/*free the document */
	xmlFreeDoc(doc);

	/*
	 *Free the global variables that may
	 *have been allocated by the parser.
	*/
	xmlCleanupParser();

	/*
	 * this is to debug memory for regression tests
	 */
	xmlMemoryDump();
	return(0);
}

static int init_hackers_from_xml(hacker_list_t* l)
{
/*
	Get the node type of the current node Reference:
	http://www.gnu.org/software/dotgnu/pnetlib-doc/System/Xml/XmlNodeType.html
*/	
	xmlTextReaderPtr reader;
	const xmlChar *key, *name, *value;
	int ret, no_value;
	hacker_t* hacker;

	if (access(HACKERS_XML_FILENAME, F_OK) != 0) {
		write_default_hackers_xml();
	}

	reader = xmlReaderForFile(HACKERS_XML_FILENAME, NULL, 0);
	if (reader != NULL) {
		ret = xmlTextReaderRead(reader);
		while (ret == 1) {
			/* 
			 *parse XML node 
			 */
			name = xmlTextReaderConstName(reader);
			if (name == NULL)
				name = BAD_CAST "--";
			// check hacker & create new node
			key = xmlTextReaderConstName(reader);
			if (strcasecmp("hacker", key) == 0 && xmlTextReaderNodeType(reader) == 1) {
				hacker = new_hacker(l);
				DBG_PRINTF("--------------------\n");
			}
			// check key
			key = xmlTextReaderGetAttribute(reader, BAD_CAST "key");
			if (key == NULL) {
				ret = xmlTextReaderRead(reader);
				continue;
			} else {
				if (xmlTextReaderNodeType(reader) == 1) { // XmlNodeType.Element Field
					no_value = 0;
					while (ret == 1) {
						ret = xmlTextReaderRead(reader);
						//DBG_PRINTF("%d ", xmlTextReaderNodeType(reader));
						if (xmlTextReaderNodeType(reader) == 3) // XmlNodeType.Text Field
							break;
						if (xmlTextReaderNodeType(reader) == 15) { // XmlNodeType.EndElement Field
							no_value = 1;
							break;
						}
						continue;
					}
					if (ret != 1)
						break;
				} else {
					ret = xmlTextReaderRead(reader);
					continue;
				}
			}
			if (no_value)
				continue;
			value = xmlTextReaderConstValue(reader);
			if(name) {
				value = xmlTextReaderConstValue(reader);
				// mapping data
				if (strcasecmp(HACKER_RFID_NODE, key) == 0) {
					hacker->rfid = strdup(value);
				} else if (strcasecmp(HACKER_NAME_NODE, key) == 0) {
					hacker->name = strdup(value);
				} else if (strcasecmp(HACKER_PROF_NODE, key) == 0) {
					hacker->prof = strdup(value);
				} else if (strcasecmp(HACKER_WHY_NODE, key) == 0) {
					hacker->why = strdup(value);
				} else if (strcasecmp(HACKER_EMAIL_NODE, key) == 0) {
					hacker->email = strdup(value);
				}else if (strcasecmp(HACKER_MAC_NODE, key) == 0) {
					hacker->mac = strdup(value);
				}else {
				}
				DBG_PRINTF("%s: %s\n", key, value);
			}
			ret = xmlTextReaderRead(reader);
		}
		xmlFreeTextReader(reader);
		if (ret != 0) {
			fprintf(stderr, "%s : failed to parse\n", HACKERS_XML_FILENAME);
			return -1;
		}
	} else {
		fprintf(stderr, "Unable to open %s\n", HACKERS_XML_FILENAME);
		return -1;
	}

	return 0;
}

/*
 * read hackers' information from XML file
 */
void hackers_init(hacker_list_t* l)
{
	l->list = NULL;
	pthread_mutex_init(&(l->mutex), NULL);

	init_hackers_from_xml(l);
}

void bye_hacker(gpointer data, gpointer user_data) 
{
	char *p;
	hacker_t* hacker = (hacker_t*)data;

	if (hacker == NULL)
		return;

	if (hacker->rfid) {
		p = hacker->rfid;
		hacker->rfid = NULL;
		free(p);
	}
	if (hacker->name) {
		p = hacker->name;
		hacker->name = NULL;
		free(p);
	}
	if (hacker->prof) {
		p = hacker->prof;
		hacker->prof = NULL;
		free(p);
	}
	if (hacker->email) {
		p = hacker->email;
		hacker->email = NULL;
		free(p);
	}
	if (hacker->mac) {
		p = hacker->mac;
		hacker->mac = NULL;
		free(p);
	}
	if (hacker->why) {
		p = hacker->why;
		hacker->why = NULL;
		free(p);
	}
	if (hacker->login_time) {
		p = hacker->login_time;
		hacker->login_time = NULL;
		free(p);
	}

	free(hacker);
}

void hackers_reload(hacker_list_t* l)
{
	hackers_destroy(l);
	hackers_init(l);
/*	
	g_slist_foreach(l->list, (GFunc)bye_hacker, NULL);
	g_slist_free(l->list);
	l->list = NULL;
	init_hackers_from_xml(l);
*/
}

void hackers_destroy(hacker_list_t* l)
{
	pthread_mutex_lock(&l->mutex);
	g_slist_foreach(l->list, (GFunc)bye_hacker, NULL);
	g_slist_free(l->list);
	pthread_mutex_unlock(&l->mutex);

}

void hackers_save(hacker_list_t *l)
{
	hacker_t *hacker;
	GSList *iter;


	xmlDocPtr doc = NULL;       /* document pointer */
	xmlNodePtr szdiy_node, hacker_node = NULL;	/* node pointers */
	xmlNodePtr property_node = NULL;	/* node pointers */
	xmlDtdPtr dtd = NULL;       /* DTD pointer */
	
	LIBXML_TEST_VERSION;

	/* 
	 * Creates a new document, a node and set it as a root node
	 */
	doc = xmlNewDoc(BAD_CAST "1.0");
	szdiy_node = xmlNewNode(NULL, BAD_CAST "szdiy");
	xmlDocSetRootElement(doc, szdiy_node);

	/*
	 * Creates a DTD declaration. Isn't mandatory. 
	 */
	dtd = xmlCreateIntSubset(doc, BAD_CAST "Hackerspace", NULL, BAD_CAST "tree2.dtd");

	/*
	 * Hackers data
	 */

	pthread_mutex_lock(&l->mutex);
	for (iter = l->list; iter != NULL; iter = g_slist_next (iter)) {
		hacker = (hacker_t*)iter->data;
		// rfid
		hacker_node = xmlNewChild(szdiy_node, NULL, BAD_CAST "hacker", NULL);
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->rfid);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_RFID_NODE);
		// name
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->name);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_NAME_NODE);
		// profession
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->prof);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_PROF_NODE);
		// why coming?
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->why);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_WHY_NODE);
		// email
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->email);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_EMAIL_NODE);
		// mac address
		property_node = xmlNewChild(hacker_node, NULL, BAD_CAST "property",
							BAD_CAST hacker->mac);
		xmlNewProp(property_node, BAD_CAST "key", BAD_CAST HACKER_MAC_NODE);

	}
	pthread_mutex_unlock(&l->mutex);

	/* 
	 * Dumping document to stdio or file
	 */
	xmlSaveFormatFileEnc(HACKERS_XML_FILENAME, doc, "UTF-8", 1);

	/*free the document */
	xmlFreeDoc(doc);

	/*
	 *Free the global variables that may
	 *have been allocated by the parser.
	*/
	xmlCleanupParser();

	/*
	 * this is to debug memory for regression tests
	 */
	xmlMemoryDump();
	return;
}

