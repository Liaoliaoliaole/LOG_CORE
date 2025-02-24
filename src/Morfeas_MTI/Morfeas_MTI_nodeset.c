/*
File: Morfeas_MTI_nodeset.c, implementation of OPC-UA server's
construction/deconstruction functions for Morfeas MTI_handler.

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
#include <cjson/cJSON.h>
//Include Functions implementation header
#include "../Morfeas_opc_ua/Morfeas_handlers_nodeset.h"

//Local function that adding the new_MTI_config method to the Morfeas OPC_UA nodeset
void Morfeas_add_new_MTI_config(UA_Server *server_ptr, char *Parent_id, char *Node_id);
void Morfeas_add_MTI_Global_SWs(UA_Server *server_ptr, char *Parent_id, char *Node_id);
void Morfeas_add_new_Gen_config(UA_Server *server_ptr, char *Parent_id, char *Node_id);
void Morfeas_add_ctrl_tele_SWs(UA_Server *server_ptr, char *Parent_id, char *Node_id, unsigned char dev_type);

void MTI_handler_reg(UA_Server *server_ptr, char *Dev_or_Bus_name)
{
	int negative_one = -1;
	char Node_ID_str[30];
	pthread_mutex_lock(&OPC_UA_NODESET_access);
		//Add MTI handler root node
		sprintf(Node_ID_str, "%s-if (%s)", Morfeas_IPC_handler_type_name[MTI], Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, "MTI-ifs", Dev_or_Bus_name, Node_ID_str);
		sprintf(Node_ID_str, "%s.IP_addr", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "IPv4 Address", UA_TYPES_STRING);
		//Add MTI Device name variable
		sprintf(Node_ID_str, "%s.dev_name", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Device Name", UA_TYPES_STRING);
		//Set Device name
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), Dev_or_Bus_name, UA_TYPES_STRING);
		//Add status variable and set it to "Initializing"
		sprintf(Node_ID_str, "%s.status", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "MTI Status", UA_TYPES_STRING);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), "Initializing", UA_TYPES_STRING);
		sprintf(Node_ID_str, "%s.status_value", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "MTI Status Value", UA_TYPES_INT32);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), &negative_one, UA_TYPES_INT32);
	pthread_mutex_unlock(&OPC_UA_NODESET_access);
}

void IPC_msg_from_MTI_handler(UA_Server *server, unsigned char type, IPC_message *IPC_msg_dec)
{
	UA_NodeId NodeId;
	UA_Variant dataValue;
	char Node_ID_str[100], Node_ID_parent_str[60];
	char *status_str = NULL, name_buff[20], anchor[50];
	unsigned char i, j = 1, lim = 0, status_value;
	float meas, ref;
	int cnt;

	//Msg type from MTI_handler
	switch(type)
	{
		case IPC_MTI_report:
			sprintf(Node_ID_str, "%s.status", IPC_msg_dec->MTI_report.Dev_or_Bus_name);
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				switch(IPC_msg_dec->MTI_report.status)
				{
					case OK_status:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), "Okay", UA_TYPES_STRING);
						break;
					default:
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), modbus_strerror(IPC_msg_dec->MTI_report.status), UA_TYPES_STRING);
						//Check "IP_addr" node and if is empty update it.
						sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->MTI_report.Dev_or_Bus_name);
						if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
						{
							if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
							{
								inet_ntop(AF_INET, &(IPC_msg_dec->MTI_report.MTI_IPv4), name_buff, sizeof(name_buff));
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), name_buff, UA_TYPES_STRING);
							}
							UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
						}
						if(IPC_msg_dec->MTI_report.Tele_dev_type>=Dev_type_min && IPC_msg_dec->MTI_report.Tele_dev_type<=Dev_type_max)
						{
							meas = NAN;
							ref = NAN;
							cnt = 0;
							status_value = OFF_line;
							status_str = "OFF-line";
							switch(IPC_msg_dec->MTI_report.Tele_dev_type)
							{
								case Tele_TC16: lim = 16; break;
								case Tele_TC8:  lim =  8; break;
								case Tele_TC4:  lim =  4; break;
								case Tele_quad: lim =  2; break;
								case RMSW_MUX:  lim =  4; break;
							}
							for(j=0; j<IPC_msg_dec->MTI_report.amount_of_Linkable_tele; j++)
							{
								if(IPC_msg_dec->MTI_report.Tele_dev_type != RMSW_MUX)
									sprintf(anchor, "MTI.%u.%s", IPC_msg_dec->MTI_report.MTI_IPv4, MTI_Tele_dev_type_str[IPC_msg_dec->MTI_report.Tele_dev_type]);
								else
									sprintf(anchor, "MTI.%u.ID:%u", IPC_msg_dec->MTI_report.MTI_IPv4, IPC_msg_dec->MTI_report.IDs_of_MiniRMSWs[j]);
								for(i=1; i<=lim; i++)
								{
									//Update telemetry's Channel specific variables (Linkable)
									sprintf(Node_ID_str, "%s.CH%u.meas", anchor, i);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
									sprintf(Node_ID_str, "%s.CH%u.status", anchor, i);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), status_str, UA_TYPES_STRING);
									sprintf(Node_ID_str, "%s.CH%u.status_byte", anchor, i);
									Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BYTE);
									//Update telemetry's Channel specific variables (Non Linkable)
									switch(IPC_msg_dec->MTI_report.Tele_dev_type)
									{
										case Tele_TC8:
										case Tele_TC4:
											sprintf(Node_ID_str, "%s.Radio.Tele.CH%u.ref", IPC_msg_dec->MTI_report.Dev_or_Bus_name, i);
											Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &ref, UA_TYPES_FLOAT);
											break;
										case Tele_quad:
											sprintf(Node_ID_str, "%s.Radio.Tele.CH%u.raw", IPC_msg_dec->MTI_report.Dev_or_Bus_name, i);
											Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &cnt, UA_TYPES_INT32);
											break;
									}
								}
							}
						}
						break;
				}
				sprintf(Node_ID_str, "%s.status_value", IPC_msg_dec->MTI_report.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_report.status), UA_TYPES_INT32);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MTI_tree_reg:
			sprintf(Node_ID_str, "%s.IP_addr", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Check "IP_addr" node and if is empty update it.
				if(!UA_Server_readValue(server, UA_NODEID_STRING(1,Node_ID_str), &dataValue))
				{
					if(UA_Variant_isEmpty(&dataValue))//Update IPv4 variable
					{
						inet_ntop(AF_INET, &(IPC_msg_dec->MTI_tree_reg.MTI_IPv4), name_buff, sizeof(name_buff));
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), name_buff, UA_TYPES_STRING);
					}
					UA_clear(&dataValue, &UA_TYPES[UA_TYPES_VARIANT]);
				}
				//Add Object for MTI Health data
				sprintf(Node_ID_parent_str, "%s.health", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_object_node(server, IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name, Node_ID_parent_str, "MTI Health");
				//Add variables to MTI Health node
				sprintf(Node_ID_str, "%s.CPU_temp", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "CPU Temperature(°C)", UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_capacity", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Battery Capacity(%)", UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_voltage", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Battery Voltage(V)", UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_state", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Battery state", UA_TYPES_STRING);
				//Add Object for MTI Radio Config
				sprintf(Node_ID_parent_str, "%s.Radio", IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name);
				Morfeas_opc_ua_add_object_node(server, IPC_msg_dec->MTI_tree_reg.Dev_or_Bus_name, Node_ID_parent_str, "Radio");
				//Add variables to MTI Radio Config
				sprintf(Node_ID_str, "%s.RF_CH", Node_ID_parent_str);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "RF Channel", UA_TYPES_BYTE);
				sprintf(Node_ID_str, "%s.Data_rate", Node_ID_parent_str);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Data Rate", UA_TYPES_STRING);
				sprintf(Node_ID_str, "%s.Tele_dev_type", Node_ID_parent_str);
				Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Tele Dev Type", UA_TYPES_STRING);
				//Add new_MTI_config method
				sprintf(Node_ID_str, "%s.new_MTI_config()", Node_ID_parent_str);
				Morfeas_add_new_MTI_config(server, Node_ID_parent_str, Node_ID_str);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MTI_Update_Health:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Update MTI Health node variables
				sprintf(Node_ID_str, "%s.CPU_temp", IPC_msg_dec->MTI_Update_Health.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Health.cpu_temp), UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_capacity", IPC_msg_dec->MTI_Update_Health.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Health.batt_capacity), UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_voltage", IPC_msg_dec->MTI_Update_Health.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Health.batt_voltage), UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.batt_state", IPC_msg_dec->MTI_Update_Health.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MTI_charger_state_str[IPC_msg_dec->MTI_Update_Health.batt_state], UA_TYPES_STRING);
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MTI_Update_Radio:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				//Update MTI Radio node variables
				sprintf(Node_ID_str, "%s.Radio.RF_CH", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Radio.RF_channel), UA_TYPES_BYTE);
				sprintf(Node_ID_str, "%s.Radio.Data_rate", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MTI_Data_rate_str[IPC_msg_dec->MTI_Update_Radio.Data_rate], UA_TYPES_STRING);
				sprintf(Node_ID_str, "%s.Radio.Tele_dev_type", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MTI_Tele_dev_type_str[IPC_msg_dec->MTI_Update_Radio.Tele_dev_type], UA_TYPES_STRING);
				//Check if call is with a new configuration
				if(IPC_msg_dec->MTI_Update_Radio.isNew_config)
				{
					sprintf(Node_ID_str, "%s.Radio.Tele", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
					if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
					{
						UA_Server_deleteNode(server, NodeId, 1);
						UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
					}
					if(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type>=Dev_type_min && IPC_msg_dec->MTI_Update_Radio.Tele_dev_type<=Dev_type_max)
					{
						sprintf(Node_ID_parent_str, "%s.Radio", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						sprintf(name_buff, "Tele(%s)", MTI_Tele_dev_type_str[IPC_msg_dec->MTI_Update_Radio.Tele_dev_type]);
						Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, name_buff);
						if(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type != RMSW_MUX)
						{
							strcpy(Node_ID_parent_str, Node_ID_str);
							//Add telemetry related variables
							sprintf(Node_ID_str, "%s.index", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Packet Index", UA_TYPES_UINT16);
							sprintf(Node_ID_str, "%s.RX_status", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "RX status", UA_TYPES_BYTE);
							sprintf(Node_ID_str, "%s.success", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "RX Success ratio(%)", UA_TYPES_BYTE);
							sprintf(Node_ID_str, "%s.isValid", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "isValid", UA_TYPES_BOOLEAN);
							if(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type != Tele_quad)
							{
								sprintf(Node_ID_str, "%s.StV", Node_ID_parent_str);
								Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Samples to Validate", UA_TYPES_BYTE);
								sprintf(Node_ID_str, "%s.StF", Node_ID_parent_str);
								Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Samples to Fail", UA_TYPES_BYTE);
							}
							//Add telemetry specific variables
							sprintf(anchor, "MTI.%u.%s", IPC_msg_dec->MTI_Update_Radio.MTI_IPv4, MTI_Tele_dev_type_str[IPC_msg_dec->MTI_Update_Radio.Tele_dev_type]);
							switch(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type)
							{
								case Tele_TC16: lim = 16; break;
								case Tele_TC8:  lim =  8; break;
								case Tele_TC4:  lim =  4; break;
								case Tele_quad: lim =  2; break;
							}
							for(i=1; i<=lim; i++)
							{
								//Add telemetry's Channel node
								sprintf(Node_ID_parent_str, "%s.Radio.Tele", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
								IPC_msg_dec->MTI_Update_Radio.Tele_dev_type == Tele_TC16 ? sprintf(name_buff, "CH%02u", i) : sprintf(name_buff, "CH%u", i);
								sprintf(Node_ID_str, "%s.CH%u", Node_ID_parent_str, i);
								Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, name_buff);
								//Add telemetry's Channel specific variables (Linkable)
								sprintf(Node_ID_parent_str, "%s.Radio.Tele.CH%u", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name, i);
								sprintf(Node_ID_str, "%s.CH%u.status", anchor, i);
								Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Status", UA_TYPES_STRING);
								sprintf(Node_ID_str, "%s.CH%u.status_byte", anchor, i);
								Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Status Value", UA_TYPES_BYTE);
								sprintf(Node_ID_str, "%s.CH%u.meas", anchor, i);
								Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Value", UA_TYPES_FLOAT);
								//Add telemetry's Channel specific variables (Non Linkable)
								sprintf(Node_ID_parent_str, "%s.Radio.Tele.CH%u", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name, i);
								switch(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type)
								{
									case Tele_TC8:
									case Tele_TC4:
										//Add temperature reference channel
										sprintf(Node_ID_str, "%s.ref", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Reference", UA_TYPES_FLOAT);
										break;
									case Tele_quad:
										//Add Raw counter channel
										sprintf(Node_ID_str, "%s.raw", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Raw Counter", UA_TYPES_INT32);
										sprintf(Node_ID_str, "%s.pwm_gen", Node_ID_parent_str);
										Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, "Pulses Gen");
										strcpy(Node_ID_parent_str, Node_ID_str);
										sprintf(Node_ID_str, "%s.scaler", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Scaler", UA_TYPES_FLOAT);
										sprintf(Node_ID_str, "%s.min", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Min", UA_TYPES_FLOAT);
										sprintf(Node_ID_str, "%s.max", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Max", UA_TYPES_FLOAT);
										sprintf(Node_ID_str, "%s.saturate", Node_ID_parent_str);
										Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Saturate", UA_TYPES_BOOLEAN);
										sprintf(Node_ID_str, "%s.new_Gen_config", Node_ID_parent_str);
										Morfeas_add_new_Gen_config(server, Node_ID_parent_str, Node_ID_str);
										break;
								}
							}
						}
						else
						{
							//Add Global object node
							sprintf(Node_ID_parent_str, "%s.Radio.Tele", IPC_msg_dec->MTI_report.Dev_or_Bus_name);
							sprintf(Node_ID_str, "%s.amount", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Amount of Devices", UA_TYPES_BYTE);
							sprintf(Node_ID_str, "%s.Global", Node_ID_parent_str);
							Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, "Global Controls");
							//Add RMSW/MUX related Global switches variables nodes
							sprintf(Node_ID_parent_str, "%s.Radio.Tele.Global", IPC_msg_dec->MTI_report.Dev_or_Bus_name);
							sprintf(Node_ID_str, "%s.G_SW", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Global ON/OFF Control", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.G_SL", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Global Sleep Control", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.G_P_state", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Global ON/OFF state", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.G_S_state", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Global Sleep state", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.MTI_Global_SWs()", Node_ID_parent_str);
							Morfeas_add_MTI_Global_SWs(server, Node_ID_parent_str, Node_ID_str);
						}
					}
				}
				//Update values from MTI's Special registers
				switch(IPC_msg_dec->MTI_Update_Radio.Tele_dev_type)
				{
					case Tele_TC4:
					case Tele_TC8:
					case Tele_TC16:
						sprintf(Node_ID_str, "%s.Radio.Tele.StV", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Radio.sRegs.for_temp_tele.StV), UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.Radio.Tele.StF", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_Update_Radio.sRegs.for_temp_tele.StF), UA_TYPES_BYTE);
						break;
					case RMSW_MUX:
						sprintf(Node_ID_str, "%s.Radio.Tele.Global.G_SW", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						status_value = IPC_msg_dec->MTI_Update_Radio.sRegs.for_rmsw_dev.G_SW;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.Radio.Tele.Global.G_SL", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						status_value = IPC_msg_dec->MTI_Update_Radio.sRegs.for_rmsw_dev.G_SL;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.Radio.Tele.Global.G_P_state", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						status_value = IPC_msg_dec->MTI_Update_Radio.sRegs.for_rmsw_dev.G_P_state;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.Radio.Tele.Global.G_S_state", IPC_msg_dec->MTI_Update_Radio.Dev_or_Bus_name);
						status_value = IPC_msg_dec->MTI_Update_Radio.sRegs.for_rmsw_dev.G_S_state;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
						break;
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MTI_Tele_data:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				if(IPC_msg_dec->MTI_tele_data.Tele_dev_type>=Dev_type_min&&
				   IPC_msg_dec->MTI_tele_data.Tele_dev_type<=Dev_type_max&&
				   IPC_msg_dec->MTI_tele_data.Tele_dev_type!= RMSW_MUX)
				{
					sprintf(Node_ID_parent_str, "%s.Radio", IPC_msg_dec->MTI_tele_data.Dev_or_Bus_name);
					//Update telemetry related variables. (All memory position at same order, TC4 used as reference
					sprintf(Node_ID_str, "%s.Tele.index", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_tele_data.data.as_TC4.packet_index), UA_TYPES_UINT16);
					sprintf(Node_ID_str, "%s.Tele.RX_status", Node_ID_parent_str);
					status_value=IPC_msg_dec->MTI_tele_data.data.as_TC4.RX_status;
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BYTE);
					sprintf(Node_ID_str, "%s.Tele.success", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_tele_data.data.as_TC4.RX_Success_ratio), UA_TYPES_BYTE);
					sprintf(Node_ID_str, "%s.Tele.isValid", Node_ID_parent_str);
					status_value=IPC_msg_dec->MTI_tele_data.data.as_TC4.Data_isValid;
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
					switch(IPC_msg_dec->MTI_tele_data.Tele_dev_type)
					{
						case Tele_TC16: lim = 16; break;
						case Tele_TC8:  lim =  8; break;
						case Tele_TC4:  lim =  4; break;
						case Tele_quad: lim =  2; break;
					}
					sprintf(anchor, "MTI.%u.%s", IPC_msg_dec->MTI_tele_data.MTI_IPv4, MTI_Tele_dev_type_str[IPC_msg_dec->MTI_tele_data.Tele_dev_type]);
					for(i=1; i<=lim; i++)
					{	//Get and decode Telemetry's data;
						switch(IPC_msg_dec->MTI_tele_data.Tele_dev_type)
						{
							case Tele_TC16:
								meas = IPC_msg_dec->MTI_tele_data.data.as_TC16.CHs[i-1];
								break;
							case Tele_TC8:
								meas = IPC_msg_dec->MTI_tele_data.data.as_TC8.CHs[i-1];
								ref = IPC_msg_dec->MTI_tele_data.data.as_TC8.Refs[i-1];
								break;
							case Tele_TC4:
								meas = IPC_msg_dec->MTI_tele_data.data.as_TC4.CHs[i-1];
								ref = (i>=1&&i<=2)?IPC_msg_dec->MTI_tele_data.data.as_TC4.Refs[0]:
												   IPC_msg_dec->MTI_tele_data.data.as_TC4.Refs[1];
								break;
							case Tele_quad:
								meas = IPC_msg_dec->MTI_tele_data.data.as_QUAD.CHs[i-1];
								cnt = IPC_msg_dec->MTI_tele_data.data.as_QUAD.CNTs[i-1];
								break;
						}
						if(IPC_msg_dec->MTI_tele_data.data.as_TC4.Data_isValid)
						{
							status_value = Okay;
							status_str = "Okay";
							if(IPC_msg_dec->MTI_tele_data.Tele_dev_type != Tele_quad)
							{
								if(meas >= NO_SENSOR_VALUE)//Check for No sensor value
								{
									status_str = "No Sensor";
									status_value = Tele_channel_noSensor;
									meas = NAN;
									ref = NAN;
								}
								else if(meas != meas)//Check for Telemetry Error (meas == NAN)
								{
									status_str = "Error";
									status_value = Tele_channel_Error;
									ref = NAN;
								}
							}
						}
						else
						{
							status_value = Disconnected;
							status_str = "Disconnected";
						}
						//Update telemetry's Channel specific variables (Linkable)
						sprintf(Node_ID_str, "%s.CH%u.meas", anchor, i);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
						sprintf(Node_ID_str, "%s.CH%u.status", anchor, i);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), status_str, UA_TYPES_STRING);
						sprintf(Node_ID_str, "%s.CH%u.status_byte", anchor, i);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BYTE);
						//Update telemetry's Channel specific variables (Non Linkable)
						switch(IPC_msg_dec->MTI_tele_data.Tele_dev_type)
						{
							case Tele_TC8:
							case Tele_TC4:
								sprintf(Node_ID_str, "%s.Radio.Tele.CH%u.ref", IPC_msg_dec->MTI_tele_data.Dev_or_Bus_name, i);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &ref, UA_TYPES_FLOAT);
								break;
							case Tele_quad:
								sprintf(Node_ID_str, "%s.Radio.Tele.CH%u.raw", IPC_msg_dec->MTI_tele_data.Dev_or_Bus_name, i);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &cnt, UA_TYPES_INT32);
								//Update Channel's Pulse Gen Status
								sprintf(Node_ID_parent_str, "%s.Radio.Tele.CH%u.pwm_gen", IPC_msg_dec->MTI_tele_data.Dev_or_Bus_name, i);
								sprintf(Node_ID_str, "%s.scaler", Node_ID_parent_str);
								ref = IPC_msg_dec->MTI_tele_data.data.as_QUAD.gen_config[i-1].scaler;
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &ref, UA_TYPES_FLOAT);
								sprintf(Node_ID_str, "%s.min", Node_ID_parent_str);
								meas = IPC_msg_dec->MTI_tele_data.data.as_QUAD.gen_config[i-1].min*ref;
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
								sprintf(Node_ID_str, "%s.max", Node_ID_parent_str);
								meas = IPC_msg_dec->MTI_tele_data.data.as_QUAD.gen_config[i-1].max*ref;
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
								sprintf(Node_ID_str, "%s.saturate", Node_ID_parent_str);
								status_value = IPC_msg_dec->MTI_tele_data.data.as_QUAD.gen_config[i-1].pwm_mode.dec.saturation;
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
								break;
						}
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_MTI_RMSW_MUX_data:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				if(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.amount_to_be_remove)//Check if RMSW/MUX nodes is need to be remove
				{
					for(i=0; i<IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.amount_to_be_remove; i++)
					{
						sprintf(Node_ID_str, "%s.Radio.Tele.%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.IDs_to_be_removed[i]);
						if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
						{
							UA_Server_deleteNode(server, NodeId, 1);
							UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
						}
					}
				}
				//Add and update data of RMSW/MUX devices
				sprintf(Node_ID_str, "%s.Radio.Tele.amount", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.amount_of_devices), UA_TYPES_BYTE);
				for(i=0; i<IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.amount_of_devices; i++)
				{
					sprintf(Node_ID_str, "%s.Radio.Tele.%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
					if(UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))//Check if device is in Nodestore, if not register it
					{
						//Add current RMSW/MUX Device object node
						sprintf(Node_ID_parent_str, "%s.Radio.Tele", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name);
						sprintf(name_buff, "%s(ID:%u)", MTI_RM_dev_type_str[IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type],
													 IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
						Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, name_buff);
						strcpy(Node_ID_parent_str, Node_ID_str);
						//Add current RMSW/MUX related variables nodes and load static values
						sprintf(Node_ID_str, "%s.mem_offset", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Memory Offset", UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.type", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Type", UA_TYPES_STRING);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), MTI_RM_dev_type_str[IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type], UA_TYPES_STRING);
						sprintf(Node_ID_str, "%s.id", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "ID", UA_TYPES_BYTE);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id), UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.last_msg", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Last RX(sec)", UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.dev_temp", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Device temp(°C)", UA_TYPES_FLOAT);
						sprintf(Node_ID_str, "%s.sup_volt", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Supply Voltage(V)", UA_TYPES_FLOAT);
						//Add objects and variables nodes for Controls and Measurement
						sprintf(Node_ID_str, "%s.Controls", Node_ID_parent_str);
						Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, "Controls");
						strcpy(Node_ID_parent_str, Node_ID_str);
						if(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type != MUX)
						{
							sprintf(Node_ID_str, "%s.Main_SW", Node_ID_parent_str);
							Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Main Switch", UA_TYPES_BOOLEAN);
						}
						sprintf(Node_ID_str, "%s.ctrl_tele_SWs()", Node_ID_parent_str);
						Morfeas_add_ctrl_tele_SWs(server, Node_ID_parent_str, Node_ID_str, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type);
						sprintf(Node_ID_str, "%s.TX_rate", Node_ID_parent_str);
						Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "TX Rate(sec)", UA_TYPES_FLOAT);
						switch(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type)
						{
							case Mini_RMSW:
								sprintf(anchor, "MTI.%u.ID:%u", IPC_msg_dec->MTI_RMSW_MUX_data.MTI_IPv4, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
								//Add Mini_RMSW Channels related nodes (Linkable)
								for(j=1; j<=4; j++)
								{
									sprintf(Node_ID_parent_str, "%s.Radio.Tele.%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
									sprintf(Node_ID_str, "%s.Radio.Tele.%u.CH%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name,
																				  IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id,
																				  j);
									sprintf(name_buff, "CH%u", j);
									Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, name_buff);
									strcpy(Node_ID_parent_str, Node_ID_str);
									sprintf(Node_ID_str, "%s.%s.status", anchor, name_buff);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Status", UA_TYPES_STRING);
									sprintf(Node_ID_str, "%s.%s.status_byte", anchor, name_buff);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Status Value", UA_TYPES_BYTE);
									sprintf(Node_ID_str, "%s.%s.meas", anchor, name_buff);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, "Value", UA_TYPES_FLOAT);
								}
								break;
							case RMSW_2CH:
								for(j=1; j<=2; j++)
								{
									sprintf(name_buff, "CH%u_state", j);
									sprintf(Node_ID_str, "%s.%s", Node_ID_parent_str, name_buff);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, name_buff, UA_TYPES_BOOLEAN);
								}
								//Add RMSW_2CH Channels related nodes
								for(j=1; j<=2; j++)
								{
									sprintf(Node_ID_parent_str, "%s.Radio.Tele.%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
									sprintf(Node_ID_str, "%s.Radio.Tele.%u.CH%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name,
																				  IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id,
																				  j);
									sprintf(name_buff, "CH%u", j);
									Morfeas_opc_ua_add_object_node(server, Node_ID_parent_str, Node_ID_str, name_buff);
									strcpy(Node_ID_parent_str, Node_ID_str);
									sprintf(Node_ID_str, "%s.voltage", Node_ID_parent_str);
									sprintf(name_buff, "CH%u Voltage(V)", j);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, name_buff, UA_TYPES_FLOAT);
									sprintf(Node_ID_str, "%s.amperage", Node_ID_parent_str);
									sprintf(name_buff, "CH%u Amperage(A)", j);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, name_buff, UA_TYPES_FLOAT);
								}
								break;
							case MUX:
								for(j=1; j<=4; j++)
								{
									sprintf(name_buff, "CH%u_state", j);
									sprintf(Node_ID_str, "%s.%s", Node_ID_parent_str, name_buff);
									Morfeas_opc_ua_add_variable_node(server, Node_ID_parent_str, Node_ID_str, name_buff, UA_TYPES_BOOLEAN);
								}
								break;
						}
					}
					else
						UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
					sprintf(Node_ID_parent_str, "%s.Radio.Tele.%u", IPC_msg_dec->MTI_RMSW_MUX_data.Dev_or_Bus_name, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
					//Update current RMSW/MUX Device values
					sprintf(Node_ID_str, "%s.mem_offset", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].pos_offset), UA_TYPES_BYTE);
					sprintf(Node_ID_str, "%s.last_msg", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].time_from_last_mesg), UA_TYPES_BYTE);
					sprintf(Node_ID_str, "%s.dev_temp", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_temp), UA_TYPES_FLOAT);
					sprintf(Node_ID_str, "%s.sup_volt", Node_ID_parent_str);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].input_voltage), UA_TYPES_FLOAT);
					//Update variables nodes for controls and meas
					switch(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_type)
					{
						case Mini_RMSW:
							sprintf(Node_ID_str, "%s.Controls.Main_SW", Node_ID_parent_str);
							status_value = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.mini_dec.Main;
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.Controls.TX_rate", Node_ID_parent_str);
							switch(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.mini_dec.Rep_rate)
							{
								case 0: meas = 20; break;
								case 2: meas = 2; break;
								case 3:
								case 1: meas = .2; break;
							}
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
							sprintf(anchor, "MTI.%u.ID:%u", IPC_msg_dec->MTI_RMSW_MUX_data.MTI_IPv4, IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].dev_id);
							for(j=1; j<=4; j++)
							{
								meas = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].meas_data[j-1];
								meas = meas<NO_SENSOR_VALUE ? meas:NAN;
								status_value = meas!=meas ? Tele_channel_noSensor:Okay;
								status_str = status_value ? "No Sensor":"Okay";
								sprintf(Node_ID_str, "%s.CH%u.meas", anchor, j);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
								sprintf(Node_ID_str, "%s.CH%u.status", anchor, j);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), status_str, UA_TYPES_STRING);
								sprintf(Node_ID_str, "%s.CH%u.status_byte", anchor, j);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BYTE);
							}
							break;
						case RMSW_2CH:
							sprintf(Node_ID_str, "%s.Controls.TX_rate", Node_ID_parent_str);
							switch(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.rmsw_dec.Rep_rate)
							{
								case 0: meas = 20; break;
								case 1: meas = 2; break;
							}
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
							sprintf(Node_ID_str, "%s.Controls.Main_SW", Node_ID_parent_str);
							status_value = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.as_byte & 1;
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
							for(j=1; j<=2; j++)
							{
								sprintf(Node_ID_str, "%s.Controls.CH%u_state", Node_ID_parent_str, j);
								status_value = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.as_byte & (1<<j);
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
								sprintf(Node_ID_str, "%s.CH%u.voltage", Node_ID_parent_str, j);
								meas = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].meas_data[j==1?0:2];
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
								sprintf(Node_ID_str, "%s.CH%u.amperage", Node_ID_parent_str, j);
								meas = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].meas_data[j==1?1:3];
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
							}
							break;
						case MUX:
							sprintf(Node_ID_str, "%s.Controls.TX_rate", Node_ID_parent_str);
							switch(IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.mux_dec.Rep_rate)
							{
								case 0: meas = 20; break;
								case 1: meas = 2; break;
							}
							Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &meas, UA_TYPES_FLOAT);
							for(j=1; j<=4; j++)
							{
								sprintf(Node_ID_str, "%s.Controls.CH%u_state", Node_ID_parent_str, j);
								status_value = IPC_msg_dec->MTI_RMSW_MUX_data.Devs_data.det_devs_data[i].switch_status.as_byte & (1<<(j-1));
								Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &status_value, UA_TYPES_BOOLEAN);
							}
							break;
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
	}
}

UA_StatusCode Morfeas_new_MTI_config_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[200], dev_name[Dev_or_Bus_name_str_size], new_mode_str[10] = {0};
    int ret = UA_STATUSCODE_GOOD, i=0;
	unsigned char new_mode_val=0,
				  new_RF_CH = input[0].data ? *(unsigned char*)(input[0].data):0,
				  StV = input[2].data ? *(unsigned char*)(input[2].data):0,
				  StF = input[3].data ? *(unsigned char*)(input[3].data):0;
	bool G_SW = input[4].data ? *(bool *)(input[4].data):0,
		 G_SL = input[5].data ? *(bool *)(input[5].data):0;
	cJSON *root = NULL;
	UA_String reply={0}, *new_mode_UA_str = (UA_String *)(input[1].data);
	//Sanitize new_mode_UA_str
	if(!new_mode_UA_str)
		return UA_STATUSCODE_BADSYNTAXERROR;
	if(new_mode_UA_str->length >= sizeof(new_mode_str) || !new_mode_UA_str->length)
		return UA_STATUSCODE_BADSYNTAXERROR;
	//Convert new_mode_UA_str to new_mode_str
	while(i < new_mode_UA_str->length)
	{
		new_mode_str[i] = new_mode_UA_str->data[i];
		i++;
	}
	new_mode_str[i]='\0';
	//Check validity of new_mode_str
	while(MTI_Tele_dev_type_str[new_mode_val] && strcmp(new_mode_str,MTI_Tele_dev_type_str[new_mode_val]))
		new_mode_val++;
	if(new_mode_val > Dev_type_max)
		return UA_STATUSCODE_BADSYNTAXERROR;
	//Get Dev_name
	if(get_NodeId_sec(methodId, 0, dev_name, sizeof(dev_name)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "new_RF_CH", new_RF_CH);
	cJSON_AddItemToObject(root, "new_mode", cJSON_CreateString(new_mode_str));
	switch(new_mode_val)
	{
		case Tele_TC4:
		case Tele_TC8:
		case Tele_TC16:
			cJSON_AddNumberToObject(root, "StV", StV);
			cJSON_AddNumberToObject(root, "StF", StF);
			break;
		case RMSW_MUX:
			cJSON_AddItemToObject(root, "G_SW", cJSON_CreateBool(G_SW));
			cJSON_AddItemToObject(root, "G_SL", cJSON_CreateBool(G_SL));
			break;
	}
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("MTI", dev_name, "new_MTI_config", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
    return ret;
}

void Morfeas_add_new_MTI_config(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
	const char *inp_descriptions[] = {"Range:(0..126) Only even numbers",
									  "Accepted values:{Disabled,TC16,TC8,TC4,QUAD,RMSW/MUX}",
									  "Successful receptions to set valid flag(Range:1..254, 0=Unchanged, 255=Disable)",
									  "Failed receptions to reset valid flag(Range:1..254, 0=Unchanged, 255=Disable)",
									  "Global ON/OFF mode(Used with mode RMSW/MUX)",
									  "Global Sleep mode(Used with mode RMSW/MUX)"};
	const char *inp_names[] = {"new_RF_CH","new_Radio_mode","StV","StF","G_SW","G_SL",NULL};
	const unsigned int inp_value_type[] = {UA_TYPES_BYTE, UA_TYPES_STRING, UA_TYPES_BYTE, UA_TYPES_BYTE, UA_TYPES_BOOLEAN, UA_TYPES_BOOLEAN};

	//Configure inputArguments
    UA_Argument inputArguments[6];
	for(int i=0; i<sizeof(inputArguments)/sizeof(*inputArguments); i++)
	{
		UA_Argument_init(&inputArguments[i]);
		inputArguments[i].description = UA_LOCALIZEDTEXT("en-US", (char *)inp_descriptions[i]);
		inputArguments[i].name = UA_STRING((char *)inp_names[i]);
		inputArguments[i].dataType = UA_TYPES[inp_value_type[i]].typeId;
		inputArguments[i].valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	}
	//Configure outputArgument
	UA_Argument outputArgument;
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method new_MTI_config");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","new_MTI_config()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_new_MTI_config_method_callback,
							sizeof(inputArguments)/sizeof(*inputArguments), inputArguments,
							1, &outputArgument,
							NULL, NULL);
}

UA_StatusCode Morfeas_MTI_Global_SWs_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[50], dev_name[Dev_or_Bus_name_str_size];
    int ret = UA_STATUSCODE_GOOD;
	bool G_P_state = input[0].data ? *(bool *)(input[0].data):0,
		 G_S_state = input[1].data ? *(bool *)(input[1].data):0;
	cJSON *root = NULL;
	UA_String reply={0};

	//Sanitize input
	if(!input[0].data)
		return UA_STATUSCODE_BADSYNTAXERROR;
	//Get Dev_name
	if(get_NodeId_sec(methodId, 0, dev_name, sizeof(dev_name)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "G_P_state", cJSON_CreateBool(G_P_state));
	cJSON_AddItemToObject(root, "G_S_state", cJSON_CreateBool(G_S_state));
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("MTI", dev_name, "MTI_Global_SWs", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
    return ret;
}

void Morfeas_add_MTI_Global_SWs(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
	const char *inp_descriptions[] = {"Global ON/OFF state control",
									  "Global Sleep state control (only for MiniRMSWs)",
									  NULL};
	const char *inp_names[] = {"G_P_state","G_S_state",NULL};
	const unsigned int inp_value_type[] = {UA_TYPES_BOOLEAN, UA_TYPES_BOOLEAN};

	//Configure inputArguments
    UA_Argument inputArguments[2];
	for(int i=0; i<sizeof(inputArguments)/sizeof(*inputArguments); i++)
	{
		UA_Argument_init(&inputArguments[i]);
		inputArguments[i].description = UA_LOCALIZEDTEXT("en-US", (char *)inp_descriptions[i]);
		inputArguments[i].name = UA_STRING((char *)inp_names[i]);
		inputArguments[i].dataType = UA_TYPES[inp_value_type[i]].typeId;
		inputArguments[i].valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	}
	//Configure outputArgument
	UA_Argument outputArgument;
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method MTI_Global_SWs");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","MTI_Global_SWs()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_MTI_Global_SWs_method_callback,
							sizeof(inputArguments)/sizeof(*inputArguments), inputArguments,
							1, &outputArgument,
							NULL, NULL);
}

UA_StatusCode Morfeas_new_Gen_config_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[100], dev_name[Dev_or_Bus_name_str_size];
    int ret = UA_STATUSCODE_GOOD;
	unsigned char CH_num;
	float gain, min, max;
	bool saturate;
	cJSON *root = NULL, *array = NULL, *PWM_gens_config_item = NULL;
	UA_String reply={0};

	//Sanitize input
	if(!input[0].data || !input[1].data || !input[2].data)
		return UA_STATUSCODE_BADSYNTAXERROR;
	gain = *(float *)(input[0].data);
	min  = *(float *)(input[1].data);
	max  = *(float *)(input[2].data);
	saturate = input[3].data ? *(bool *)(input[3].data):0;
	//Get Dev_name
	if(get_NodeId_sec(methodId, 0, dev_name, sizeof(dev_name)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Get Channel Number
	sscanf(strstr((char *)methodId->identifier.string.data, "CH"), "CH%hhu.", &CH_num);
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "PWM_gens_config", array = cJSON_CreateArray());
	cJSON_AddItemToArray(array, cJSON_CreateNull());//Array element for CH1
	cJSON_AddItemToArray(array, cJSON_CreateNull());//Array element for CH2
	PWM_gens_config_item = cJSON_CreateObject();
	cJSON_AddNumberToObject(PWM_gens_config_item, "scaler", gain);
	cJSON_AddNumberToObject(PWM_gens_config_item, "min", min);
	cJSON_AddNumberToObject(PWM_gens_config_item, "max", max);
	cJSON_AddItemToObject(PWM_gens_config_item, "saturation", cJSON_CreateBool(saturate));
	cJSON_ReplaceItemInArray(array, CH_num-1, PWM_gens_config_item);//Load data to specific element according to the channel number
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("MTI", dev_name, "new_PWM_config", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
    return ret;
}

void Morfeas_add_new_Gen_config(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
	const char *inp_descriptions[] = {"CNT -> CH (Scaler)",
									  "Scaled CH's Value for 0% modulation index",
									  "Scaled CH's Value for 100% modulation index",
									  "Saturate at limits",
									  NULL};
	const char *inp_names[] = {"Scaler","Gen_MIN","Gen_MAX","Limits",NULL};
	const unsigned int inp_value_type[] = {UA_TYPES_FLOAT, UA_TYPES_FLOAT, UA_TYPES_FLOAT, UA_TYPES_BOOLEAN};

	//Configure inputArguments
    UA_Argument inputArguments[4];
	for(int i=0; i<sizeof(inputArguments)/sizeof(*inputArguments); i++)
	{
		UA_Argument_init(&inputArguments[i]);
		inputArguments[i].description = UA_LOCALIZEDTEXT("en-US", (char *)inp_descriptions[i]);
		inputArguments[i].name = UA_STRING((char *)inp_names[i]);
		inputArguments[i].dataType = UA_TYPES[inp_value_type[i]].typeId;
		inputArguments[i].valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	}
	//Configure outputArgument
	UA_Argument outputArgument;
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method new_Gen_config");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","new_Gen_config()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_new_Gen_config_method_callback,
							sizeof(inputArguments)/sizeof(*inputArguments), inputArguments,
							1, &outputArgument,
							NULL, NULL);
}

UA_StatusCode Morfeas_ctrl_tele_SWs_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[80], mem_offset_NodeId_str[50], dev_name[Dev_or_Bus_name_str_size], Ctrl_Dev_ID[5];
	const char **sw_names;
	unsigned char Ctrl_Dev_type, mem_offset, i, null_inp;
	cJSON *root = NULL;
	UA_NodeId mem_offset_NodeId;

	//Get Dev_name
	if(get_NodeId_sec(methodId, 0, dev_name, sizeof(dev_name)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Get Ctrl_Dev_ID
	if(get_NodeId_sec(methodId, 3, Ctrl_Dev_ID, sizeof(Ctrl_Dev_ID)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	switch(inputSize)
	{
		case 1: //Mini_RMSW
			Ctrl_Dev_type = Mini_RMSW;
			sw_names = MTI_RMSW_SW_names;
			break;
		case 3: //RMSW_2CH
			Ctrl_Dev_type = RMSW_2CH;
			sw_names = MTI_RMSW_SW_names;
			break;
		case 4: //MUX
			Ctrl_Dev_type = MUX;
			sw_names = MTI_MUX_Sel_names;
			break;
		default: return UA_STATUSCODE_BADSYNTAXERROR;
	}
	//Get MODBus Memory offset for the Device of method call
	sprintf(mem_offset_NodeId_str, "%s.Radio.Tele.%s.mem_offset", dev_name, Ctrl_Dev_ID);
	if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, mem_offset_NodeId_str), &mem_offset_NodeId))//Check if mem_offset_NodeId exist
	{
		UA_Variant temp;
		UA_Server_readValue(server, mem_offset_NodeId, &temp);
		mem_offset = *(char *)(temp.data);
		UA_clear(&mem_offset_NodeId, &UA_TYPES[UA_TYPES_NODEID]);
		UA_clear(&temp, &UA_TYPES[UA_TYPES_VARIANT]);
	}
	else
		return UA_STATUSCODE_BADNODATA;
	for(i=0, null_inp=0; i<inputSize; i++)
	{
		if(!input[i].data)
		{
			null_inp++;
			continue;
		}
		//Construct contents for DBus method call (as JSON object)
		root = cJSON_CreateObject();
		cJSON_AddNumberToObject(root, "mem_pos", mem_offset);
		cJSON_AddStringToObject(root, "tele_type", MTI_RM_dev_type_str[Ctrl_Dev_type]);
		cJSON_AddStringToObject(root, "sw_name", sw_names[i]);
		cJSON_AddBoolToObject(root, "new_state", *(bool *)(input[i].data));
		//Stringify the root JSON object
		if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		{
			cJSON_Delete(root);
			return UA_STATUSCODE_BADOUTOFMEMORY;
		}
		cJSON_Delete(root);
		if(Morfeas_DBus_method_call("MTI", dev_name, "ctrl_tele_SWs", contents, NULL))
			return UA_STATUSCODE_BADDEVICEFAILURE;
	}
	if(null_inp == inputSize)
		return UA_STATUSCODE_BADSYNTAXERROR;
	return UA_STATUSCODE_GOOD;
}

void Morfeas_add_ctrl_tele_SWs(UA_Server *server_ptr, char *Parent_id, char *Node_id, unsigned char dev_type)
{
	const char inp_description_rmsw[] = "FALSE:OFF, TRUE:ON";
	const char inp_description_mux[] = "FALSE:A, TRUE:B";
	const char *inp_names_rmsw[] = {"Main","CH1","CH2",NULL};
	const char *inp_names_mux[] = {"CH1","CH2","CH3","CH4",NULL};
	size_t inputArgumentsAmount;
	char *inp_description, **inp_names;
	UA_Argument inputArguments[4];

	//Configure inputArguments
	switch(dev_type)
	{
		case Mini_RMSW:
			inputArgumentsAmount = 1;
			inp_description = (char *)inp_description_rmsw;
			inp_names = (char **)inp_names_rmsw;
			break;
		case RMSW_2CH:
			inputArgumentsAmount = 3;
			inp_description = (char *)inp_description_rmsw;
			inp_names = (char **)inp_names_rmsw;
			break;
		case MUX:
			inputArgumentsAmount = 4;
			inp_description = (char *)inp_description_mux;
			inp_names = (char **)inp_names_mux;
			break;
		default: return;
	}
	for(int i=0; i<inputArgumentsAmount; i++)
	{
		UA_Argument_init(&inputArguments[i]);
		inputArguments[i].description = UA_LOCALIZEDTEXT("en-US", inp_description);
		inputArguments[i].name = UA_STRING(inp_names[i]);
		inputArguments[i].dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
		inputArguments[i].valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	}

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method ctrl_tele_SWs");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","ctrl_tele_SWs()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr, UA_NODEID_STRING(1, Node_id),
										UA_NODEID_STRING(1, Parent_id),
										UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
										UA_QUALIFIEDNAME(1, Node_id),
										Method_Attr, &Morfeas_ctrl_tele_SWs_method_callback,
										inputArgumentsAmount, inputArguments,
										0, NULL, NULL, NULL);
}
