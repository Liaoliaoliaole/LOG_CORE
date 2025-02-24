/*
File "Morfeas_NOX_DBus.c" Implementation of D-Bus listener for the NOX handler.
Copyright (C) 12021-12022  Sam harry Tzavaras

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

#define MORFEAS_DBUS_NAME_PROTO "org.freedesktop.Morfeas.NOX."
#define IF_NAME_PROTO "Morfeas.NOX."

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include <dbus/dbus.h>

#include <modbus.h>
#include <cjson/cJSON.h>

#include "../Morfeas_Types.h"
#include "../Supplementary/Morfeas_Logger.h"

//External Global variables from Morfeas_ΝΟΧ_if.c
extern volatile unsigned char NOX_handler_run;
extern pthread_mutex_t NOX_access;

int NOX_handler_config_file(struct Morfeas_NOX_if_stats *stats, const char *mode);//Read and write NOX_handler configuration file

	//--- Local Enumerators and constants---//
static const char *const Method[] = {"NOX_heater", "NOX_auto_sw_off", "echo", NULL};
enum Method_enum{NOX_heater, NOX_auto_sw_off, echo};
static DBusError dbus_error;

	//--- Local DBus Error Logging function ---//
void Log_DBus_error(char *str);
int DBus_reply_msg(DBusConnection *conn, DBusMessage *msg, char *reply_str);
int DBus_reply_msg_with_error(DBusConnection *conn, DBusMessage *msg, char *reply_str);

//D-Bus listener Thread function
void * NOX_DBus_listener(void *varg_pt)
{
	//Decoded variables from passer
	struct Morfeas_NOX_if_stats *stats = ((struct NOX_DBus_thread_arguments_passer *)varg_pt)->stats;
	NOX_start_code *startcode = ((struct NOX_DBus_thread_arguments_passer *)varg_pt)->startcode;
	//JSON objects
	cJSON *JSON_args = NULL, *NOx_addr, *NOx_heater_val, *auto_pw_off_val;
	//D-Bus related variables
	char *dbus_server_name_if, *param;
	int ret, method_num, libdbus_ver[3];
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter call_args;

	if(!NOX_handler_run)//Immediately exit if called with MTI handler is terminating
		return NULL;

	dbus_error_init (&dbus_error);

	//Get libDBus version
	dbus_get_version(&libdbus_ver[0], &libdbus_ver[1], &libdbus_ver[2]);
	Logger("libDBus Version: %d.%d.%d\n", libdbus_ver[0], libdbus_ver[1], libdbus_ver[2]);

	Logger("Thread for D-Bus listener Started\n");
	//Connects to a bus daemon and registers with it.
	if(!(conn=dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error)))
	{
		Log_DBus_error("dbus_bus_get() Failed!!!");
		return NULL;
	}
	//Allocate space and create dbus_server_name
	if(!(dbus_server_name_if = calloc(sizeof(char), strlen(MORFEAS_DBUS_NAME_PROTO)+strlen(stats->CAN_IF_name)+1)))
	{
		fprintf(stderr,"Memory error!!!\n");
		exit(EXIT_FAILURE);
	}
	sprintf(dbus_server_name_if, "%s%s", MORFEAS_DBUS_NAME_PROTO, stats->CAN_IF_name);
	Logger("DBus_Name:\"%s\"\n", dbus_server_name_if);

    ret = dbus_bus_request_name(conn, dbus_server_name_if, DBUS_NAME_FLAG_DO_NOT_QUEUE, &dbus_error);
    if(dbus_error_is_set (&dbus_error))
        Log_DBus_error("dbus_bus_request_name() Failed!!!");

    if(ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
	{
        Logger("DBus: Name not primary owner, ret = %d\n", ret);
        return NULL;
    }
	free(dbus_server_name_if);

	//Allocate space and create dbus_server_if
	if(!(dbus_server_name_if = calloc(sizeof(char), strlen(IF_NAME_PROTO)+strlen(stats->CAN_IF_name)+1)))
	{
		fprintf(stderr,"Memory error!!!\n");
		exit(EXIT_FAILURE);
	}
	sprintf(dbus_server_name_if, "%s%s", IF_NAME_PROTO, stats->CAN_IF_name);
	Logger("I/O at DBus_interface:\"%s\"\n", dbus_server_name_if);

	while(NOX_handler_run)
	{
		//Wait for incoming messages, timeout in 1 second
        if(dbus_connection_read_write_dispatch(conn, 1000))
		{
			if((msg = dbus_connection_pop_message(conn)))
			{
				//Analyze received message
				method_num=0;
				while(Method[method_num])
				{
					if(dbus_message_is_method_call(msg, dbus_server_name_if, Method[method_num]))//Validate the method of the call
					{
						//Read the arguments of the call
						if (!dbus_message_iter_init(msg, &call_args))//Validate for zero amount of arguments
						  DBus_reply_msg(conn, msg, "Call with NO argument!!!");
						else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&call_args))//Validate for not string argument
						  DBus_reply_msg(conn, msg, "Method's argument is NOT a string!!!");
						else
						{
							dbus_message_iter_get_basic(&call_args, &param);//Get first argument.
							if(method_num==echo)//Check for echo method
							{
								DBus_reply_msg(conn, msg, param);
								break;
							}
							if(!(JSON_args = cJSON_Parse(param)))//Parser called argument as JSON.
							{
								DBus_reply_msg(conn, msg, "JSON Parsing failed!!!");
								break;
							}
							switch(method_num)//Method execution
							{
								case NOX_heater:
									if(!cJSON_HasObjectItem(JSON_args, "NOx_address") || !cJSON_HasObjectItem(JSON_args, "NOx_heater"))
									{
										DBus_reply_msg(conn, msg, "NOX_heater(): Missing Arguments");
										break;
									}
									NOx_addr = cJSON_GetObjectItem(JSON_args, "NOx_address");
									NOx_heater_val = cJSON_GetObjectItem(JSON_args, "NOx_heater");
									if(cJSON_IsNumber(NOx_addr) && cJSON_IsBool(NOx_heater_val))
									{
										if((NOx_addr->valueint >= 0 && NOx_addr->valueint < 2) || NOx_addr->valueint == -1)
										{
											pthread_mutex_lock(&NOX_access);
												switch(NOx_addr->valueint)
												{
													case 0: startcode->fields.meas_low_addr = NOx_heater_val->valueint; break;
													case 1: startcode->fields.meas_high_addr = NOx_heater_val->valueint; break;
													case -1: startcode->as_byte = NOx_heater_val->valueint ? NOx_all_heaters_on : 0; break;
												}
												stats->auto_switch_off_cnt = 0;
											pthread_mutex_unlock(&NOX_access);
											DBus_reply_msg(conn, msg, "NOX_heater(): Success");
										}
										else
											DBus_reply_msg(conn, msg, "NOX_heater(): NOx_address is out of range!!!");
									}
									else
										DBus_reply_msg(conn, msg, "NOX_heater(): Wrong arguments type");
									break;
								case NOX_auto_sw_off:
									if(!cJSON_HasObjectItem(JSON_args, "NOx_auto_sw_off_value"))
									{
										DBus_reply_msg(conn, msg, "NOX_auto_sw_off(): Missing Arguments");
										break;
									}
									auto_pw_off_val = cJSON_GetObjectItem(JSON_args, "NOx_auto_sw_off_value");
									if(cJSON_IsNumber(auto_pw_off_val))
									{
										pthread_mutex_lock(&NOX_access);
											stats->auto_switch_off_value = auto_pw_off_val->valueint;
											if(stats->auto_switch_off_value != auto_pw_off_val->valueint)
												Logger("Overflow at auto_switch_off_value (%d != %d)!!!\n", stats->auto_switch_off_value,
																											auto_pw_off_val->valueint);
											if(NOX_handler_config_file(stats, "w"))
												Logger("Error at write of configuration file!!!\n");
										pthread_mutex_unlock(&NOX_access);
										DBus_reply_msg(conn, msg, "NOX_auto_sw_off(): Success");
									}
									else
										DBus_reply_msg(conn, msg, "NOX_auto_sw_off(): Wrong argument type");
									break;
							}
							cJSON_Delete(JSON_args);
						}
						break;
					}
					method_num++;
				}
				dbus_message_unref(msg);
			}
		}
	}
	//Free memory
	dbus_error_free(&dbus_error);
	free(dbus_server_name_if);
	//dbus_connection_close(conn);
	Logger("D-Bus listener thread terminated\n");
	return NULL;
}

void Log_DBus_error(char *str)
{
    Logger("%s: %s\n", str, dbus_error.message);
    dbus_error_free (&dbus_error);
}

int DBus_reply_msg(DBusConnection *conn, DBusMessage *msg, char *reply_str)
{
	DBusMessage *reply;
	DBusMessageIter reply_args;
	//Send reply
	if(!(reply = dbus_message_new_method_return(msg)))
	{
		Logger("Error in dbus_message_new_method_return()\n");
		return EXIT_FAILURE;
	}

	dbus_message_iter_init_append(reply, &reply_args);

	if (!dbus_message_iter_append_basic(&reply_args, DBUS_TYPE_STRING, &reply_str))
	{
		Logger("Error in dbus_message_iter_append_basic()\n");
		return EXIT_FAILURE;
	}
	if (!dbus_connection_send(conn, reply, NULL))
	{
		Logger("Error in dbus_connection_send()\n");
		return EXIT_FAILURE;
	}
	dbus_connection_flush(conn);
	dbus_message_unref(reply);
	return EXIT_SUCCESS;
}

int DBus_reply_msg_with_error(DBusConnection *conn, DBusMessage *msg, char *reply_str)
{
	DBusMessage *dbus_error_msg;
	if ((dbus_error_msg = dbus_message_new_error(msg, DBUS_ERROR_FAILED, reply_str)) == NULL)
	{
		Logger("Error in dbus_message_new_error()\n");
		return EXIT_FAILURE;
	}
	if (!dbus_connection_send(conn, dbus_error_msg, NULL)) {
		Logger("Error in dbus_connection_send()\n");
		return EXIT_FAILURE;
	}
	dbus_connection_flush(conn);
	dbus_message_unref(dbus_error_msg);
	return EXIT_SUCCESS;
}