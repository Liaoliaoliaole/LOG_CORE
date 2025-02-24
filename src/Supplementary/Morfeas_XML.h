/*
File: Morfeas_XML.h, Declaration of functions for read XML files
Copyright (C) 12019-12021  Sam harry Tzavaras

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3 of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <glib.h>
#include <gmodule.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

// *** All the function Returns: EXIT_SUCCESS on success, or EXIT_FAILURE or failure. Except of other notice *** //

//-- General XML Functions --//
//Function that parser and validate the Nodeset configuration XML
int Morfeas_XML_parsing(const char *filename, xmlDocPtr *doc);
//Function that find and return the node with name "Node_name"
xmlNode * get_XML_node(xmlNode *root_node, const char *Node_name);
//Function where scan a root xmlNodePtr for node with node_name, return the contents of the node, or NULL if it does not exist
char * XML_node_get_content(xmlNode *node, const char *node_name);

//-- Morfeas_daemon's Configuration XML related --//
//Function that validate the content of the Configuration for Morfeas_deamon
int Morfeas_daemon_config_valid(xmlNode *root_element);

//-- Morfeas_OPC_UA's Configuration XML related --//
//Function that validate the content of the Configuration for Morfeas OPC_UA
int Morfeas_opc_ua_config_valid(xmlNode *root_element);
//Build list diff with content the ISO_Channels that will be removed
int Morfeas_OPC_UA_calc_diff_of_ISO_Channel_node(xmlNode *root_element, GSList **cur_Links);
//Clean all elements of List "cur_ISOChannels" with Re-Build it with data from xmlDoc doc
int XML_doc_to_List_ISO_Channels(xmlNode *root_element, GSList **cur_Links);
//GCompareFunc used in g_slist_find_custom
gint List_Links_cmp (gconstpointer a, gconstpointer b);
//Deconstructor for Data of Lists with data type "struct Link_entry"
void free_Link_entry(gpointer data);
//Debugging function Print node from List with data type "struct Link_entry"
//void print_List (gpointer data, gpointer user_data);
