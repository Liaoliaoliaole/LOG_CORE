/*
File: Morfeas_IPC.h, Declaration of functions, structs and union for IPC.
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
#define OK_status 0
#define REG 0
#define UNREG 1
#define Dev_or_Bus_name_str_size IFNAMSIZ
#define Data_FIFO "/tmp/.Morfeas_handlers_FIFO"

#include <net/if.h>

#include "../Morfeas_Types.h"

enum Morfeas_IPC_msg_type{
	IPC_Handler_register = 1,
	IPC_Handler_unregister,
	//SDAQ_related IPC messages
	IPC_SDAQ_register_or_update,
	IPC_SDAQ_CAN_BUS_info,
	IPC_SDAQ_clean_up,
	IPC_SDAQ_info,
	IPC_SDAQ_inpMode,
	IPC_SDAQ_cal_date,
	IPC_SDAQ_timediff,
	IPC_SDAQ_meas,
	//IOBOX_related IPC messages
	IPC_IOBOX_data,
	IPC_IOBOX_channels_reg,
	IPC_IOBOX_report,
	//MDAQ_related IPC messages
	IPC_MDAQ_data,
	IPC_MDAQ_channels_reg,
	IPC_MDAQ_report,
	//MTI_related IPC messages
	IPC_MTI_Tele_data,
	IPC_MTI_RMSW_MUX_data,
	IPC_MTI_tree_reg,
	IPC_MTI_Update_Health,
	IPC_MTI_Update_Radio,
	IPC_MTI_report,
	//NOX_related IPC messages
	IPC_NOX_data,
	IPC_NOX_CAN_BUS_info,
	/*
	//CPAD_related IPC messages
	*/
	//Set MIN/MAX_num_type, (Min and Max for each IPC_handler_type)
	//---SDAQ---//
	Morfeas_IPC_SDAQ_MIN_type = IPC_SDAQ_register_or_update,
	Morfeas_IPC_SDAQ_MAX_type = IPC_SDAQ_meas,
	//---IO-BOX---//
	Morfeas_IPC_IOBOX_MIN_type = IPC_IOBOX_data,
	Morfeas_IPC_IOBOX_MAX_type = IPC_IOBOX_report,
	//---MDAQ---//
	Morfeas_IPC_MDAQ_MIN_type = IPC_MDAQ_data,
	Morfeas_IPC_MDAQ_MAX_type = IPC_MDAQ_report,
	//---MTI---//
	Morfeas_IPC_MTI_MIN_type = IPC_MTI_Tele_data,
	Morfeas_IPC_MTI_MAX_type = IPC_MTI_report,
	//---NOX---//
	Morfeas_IPC_NOX_MIN_type = IPC_NOX_data,
	Morfeas_IPC_NOX_MAX_type = IPC_NOX_CAN_BUS_info,
	/*
	//---CPAD---//
	Morfeas_IPC_CPAD_MIN_type = ,
	Morfeas_IPC_CPAD_MAX_type = ,
	*/
	//MAX number of any type of IPC message
	Morfeas_IPC_MAX_type = Morfeas_IPC_NOX_MAX_type //Morfeas_IPC_CPAD_MAX_type
};

enum Morfeas_IPC_handler_type{
	SDAQ = 0,
	MDAQ,
	IOBOX,
	MTI, //Mobile Telemetry Interface
	NOX, //UNI-NOx
	CPAD
};

#pragma pack(push, 1)//use pragma pack() to pack the following structs to 1 byte size (aka no zero padding)
  //---Bus Handlers related---//
typedef struct Handler_register_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned char handler_type;
}Handler_reg_op_msg;

  //--- SDAQnet Port related ---//
typedef struct CAN_BUS_info_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	float BUS_utilization;
	float Bus_error_rate;
	unsigned char Electrics;//Boolean: false -> No Electrics info
	float amperage;
	float voltage;
	float shunt_temp;
}CAN_BUS_info_msg;

  //------ SDAQ related ------//
typedef struct SDAQ_register_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned char address;
	sdaq_status SDAQ_status;
	unsigned char reg_status;
	unsigned char t_amount;
}SDAQ_reg_update_msg;

typedef struct SDAQ_clean_registeration_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	unsigned char t_amount;
}SDAQ_clear_msg;

typedef struct SDAQ_info_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	sdaq_info SDAQ_info_data;
}SDAQ_info_msg;

typedef struct SDAQ_input_mode_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	unsigned char Dev_type;
	unsigned char Input_mode;
}SDAQ_inpMode_msg;

typedef struct SDAQ_cal_date_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	unsigned char channel;
	sdaq_calibration_date SDAQ_cal_date;
}SDAQ_cal_date_msg;

typedef struct SDAQ_timediff_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	short Timediff;
}SDAQ_timediff_msg;

typedef struct SDAQ_measure_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int SDAQ_serial_number;
	unsigned char Amount_of_channels;
	unsigned short Last_Timestamp;
	struct Channel_curr_meas SDAQ_channel_meas[SDAQ_MAX_AMOUNT_OF_CHANNELS];
}SDAQ_meas_msg;

	//------ IO-BOX related ------//
typedef struct IOBOX_data_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int IOBOX_IPv4;
	float Supply_Vin;
	struct IOBOX_Power_Supply Supply_meas[IOBOX_Amount_of_STD_RXs];
	struct IOBOX_RXs RX[IOBOX_Amount_of_All_RXs];
}IOBOX_data_msg;

typedef struct IOBOX_channels_reg_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int IOBOX_IPv4;
}IOBOX_channels_reg_msg;

typedef struct IOBOX_report_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int IOBOX_IPv4;
	int status;
}IOBOX_report_msg;

	//------ MDAQ related ------//
typedef struct MDAQ_data_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MDAQ_IPv4;
	unsigned int meas_index;
	float board_temp;
	struct MDAQ_Channel meas[MDAQ_Amount_of_Channels];
}MDAQ_data_msg;

typedef struct MDAQ_channels_reg_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MDAQ_IPv4;
}MDAQ_channels_reg_msg;

typedef struct MDAQ_report_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MDAQ_IPv4;
	int status;
}MDAQ_report_msg;

	//------ MTI related ------//
typedef struct MTI_tree_reg_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MTI_IPv4;
}MTI_tree_reg_msg;

typedef struct MTI_report_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MTI_IPv4;
	unsigned Tele_dev_type:3;
	unsigned char amount_of_Linkable_tele;
	unsigned char IDs_of_MiniRMSWs[MAX_RMSW_DEVs];
	int status;
}MTI_report_msg;

typedef struct MTI_Update_Health_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	float cpu_temp;
	float batt_capacity;
	float batt_voltage;
	unsigned char batt_state;
}MTI_Update_Health_msg;

typedef struct MTI_Update_Radio_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MTI_IPv4;
	unsigned char RF_channel;
	unsigned Data_rate:2;
	unsigned Tele_dev_type:3;
	union MTI_specific_regs sRegs;
	unsigned isNew_config:1;
}MTI_Update_Radio_msg;

typedef struct MTI_tele_data_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MTI_IPv4;
	unsigned Tele_dev_type:3;
	union MTI_Tele_data_union{
		struct TC4_data_struct as_TC4;
		struct TC8_data_struct as_TC8;
		struct TC16_data_struct as_TC16;
		struct QUAD_data_struct as_QUAD;
	}data;
}MTI_tele_data_msg;

typedef struct MTI_RMSW_MUX_data_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned int MTI_IPv4;
	struct RMSW_MUX_Devs_data_struct Devs_data;
}MTI_RMSW_MUX_data_msg;

	//------ NOX related ------//
typedef struct NOX_data_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	unsigned char sensor_addr;
	struct UniNOx_sensor NOXs_data;
}NOX_data_msg;

typedef struct NOXs_info_msg_struct{
	unsigned char IPC_msg_type;
	char Dev_or_Bus_name[Dev_or_Bus_name_str_size];
	float BUS_utilization;
	float Bus_error_rate;
	unsigned char Electrics;//Boolean: false -> No Electrics info
	float amperage;
	float voltage;
	float shunt_temp;
	unsigned char Dev_on_bus;
	auto_switch_off_var auto_switch_off_value;
	auto_switch_off_var auto_switch_off_cnt;
	unsigned char active_devs[2];
}NOXs_info_msg;
#pragma pack(pop)//Disable packing

//--IPC_MESSAGE--//
typedef union Morfeas_IPC_msg_union{
	Handler_reg_op_msg Handler_reg;
	//SDAQ + CANBus related
	SDAQ_reg_update_msg SDAQ_reg_update;
	SDAQ_clear_msg SDAQ_clean;
	SDAQ_info_msg SDAQ_info;
	SDAQ_inpMode_msg SDAQ_inpMode;
	SDAQ_cal_date_msg SDAQ_cal_date;
	SDAQ_timediff_msg SDAQ_timediff;
	SDAQ_meas_msg SDAQ_meas;
	CAN_BUS_info_msg SDAQ_BUS_info;
	//IO-BOX related
	IOBOX_data_msg IOBOX_data;
	IOBOX_channels_reg_msg IOBOX_channels_reg;
	IOBOX_report_msg IOBOX_report;
	//MDAQ related
	MDAQ_data_msg MDAQ_data;
	MDAQ_channels_reg_msg MDAQ_channels_reg;
	MDAQ_report_msg MDAQ_report;
	//MTI related
	MTI_tree_reg_msg MTI_tree_reg;
	MTI_report_msg MTI_report;
	MTI_Update_Health_msg MTI_Update_Health;
	MTI_Update_Radio_msg MTI_Update_Radio;
	MTI_tele_data_msg MTI_tele_data;
	MTI_RMSW_MUX_data_msg MTI_RMSW_MUX_data;
	//NOX related
	NOX_data_msg NOX_data;
	NOXs_info_msg NOX_BUS_info;
}IPC_message;

//Function that convert interface_type_string to interface_type_num
int if_type_str_2_num(const char * if_type_str);

	//----RX Functions----//
//function for RX, return the type of the received message or 0 in failure
unsigned char IPC_msg_RX(int FIFO_fd, IPC_message *IPC_msg_ptr);

	//----TX Functions----//
//function for TX, return the amount of bytes that transmitted through the FIFO, or 0 in failure
size_t IPC_msg_TX(int FIFO_fd, IPC_message *IPC_msg_ptr);
//Function for construction of message for registration of a Handler
size_t IPC_Handler_reg_op(int FIFO_fd, unsigned char handler_type, char *Dev_or_Bus_name, unsigned char unreg);

