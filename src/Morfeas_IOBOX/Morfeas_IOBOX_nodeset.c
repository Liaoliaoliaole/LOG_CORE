/*
File: Morfeas_IOBOX_nodeset.c, implementation of OPC-UA server's
construction/deconstruction functions for Morfeas IOBOX_handler.

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
#include <math.h>
#include <arpa/inet.h>

#include <modbus.h>
//Include Functions implementation header
#include "../Morfeas_opc_ua/Morfeas_handlers_nodeset.h"

void IOBOX_handler_reg(UA_Server *server_ptr, char *Dev_or_Bus_name)
{
	int negative_one = -1;
	char Node_name[30], Node_ID_str[30], Node_ID_child_str[50], Node_ID_child_child_str[80];
	pthread_mutex_lock(&OPC_UA_NODESET_access);
		sprintf(Node_ID_str, "%s-if (%s)", Morfeas_IPC_handler_type_name[IOBOX], Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, "IOBOX-ifs", Dev_or_Bus_name, Node_ID_str);
		sprintf(Node_ID_str, "%s.IP_addr", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "IPv4 Address", UA_TYPES_STRING);
		//Add IOBOX Dev name variable
		sprintf(Node_ID_str, "%s.dev_name", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Device Name", UA_TYPES_STRING);
		//Set Dev name
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), Dev_or_Bus_name, UA_TYPES_STRING);
		//Add status variable and set it to "Initializing"
		sprintf(Node_ID_str, "%s.status", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "IO-BOX Status", UA_TYPES_STRING);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), "Initializing", UA_TYPES_STRING);
		sprintf(Node_ID_str, "%s.status_value", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "IO-BOX Status Value", UA_TYPES_INT32);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), &negative_one, UA_TYPES_INT32);
		//Object with electric status of a IOBOX Induction link power supplies
		sprintf(Node_ID_str, "%s.Ind_link", Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "I-Link Power Supply");
		sprintf(Node_ID_child_str, "%s.Vin", Node_ID_str);
		Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_str, Node_ID_child_str, "Power Supply Vin(V)", UA_TYPES_FLOAT);
		for(unsigned char i=1; i<=4; i++)
		{
			sprintf(Node_ID_child_str, "%s.CH%1hhu", Node_ID_str, i);
			sprintf(Node_name, "CH%1hhu", i);
			Morfeas_opc_ua_add_object_node(server_ptr, Node_ID_str, Node_ID_child_str, Node_name);
			sprintf(Node_ID_child_child_str, "%s.Vout", Node_ID_child_str);
			Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_child_str, Node_ID_child_child_str, "Vout(V)", UA_TYPES_FLOAT);
			sprintf(Node_ID_child_child_str, "%s.Iout", Node_ID_child_str);
			Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_child_str, Node_ID_child_child_str, "Iout(A)", UA_TYPES_FLOAT);
		}
	pthread_mutex_unlock(&OPC_UA_NODESET_access);
}

void IPC_msg_from_IOBOX_handler(UA_Server *server, unsigned char type, IPC_message *IPC_msg_dec)
{
	UA_NodeId NodeId;
	UA_Variant dataValue;
	char IOBOX_IPv4_addr_str[20], Node_name[30], status_byte = Okay;
	char Node_ID_str[60], Node_ID_child_str[80], Node_ID_child_child_str[100], val_Node_ID_str[160];
	float nan = NAN; unsigned char negative_one = -1;

	//Msg type from IOBOX_handler
	switch(type)
	{
		case IPC_IOBOX_report:
			sprintf(Node_ID_str, "%s.status", IPC_msg_dec->IOBOX_report.Dev_or_Bus_name);
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				switch(IPC_msg_dec->IOBOX_report.status)
				{
					case OK_status:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), "Okay", UA_TYPES_STRING);
						break;
					default:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), modbus_strerror(IPC_msg_dec->IOBOX_report.status), UA_TYPES_STRING);
						//Check "IP_addr" node and if is empty update it.
						sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->IOBOX_report.Dev_or_Bus_name);
						if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
						{
							if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
							{
								inet_ntop(AF_INET, &(IPC_msg_dec->IOBOX_report.IOBOX_IPv4), IOBOX_IPv4_addr_str, sizeof(IOBOX_IPv4_addr_str));
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), IOBOX_IPv4_addr_str, UA_TYPES_STRING);
							}
							UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
						}
						//Check if node for object Receivers exist
						sprintf(Node_ID_str, "%s.RXs", IPC_msg_dec->IOBOX_report.Dev_or_Bus_name);
						if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
						{
							UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
							for(unsigned char i=1; i<=IOBOX_Amount_of_All_RXs; i++)
							{
								//Change Value and status of Channels to error code
								for(unsigned char j=1; j<=IOBOX_Amount_of_channels; j++)
								{
									//Add variables for channels: meas, status, status_value (Linkable)
									sprintf(Node_name, "IOBOX.%u.RX%hhu.CH%hhu", IPC_msg_dec->IOBOX_report.IOBOX_IPv4, i, j);
									sprintf(val_Node_ID_str, "%s.meas", Node_name);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &nan, UA_TYPES_FLOAT);
									sprintf(val_Node_ID_str, "%s.status", Node_name);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), "OFF-Line", UA_TYPES_STRING);
									sprintf(val_Node_ID_str, "%s.status_byte", Node_name);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &negative_one, UA_TYPES_BYTE);
								}
							}
						}
						break;
				}
				sprintf(Node_ID_str, "%s.status_value", IPC_msg_dec->IOBOX_report.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->IOBOX_report.status), UA_TYPES_INT32);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_IOBOX_channels_reg:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Check if node for object Receivers exist
				sprintf(Node_ID_str, "%s.RXs", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name);
				if(UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
				{
					//Check "IP_addr" node and if is empty update it.
					sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name);
					if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
					{
						if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
						{
							inet_ntop(AF_INET, &(IPC_msg_dec->IOBOX_data.IOBOX_IPv4), IOBOX_IPv4_addr_str, sizeof(IOBOX_IPv4_addr_str));
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), IOBOX_IPv4_addr_str, UA_TYPES_STRING);
						}
						UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
					}
					//Add Object for Receivers
					sprintf(Node_ID_str, "%s.RXs", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name);
					Morfeas_opc_ua_add_object_node(server, IPC_msg_dec->IOBOX_data.Dev_or_Bus_name, Node_ID_str, "Receivers");
					for(unsigned char i=1; i<=IOBOX_Amount_of_All_RXs; i++)
					{
						sprintf(Node_ID_child_str, "%s.RX%hhu", Node_ID_str, i);
						sprintf(Node_name, "RX%1hhu", i);
						Morfeas_opc_ua_add_object_node(server, Node_ID_str, Node_ID_child_str, Node_name);
						//Add variables for Receiver's packet_index, status and success_ratio
						sprintf(val_Node_ID_str, "%s.index", Node_ID_child_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_child_str, val_Node_ID_str, "Index", UA_TYPES_UINT16);
						sprintf(val_Node_ID_str, "%s.status", Node_ID_child_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_child_str, val_Node_ID_str, "RX_Status", UA_TYPES_BYTE);
						sprintf(val_Node_ID_str, "%s.success", Node_ID_child_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_child_str, val_Node_ID_str, "RX_success", UA_TYPES_BYTE);
						//Variables of Channels measurements
						for(unsigned char j=1; j<=IOBOX_Amount_of_channels; j++)
						{
							sprintf(Node_ID_child_child_str, "%s.CH%hhu", Node_ID_child_str, j);
							sprintf(Node_name, "CH%02hhu", j);
							Morfeas_opc_ua_add_object_node(server, Node_ID_child_str, Node_ID_child_child_str, Node_name);

							//Add variables for channels: meas, status, status_value (Linkable)
							sprintf(Node_name, "IOBOX.%u.RX%hhu.CH%hhu", IPC_msg_dec->IOBOX_data.IOBOX_IPv4, i, j);
							sprintf(val_Node_ID_str, "%s.meas", Node_name);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, val_Node_ID_str, "Value", UA_TYPES_FLOAT);
							sprintf(val_Node_ID_str, "%s.status", Node_name);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, val_Node_ID_str, "Status", UA_TYPES_STRING);
							sprintf(val_Node_ID_str, "%s.status_byte", Node_name);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_child_child_str, val_Node_ID_str, "Status_value", UA_TYPES_BYTE);
						}
					}
				}
				else
					UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_IOBOX_data:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Load power supply measurements to OPC-UA variables
				sprintf(Node_ID_str, "%s.Ind_link.Vin", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->IOBOX_data.Supply_Vin), UA_TYPES_FLOAT);
				for(unsigned char i=0; i<IOBOX_Amount_of_STD_RXs; i++)
				{
					sprintf(Node_ID_str, "%s.Ind_link.CH%1hhu.Vout", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name, i+1);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->IOBOX_data.Supply_meas[i].Vout), UA_TYPES_FLOAT);
					sprintf(Node_ID_str, "%s.Ind_link.CH%1hhu.Iout", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name, i+1);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->IOBOX_data.Supply_meas[i].Iout), UA_TYPES_FLOAT);
				}
				//Load values to variables
				for(unsigned char i=0; i<IOBOX_Amount_of_All_RXs; i++)
				{
					//Variables for receiver
					sprintf(Node_ID_str, "%s.RXs.RX%hhu", IPC_msg_dec->IOBOX_data.Dev_or_Bus_name, i+1);
					sprintf(Node_name, "IOBOX.%u", IPC_msg_dec->IOBOX_data.IOBOX_IPv4);
					sprintf(val_Node_ID_str, "%s.index", Node_ID_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &(IPC_msg_dec->IOBOX_data.RX[i].index), UA_TYPES_UINT16);
					sprintf(val_Node_ID_str, "%s.status", Node_ID_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &(IPC_msg_dec->IOBOX_data.RX[i].status), UA_TYPES_BYTE);
					sprintf(val_Node_ID_str, "%s.success", Node_ID_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &(IPC_msg_dec->IOBOX_data.RX[i].success), UA_TYPES_BYTE);

					//Variables of Telemetry Channels (Linkable)
					sprintf(Node_name, "IOBOX.%u", IPC_msg_dec->IOBOX_data.IOBOX_IPv4);
					for(unsigned char j=0; j<IOBOX_Amount_of_channels; j++)
					{
						sprintf(Node_ID_str, "%s.RX%hhu.CH%hhu", Node_name, i+1, j+1);
						sprintf(val_Node_ID_str, "%s.meas", Node_ID_str);
						//Check if RX status to check if telemetry is active
						if(IPC_msg_dec->IOBOX_data.RX[i].status)
						{
							if(IPC_msg_dec->IOBOX_data.RX[i].CH_value[j] >= NO_SENSOR_VALUE)//Check for No sensor
							{
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &nan, UA_TYPES_FLOAT);
								sprintf(val_Node_ID_str, "%s.status", Node_ID_str);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), "No sensor", UA_TYPES_STRING);
								status_byte = Tele_channel_noSensor;
							}
							else //Sensor okay, update value
							{
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str),
																   &(IPC_msg_dec->IOBOX_data.RX[i].CH_value[j]), UA_TYPES_FLOAT);
								sprintf(val_Node_ID_str, "%s.status", Node_ID_str);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), "Okay", UA_TYPES_STRING);
							}
						}//Check success for messages in receivers buffer, if is zero report "Disconnected"
						else if(!IPC_msg_dec->IOBOX_data.RX[i].success)
						{
							sprintf(val_Node_ID_str, "%s.status", Node_ID_str);
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), "Disconnected", UA_TYPES_STRING);
							status_byte = Disconnected;
						}
						sprintf(val_Node_ID_str, "%s.status_byte", Node_ID_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,val_Node_ID_str), &status_byte, UA_TYPES_BYTE);
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
	}
}
