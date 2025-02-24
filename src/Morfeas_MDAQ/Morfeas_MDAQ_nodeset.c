/*
File: Morfeas_MDAQ_nodeset.c, implementation of OPC-UA server's
construction/deconstruction functions for Morfeas MDAQ_handler.

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
#define no_sensor -909.0 //Error code for no sensor

#include <math.h>
#include <arpa/inet.h>

#include <modbus.h>
//Include Functions implementation header
#include "../Morfeas_opc_ua/Morfeas_handlers_nodeset.h"

void MDAQ_handler_reg(UA_Server *server_ptr, char *Dev_or_Bus_name)
{
	int negative_one = -1;
	char Node_ID_str[30];
	pthread_mutex_lock(&OPC_UA_NODESET_access);
		sprintf(Node_ID_str, "%s-if (%s)", Morfeas_IPC_handler_type_name[MDAQ], Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, "MDAQ-ifs", Dev_or_Bus_name, Node_ID_str);
		sprintf(Node_ID_str, "%s.IP_addr", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "IPv4 Address", UA_TYPES_STRING);
		//Add MDAQ Device name variable
		sprintf(Node_ID_str, "%s.dev_name", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Device Name", UA_TYPES_STRING);
		//Set Device name
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), Dev_or_Bus_name, UA_TYPES_STRING);
		//Add status variable and set it to "Initializing"
		sprintf(Node_ID_str, "%s.status", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "MDAQ Status", UA_TYPES_STRING);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), "Initializing", UA_TYPES_STRING);
		sprintf(Node_ID_str, "%s.status_value", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "MDAQ Status Value", UA_TYPES_INT32);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), &negative_one, UA_TYPES_INT32);
		//Object with MDAQ Board status data
		sprintf(Node_ID_str, "%s.index", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Index", UA_TYPES_UINT32);
		sprintf(Node_ID_str, "%s.board_temp", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Board Temperature(°C)", UA_TYPES_FLOAT);
	pthread_mutex_unlock(&OPC_UA_NODESET_access);
}

void IPC_msg_from_MDAQ_handler(UA_Server *server, unsigned char type, IPC_message *IPC_msg_dec)
{
	UA_NodeId NodeId;
	UA_Variant dataValue;
	char MDAQ_IPv4_addr_str[20], Node_name[100], status;
	char Node_ID_str[50], Node_ID_child_str[80], Node_ID_child_child_str[100];
	float meas = NAN; unsigned char negative_one = -1;

	//Msg type from MDAQ_handler
	switch(type)
	{
		case IPC_MDAQ_report:
			sprintf(Node_ID_str, "%s.status", IPC_msg_dec->MDAQ_report.Dev_or_Bus_name);
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				switch(IPC_msg_dec->MDAQ_report.status)
				{
					case OK_status:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), "Okay", UA_TYPES_STRING);
						break;
					default:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), modbus_strerror(IPC_msg_dec->MDAQ_report.status), UA_TYPES_STRING);
						//Check "IP_addr" node and if is empty update it.
						sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->MDAQ_report.Dev_or_Bus_name);
						if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
						{
							if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
							{
								inet_ntop(AF_INET, &(IPC_msg_dec->MDAQ_report.MDAQ_IPv4), MDAQ_IPv4_addr_str, sizeof(MDAQ_IPv4_addr_str));
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MDAQ_IPv4_addr_str, UA_TYPES_STRING);
							}
							UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
						}
						//Check if node for object Channels exist
						sprintf(Node_ID_str, "%s.Channels", IPC_msg_dec->MDAQ_report.Dev_or_Bus_name);
						if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
						{
							UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
							for(unsigned char i=1; i<=8; i++)
							{
								//Change Value and status of Channels to error code
								for(unsigned char j=1; j<=3; j++)
								{
									//Add variables for channels: meas, status, status_value (Linkable)
									sprintf(Node_ID_str, "MDAQ.%u.CH%hhu.Val%hhu", IPC_msg_dec->MDAQ_report.MDAQ_IPv4, i, j);
									sprintf(Node_ID_child_child_str, "%s.meas", Node_ID_str);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), &meas, UA_TYPES_FLOAT);
									sprintf(Node_ID_child_child_str, "%s.status", Node_ID_str);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), "OFF-Line", UA_TYPES_STRING);
									sprintf(Node_ID_child_child_str, "%s.status_byte", Node_ID_str);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), &negative_one, UA_TYPES_BYTE);
								}
							}
						}
						break;
				}
				sprintf(Node_ID_str, "%s.status_value", IPC_msg_dec->MDAQ_report.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MDAQ_report.status), UA_TYPES_INT32);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MDAQ_channels_reg:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Check if node for object Channels exist
				sprintf(Node_ID_str, "%s.Channels", IPC_msg_dec->MDAQ_data.Dev_or_Bus_name);
				if(UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
				{
					//Check "IP_addr" node and if is empty update it.
					sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->MDAQ_data.Dev_or_Bus_name);
					if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
					{
						if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
						{
							inet_ntop(AF_INET, &(IPC_msg_dec->MDAQ_data.MDAQ_IPv4), MDAQ_IPv4_addr_str, sizeof(MDAQ_IPv4_addr_str));
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MDAQ_IPv4_addr_str, UA_TYPES_STRING);
						}
						UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
					}
					//Add Object for Channels
					sprintf(Node_ID_str, "%s.Channels", IPC_msg_dec->MDAQ_data.Dev_or_Bus_name);
					Morfeas_opc_ua_add_object_node(server, IPC_msg_dec->MDAQ_data.Dev_or_Bus_name, Node_ID_str, "Channels");
					for(unsigned char i=1; i<=8; i++)
					{
						sprintf(Node_ID_child_str, "%s.CH%hhu", Node_ID_str, i);
						sprintf(Node_name, "CH%1hhu", i);
						Morfeas_opc_ua_add_object_node(server, Node_ID_str, Node_ID_child_str, Node_name);
						//Variables of Channels measurements
						for(unsigned char j=1; j<=3; j++)
						{
							sprintf(Node_ID_child_child_str, "%s.Value%hhu", Node_ID_child_str, j);
							sprintf(Node_name, "Value %1hhu", j);
							Morfeas_opc_ua_add_object_node(server, Node_ID_child_str, Node_ID_child_child_str, Node_name);
							//Add variables for measurements children of Value# object (Linkable)
							sprintf(Node_name, "MDAQ.%u.CH%hhu.Val%hhu.meas", IPC_msg_dec->MDAQ_data.MDAQ_IPv4, i, j);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, Node_name, "Measurement", UA_TYPES_FLOAT);
							sprintf(Node_name, "MDAQ.%u.CH%hhu.Val%hhu.status", IPC_msg_dec->MDAQ_data.MDAQ_IPv4, i, j);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, Node_name, "Status", UA_TYPES_STRING);
							sprintf(Node_name, "MDAQ.%u.CH%hhu.Val%hhu.status_byte", IPC_msg_dec->MDAQ_data.MDAQ_IPv4, i, j);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, Node_name, "Status_value", UA_TYPES_BYTE);
						}
					}
				}
				else
					UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MDAQ_data:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Load Index and Board temp to OPC-UA variables
				sprintf(Node_ID_str, "%s.index", IPC_msg_dec->MDAQ_data.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MDAQ_data.meas_index), UA_TYPES_UINT32);
				sprintf(Node_ID_str, "%s.board_temp", IPC_msg_dec->MDAQ_data.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MDAQ_data.board_temp), UA_TYPES_FLOAT);
				//Load Values and status for each MDAQ channel to OPC-UA variables
				sprintf(Node_ID_str, "MDAQ.%u", IPC_msg_dec->MDAQ_data.MDAQ_IPv4);
				for(unsigned char i=0; i<8; i++)
				{
					for(unsigned char j=0; j<3; j++)
					{
						sprintf(Node_ID_child_str, "%s.CH%hhu.Val%hhu", Node_ID_str, i+1, j+1);
						status = ((IPC_msg_dec->MDAQ_data.meas[i].warnings)&1<<j)?Out_of_range:Okay;
						sprintf(Node_ID_child_child_str, "%s.status_byte", Node_ID_child_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), &status, UA_TYPES_BYTE);
						sprintf(Node_ID_child_child_str, "%s.status", Node_ID_child_str);
						//Set status variables according to the status of the value
						if(!status)
						{
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), "Okay", UA_TYPES_STRING);
							meas = IPC_msg_dec->MDAQ_data.meas[i].value[j];
						}
						else
						{
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), "Out of range", UA_TYPES_STRING);
							meas = NAN;
						}
						sprintf(Node_ID_child_child_str, "%s.meas", Node_ID_child_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_child_child_str), &meas, UA_TYPES_FLOAT);
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
	}
}
