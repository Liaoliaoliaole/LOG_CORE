/*
File: Morfeas_JSON.c part of Morfeas project, contains implementation of "Morfeas_JSON.h".
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
#include <math.h>
#include <time.h>
#include <arpa/inet.h>

#include <modbus.h>
//Header for cJSON
#include <cjson/cJSON.h>
//Include Functions implementation header
#include "../Morfeas_Types.h"

//Local functions
void extract_list_SDAQnode_data(gpointer node, gpointer arg_pass);

//Delete logstat file for SDAQnet_Handler
int delete_logstat_sys(char *logstat_path)
{
	if(!logstat_path)
		return -1;
	int ret_val;
	char *logstat_path_and_name, *slash;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen("/logstat_sys.json") + 2);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_sys.json",logstat_path, slash);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}

//Converting and exporting function for struct stats (Type Morfeas_sys_stats). Convert it to JSON format and save it to logstat_path
int logstat_sys(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;

	struct system_stats *sys_stats = stats_arg;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;

	//cJSON related variables
	char *JSON_str;
	cJSON *root;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen("/logstat_sys.json") + 2);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_sys.json",logstat_path, slash);

	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", time(NULL));
	cJSON_AddNumberToObject(root, "CPU_temp", sys_stats->CPU_temp);
	cJSON_AddNumberToObject(root, "CPU_Util", sys_stats->CPU_Util);
	cJSON_AddNumberToObject(root, "RAM_Util", sys_stats->RAM_Util);
	cJSON_AddNumberToObject(root, "Disk_Util", sys_stats->Disk_Util);
	cJSON_AddNumberToObject(root, "Up_time", sys_stats->Up_time);

	//JSON_str = cJSON_Print(root);
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}

	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}

//Delete logstat file for SDAQ_handler
int delete_logstat_SDAQ(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	int ret_val;
	char *logstat_path_and_name, *slash;
	struct Morfeas_SDAQ_if_stats *stats = stats_arg;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->CAN_IF_name) + strlen("/logstat_SDAQs_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_SDAQs_%s.json",logstat_path, slash, stats->CAN_IF_name);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}

int logstat_SDAQ(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	struct Morfeas_SDAQ_if_stats *stats = stats_arg;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->CAN_IF_name) + strlen("/logstat_SDAQs_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_SDAQs_%s.json",logstat_path, slash, stats->CAN_IF_name);
	//cJSON related variables
	char *JSON_str;
	cJSON *root;
	cJSON *electrics;
    cJSON *logstat;

    //printf("Version: %s\n", cJSON_Version());
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", time(NULL));
	cJSON_AddItemToObject(root, "CANBus_interface", cJSON_CreateString(stats->CAN_IF_name));
	//Add electrics to LogStat JSON, if port SCA exist.
	if(!isnan(stats->Shunt_temp))
	{
		cJSON_AddItemToObject(root, "Electrics", electrics = cJSON_CreateObject());
		cJSON_AddNumberToObject(electrics, "Last_calibration_UNIX", stats->Morfeas_RPi_Hat_last_cal);
		cJSON_AddNumberToObject(electrics, "BUS_voltage", stats->Bus_voltage);
		cJSON_AddNumberToObject(electrics, "BUS_amperage", stats->Bus_amperage);
		cJSON_AddNumberToObject(electrics, "BUS_Shunt_Res_temp", stats->Shunt_temp);
	}
	//Add BUS_util, Amount of Detected_SDAQs, and SDAQs Data
	cJSON_AddNumberToObject(root, "BUS_Utilization", stats->Bus_util);
	cJSON_AddNumberToObject(root, "BUS_Error_rate", stats->Bus_error_rate);
	cJSON_AddNumberToObject(root, "Detected_SDAQs", stats->detected_SDAQs);
	cJSON_AddNumberToObject(root, "Incomplete_SDAQs", stats->incomplete_SDAQs);
	if(stats->detected_SDAQs)
	{
		cJSON_AddItemToObject(root, "SDAQs_data",logstat = cJSON_CreateArray());
		g_slist_foreach(stats->list_SDAQs, extract_list_SDAQnode_data, logstat);
	}
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}
	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}

void extract_list_SDAQ_Channels_cal_dates(gpointer node, gpointer arg_pass)
{
	struct Channel_date_entry *node_dec = node;
	struct tm cal_date = {0};
	cJSON *array = arg_pass;
	cJSON *node_data;
	if(node)
	{
		node_data = cJSON_CreateObject();
		cal_date.tm_year = node_dec->CH_date.year + 100;//100 = 2000 - 1900
		cal_date.tm_mon = node_dec->CH_date.month - 1;
		cal_date.tm_mday = node_dec->CH_date.day;

		cJSON_AddNumberToObject(node_data, "Channel", node_dec->Channel);
		cJSON_AddNumberToObject(node_data, "Calibration_date_UNIX", mktime(&cal_date));
		cJSON_AddNumberToObject(node_data, "Calibration_period", node_dec->CH_date.period);
		cJSON_AddNumberToObject(node_data, "Amount_of_points", node_dec->CH_date.amount_of_points);
		cJSON_AddItemToObject(node_data, "Unit", cJSON_CreateString(unit_str[node_dec->CH_date.cal_units]));
		cJSON_AddItemToObject(node_data, "Is_calibrated", cJSON_CreateBool(node_dec->CH_date.cal_units >= Unit_code_base_region_size &&
																		   node_dec->CH_date.amount_of_points));
		cJSON_AddNumberToObject(node_data, "Unit_code", node_dec->CH_date.cal_units);
		cJSON_AddItemToArray(array, node_data);
	}
}

void extract_list_SDAQ_Channels_acc_to_avg_meas(gpointer node, gpointer arg_pass)
{
	const char *unit_str_ptr;
	struct Channel_acc_meas_entry *node_dec = node;
	cJSON *array = arg_pass;
	cJSON *node_data, *channel_status;

	if(node)
	{
		node_data = cJSON_CreateObject();
		cJSON_AddNumberToObject(node_data, "Channel", node_dec->Channel);
		//-- Add SDAQ's Channel Status --//
		cJSON_AddItemToObject(node_data, "Channel_Status", channel_status = cJSON_CreateObject());
		cJSON_AddNumberToObject(channel_status, "Channel_status_val", node_dec->status);
		cJSON_AddItemToObject(channel_status, "Out_of_Range", cJSON_CreateBool(node_dec->status & (1<<Out_of_range)));
		cJSON_AddItemToObject(channel_status, "No_Sensor", cJSON_CreateBool(node_dec->status & (1<<No_sensor)));
		cJSON_AddItemToObject(channel_status, "Over_Range", cJSON_CreateBool(node_dec->status & (1<<Over_range)));
		//-- Add Unit of Channel --//
		unit_str_ptr = unit_str[node_dec->unit_code];
		if(!unit_str_ptr)
			unit_str_ptr = "Unclassified";
		cJSON_AddItemToObject(node_data, "Unit", cJSON_CreateString(unit_str_ptr));
		//-- Add Averaged measurement of Channel --//
		if((node_dec->status & (1<<No_sensor)))//Check if No_Sensor bit is set
		{
			node_dec->meas_acc = NAN;
			node_dec->meas_max = NAN;
			node_dec->meas_min = NAN;
		}
		else if(node_dec->cnt)
			node_dec->meas_acc /= node_dec->cnt;
		cJSON_AddNumberToObject(node_data, "CNT", node_dec->cnt);
		cJSON_AddNumberToObject(node_data, "Meas_avg", node_dec->meas_acc);
		cJSON_AddNumberToObject(node_data, "Meas_max", node_dec->meas_max);
		cJSON_AddNumberToObject(node_data, "Meas_min", node_dec->meas_min);
		cJSON_AddNumberToObject(node_data, "Last_Meas", node_dec->last_meas);
		node_dec->last_meas = node_dec->meas_acc;
		node_dec->cnt = 0;
		cJSON_AddItemToObject(array, "Measurement_data", node_data);
	}
}

void extract_list_SDAQnode_data(gpointer node, gpointer arg_pass)
{
	struct SDAQ_info_entry *node_dec = (struct SDAQ_info_entry *)node;
	GSList *SDAQ_Channels_cal_dates = node_dec->SDAQ_Channels_cal_dates;
	GSList *SDAQ_Channels_acc_meas = node_dec->SDAQ_Channels_acc_meas;
	cJSON *list_SDAQs_array, *node_data, *list_SDAQ_Channels_cal_dates, *SDAQ_status, *SDAQ_info, *list_SDAQ_Channels_acc_meas;
	if(node)
	{
		list_SDAQs_array = arg_pass;
		node_data = cJSON_CreateObject();
		//-- Add SDAQ's general data to SDAQ's JSON object --//
		cJSON_AddNumberToObject(node_data, "Last_seen_UNIX", node_dec->last_seen);
		cJSON_AddNumberToObject(node_data, "Address", node_dec->SDAQ_address);
		cJSON_AddNumberToObject(node_data, "Serial_number", (node_dec->SDAQ_status).dev_sn);
		cJSON_AddItemToObject(node_data, "SDAQ_type", cJSON_CreateString(dev_type_str[(node_dec->SDAQ_info).dev_type]));
		cJSON_AddNumberToObject(node_data, "Timediff", node_dec->Timediff);
		//-- Add SDAQ's Status --//
		cJSON_AddItemToObject(node_data, "SDAQ_Status", SDAQ_status = cJSON_CreateObject());
		cJSON_AddNumberToObject(SDAQ_status, "SDAQ_status_val", (node_dec->SDAQ_status).status);
		cJSON_AddItemToObject(SDAQ_status, "In_sync", cJSON_CreateBool((node_dec->SDAQ_status).status & (1<<In_sync)));
		cJSON_AddItemToObject(SDAQ_status, "Error", cJSON_CreateBool((node_dec->SDAQ_status).status & (1<<Error)));
		cJSON_AddItemToObject(SDAQ_status, "State", cJSON_CreateString(status_byte_dec((node_dec->SDAQ_status).status, State)));
		cJSON_AddItemToObject(SDAQ_status, "Mode", cJSON_CreateString(status_byte_dec((node_dec->SDAQ_status).status, Mode)));
		cJSON_AddItemToObject(SDAQ_status, "Registration_status", cJSON_CreateString(SDAQ_reg_status_str[node_dec->reg_status]));
		//-- Add SDAQ's Info --//
		cJSON_AddItemToObject(node_data, "SDAQ_info", SDAQ_info = cJSON_CreateObject());
		if(dev_input_mode_str[node_dec->SDAQ_info.dev_type][node_dec->inp_mode])
			cJSON_AddItemToObject(SDAQ_info, "input_mode", cJSON_CreateString(dev_input_mode_str[node_dec->SDAQ_info.dev_type][node_dec->inp_mode]));
		cJSON_AddNumberToObject(SDAQ_info, "firm_rev", (node_dec->SDAQ_info).firm_rev);
		cJSON_AddNumberToObject(SDAQ_info, "hw_rev", (node_dec->SDAQ_info).hw_rev);
		cJSON_AddNumberToObject(SDAQ_info, "Number_of_channels", (node_dec->SDAQ_info).num_of_ch);
		cJSON_AddNumberToObject(SDAQ_info, "Sample_rate", (node_dec->SDAQ_info).sample_rate);
		cJSON_AddNumberToObject(SDAQ_info, "Max_cal_point", (node_dec->SDAQ_info).max_cal_point);
		//-- Add SDAQ's channel Cal dates  --//
		cJSON_AddItemToObject(node_data, "Calibration_Data",list_SDAQ_Channels_cal_dates = cJSON_CreateArray());
		g_slist_foreach(SDAQ_Channels_cal_dates, extract_list_SDAQ_Channels_cal_dates, list_SDAQ_Channels_cal_dates);
		//-- Add SDAQ's channel average measurement  --//
		cJSON_AddItemToObject(node_data, "Meas",list_SDAQ_Channels_acc_meas = cJSON_CreateArray());
		g_slist_foreach(SDAQ_Channels_acc_meas, extract_list_SDAQ_Channels_acc_to_avg_meas, list_SDAQ_Channels_acc_meas);
		//-- Add SDAQ's Data to Array --//
		cJSON_AddItemToArray(list_SDAQs_array, node_data);
	}
}

//Delete logstat file for IOBOX_handler
int delete_logstat_IOBOX(char *logstat_path, void *stats_arg)
{
	int ret_val;
	if(!logstat_path || !stats_arg)
		return -1;
	char *logstat_path_and_name, *slash;
	struct Morfeas_IOBOX_if_stats *stats = stats_arg;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_IOBOX_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_IOBOX_%s.json",logstat_path, slash, stats->dev_name);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}
//Converting and exporting function for IOBOX Modbus register. Convert it to JSON format and save it to logstat_path
int logstat_IOBOX(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	struct Morfeas_IOBOX_if_stats *stats = stats_arg;
	unsigned int Identifier;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;
	char str_buff[10] = {0};
	float value;

	//Correct logstat_path_and_name
	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_IOBOX_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_IOBOX_%s.json",logstat_path, slash, stats->dev_name);
	//cJSON related variables
	char *JSON_str;
	cJSON *root, *pow_supp_data, *RX_json;

	//Convert IPv4 to IOBOX's Identifier
	inet_pton(AF_INET, stats->IOBOX_IPv4_addr, &Identifier);
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", time(NULL));
	cJSON_AddItemToObject(root, "Dev_name", cJSON_CreateString(stats->dev_name));
	cJSON_AddItemToObject(root, "IPv4_address", cJSON_CreateString(stats->IOBOX_IPv4_addr));
	cJSON_AddNumberToObject(root, "Identifier", Identifier);
	if(!stats->error)
	{
		//Add connection status string item in JSON
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString("Okay"));
		//Add Wireless_Inductive_Power_Supply data
		cJSON_AddItemToObject(root, "Power_Supply",pow_supp_data = cJSON_CreateObject());
		cJSON_AddNumberToObject(pow_supp_data, "Vin", stats->Supply_Vin/stats->counter);
		stats->Supply_Vin = 0;
		for(int i=0; i<IOBOX_Amount_of_STD_RXs; i++)
		{
			sprintf(str_buff, "CH%1u_Vout", i+1);
			cJSON_AddNumberToObject(pow_supp_data, str_buff, stats->Supply_meas[i].Vout/stats->counter);
			stats->Supply_meas[i].Vout = 0;
			sprintf(str_buff, "CH%1u_Iout", i+1);
			cJSON_AddNumberToObject(pow_supp_data, str_buff, stats->Supply_meas[i].Iout/stats->counter);
			stats->Supply_meas[i].Iout = 0;
		}
		//Add RX_Data
		for(int i=0; i<IOBOX_Amount_of_All_RXs; i++)
		{
			sprintf(str_buff, "RX%1u", i+1);
			if(stats->RX[i].status && stats->RX[i].success)
			{
				cJSON_AddItemToObject(root, str_buff, RX_json = cJSON_CreateObject());
				for(int j=0; j<IOBOX_Amount_of_channels; j++)
				{

					sprintf(str_buff, "CH%1u", j+1);
					value = stats->RX[i].CH_value[j]/stats->counter;
					stats->RX[i].CH_value[j] = 0;
					if(value<1500.0)
						cJSON_AddNumberToObject(RX_json, str_buff, value);
					else
						cJSON_AddItemToObject(RX_json, str_buff, cJSON_CreateString("No sensor"));

				}
				cJSON_AddNumberToObject(RX_json, "Index", stats->RX[i].index);
				cJSON_AddNumberToObject(RX_json, "Status", stats->RX[i].status);
				cJSON_AddNumberToObject(RX_json, "Success", stats->RX[i].success);
			}
			else
				cJSON_AddItemToObject(root, str_buff, cJSON_CreateString("Disconnected"));
		}
	}
	else
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString(modbus_strerror(stats->error)));
	//Reset accumulator counter
	stats->counter = 0;
	//Print JSON to File
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}
	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}

//Delete logstat file for MDAQ_handler
int delete_logstat_MDAQ(char *logstat_path, void *stats_arg)
{
	int ret_val;
	if(!logstat_path || !stats_arg)
		return -1;
	char *logstat_path_and_name, *slash;
	struct Morfeas_MDAQ_if_stats *stats = stats_arg;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_MDAQ_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_MDAQ_%s.json",logstat_path, slash, stats->dev_name);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}

//Converting and exporting function for MDAQ Modbus register. Convert it to JSON format and save it to logstat_path
int logstat_MDAQ(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	struct Morfeas_MDAQ_if_stats *stats = stats_arg;
	unsigned int Identifier;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;
	char str_buff[30] = {0};
	//Correct logstat_path_and_name
	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_MDAQ_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_MDAQ_%s.json",logstat_path, slash, stats->dev_name);
	//cJSON related variables
	char *JSON_str;
	cJSON *root, *Channels_array, *Channel, *values, *warnings;

	//Convert IPv4 to MDAQ's Identifier
	inet_pton(AF_INET, stats->MDAQ_IPv4_addr, &Identifier);
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", time(NULL));
	cJSON_AddItemToObject(root, "Dev_name", cJSON_CreateString(stats->dev_name));
	cJSON_AddItemToObject(root, "IPv4_address", cJSON_CreateString(stats->MDAQ_IPv4_addr));
	cJSON_AddNumberToObject(root, "Identifier", Identifier);
	if(!stats->error)
	{
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString("Okay"));
		//Add MDAQ Board data
		cJSON_AddNumberToObject(root, "Index", stats->meas_index);
		cJSON_AddNumberToObject(root, "Board_temp", (stats->board_temp/stats->counter));
		stats->board_temp = 0;
		//Add array with channels measurements on JSON
		cJSON_AddItemToObject(root, "MDAQ_Channels",Channels_array = cJSON_CreateArray());
		//Load MDAQ Channels Data to stats
		for(int i=0; i<8; i++)
		{
			Channel = cJSON_CreateObject();
			cJSON_AddItemToObject(Channel, "Values", values = cJSON_CreateObject());
			for(int j=0; j<4; j++)
			{
				cJSON_AddNumberToObject(Channel, "Channel", i+1);
				sprintf(str_buff, "Value%1hhu", j+1);
				if(!((stats->meas[i].warnings>>j)&1))
					cJSON_AddNumberToObject(values, str_buff, (stats->meas[i].value[j]/stats->counter));
				else
					cJSON_AddNumberToObject(values, str_buff, NAN);
				stats->meas[i].value[j] = 0;
			}
			cJSON_AddItemToObject(Channel, "Warnings", warnings = cJSON_CreateObject());
			cJSON_AddNumberToObject(warnings, "Warnings_value", stats->meas[i].warnings);
			for(int j=0; j<4; j++)
			{
				sprintf(str_buff, "Is_Value%1hhu_valid", j+1);
				cJSON_AddItemToObject(warnings, str_buff, cJSON_CreateBool(!((stats->meas[i].warnings>>j)&1)));
			}
			cJSON_AddItemToArray(Channels_array, Channel);
		}
	}
	else
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString(modbus_strerror(stats->error)));
	//Reset accumulator counter
	stats->counter = 0;
	//Print JSON to File
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}
	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}

//Converting and exporting function for MTI's stats struct. Convert it to JSON format and save it to logstat_path
int logstat_MTI(char *logstat_path, void *stats_arg)
{
		if(!logstat_path || !stats_arg)
		return -1;
	struct Morfeas_MTI_if_stats *stats = stats_arg;
	unsigned int Identifier, i;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;

	//Correct logstat_path_and_name
	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_MTI_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_MTI_%s.json",logstat_path, slash, stats->dev_name);
	//cJSON related variables
	char *JSON_str;
	cJSON *root, *MTI_status, *MTI_button_state, *PWM_outDuty_CHs, *PWM_config,
	      *MTI_global_state, *Tele_data, *CHs, *REFs, *RMSW_t;

	//Convert IPv4 to MTI's Identifier
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &Identifier);
	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", time(NULL));
	cJSON_AddItemToObject(root, "Dev_name", cJSON_CreateString(stats->dev_name));
	cJSON_AddItemToObject(root, "IPv4_address", cJSON_CreateString(stats->MTI_IPv4_addr));
	cJSON_AddNumberToObject(root, "Identifier", Identifier);
	if(!stats->error)
	{
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString("Okay"));
		//Create MTI's_stats object
		cJSON_AddItemToObject(root, "MTI_status", MTI_status = cJSON_CreateObject());
		cJSON_AddNumberToObject(MTI_status, "Radio_CH", stats->MTI_Radio_config.RF_channel);
		cJSON_AddItemToObject(MTI_status, "Modem_data_rate", cJSON_CreateString(MTI_Data_rate_str[stats->MTI_Radio_config.Data_rate]));
		cJSON_AddItemToObject(MTI_status, "Tele_Device_type", cJSON_CreateString(MTI_Tele_dev_type_str[stats->MTI_Radio_config.Tele_dev_type]));
		cJSON_AddNumberToObject(MTI_status, "MTI_batt_volt", stats->MTI_status.MTI_batt_volt);
		cJSON_AddNumberToObject(MTI_status, "MTI_batt_capacity", stats->MTI_status.MTI_batt_capacity);
		cJSON_AddItemToObject(MTI_status, "MTI_charge_status", cJSON_CreateString(MTI_charger_state_str[stats->MTI_status.MTI_charge_status]));
		cJSON_AddNumberToObject(MTI_status, "MTI_CPU_temp", stats->MTI_status.MTI_CPU_temp);
		cJSON_AddNumberToObject(MTI_status, "PWM_gen_out_freq", stats->MTI_status.PWM_gen_out_freq);
		cJSON_AddItemToObject(MTI_status, "PWM_CHs_outDuty", PWM_outDuty_CHs = cJSON_CreateArray());
		for(i=0; i<4; i++)
			cJSON_AddItemToArray(PWM_outDuty_CHs, cJSON_CreateNumber(stats->MTI_status.PWM_outDuty_CHs[i]));
		//Add button states on MTI_status
		cJSON_AddItemToObject(MTI_status, "MTI_buttons_state", MTI_button_state = cJSON_CreateObject());
		cJSON_AddItemToObject(MTI_button_state, "PB1", cJSON_CreateBool(stats->MTI_status.buttons_state.pb1));
		cJSON_AddItemToObject(MTI_button_state, "PB2", cJSON_CreateBool(stats->MTI_status.buttons_state.pb2));
		cJSON_AddItemToObject(MTI_button_state, "PB3", cJSON_CreateBool(stats->MTI_status.buttons_state.pb3));
		if(stats->MTI_Radio_config.Tele_dev_type>1)//transceiver enabled
		{
			//Add device specific values on JSON root
			if(stats->MTI_Radio_config.Tele_dev_type != RMSW_MUX)
			{
				//Add RX status data for all the Telemetries (exception to Remote switches). Format and position same for all, TC4 used as reference
				cJSON_AddItemToObject(root, "Tele_data", Tele_data = cJSON_CreateObject());
				cJSON_AddNumberToObject(Tele_data, "Packet_Index", stats->Tele_data.as_TC4.packet_index);
				cJSON_AddNumberToObject(Tele_data, "RX_Status", stats->Tele_data.as_TC4.RX_status);
				cJSON_AddNumberToObject(Tele_data, "RX_Success_Ratio", stats->Tele_data.as_TC4.RX_Success_ratio);
				cJSON_AddItemToObject(Tele_data, "IsValid", cJSON_CreateBool(stats->Tele_data.as_TC4.Data_isValid));
				if(stats->MTI_Radio_config.Tele_dev_type != Tele_quad)
				{
					cJSON_AddNumberToObject(Tele_data, "Samples_toValid", stats->MTI_Radio_config.sRegs.for_temp_tele.StV);
					cJSON_AddNumberToObject(Tele_data, "samples_toInvalid", stats->MTI_Radio_config.sRegs.for_temp_tele.StF);
				}
				switch(stats->MTI_Radio_config.Tele_dev_type)
				{
					case Tele_TC4:
						cJSON_AddItemToObject(Tele_data, "CHs", CHs = cJSON_CreateArray());
						cJSON_AddItemToObject(Tele_data, "CHs_refs", REFs = cJSON_CreateArray());
						for(i=0; i<4; i++)
						{
							if(stats->Tele_data.as_TC4.CHs[i]<NO_SENSOR_VALUE)
								cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_TC4.CHs[i]));
							else
								cJSON_AddItemToArray(CHs, cJSON_CreateString("No sensor"));
						}
						for(i=0; i<2; i++)
							cJSON_AddItemToArray(REFs, cJSON_CreateNumber(stats->Tele_data.as_TC4.Refs[i]));
						break;
					case Tele_TC8:
						cJSON_AddItemToObject(Tele_data, "CHs", CHs = cJSON_CreateArray());
						cJSON_AddItemToObject(Tele_data, "CHs_refs", REFs = cJSON_CreateArray());
						for(i=0; i<8; i++)
						{
							if(stats->Tele_data.as_TC8.CHs[i]<NO_SENSOR_VALUE)
								cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_TC8.CHs[i]));
							else
								cJSON_AddItemToArray(CHs, cJSON_CreateString("No sensor"));
						}
						for(i=0; i<8; i++)
							cJSON_AddItemToArray(REFs, cJSON_CreateNumber(stats->Tele_data.as_TC8.Refs[i]));
						break;
					case Tele_TC16:
						cJSON_AddItemToObject(Tele_data, "CHs", CHs = cJSON_CreateArray());
						for(i=0; i<16; i++)
						{
							if(stats->Tele_data.as_TC16.CHs[i]<NO_SENSOR_VALUE)
								cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_TC16.CHs[i]));
							else
								cJSON_AddItemToArray(CHs, cJSON_CreateString("No sensor"));
						}
						break;
					case Tele_quad:
						cJSON_AddItemToObject(MTI_status, "PWMs_config", PWM_config = cJSON_CreateArray());
						for(i=0; i<Amount_OF_GENS; i++)
						{
							CHs = cJSON_CreateObject();
							cJSON_AddNumberToObject(CHs, "PWM_max", stats->Tele_data.as_QUAD.gen_config[i].max);
							cJSON_AddNumberToObject(CHs, "PWM_min", stats->Tele_data.as_QUAD.gen_config[i].min);
							cJSON_AddItemToObject(CHs, "Saturation_mode", cJSON_CreateBool(stats->Tele_data.as_QUAD.gen_config[i].pwm_mode.dec.saturation));
							cJSON_AddNumberToObject(CHs, "Scaler", stats->Tele_data.as_QUAD.gen_config[i].scaler);
							cJSON_AddItemToArray(PWM_config, CHs);
						}
						cJSON_AddItemToObject(Tele_data, "CHs", CHs = cJSON_CreateArray());
						for(i=0; i<Amount_OF_GENS; i++)
							cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_QUAD.CHs[i]));
						cJSON_AddItemToObject(Tele_data, "CNTs", REFs = cJSON_CreateArray());
						for(i=0; i<Amount_OF_GENS; i++)
							cJSON_AddItemToArray(REFs, cJSON_CreateNumber(stats->Tele_data.as_QUAD.CNTs[i]));
						break;
				}
			}
			else if(stats->MTI_Radio_config.Tele_dev_type == RMSW_MUX)
			{
				//Add MTI_global_state to MTI_status
				cJSON_AddItemToObject(MTI_status, "MTI_Global_state", MTI_global_state = cJSON_CreateObject());
				cJSON_AddItemToObject(MTI_global_state, "Global_ON_OFF", cJSON_CreateBool(stats->MTI_Radio_config.sRegs.for_rmsw_dev.G_SW));
				cJSON_AddItemToObject(MTI_global_state, "Global_Sleep", cJSON_CreateBool(stats->MTI_Radio_config.sRegs.for_rmsw_dev.G_SL));
				cJSON_AddItemToObject(MTI_global_state, "Global_Power_state", cJSON_CreateBool(stats->MTI_Radio_config.sRegs.for_rmsw_dev.G_P_state));
				cJSON_AddItemToObject(MTI_global_state, "Global_Sleep_state", cJSON_CreateBool(stats->MTI_Radio_config.sRegs.for_rmsw_dev.G_S_state));
				//Add remote switch data to JSON
				cJSON_AddItemToObject(root, "Tele_data", Tele_data = cJSON_CreateArray());
				for(i=0; i<stats->Tele_data.as_RMSWs.amount_of_devices; i++)
				{
					RMSW_t = cJSON_CreateObject();
					cJSON_AddNumberToObject(RMSW_t, "Mem_offset", stats->Tele_data.as_RMSWs.det_devs_data[i].pos_offset);
					cJSON_AddItemToObject(RMSW_t, "Dev_type", cJSON_CreateString(MTI_RM_dev_type_str[stats->Tele_data.as_RMSWs.det_devs_data[i].dev_type]));
					cJSON_AddNumberToObject(RMSW_t, "Dev_ID", stats->Tele_data.as_RMSWs.det_devs_data[i].dev_id);
					cJSON_AddNumberToObject(RMSW_t, "Time_from_last_msg", stats->Tele_data.as_RMSWs.det_devs_data[i].time_from_last_mesg);
					cJSON_AddNumberToObject(RMSW_t, "Dev_temp", stats->Tele_data.as_RMSWs.det_devs_data[i].dev_temp);
					cJSON_AddNumberToObject(RMSW_t, "Supply_volt", stats->Tele_data.as_RMSWs.det_devs_data[i].input_voltage);
					switch(stats->Tele_data.as_RMSWs.det_devs_data[i].dev_type)
					{
						case MUX:
							CHs = cJSON_CreateObject();
							cJSON_AddNumberToObject(CHs, "Control_byte", stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.as_byte);
							cJSON_AddNumberToObject(CHs, "TX_Rate", stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.Rep_rate?2:20);
							cJSON_AddItemToObject(CHs, "CH1", cJSON_CreateString(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH1?"B":"A"));
							cJSON_AddItemToObject(CHs, "CH2", cJSON_CreateString(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH2?"B":"A"));
							cJSON_AddItemToObject(CHs, "CH3", cJSON_CreateString(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH3?"B":"A"));
							cJSON_AddItemToObject(CHs, "CH4", cJSON_CreateString(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH4?"B":"A"));
							cJSON_AddItemToObject(RMSW_t, "Controls", CHs);
							break;
						case RMSW_2CH:
							//Add measurements
							CHs = cJSON_CreateArray();
							for(int j=0; j<4; j++)
								cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_RMSWs.det_devs_data[i].meas_data[j]));
							cJSON_AddItemToObject(RMSW_t, "CHs_meas", CHs);
							//Add control status
							REFs = cJSON_CreateObject();
							cJSON_AddNumberToObject(REFs, "Control_byte", stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.as_byte);
							cJSON_AddNumberToObject(REFs, "TX_Rate", stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.Rep_rate?2:20);
							cJSON_AddItemToObject(REFs, "Main", cJSON_CreateBool(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.Main));
							cJSON_AddItemToObject(REFs, "CH1", cJSON_CreateBool(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.CH1));
							cJSON_AddItemToObject(REFs, "CH2", cJSON_CreateBool(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.CH2));
							cJSON_AddItemToObject(RMSW_t, "Controls", REFs);
							break;
						case Mini_RMSW:
							//Add measurements
							CHs = cJSON_CreateArray();
							for(int j=0; j<4; j++)
							{
								if(stats->Tele_data.as_RMSWs.det_devs_data[i].meas_data[j]<NO_SENSOR_VALUE)
									cJSON_AddItemToArray(CHs, cJSON_CreateNumber(stats->Tele_data.as_RMSWs.det_devs_data[i].meas_data[j]));
								else
									cJSON_AddItemToArray(CHs, cJSON_CreateString("No sensor"));
							}
							cJSON_AddItemToObject(RMSW_t, "CHs_meas", CHs);
							//Add control status
							REFs = cJSON_CreateObject();
							cJSON_AddNumberToObject(REFs, "Control_byte", stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.as_byte);
							switch(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mini_dec.Rep_rate)
							{
								case 0: cJSON_AddNumberToObject(REFs, "TX_Rate", 20); break;
								case 2: cJSON_AddNumberToObject(REFs, "TX_Rate", 2); break;
								case 3:
								case 1: cJSON_AddNumberToObject(REFs, "TX_Rate", .2); break;
							}
							cJSON_AddItemToObject(REFs, "Main", cJSON_CreateBool(stats->Tele_data.as_RMSWs.det_devs_data[i].switch_status.mini_dec.Main));
							cJSON_AddItemToObject(RMSW_t, "Controls", REFs);
							break;
					}
					cJSON_AddItemToArray(Tele_data, RMSW_t);
				}

			}
		}
	}
	else
		cJSON_AddItemToObject(root, "Connection_status", cJSON_CreateString(modbus_strerror(stats->error)));
	//Reset accumulator counter
	stats->counter = 0;
	//Print JSON to File
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}
	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}
//Delete logstat file for MTI_handler
int delete_logstat_MTI(char *logstat_path, void *stats_arg)
{
	int ret_val;
	if(!logstat_path || !stats_arg)
		return -1;
	char *logstat_path_and_name, *slash;
	struct Morfeas_MTI_if_stats *stats = stats_arg;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->dev_name) + strlen("/logstat_MTI_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_MTI_%s.json",logstat_path, slash, stats->dev_name);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}

//Converting and exporting function for ΝΟΧ stats, Convert it to JSON format and save it to logstat_path
int logstat_NOX(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	struct Morfeas_NOX_if_stats *stats = stats_arg;
	float value;
	FILE * pFile;
	static unsigned char write_error = 0;
	char *logstat_path_and_name, *slash;
	//make time_t variable and get unix time
	time_t now_time = time(NULL);
	//Correct logstat_path_and_name
	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->CAN_IF_name) + strlen("/logstat_NOXs_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_NOXs_%s.json",logstat_path, slash, stats->CAN_IF_name);
	//cJSON related variables
	char *JSON_str;
	cJSON *root, *electrics, *NOx_array, *curr_NOx_data, *NOx_status, *NOx_errors;

	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "logstat_build_date_UNIX", now_time);
	cJSON_AddNumberToObject(root, "ws_port", stats->ws_port);
	cJSON_AddItemToObject(root, "CANBus_interface", cJSON_CreateString(stats->CAN_IF_name));
	//Add electrics to LogStat JSON, if port SCA exist.
	if(!isnan(stats->Shunt_temp))
	{
		cJSON_AddItemToObject(root, "Electrics", electrics = cJSON_CreateObject());
		cJSON_AddNumberToObject(electrics, "Last_calibration_UNIX", stats->Morfeas_RPi_Hat_last_cal);
		cJSON_AddNumberToObject(electrics, "BUS_voltage", stats->Bus_voltage);
		cJSON_AddNumberToObject(electrics, "BUS_amperage", stats->Bus_amperage);
		cJSON_AddNumberToObject(electrics, "BUS_Shunt_Res_temp", stats->Shunt_temp);
	}
	//Add BUS_util and Bus_error_rate to JSON root
	cJSON_AddNumberToObject(root, "BUS_Utilization", stats->Bus_util);
	cJSON_AddNumberToObject(root, "BUS_Error_rate", stats->Bus_error_rate);
	//Add auto power off value and counter
	cJSON_AddNumberToObject(root, "Auto_SW_OFF_value", stats->auto_switch_off_value);
	cJSON_AddNumberToObject(root, "Auto_SW_OFF_cnt", stats->auto_switch_off_cnt);
	//Add NOx Sensor's data to NOx_array
	cJSON_AddItemToObject(root, "NOx_sensors", NOx_array = cJSON_CreateArray());
	for(int i=0; i<2; i++)
	{
		if((now_time - stats->NOXs_data[i].last_seen) <= 10)
		{
			cJSON_AddItemToArray(NOx_array, curr_NOx_data = cJSON_CreateObject());
			cJSON_AddNumberToObject(curr_NOx_data, "addr", i);
			cJSON_AddNumberToObject(curr_NOx_data, "last_seen", stats->NOXs_data[i].last_seen);
			//NOx value statistics
			cJSON_AddNumberToObject(curr_NOx_data, "NOx_value_integral_size", stats->NOx_statistics[i].NOx_value_sample_cnt);
			cJSON_AddNumberToObject(curr_NOx_data, "NOx_value_min", stats->NOx_statistics[i].NOx_value_min);
			cJSON_AddNumberToObject(curr_NOx_data, "NOx_value_max", stats->NOx_statistics[i].NOx_value_max);
			if(stats->NOx_statistics[i].NOx_value_sample_cnt)
			{
				value = stats->NOx_statistics[i].NOx_value_acc/stats->NOx_statistics[i].NOx_value_sample_cnt;
				stats->NOx_statistics[i].NOx_value_acc = 0;
				stats->NOx_statistics[i].NOx_value_sample_cnt = 0;
			}
			else
				value = NAN;
			cJSON_AddNumberToObject(curr_NOx_data, "NOx_value_avg", value);
			//O2 value statistics
			cJSON_AddNumberToObject(curr_NOx_data, "O2_value_integral_size", stats->NOx_statistics[i].O2_value_sample_cnt);
			cJSON_AddNumberToObject(curr_NOx_data, "O2_value_min", stats->NOx_statistics[i].O2_value_min);
			cJSON_AddNumberToObject(curr_NOx_data, "O2_value_max", stats->NOx_statistics[i].O2_value_max);
			if(stats->NOx_statistics[i].O2_value_sample_cnt)
			{
				value = stats->NOx_statistics[i].O2_value_acc/stats->NOx_statistics[i].O2_value_sample_cnt;
				stats->NOx_statistics[i].O2_value_acc = 0;
				stats->NOx_statistics[i].O2_value_sample_cnt = 0;
			}
			else
				value = NAN;
			cJSON_AddNumberToObject(curr_NOx_data, "O2_value_avg", value);
			//Add status for curr_NOx_data
			cJSON_AddItemToObject(curr_NOx_data, "status", NOx_status = cJSON_CreateObject());
			cJSON_AddItemToObject(NOx_status, "meas_state", cJSON_CreateBool(stats->NOXs_data[i].meas_state));
			cJSON_AddItemToObject(NOx_status, "supply_in_range", cJSON_CreateBool(stats->NOXs_data[i].status.supply_in_range));
			cJSON_AddItemToObject(NOx_status, "in_temperature", cJSON_CreateBool(stats->NOXs_data[i].status.in_temperature));
			cJSON_AddItemToObject(NOx_status, "is_NOx_value_valid", cJSON_CreateBool(stats->NOXs_data[i].status.is_NOx_value_valid));
			cJSON_AddItemToObject(NOx_status, "is_O2_value_valid", cJSON_CreateBool(stats->NOXs_data[i].status.is_O2_value_valid));
			cJSON_AddStringToObject(NOx_status, "heater_mode_state", Heater_mode_str[stats->NOXs_data[i].status.heater_mode_state]);
			//Add errors for curr_NOx_data
			cJSON_AddItemToObject(curr_NOx_data, "errors", NOx_errors = cJSON_CreateObject());
			cJSON_AddStringToObject(NOx_errors, "heater", Errors_dec_str[stats->NOXs_data[i].errors.heater]);
			cJSON_AddStringToObject(NOx_errors, "NOx", Errors_dec_str[stats->NOXs_data[i].errors.NOx]);
			cJSON_AddStringToObject(NOx_errors, "O2", Errors_dec_str[stats->NOXs_data[i].errors.O2]);
		}
		else
			cJSON_AddItemToArray(NOx_array, cJSON_CreateObject());//Add empty object to NOx_array.
	}
	//Print JSON to File
	JSON_str = cJSON_PrintUnformatted(root);
	pFile = fopen(logstat_path_and_name, "w");
	if(pFile)
	{
		fputs(JSON_str, pFile);
		fclose (pFile);
		if(write_error)
			fprintf(stderr,"Write error @ Statlog file, Restored\n");
		write_error = 0;
	}
	else if(!write_error)
	{
		fprintf(stderr,"Write error @ Statlog file!!!\n");
		write_error = -1;
	}
	cJSON_Delete(root);
	free(JSON_str);
	free(logstat_path_and_name);
	return 0;
}
//Delete logstat file for NOX_handler
int delete_logstat_NOX(char *logstat_path, void *stats_arg)
{
	if(!logstat_path || !stats_arg)
		return -1;
	int ret_val;
	char *logstat_path_and_name, *slash;
	struct Morfeas_NOX_if_stats *stats = stats_arg;

	logstat_path_and_name = (char *) malloc(sizeof(char) * strlen(logstat_path) + strlen(stats->CAN_IF_name) + strlen("/logstat_NOXs_.json") + 1);
	slash = logstat_path[strlen(logstat_path)-1] == '/' ? "" : "/";
	sprintf(logstat_path_and_name,"%s%slogstat_NOXs_%s.json",logstat_path, slash, stats->CAN_IF_name);
	//Delete logstat file
	ret_val = unlink(logstat_path_and_name);
	free(logstat_path_and_name);
	return ret_val;
}
