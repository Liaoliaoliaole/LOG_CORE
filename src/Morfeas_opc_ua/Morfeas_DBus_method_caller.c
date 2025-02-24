/*
File: Morfeas_DBus_method_caller.c, Implementation of the Morfeas DBus method caller
construction/deconstruction functions for all the Morfeas handlers.

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
#define DBUS_API_SUBJECT_TO_CHANGE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dbus/dbus.h>
#include <stdbool.h>
#include <unistd.h>

//Include Functions implementation header
#include "../Morfeas_opc_ua/Morfeas_handlers_nodeset.h"

//The DBus method caller function
int Morfeas_DBus_method_call(const char *handler_type, const char *dev_name, const char *method, const char *contents, UA_String *reply)
{
	int ret = EXIT_SUCCESS;
	char target[100], interface[100], *reply_str=NULL;
	DBusMessage *msg;
	DBusMessageIter args;
	DBusConnection *conn;
	DBusError err;
	DBusPendingCall *pending;

	// Initialize the errors
	dbus_error_init(&err);
	// Connect to the system bus and check for errors
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err))
	{
		UA_LOG_WARNING(UA_Log_Stdout, UA_LOGLEVEL_ERROR, "DBUS_Error: %s", err.message);
		dbus_error_free(&err);
	}
	if (!conn)
	  return EXIT_FAILURE;
	//Prepare target and interface strings
	sprintf(target, "org.freedesktop.Morfeas.%s.%s", handler_type, dev_name);
	sprintf(interface, "Morfeas.%s.%s", handler_type, dev_name);
	//Create a new method call and check for errors
	if (!(msg = dbus_message_new_method_call(target, "/", interface, method)))
	  return EXIT_FAILURE;

	dbus_message_iter_init_append(msg, &args);
	//Append arguments
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &contents))
	{
	  fprintf(stderr, "Memory Error!!!!\n");
	  exit(EXIT_FAILURE);
	}
	//Send message and get a handle for a reply
	if (!dbus_connection_send_with_reply(conn, msg, &pending, 1000))
	{
	  fprintf(stderr, "Out Of Memory!\n");
	  exit(EXIT_FAILURE);
	}
	if (!pending)
	  return EXIT_FAILURE;
	dbus_connection_flush(conn);
	//Free message
	dbus_message_unref(msg);
	//Block until we recieve a reply
	dbus_pending_call_block(pending);
	//get the reply message
	if(!(msg = dbus_pending_call_steal_reply(pending)))
		return EXIT_FAILURE;
	//Free the pending message handle
	dbus_pending_call_unref(pending);
	//Read the parameters
	if(!dbus_message_iter_init(msg, &args))
	  return EXIT_FAILURE;
	if(DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
	  return EXIT_FAILURE;
	dbus_message_iter_get_basic(&args, &reply_str);
	if(reply)
		*reply = UA_STRING_ALLOC(reply_str);
	else
	{
		if((ret=strstr(reply_str, "Success")?EXIT_SUCCESS:EXIT_FAILURE))
			UA_LOG_WARNING(UA_Log_Stdout, UA_LOGLEVEL_WARNING, "DBUS_method_call:\"%s\" on interface:\"%s\" return:\"%s\"", method, interface, reply_str);
	}
	//Free error, reply and close connection
	dbus_error_free(&err);
	dbus_message_unref(msg);
	return ret;
}

//Function that get the string sec_num section from NodeId. Return EXIT_SUCCESS on success, EXIT_FAILURE otherwise.
int get_NodeId_sec(const UA_NodeId *NodeId, unsigned char sec_num, char *sec_str, size_t sec_str_size)
{
	int i=0;
	unsigned char dot_cnt=0, *NodeId_str;

	if(!NodeId || !sec_str || !sec_str_size)
		return EXIT_FAILURE;
	if(NodeId->identifierType != UA_NODEIDTYPE_STRING)
		return EXIT_FAILURE;
	NodeId_str = (NodeId->identifier.string.data);
	if(sec_num)
	{
		while(dot_cnt<sec_num)
		{
			while(NodeId_str[i]!='.')
			{
				i++;
				if(i>=NodeId->identifier.string.length)
					return EXIT_FAILURE;
			}
			i++;
			dot_cnt++;
		}
		NodeId_str = NodeId->identifier.string.data+i;
	}
	i=0;
	while(NodeId_str[i]!='.')
	{
		sec_str[i]=NodeId_str[i];
		i++;
		if(i >= sec_str_size)
			return EXIT_FAILURE;
	}
	sec_str[i]='\0';
	return EXIT_SUCCESS;
}