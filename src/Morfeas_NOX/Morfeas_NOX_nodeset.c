/*
File: Morfeas_ΝΟΧ_nodeset.c, implementation of OPC-UA server's
construction/deconstruction functions for Morfeas ΝΟΧ_handler.

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
#include <stdio.h>
#include <math.h>
#include <arpa/inet.h>

#include <cjson/cJSON.h>
//Include Functions implementation header
#include "../Morfeas_opc_ua/Morfeas_handlers_nodeset.h"

//Local function that adding methods to Morfeas OPC_UA nodeset
void Morfeas_add_new_NOX_config(UA_Server *server_ptr, char *Parent_id, char *Node_id);
void Morfeas_add_NOX_heater_ctrl(UA_Server *server_ptr, char *Parent_id, char *Node_id);
void Morfeas_add_NOX_global_heater_ctrl(UA_Server *server_ptr, char *Parent_id, char *Node_id);

void NOX_handler_reg(UA_Server *server_ptr, char *Dev_or_Bus_name)
{
	char Node_ID_str[60], Child_Node_ID_str[90];
	pthread_mutex_lock(&OPC_UA_NODESET_access);
		//Add NOX handler root node
		sprintf(Node_ID_str, "%s-if (%s)", Morfeas_IPC_handler_type_name[NOX], Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, "NOX-ifs", Dev_or_Bus_name, Node_ID_str);
		sprintf(Node_ID_str, "%s.amount", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Dev_on_BUS", UA_TYPES_BYTE);
		sprintf(Node_ID_str, "%s.BUS_util", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "BUS_Util (%)", UA_TYPES_FLOAT);
		sprintf(Node_ID_str, "%s.BUS_err", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "BUS_Error (%)", UA_TYPES_FLOAT);
		sprintf(Node_ID_str, "%s.BUS_name", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "CANBus", UA_TYPES_STRING);
		Update_NodeValue_by_nodeID(server_ptr, UA_NODEID_STRING(1,Node_ID_str), Dev_or_Bus_name, UA_TYPES_STRING);
		sprintf(Node_ID_str, "%s.auto_switch_off_value", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Auto-off Set (Sec)", UA_TYPES_UINT16);
		sprintf(Node_ID_str, "%s.auto_switch_off_cnt", Dev_or_Bus_name);
		Morfeas_opc_ua_add_variable_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Auto-off CNT (Sec)", UA_TYPES_UINT16);
		if(!strstr(Dev_or_Bus_name, "vcan"))
		{	//Object with electric status of a SDAQnet port
			sprintf(Node_ID_str, "%s.Electrics", Dev_or_Bus_name);
			Morfeas_opc_ua_add_object_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "Electric");
			sprintf(Child_Node_ID_str, "%s.volts", Dev_or_Bus_name);
			Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_str, Child_Node_ID_str, "Voltage (V)", UA_TYPES_FLOAT);
			sprintf(Child_Node_ID_str, "%s.amps", Dev_or_Bus_name);
			Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_str, Child_Node_ID_str, "Amperage (A)", UA_TYPES_FLOAT);
			sprintf(Child_Node_ID_str, "%s.shunt", Dev_or_Bus_name);
			Morfeas_opc_ua_add_variable_node(server_ptr, Node_ID_str, Child_Node_ID_str, "Shunt Temp (°F)", UA_TYPES_FLOAT);
		}
		sprintf(Node_ID_str, "%s.sensors", Dev_or_Bus_name);
		Morfeas_opc_ua_add_object_node(server_ptr, Dev_or_Bus_name, Node_ID_str, "UniNOx_net");
		//Add NOX related methods
		sprintf(Node_ID_str, "%s.new_NOX_config()", Dev_or_Bus_name);
		Morfeas_add_new_NOX_config(server_ptr, Dev_or_Bus_name, Node_ID_str);
		sprintf(Node_ID_str, "%s.global_heaters_ctrl()", Dev_or_Bus_name);
		Morfeas_add_NOX_global_heater_ctrl(server_ptr, Dev_or_Bus_name, Node_ID_str);
	pthread_mutex_unlock(&OPC_UA_NODESET_access);
}

unsigned char UniNOx_status(IPC_message *IPC_msg_dec, unsigned char NOx_or_O2)
{
	if(!IPC_msg_dec->NOX_data.NOXs_data.meas_state)
		return UniNOx_notMeas;
	if(!IPC_msg_dec->NOX_data.NOXs_data.status.in_temperature)
		return UniNOx_notInTemp;
	switch(NOx_or_O2)
	{
		case NOx_val:
			if(!IPC_msg_dec->NOX_data.NOXs_data.status.is_NOx_value_valid)
				return NOX_notValid;
		case O2_val:
			if(!IPC_msg_dec->NOX_data.NOXs_data.status.is_O2_value_valid)
				return O2_notValid;
	}
	return Okay;
}

char* UniNOx_status_str(unsigned char status)
{
	switch(status)
	{
		case Okay: return "Okay";
		case UniNOx_notMeas: return "Not Measuring";
		case UniNOx_notInTemp: return "Not In Temperature";
		case NOX_notValid: return "NOx is not Valid";
		case O2_notValid: return "O2 is not Valid";
		default: return "Unknown status";
	}
}

void IPC_msg_from_NOX_handler(UA_Server *server, unsigned char type, IPC_message *IPC_msg_dec)
{
	UA_NodeId NodeId;
	UA_DateTime last_seen;
	unsigned char value;
	char label_str[30], parent_Node_ID_str[60], Node_ID_str[90];

	//Msg type from SDAQ_handler
	switch(type)
	{
		case IPC_NOX_data:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				if(IPC_msg_dec->NOX_data.sensor_addr>=0 && IPC_msg_dec->NOX_data.sensor_addr <2)
				{
					sprintf(parent_Node_ID_str, "%s.sensors.addr_%d", IPC_msg_dec->NOX_data.Dev_or_Bus_name, IPC_msg_dec->NOX_data.sensor_addr);
					if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, parent_Node_ID_str), &NodeId))
					{
						UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
						sprintf(Node_ID_str, "%s.last_seen", parent_Node_ID_str);
						last_seen = UA_DateTime_fromUnixTime(IPC_msg_dec->NOX_data.NOXs_data.last_seen);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &last_seen, UA_TYPES_DATETIME);
						//Decode and load value, status and status_byte for NOx
						sprintf(Node_ID_str, "%s.NOx_value", parent_Node_ID_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_data.NOXs_data.NOx_value), UA_TYPES_FLOAT);
						sprintf(Node_ID_str, "%s.NOx_status_byte", parent_Node_ID_str);
						value = UniNOx_status(IPC_msg_dec, NOx_val);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.NOx_status", parent_Node_ID_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), UniNOx_status_str(value), UA_TYPES_STRING);
						//Decode and load value, status and status_byte for O2
						sprintf(Node_ID_str, "%s.O2_value", parent_Node_ID_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_data.NOXs_data.O2_value), UA_TYPES_FLOAT);
						sprintf(Node_ID_str, "%s.O2_status_byte", parent_Node_ID_str);
						value = UniNOx_status(IPC_msg_dec, O2_val);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BYTE);
						sprintf(Node_ID_str, "%s.O2_status", parent_Node_ID_str);
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), UniNOx_status_str(value), UA_TYPES_STRING);
						//Decode and load to variables of UniNOx sensor's status object
						sprintf(parent_Node_ID_str, "%s.sensors.addr_%d.status", IPC_msg_dec->NOX_data.Dev_or_Bus_name, IPC_msg_dec->NOX_data.sensor_addr);
						sprintf(Node_ID_str, "%s.meas_state", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.meas_state;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.supply", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.status.supply_in_range;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.inTemp", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.status.in_temperature;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.NOx_valid", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.status.is_NOx_value_valid;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.O2_valid", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.status.is_O2_value_valid;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &value, UA_TYPES_BOOLEAN);
						sprintf(Node_ID_str, "%s.heater_state", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.status.heater_mode_state;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), Heater_mode_str[value], UA_TYPES_STRING);
						//Populate objects and variables for UniNOx sensor's errors
						sprintf(parent_Node_ID_str, "%s.sensors.addr_%d.errors", IPC_msg_dec->NOX_data.Dev_or_Bus_name, IPC_msg_dec->NOX_data.sensor_addr);
						sprintf(Node_ID_str, "%s.heater_element", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.errors.heater;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), Errors_dec_str[value], UA_TYPES_STRING);
						sprintf(Node_ID_str, "%s.NOx_element", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.errors.NOx;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), Errors_dec_str[value], UA_TYPES_STRING);
						sprintf(Node_ID_str, "%s.O2_element", parent_Node_ID_str);
						value = IPC_msg_dec->NOX_data.NOXs_data.errors.O2;
						Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), Errors_dec_str[value], UA_TYPES_STRING);
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
		case IPC_NOX_CAN_BUS_info:
			pthread_mutex_lock(&OPC_UA_NODESET_access);
				sprintf(Node_ID_str, "%s.BUS_util", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.BUS_utilization), UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.BUS_err", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.Bus_error_rate), UA_TYPES_FLOAT);
				sprintf(Node_ID_str, "%s.amount", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.Dev_on_bus), UA_TYPES_BYTE);
				sprintf(Node_ID_str, "%s.auto_switch_off_value", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.auto_switch_off_value), UA_TYPES_UINT16);
				sprintf(Node_ID_str, "%s.auto_switch_off_cnt", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
				Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.auto_switch_off_cnt), UA_TYPES_UINT16);
				if(IPC_msg_dec->NOX_BUS_info.Electrics)
				{
					sprintf(Node_ID_str, "%s.volts", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.voltage), UA_TYPES_FLOAT);
					sprintf(Node_ID_str, "%s.amps", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.amperage), UA_TYPES_FLOAT);
					sprintf(Node_ID_str, "%s.shunt", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
					Update_NodeValue_by_nodeID(server, UA_NODEID_STRING(1,Node_ID_str), &(IPC_msg_dec->NOX_BUS_info.shunt_temp), UA_TYPES_FLOAT);
				}
				for(int i=0; i<2; i++)
				{
					sprintf(Node_ID_str, "%s.sensors.addr_%d", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name, i);
					if(IPC_msg_dec->NOX_BUS_info.active_devs[i])
					{
						if(UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
						{
							sprintf(parent_Node_ID_str, "%s.sensors", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name);
							sprintf(label_str, "UniNOx(Addr:%d)", i);
							Morfeas_opc_ua_add_object_node(server, parent_Node_ID_str, Node_ID_str, label_str);
							//Populate objects and variables for UniNOx sensor's data
							strcpy(parent_Node_ID_str, Node_ID_str);
							sprintf(Node_ID_str, "%s.last_seen", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "Last seen", UA_TYPES_DATETIME);
							sprintf(Node_ID_str, "%s.NOx_value", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "NOx value (ppm)", UA_TYPES_FLOAT);
							sprintf(Node_ID_str, "%s.NOx_status_byte", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "NOx status value", UA_TYPES_BYTE);
							sprintf(Node_ID_str, "%s.NOx_status", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "NOx status", UA_TYPES_STRING);
							sprintf(Node_ID_str, "%s.O2_value", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "O2 value (%)", UA_TYPES_FLOAT);
							sprintf(Node_ID_str, "%s.O2_status_byte", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "O2 status value", UA_TYPES_BYTE);
							sprintf(Node_ID_str, "%s.O2_status", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "O2 status", UA_TYPES_STRING);
							sprintf(Node_ID_str, "%s.heaters_ctrl()", parent_Node_ID_str);
							Morfeas_add_NOX_heater_ctrl(server, parent_Node_ID_str, Node_ID_str);
							//Populate objects and variables for UniNOx sensor's status
							sprintf(Node_ID_str, "%s.sensors.addr_%d.status", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name, i);
							Morfeas_opc_ua_add_object_node(server, parent_Node_ID_str, Node_ID_str, "Status");
							strcpy(parent_Node_ID_str, Node_ID_str);
							sprintf(Node_ID_str, "%s.meas_state", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "Measure state", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.supply", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "Supply in range", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.inTemp", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "in Temperature", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.NOx_valid", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "is NOx_value valid", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.O2_valid", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "is O2_value valid", UA_TYPES_BOOLEAN);
							sprintf(Node_ID_str, "%s.heater_state", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "Heater state", UA_TYPES_STRING);
							//Populate objects and variables for UniNOx sensor's errors
							sprintf(Node_ID_str, "%s.sensors.addr_%d.errors", IPC_msg_dec->NOX_BUS_info.Dev_or_Bus_name, i);
							Morfeas_opc_ua_add_object_node(server, parent_Node_ID_str, Node_ID_str, "Errors");
							strcpy(parent_Node_ID_str, Node_ID_str);
							sprintf(Node_ID_str, "%s.heater_element", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "Heater element", UA_TYPES_STRING);
							sprintf(Node_ID_str, "%s.NOx_element", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "NOx element", UA_TYPES_STRING);
							sprintf(Node_ID_str, "%s.O2_element", parent_Node_ID_str);
							Morfeas_opc_ua_add_variable_node(server, parent_Node_ID_str, Node_ID_str, "O2 element", UA_TYPES_STRING);
						}
						else
							UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
					}
					else
					{
						if(!UA_Server_readNodeId(server, UA_NODEID_STRING(1, Node_ID_str), &NodeId))
						{
							UA_Server_deleteNode(server, NodeId, 1);
							UA_clear(&NodeId, &UA_TYPES[UA_TYPES_NODEID]);
						}
					}
				}
			pthread_mutex_unlock(&OPC_UA_NODESET_access);
			break;
	}
}

UA_StatusCode Morfeas_add_new_NOX_config_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[70], CAN_if[Dev_or_Bus_name_str_size];
    int ret = UA_STATUSCODE_GOOD;
	unsigned short new_auto_sw_off_val;
	cJSON *root = NULL;
	UA_String reply={0};

	//Sanitize method's Input
	if(!input[0].data)
		return UA_STATUSCODE_BADSYNTAXERROR;
	else //Get new_auto_sw_off_val
		new_auto_sw_off_val = *(unsigned short*)(input[0].data);
	//Get CAN_if
	if(get_NodeId_sec(methodId, 0, CAN_if, sizeof(CAN_if)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "NOx_auto_sw_off_value", new_auto_sw_off_val);
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("NOX", CAN_if, "NOX_auto_sw_off", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
    return ret;
}

void Morfeas_add_new_NOX_config(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
    UA_Argument inputArgument;
	UA_Argument outputArgument;
	//Configure inputArgument & outputArgument
	UA_Argument_init(&inputArgument);
	inputArgument.description = UA_LOCALIZEDTEXT("en-US", "Auto switch off value (Seconds) Range:(0..65535, 0:Disable)");
	inputArgument.name = UA_STRING("Auto_SW_off_val");
	inputArgument.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
	inputArgument.valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method new_NOX_config");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","new_NOX_config()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_add_new_NOX_config_method_callback,
							1, &inputArgument,
							1, &outputArgument,
							NULL, NULL);
}

UA_StatusCode Morfeas_add_NOX_global_heater_ctrl_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[60], CAN_if[Dev_or_Bus_name_str_size];
    int ret = UA_STATUSCODE_GOOD;
	cJSON *root = NULL;
	UA_String reply={0};

	if(!input[0].data)
		return UA_STATUSCODE_BADSYNTAXERROR;
	//Get CAN_if
	if(get_NodeId_sec(methodId, 0, CAN_if, sizeof(CAN_if)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "NOx_address", -1);
	cJSON_AddBoolToObject(root, "NOx_heater", *(bool *)input[0].data);
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("NOX", CAN_if, "NOX_heater", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
	return ret;
}

void Morfeas_add_NOX_global_heater_ctrl(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
	UA_Argument inputArgument;
	UA_Argument outputArgument;

	//Configure inputArguments
	UA_Argument_init(&inputArgument);
	inputArgument.description = UA_LOCALIZEDTEXT("en-US", "Global Heater control");
	inputArgument.name = UA_STRING("Global_heater_ctrl");
	inputArgument.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
	inputArgument.valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	//Configure outputArgument
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method global_heaters_ctrl");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","global_heaters_ctrl()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_add_NOX_global_heater_ctrl_method_callback,
							1, &inputArgument,
							1, &outputArgument,
							NULL, NULL);
}

UA_StatusCode Morfeas_add_NOX_heater_ctrl_method_callback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output)
{
	char contents[60], CAN_if[Dev_or_Bus_name_str_size], NOx_addr_str[10], NOx_addr;
    int ret = UA_STATUSCODE_GOOD;
	cJSON *root = NULL;
	UA_String reply={0};

	if(!input[0].data)
		return UA_STATUSCODE_BADSYNTAXERROR;
	//Get CAN_if
	if(get_NodeId_sec(methodId, 0, CAN_if, sizeof(CAN_if)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	//Get NOx_addr_str from NodeId's second element
	if(get_NodeId_sec(methodId, 2, NOx_addr_str, sizeof(NOx_addr_str)))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	if(!strstr(NOx_addr_str, "addr_"))
		return UA_STATUSCODE_BADOUTOFMEMORY;
	sscanf(NOx_addr_str, "addr_%hhu", &NOx_addr);
	//Construct contents for DBus method call (as JSON object)
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "NOx_address", NOx_addr);
	cJSON_AddBoolToObject(root, "NOx_heater", *(bool *)input[0].data);
	//Stringify the root JSON object
	if(!cJSON_PrintPreallocated(root, contents, sizeof(contents), 0))
		ret = UA_STATUSCODE_BADOUTOFMEMORY;
	cJSON_Delete(root);
	if(!ret)
	{
		if(!Morfeas_DBus_method_call("NOX", CAN_if, "NOX_heater", contents, &reply))
		{
			UA_Variant_setScalarCopy(output, &reply, &UA_TYPES[UA_TYPES_STRING]);
			UA_String_clear(&reply);
		}
		else
			ret = UA_STATUSCODE_BADINTERNALERROR;
	}
	return ret;
}

void Morfeas_add_NOX_heater_ctrl(UA_Server *server_ptr, char *Parent_id, char *Node_id)
{
	UA_Argument inputArgument;
	UA_Argument outputArgument;

	//Configure inputArguments
	UA_Argument_init(&inputArgument);
	inputArgument.description = UA_LOCALIZEDTEXT("en-US", "Heater control");
	inputArgument.name = UA_STRING("heater_ctrl");
	inputArgument.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
	inputArgument.valueRank = UA_VALUERANK_SCALAR_OR_ONE_DIMENSION;
	//Configure outputArgument
	UA_Argument_init(&outputArgument);
	outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Return of the method call");
	outputArgument.name = UA_STRING("Return");
	outputArgument.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
	outputArgument.valueRank = UA_VALUERANK_SCALAR;

	UA_MethodAttributes Method_Attr = UA_MethodAttributes_default;
	Method_Attr.description = UA_LOCALIZEDTEXT("en-US","Method heaters_ctrl");
	Method_Attr.displayName = UA_LOCALIZEDTEXT("en-US","heaters_ctrl()");
	Method_Attr.executable = true;
	Method_Attr.userExecutable = true;
	UA_Server_addMethodNode(server_ptr,
							UA_NODEID_STRING(1, Node_id),
							UA_NODEID_STRING(1, Parent_id),
							UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
							UA_QUALIFIEDNAME(1, Node_id),
							Method_Attr, &Morfeas_add_NOX_heater_ctrl_method_callback,
							1, &inputArgument,
							1, &outputArgument,
							NULL, NULL);
}
