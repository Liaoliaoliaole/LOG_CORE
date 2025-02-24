/*
File: Morfeas_XML.c, Implementation of functions for read XML files
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include <glib.h>
#include <gmodule.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "../IPC/Morfeas_IPC.h"// -> #include "Morfeas_Types.h"
#include "Morfeas_run_check.h"

/*
void print_XML_node(xmlNode * node)
{
    xmlNode *cur_node;
	if(node->type == XML_ELEMENT_NODE)
	{
		printf("Node name: %s\n", node->name);
		for (cur_node = node->children; cur_node; cur_node = cur_node->next)
		{
			if(cur_node->type == XML_ELEMENT_NODE)
			{
				if(cur_node->children)
				{
					if(cur_node->children->content)
					{
						printf("\tChild Node name: %s\n", cur_node->name);
						printf("\t\tHave contents: %s\n", cur_node->children->content);
					}
					else if(cur_node->children)
					{
						print_XML_node(cur_node);
					}
				}
				else
					printf("\t\tEmpty\n");
			}
			else
				printf("Is Not 'XML_ELEMENT_NODE'\n");
		}
	}
}
*/
xmlNode* scaning_XML_nodes_for_empty(xmlNode * node)
{
    xmlNode *cur_node, *ret = NULL;
	if(node->type == XML_ELEMENT_NODE)
	{
		for (cur_node = node->children; cur_node; cur_node = cur_node->next)
		{
			if(cur_node->type == XML_ELEMENT_NODE)
			{
				if(cur_node->children)
				{
					ret = scaning_XML_nodes_for_empty(cur_node);
					if(ret)
						return ret;
				}
				else
					return cur_node;
			}
		}
	}
	return NULL;
}

xmlNode * get_XML_node(xmlNode *root_node, const char *Node_name)
{
	xmlNode *cur_node, *ret = NULL;
	if(root_node->type == XML_ELEMENT_NODE)
	{
		for (cur_node = root_node->children; cur_node; cur_node = cur_node->next)
		{
			if(cur_node->type == XML_ELEMENT_NODE)
			{
				if(!strcmp((char *)(cur_node->name), Node_name))
					return cur_node;
				if(cur_node->children)
					if((ret = get_XML_node(cur_node, Node_name)))
						return ret;
			}
		}
	}
	return NULL;
}

char * XML_node_get_content(xmlNode *node, const char *node_name)
{
    xmlNode *cur_node;
	if(node)
	{
		for (cur_node = node->children; cur_node; cur_node = cur_node->next)
		{
			if(cur_node->type == XML_ELEMENT_NODE)
			{
				if(!strcmp((char *)(cur_node->name), node_name))
				{
					if(cur_node->children)
						return (char *)(cur_node->children->content);
					else
						return NULL;
				}
			}
		}
	}
	return NULL;
}
/*
 * Function where check node for attribute "Disable".
 * Return: 0 if Disable="false",
		   1 if Disable="true",
		   -1 if attribute is not found.
 */
int getprop_disable(xmlNode *node)
{
	int retval;
	xmlChar *content;
	while(node)
	{
		if(node->type == XML_ELEMENT_NODE)
		{
			if((content = xmlGetProp(node, BAD_CAST"Disable")))
			{
				if(!strcmp((char *)content, "true"))
					retval = 1;
				else if(!strcmp((char *)content, "false"))
					retval = 0;
				else
					retval = -1;
				xmlFree(content);
				return retval;
			}
		}
		node = node->next;
	}
	return -1;
}

int Morfeas_XML_parsing(const char *filename, xmlDocPtr *doc)
{
    xmlParserCtxtPtr ctxt;
    //--- create a parser context ---//
    if(!(ctxt = xmlNewParserCtxt()))
    {
        fprintf(stderr, "Failed to allocate parser context\n");
		return EXIT_FAILURE;
    }
    //--- parse the file, activating the DTD validation option ---//
    if(!(*doc = xmlCtxtReadFile(ctxt, filename, NULL, XML_PARSE_DTDVALID | XML_PARSE_NOBLANKS)))
    {
        fprintf(stderr, "Failed to parse %s\n", filename);
        xmlFreeParserCtxt(ctxt);
        return EXIT_FAILURE;
    }
    else
    {	//check if validation succeeded
		if(!(ctxt->valid))
		{
        	fprintf(stderr, "Failed to validate %s\n", filename);
        	xmlFreeParserCtxt(ctxt);
        	xmlFreeDoc(*doc);
        	return EXIT_FAILURE;
        }
    }
    xmlFreeParserCtxt(ctxt);
    return EXIT_SUCCESS;
}

#define max_arg_range 3
#define anchor_check_buff_size 100

//Function that validate the anchor's components. Return 0 on success or -1 on failure.
int validate_anchor_comp(char *anchor_str, char handler_type)
{
	unsigned int anchor_arg_int[max_arg_range], i, dots;
	char *channel, *receiver_or_value, *tele_type_or_id, *addr=NULL, *UniNOx_val=NULL;
	if(!anchor_str)
		return EXIT_FAILURE;
	if(handler_type != NOX)
	{
		//Check anchor_str for correct anchor component names
		if(!atoi(anchor_str)) //identifier component check
			return EXIT_FAILURE;
		if(!(channel = strstr(anchor_str, ".CH")))//channel component check
			return EXIT_FAILURE;
		if(!atoi(channel+strlen(".CH")))//Channel value check
			return EXIT_FAILURE;
	}
	switch(handler_type)
	{
		case IOBOX:
			if(!(receiver_or_value = strchr(anchor_str, '.')))//Receiver component check
				return EXIT_FAILURE;
			if(*(receiver_or_value+1)=='R'&&*(receiver_or_value+2)=='X')
			{
				if(!atoi(receiver_or_value+strlen(".RX")))//Receiver value check
					return EXIT_FAILURE;
				if(strstr(anchor_str, ".RX")>=channel)//If Receiver exist must be before channels component
					return EXIT_FAILURE;
				sscanf(anchor_str, "%u.RX%u.CH%u", &anchor_arg_int[0], &anchor_arg_int[1], &anchor_arg_int[2]);
				if(!anchor_arg_int[0] || !anchor_arg_int[1] || !anchor_arg_int[2])
					return EXIT_FAILURE;
			}
			else
				return EXIT_FAILURE;
			break;
		case MDAQ:
			if(!(receiver_or_value = strstr(anchor_str, ".CH")))//Value component check
				return EXIT_FAILURE;
			receiver_or_value+=strlen(".CH?");
			if(*(receiver_or_value+1)=='V'&&*(receiver_or_value+2)=='a'&&*(receiver_or_value+3)=='l')
			{
				if(!atoi(receiver_or_value+strlen(".CH?")))//Value's value check
					return EXIT_FAILURE;
				if(strstr(anchor_str, ".Val")<=channel)//If Value exist must be after channels component
					return EXIT_FAILURE;
				sscanf(anchor_str, "%u.CH%u.Val%u", &anchor_arg_int[0], &anchor_arg_int[1], &anchor_arg_int[2]);
				if(!anchor_arg_int[0] || !anchor_arg_int[1] || !anchor_arg_int[2])
					return EXIT_FAILURE;
			}
			else
				return EXIT_FAILURE;
			break;
		case SDAQ:
			if(!(channel = strchr(anchor_str, '.')))//Channel component check
				return EXIT_FAILURE;
			if(*(channel+1)=='C' && *(channel+2)=='H')
			{
				sscanf(anchor_str, "%u.CH%u", &anchor_arg_int[0], &anchor_arg_int[1]);
				if(!anchor_arg_int[0]||!anchor_arg_int[1])
					return EXIT_FAILURE;
			}
			else
				return EXIT_FAILURE;
			break;
		case MTI:
			//Check existence and validity of Tele_type field
			i=Tele_TC16;//Start checking from Tele_TC16 value
			while(MTI_Tele_dev_type_str[i])
			{
				if(!(tele_type_or_id = strstr(anchor_str, MTI_Tele_dev_type_str[i])))
					i++;
				else
					break;
			}
			if(!MTI_Tele_dev_type_str[i])
			{
				if(!(tele_type_or_id = strstr(anchor_str, ".ID:")))
					return EXIT_FAILURE;
				if(!atoi(tele_type_or_id+strlen(".ID:")))
					return EXIT_FAILURE;
			}
			else
				if(*tele_type_or_id != '.' && *(tele_type_or_id+strlen(MTI_Tele_dev_type_str[i])) != '.')
					return EXIT_FAILURE;
			if(tele_type_or_id>channel)//Check if channel is after tele_type_or_id
				return EXIT_FAILURE;
			break;
		case NOX:
			//Check amount of dots('.') in anchor_str.
			for(dots=0, i=0; anchor_str[i]; i++)
			{
				if(anchor_str[i] == '.')
				{
					dots++;
					switch(dots)
					{
						case 1: addr = anchor_str+i+1; break;
						case 2: UniNOx_val = anchor_str+i+1; break;
					}
				}
			}
			if(dots != 2)//Check if anchor_str have only 2 dots.
				return EXIT_FAILURE;
			if(!addr || !UniNOx_val)
				return EXIT_FAILURE;
			for(i=0; anchor_str[i] != '.'; i++); //Count characters of CAN_if_name section of anchor_str.
			if(i >= Dev_or_Bus_name_str_size)
				return EXIT_FAILURE;
			size_t addr_str_len = strlen("addr_x");
			if(strncmp(addr, "addr_0", addr_str_len) && strncmp(addr, "addr_1", addr_str_len))
				return EXIT_FAILURE;
			if(strcmp(UniNOx_val, "NOx") && strcmp(UniNOx_val, "O2"))
				return EXIT_FAILURE;
			break;
		default: return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

int Morfeas_opc_ua_config_valid(xmlNode *root_element)
{
	union check_flags{
		struct xml_check_flags{
			unsigned interface_name : 1;
			unsigned iso_channel : 1;
			unsigned anchor : 1;
		}as_struct;
		unsigned char as_byte;
	}fl = {.as_byte = 0};
	xmlNode *check_element, *element;
	char *content, *iso_channel, *dev_type_str;
	unsigned int if_name_okay;
	if(!root_element->children)
		return EXIT_SUCCESS;
	//Check for Empty XML nodes content and for Invalid Interface_name content
	for(element = root_element->children; element; element = element->next)
	{
		if(element->type == XML_ELEMENT_NODE)
		{
			for(check_element = element->children; check_element; check_element = check_element->next)
			{
				if(check_element->type == XML_ELEMENT_NODE)
				{
					if(!check_element->children)//Empty Check
					{
						fprintf(stderr, "\nNode:%s (on line:%d) found to have zero content!!!!\n\n", check_element->name, check_element->line);
						return EXIT_FAILURE;
					}
					else if(!strcmp((char *)(check_element->name), "INTERFACE_TYPE"))//Invalid Interface name
					{
						fl.as_struct.interface_name = 1;
						if_name_okay = 0;
						for(int i=0; Morfeas_IPC_handler_type_name[i]; i++)
							if(!strcmp(Morfeas_IPC_handler_type_name[i], (char *)(check_element->children->content)))
								if_name_okay = 1;
						if(!if_name_okay)
						{
							fprintf(stderr, "\nContent:\"%s\" of Node:\"INTERFACE_TYPE\" (on line: %d) is Out of Range (",
								check_element->children->content,
							    check_element->line);
							for(int j=0; Morfeas_IPC_handler_type_name[j]; j++)
							{
								fprintf(stderr, "%s",Morfeas_IPC_handler_type_name[j]);
								if(Morfeas_IPC_handler_type_name[j+1])
									fprintf(stderr,", ");
							}
							fprintf(stderr,")!!!!\n\n");
							return EXIT_FAILURE;
						}
					}
				}
			}
		}
	}
	//Check for duplicate ISO_CHANNEL
	for(element = root_element->children; element->next; element = element->next)
	{   //print_XML_node(element);
		if(element->type == XML_ELEMENT_NODE)
		{
			if((iso_channel = XML_node_get_content(element, "ISO_CHANNEL")))
			{
				fl.as_struct.iso_channel = 1;
				for(check_element = element->next; check_element; check_element = check_element->next)
				{
					if((content = XML_node_get_content(check_element, "ISO_CHANNEL")))
					{
						if(!strcmp(content, iso_channel))
						{
							fprintf(stderr, "\nISO_CHANNEL: \"%s\" Found multiple times!!!!\n\n", iso_channel);
							return EXIT_FAILURE;
						}
					}
				}
			}
		}
	}
	//Check for invalid contents in ISO_CHANNEL and ANCHOR
	for(check_element = root_element->children; check_element; check_element = check_element->next)
	{
		if((iso_channel = XML_node_get_content(check_element, "ISO_CHANNEL")))
		{
			if(strlen(iso_channel)>=ISO_channel_name_size)
			{
				fprintf(stderr, "\nISO_CHANNEL : \"%s\" is too long (>=%u) !!!!\n\n", iso_channel, ISO_channel_name_size);
				return EXIT_FAILURE;
			}
			if(strstr(iso_channel,"."))//'.' is illegal character for the ISO_Channel
			{
				fprintf(stderr, "\nISO_CHANNEL : \"%s\" is NOT valid (contains '.') !!!!\n\n", iso_channel);
				return EXIT_FAILURE;
			}
		}
		if((content = XML_node_get_content(check_element, "ANCHOR"))
		  &&(dev_type_str = XML_node_get_content(check_element, "INTERFACE_TYPE")))
		{
			fl.as_struct.anchor = 1;
			//TODO: More checks on values
			if(validate_anchor_comp(content, if_type_str_2_num(dev_type_str)))
			{
				fprintf(stderr, "\nANCHOR :\"%s\" of ISO_CHANNEL :\"%s\" (Type:\"%s\") is NOT valid!!!!\n\n", content,
																												  iso_channel,
																												  dev_type_str);
				return EXIT_FAILURE;
			}
		}
	}
	if(!fl.as_byte)
	{
		fprintf(stderr, "\nConfiguration XML have missing nodes !!!!\n\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

//GCompareFunc used in g_slist_find_custom
gint List_Links_cmp (gconstpointer a, gconstpointer b)
{
	const char *node_data = ((struct Link_entry*)a)->ISO_channel_name, *ISO_Channel_name = b;
	return strcmp(node_data, ISO_Channel_name);
}

//Constructor of Entry for List with data type "struct Link_entry"
struct Link_entry* new_Link_entry()
{
    struct Link_entry *new_node = g_slice_new0(struct Link_entry);
    return new_node;
}

//Deconstructor for Data of Lists with data type "struct Link_entry"
void free_Link_entry(gpointer data)
{
	struct Link_entry *node = (struct Link_entry *) data;
	if(node->CAN_IF_name)
	{
		free(node->CAN_IF_name);
		node->CAN_IF_name = NULL;
	}
	g_slice_free(struct Link_entry, data);
}
/*
void print_List (gpointer data, gpointer user_data)
{
	struct Link_entry *node_data = data;
	printf("Data of Node :\n");
	printf("\tISO_channel_name: %s\n", node_data->ISO_channel_name);
	printf("\tInterface_type: %s\n", node_data->interface_type);
	printf("\tIdentifier: %u\n", node_data->identifier);
	printf("\tChannel: %hhu\n", node_data->channel);
	if(node_data->CAN_IF_name)
		printf("\CAN_IF_name: %s\n", node_data->CAN_IF_name);
}
*/
int Morfeas_OPC_UA_calc_diff_of_ISO_Channel_node(xmlNode *root_element, GSList **cur_Links)
{
	xmlNode *check_element;
	GSList *node;
	char *iso_channel;
	if(cur_Links && root_element->children)
	{
		for(check_element = root_element->children; check_element; check_element = check_element->next)
		{
			if((iso_channel = XML_node_get_content(check_element, "ISO_CHANNEL")))
			{
				if((node = g_slist_find_custom(*cur_Links, iso_channel, List_Links_cmp)))
				{
					free_Link_entry(node->data);
					*cur_Links = g_slist_delete_link(*cur_Links, node);
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

int XML_doc_to_List_ISO_Channels(xmlNode *root_element, GSList **cur_Links)
{
	int i;
	xmlNode *check_element;
	struct Link_entry *list_cur_Links_node_data;
	char *iso_channel_str, *dev_type_str, *anchor_ptr, *TeleID;
	char format_str[30];

	g_slist_free_full(*cur_Links, free_Link_entry);//Free List cur_Links
	*cur_Links = NULL;
	if(root_element->children)
	{
		for(check_element = root_element->children; check_element; check_element = check_element->next)
		{
			iso_channel_str = XML_node_get_content(check_element, "ISO_CHANNEL");
			dev_type_str = XML_node_get_content(check_element, "INTERFACE_TYPE");
			anchor_ptr = XML_node_get_content(check_element, "ANCHOR");
			if(iso_channel_str && dev_type_str && anchor_ptr)
			{
				list_cur_Links_node_data = new_Link_entry();
				if(list_cur_Links_node_data)
				{
					memccpy(&(list_cur_Links_node_data->ISO_channel_name), iso_channel_str, '\0', sizeof(list_cur_Links_node_data->ISO_channel_name));
					switch((list_cur_Links_node_data->interface_type_num = if_type_str_2_num(dev_type_str)))
					{
						case IOBOX:
							sscanf(anchor_ptr, "%u.RX%hhu.CH%hhu", &(list_cur_Links_node_data->identifier),
																   &(list_cur_Links_node_data->rxNum_teleType_or_value),
																   &(list_cur_Links_node_data->channel));
							break;
						case MDAQ:
							sscanf(anchor_ptr, "%u.CH%hhu.Val%hhu", &(list_cur_Links_node_data->identifier),
																	&(list_cur_Links_node_data->channel),
																	&(list_cur_Links_node_data->rxNum_teleType_or_value));
							break;
						case SDAQ:
							sscanf(anchor_ptr, "%u.CH%hhu", &(list_cur_Links_node_data->identifier),
															&(list_cur_Links_node_data->channel));
							break;
						case MTI:
							if((TeleID=strstr(anchor_ptr, "ID:")))//Check if anchor referring to MiniRMSW
							{
								list_cur_Links_node_data->rxNum_teleType_or_value = RMSW_MUX;
								list_cur_Links_node_data->tele_ID = atoi(TeleID+strlen("ID:"));
								sprintf(format_str, "%%u.ID:%u.CH%%hhu", list_cur_Links_node_data->tele_ID);
							}
							else
							{
								i=Tele_TC16;
								while(!strstr(anchor_ptr, MTI_Tele_dev_type_str[i]))
									i++;
								list_cur_Links_node_data->rxNum_teleType_or_value = i;//number or Tele_type
								sprintf(format_str, "%%u.%s.CH%%hhu", MTI_Tele_dev_type_str[i]);
							}
							sscanf(anchor_ptr, format_str, &(list_cur_Links_node_data->identifier),
														   &(list_cur_Links_node_data->channel));
							break;
						case NOX:
							for(i=0; anchor_ptr[i]!='.'; i++);//Count characters until first dot.
							if(!(list_cur_Links_node_data->CAN_IF_name = calloc(i+1, sizeof(char))))
							{
								fprintf(stderr,"Memory error!\n");
								exit(EXIT_FAILURE);
							}
							for(i=0; anchor_ptr[i]!='.'; i++)//copy CAN_IF_name section from anchor to Links_node list.
								list_cur_Links_node_data->CAN_IF_name[i] = anchor_ptr[i];
							list_cur_Links_node_data->CAN_IF_name[i] = '\0';
							anchor_ptr += i+strlen("addr_")+1;
							list_cur_Links_node_data->channel = atoi(anchor_ptr);
							anchor_ptr += 2;//move to last anchor's argument.
							if(!strcmp(anchor_ptr, "NOx"))
								list_cur_Links_node_data->rxNum_teleType_or_value = NOx_val;
							else if(!strcmp(anchor_ptr, "O2"))
								list_cur_Links_node_data->rxNum_teleType_or_value = O2_val;
							break;
					}
					*cur_Links = g_slist_append(*cur_Links, list_cur_Links_node_data);
				}
				else
				{
					fprintf(stderr,"Memory error!\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
	return EXIT_SUCCESS;
}

//Return 0 if file is accessible, or 1 if configs_dir does not exist, otherwise -1
int check_file(const char *configs_dir, const char *file_name)
{
	DIR *configs_dir_ptr;
	int retval = 0;
	char *abs_file_path;
	if(!configs_dir || !file_name)
		return -1;
	if(!(configs_dir_ptr = opendir(configs_dir)))
		return 1;
	closedir(configs_dir_ptr);
	abs_file_path = calloc(strlen(configs_dir)+strlen(file_name)+5, sizeof(char));
	if(!abs_file_path)
	{
		fprintf(stderr, "Memory Error!!!\n");
		exit(EXIT_FAILURE);
	}
	strcat(abs_file_path, configs_dir);
	if(abs_file_path[strlen(abs_file_path)-1]!='/')
		abs_file_path[strlen(abs_file_path)]='/';
	strcat(abs_file_path, file_name);
	if(access((const char*)abs_file_path, R_OK))
		retval = -1;
	free(abs_file_path);
	return retval;
}

int Morfeas_daemon_config_valid(xmlNode *root_element)
{
	xmlNode *xml_node, *components_head_node, *check_node;
	xmlChar *content, *ipv4_addr, *dev_name, *config_Dir;
	//Check for nodes with Empty content
	if((xml_node = scaning_XML_nodes_for_empty(root_element)))
	{
		fprintf(stderr, "\nNode \"%s\" @Line: %d does not have content !!!!\n\n", xml_node->name, xml_node->line);
		return EXIT_FAILURE;
	}
	//Check for existence of node "CONFIGS_DIR"
	if(!(config_Dir = (xmlChar *) XML_node_get_content(root_element, "CONFIGS_DIR")))
	{
		fprintf(stderr, "\"CONFIGS_DIR\" XML node not found\n");
		return EXIT_FAILURE;
	}
	//Check for existence of node "LOGGERS_DIR"
	if(!XML_node_get_content(root_element, "LOGGERS_DIR"))
	{
		fprintf(stderr, "\"LOGGERS_DIR\" XML node not found\n");
		return EXIT_FAILURE;
	}
	//Check for existence of node "LOGSTAT_DIR"
	if(!XML_node_get_content(root_element, "LOGSTAT_DIR"))
	{
		fprintf(stderr, "\"LOGSTAT_DIR\" XML node not found\n");
		return EXIT_FAILURE;
	}
	//Check for existence of node "COMPONENTS"
	if(!(components_head_node = get_XML_node(root_element, "COMPONENTS")))
	{
		fprintf(stderr, "\"COMPONENTS\" XML node not found\n");
		return EXIT_FAILURE;
	}
	//Scan children of node "COMPONENTS" for Attribute errors
	xml_node = components_head_node->children;
	while(xml_node)
	{
		if(xml_node->type == XML_ELEMENT_NODE)
		{
			if((content = xmlGetProp(xml_node, BAD_CAST"Disable")))
			{
				if(strcmp((char *)content, "true") && strcmp((char *)content, "false"))
				{
					fprintf(stderr, "Attribute Value: \"%s\" for XML node \"COMPONENTS\"(Line:%d) is out of range (true,false)\n",
						(char*)content, xml_node->line);
					xmlFree(content);
					return EXIT_FAILURE;
				}
				xmlFree(content);
			}
			else
			{
				fprintf(stderr, "Unknown Attribute found at Line:%d\n", xml_node->line);
				return EXIT_FAILURE;
			}
		}
		xml_node = xml_node->next;
	}
	//Check for existence of node "OPC_UA_SERVER" and validate it's contents
	if((xml_node = get_XML_node(components_head_node, "OPC_UA_SERVER")))
	{
		if((content = (xmlChar *) XML_node_get_content(xml_node, "APP_NAME")))
		{
			if(strstr((char*)content, " "))
			{
				fprintf(stderr, "Content (\"%s\") of XML node \"APP_NAME\" is invalid (contain Whitespaces)!!!\n",content);
				return EXIT_FAILURE;
			}
		}
		else
		{
			fprintf(stderr, "\"APP_NAME\" XML child node of \"OPC_UA_SERVER\" not found\n");
			return EXIT_FAILURE;
		}
	}
	else
	{
		fprintf(stderr, "\"OPC_UA_SERVER\" XML node not found\n");
		return EXIT_FAILURE;
	}
	//Scan all SDAQ_HANDLER nodes for CANBUS_IF with duplicate content
	xml_node = components_head_node->children;
	while(xml_node)
	{
		if(xml_node->type == XML_ELEMENT_NODE)
		{
			if(!strcmp((char*)xml_node->name, "SDAQ_HANDLER"))
			{
				content = (xmlChar *) XML_node_get_content(xml_node, "CANBUS_IF");
				check_node = xml_node->next;
				while(check_node)
				{
					if(check_node->type == XML_ELEMENT_NODE)
					{
						if(!strcmp((char*)check_node->name, "SDAQ_HANDLER"))
						{
							if(!strcmp((char*)content, XML_node_get_content(check_node, "CANBUS_IF")))
							{
								fprintf(stderr, "XML Node with name \"CANBUS_IF\" with content \"%s\" for SDAQ_HANDLER found multiple times!!!\n", content);
								return EXIT_FAILURE;
							}
						}
					}
					check_node = check_node->next;
				}
			}
		}
		xml_node = xml_node->next;
	}
	//Scan all NOX_HANDLER nodes for CANBUS_IF with duplicate content
	xml_node = components_head_node->children;
	while(xml_node)
	{
		if(xml_node->type == XML_ELEMENT_NODE)
		{
			if(!strcmp((char*)xml_node->name, "NOX_HANDLER"))
			{
				content = (xmlChar *) XML_node_get_content(xml_node, "CANBUS_IF");
				check_node = xml_node->next;
				while(check_node)
				{
					if(check_node->type == XML_ELEMENT_NODE)
					{
						if(!strcmp((char*)check_node->name, "NOX_HANDLER"))
						{
							if(!strcmp((char*)content, XML_node_get_content(check_node, "CANBUS_IF")))
							{
								fprintf(stderr, "XML Node with name \"CANBUS_IF\" with content \"%s\" for NOX_HANDLER found multiple times!!!\n",content);
								return EXIT_FAILURE;
							}
						}
					}
					check_node = check_node->next;
				}
			}
		}
		xml_node = xml_node->next;
	}
	//Check duplicate usage for contents of CANBUS_IF
	xmlChar *check_content;
	xml_node = components_head_node->children;
	while(xml_node)
	{
		if(xml_node->type == XML_ELEMENT_NODE )
		{
			if(!getprop_disable(xml_node) && (content = (xmlChar *) XML_node_get_content(xml_node, "CANBUS_IF")))
			{
				check_node = xml_node->next;
				while(check_node)
				{
					if(check_node->type == XML_ELEMENT_NODE)
					{
						if(!getprop_disable(check_node) && (check_content = (xmlChar *) XML_node_get_content(check_node, "CANBUS_IF")))
						{
							if(!strcmp((char*)content, (char*)check_content))
							{
								fprintf(stderr, "\"CANBUS_IF\":\"%s\" found to be used in multiple Handlers!!!\n",content);
								return EXIT_FAILURE;
							}
						}
					}
					check_node = check_node->next;
				}
			}
		}
		xml_node = xml_node->next;
	}
	//Scan nodes MDAQ,IOBOX,MTI for child node with duplicate and validate content
	xml_node = components_head_node->children;
	while(xml_node)
	{
		if(xml_node->type == XML_ELEMENT_NODE)
		{
			if(!strcmp((char*)xml_node->name, "MDAQ_HANDLER") ||
			   !strcmp((char*)xml_node->name, "IOBOX_HANDLER")||
			   !strcmp((char*)xml_node->name, "MTI_HANDLER"))
			{
				ipv4_addr = (xmlChar *) XML_node_get_content(xml_node, "IPv4_ADDR");
				//Check "IPv4_ADDR" content if is a valid IPv4 address
				if(!is_valid_IPv4((char *)ipv4_addr))
				{
					fprintf(stderr, "The Internet protocol version 4 address (%s) on line %d is not valid !!!\n",
									 ipv4_addr,
									 get_XML_node(xml_node, "IPv4_ADDR")->line);
					return EXIT_FAILURE;
				}
				dev_name = (xmlChar *) XML_node_get_content(xml_node, "DEV_NAME");
				//Check "DEV_NAME" content for illegal characters
				for(int i=0; dev_name[i]!='\0'; i++)
				{
					if(dev_name[i] == ' ' || dev_name[i] == '\'' || dev_name[i] == '\"')
					{
						fprintf(stderr, "Content of \"DEV_NAME\" on line %d is not valid. Contains (%c) Character !!!\n",
										get_XML_node(xml_node, "DEV_NAME")->line,
										dev_name[i]);
						return EXIT_FAILURE;
					}
					if(i>=Dev_or_Bus_name_str_size)
					{
						fprintf(stderr, "Content of \"DEV_NAME\" on line %d is too long (>=%u)!!!\n",
										get_XML_node(xml_node, "DEV_NAME")->line,
										Dev_or_Bus_name_str_size);
						return EXIT_FAILURE;
					}
				}
				check_node = xml_node->next;
				while(check_node)
				{
					if(check_node->type == XML_ELEMENT_NODE)
					{
						if(!strcmp((char*)check_node->name, "MDAQ_HANDLER") ||
						   !strcmp((char*)check_node->name, "IOBOX_HANDLER")||
						   !strcmp((char*)check_node->name, "MTI_HANDLER"))
						{
							if(!strcmp((char*)ipv4_addr, XML_node_get_content(check_node, "IPv4_ADDR")))
							{
								fprintf(stderr, "XML Node with name \"IPv4_ADDR\" and content \"%s\" found multiple times!!!\n",ipv4_addr);
								return EXIT_FAILURE;
							}
							if(!strcmp((char*)dev_name, XML_node_get_content(check_node, "DEV_NAME")))
							{
								fprintf(stderr, "XML Node with name \"DEV_NAME\" and content \"%s\" found multiple times!!!\n",dev_name);
								return EXIT_FAILURE;
							}
						}
					}
					check_node = check_node->next;
				}
			}
		}
		xml_node = xml_node->next;
	}
	return EXIT_SUCCESS;
}
