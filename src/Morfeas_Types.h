/*
File "Morfeas_Types.h" part of Morfeas project, contain the shared Datatypes.
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
#define ISO_channel_name_size 20

//Default MODBus Slave address
#define default_slave_address 10

//Defs for IOBOX_handler
#define IOBOX_Max_reg_read 124
#define IOBOX_start_reg 0
#define IOBOX_imp_reg 176
#define IOBOX_Amount_of_STD_RXs 4
#define IOBOX_Amount_of_Extra_RXs 2
#define IOBOX_Amount_of_All_RXs (IOBOX_Amount_of_STD_RXs + IOBOX_Amount_of_Extra_RXs)
#define IOBOX_Amount_of_channels 16
#define IOBOX_RXs_mem_offset 25
#define IOBOX_Index_reg_pos 20
#define IOBOX_Status_reg_pos 21
#define IOBOX_Success_reg_pos 22

//Defs for MDAQ_handler
#define MDAQ_start_reg 100
#define MDAQ_imp_reg 90

//Defs for MTI_handler
#define MAX_RMSW_DEVs 32

//Defs for MDAQ_handler
#define MDAQ_Amount_of_Channels 8

#include <gmodule.h>
#include <glib.h>

//Include SDAQ Driver header
#include "sdaq-worker/src/SDAQ_drv.h"
//Include MTI data type header
#include "Morfeas_MTI/MTI_Types.h"
//Include NOX data type header
#include "Morfeas_NOX/NOX_Types.h"

enum status_byte_enum{
	Okay = 0,
	Tele_channel_noSensor,
	Tele_channel_Error=8,
	UniNOx_notMeas,
	UniNOx_notInTemp,
	NOX_notValid,
	O2_notValid,
	Connection_timed_out = 110,
	Disconnected = 127,
	OFF_line = -1
};

//Array with strings of the Supported Interface_names.
extern const char *Morfeas_IPC_handler_type_name[];
//Arrays with MTI related strings
extern const char *MTI_charger_state_str[];
extern const char *MTI_Data_rate_str[];
extern const char *MTI_Tele_dev_type_str[];
extern const char *MTI_RM_dev_type_str[];
extern const char *MTI_RMSW_SW_names[];
extern const char *MTI_MUX_Sel_names[];

/*Structs for IOBOX_handler*/
struct IOBOX_Power_Supply{
	float Vout,Iout;
};
struct IOBOX_RXs{
	float CH_value[IOBOX_Amount_of_channels];
	unsigned short index;
	unsigned char status;
	unsigned char success;
};
struct Morfeas_IOBOX_if_stats{
	char *IOBOX_IPv4_addr;
	char *dev_name;
	int error;
	float Supply_Vin;
	struct IOBOX_Power_Supply Supply_meas[IOBOX_Amount_of_STD_RXs];
	struct IOBOX_RXs RX[IOBOX_Amount_of_All_RXs];
	unsigned int counter;
};

/*Structs for MDAQ_handler*/
struct MDAQ_Channel{
	float value[4];
	unsigned char warnings;
};
struct Morfeas_MDAQ_if_stats{
	char *MDAQ_IPv4_addr;
	char *dev_name;
	int error;
	unsigned int meas_index;
	float board_temp;
	struct MDAQ_Channel meas[8];
	unsigned int counter;
};

/*Structs for MTI_handler*/
struct MTI_status_struct{
	float MTI_batt_volt;
	float MTI_batt_capacity;
	unsigned char MTI_charge_status;
	float MTI_CPU_temp;
	struct{
		unsigned char pb1:1;
		unsigned char pb2:1;
		unsigned char pb3:1;
	}buttons_state;
	float PWM_gen_out_freq;
	float PWM_outDuty_CHs[4];
};
union MTI_specific_regs{
	struct dec_for_temperature_telemetries{
		unsigned char StV;//Sample to set valid flag
		unsigned char StF;//samples to reset valid flag
	}__attribute__((packed, aligned(1))) for_temp_tele;
	struct dec_for_controlling_devices{
		unsigned G_SW:1;
		unsigned G_SL:1;
		unsigned res_1:6;
		unsigned G_P_state:1;
		unsigned G_S_state:1;
		unsigned res_2:6;
	}__attribute__((packed, aligned(1))) for_rmsw_dev;
	unsigned char as_array[2];
	unsigned short as_short;
};
struct MTI_Radio_config_struct{
	unsigned RF_channel:7;
	unsigned Data_rate:2;
	unsigned Tele_dev_type:3;
	union MTI_specific_regs sRegs;
};
//Structs for MTI related telemetry device
struct TC4_data_struct{
	unsigned short packet_index;
	unsigned RX_status:2;
	unsigned char RX_Success_ratio;
	unsigned Data_isValid:1;
	float CHs[4];
	float Refs[2];
};
struct TC8_data_struct{
	unsigned short packet_index;
	unsigned RX_status:2;
	unsigned char RX_Success_ratio;
	unsigned Data_isValid:1;
	float CHs[8];
	float Refs[8];
};
struct TC16_data_struct{
	unsigned short packet_index;
	unsigned RX_status:2;
	unsigned char RX_Success_ratio;
	unsigned Data_isValid:1;
	float CHs[16];
};
struct Gen_config_struct{
	float scaler;
	unsigned int max;
	unsigned int min;
	union generator_mode{
		struct decoder_for_generator_mode{
			unsigned saturation:1;
			unsigned mid_val_use:1;
			unsigned reserved:5;
			unsigned fixed_freq:1;
		}__attribute__((packed, aligned(1))) dec;
		unsigned char as_byte;
	}pwm_mode;
}__attribute__((packed, aligned(1)));
struct QUAD_data_struct{
	unsigned short packet_index;
	unsigned RX_status:2;
	unsigned char RX_Success_ratio;
	unsigned Data_isValid:1;
	struct Gen_config_struct gen_config[Amount_OF_GENS];
	int CNTs[2];
	float CHs[2];
};

union switch_status_dec{
	struct rmsw_switches_decoder{
		unsigned Main:1;
		unsigned CH1:1;
		unsigned CH2:1;
		unsigned reserved:4;
		unsigned Rep_rate:1;
	}__attribute__((packed, aligned(1))) rmsw_dec;
	struct mux_switches_decoder{
		unsigned CH1:1;
		unsigned CH2:1;
		unsigned CH3:1;
		unsigned CH4:1;
		unsigned reserved:3;
		unsigned Rep_rate:1;
	}__attribute__((packed, aligned(1))) mux_dec;
	struct mini_rmsw_switches_decoder{
		unsigned Main:1;
		unsigned reserved:5;
		unsigned Rep_rate:2;
	}__attribute__((packed, aligned(1))) mini_dec;
	unsigned char as_byte;
};
struct RMSW_MUX_Mini_data_struct{
	unsigned char pos_offset;
	unsigned dev_type:2;
	unsigned short dev_id;
	unsigned char time_from_last_mesg;
	union switch_status_dec switch_status;
	float dev_temp;
	float input_voltage;
	float meas_data[4];
};
struct RMSW_MUX_Devs_data_struct{
	unsigned char amount_of_devices;
	unsigned char amount_to_be_remove;
	unsigned char IDs_to_be_removed[MAX_RMSW_DEVs];
	struct RMSW_MUX_Mini_data_struct det_devs_data[MAX_RMSW_DEVs];
};

typedef struct MTI_stored_config_struct{
	unsigned char RF_channel;
	unsigned char Tele_dev_type;
	union MTI_specific_regs sRegs;
	struct Gen_config_struct gen_config[Amount_OF_GENS];
}__attribute__((packed, aligned(1))) MTI_stored_config;

//Morfeas_MTI_if_stats stats struct, used in Morfeas_MTI_if
struct Morfeas_MTI_if_stats{
	char *MTI_IPv4_addr;
	char *dev_name;
	int error;
	MTI_stored_config user_config;
	struct MTI_status_struct MTI_status;
	struct MTI_Radio_config_struct MTI_Radio_config;
	union MTI_Telemetry_data{
		struct TC4_data_struct as_TC4;
		struct TC8_data_struct as_TC8;
		struct TC16_data_struct as_TC16;
		struct QUAD_data_struct as_QUAD;
		struct RMSW_MUX_Devs_data_struct as_RMSWs;
	}Tele_data;
	unsigned int counter;
};

/*Structs and typedefs for NOX_handler*/
struct UniNOx_sensor{
	float NOx_value;
	float O2_value;
	time_t last_seen;
	unsigned meas_state : 1;
	struct NOx_sensor_status{
		unsigned supply_in_range : 1;
		unsigned in_temperature : 1;
		unsigned is_NOx_value_valid : 1;
		unsigned is_O2_value_valid : 1;
		unsigned heater_mode_state : 2;
	} status;
	struct NOx_sensor_errors{
		unsigned heater : 2;
		unsigned NOx : 2;
		unsigned O2 : 2;
	} errors;
};
//Morfeas_NOX-if stats struct, used in Morfeas_NOX_if
enum UniNOx_sensor_value_enum{
	NOx_val = 1,
	O2_val
};
typedef unsigned short auto_switch_off_var;
struct Morfeas_NOX_if_stats{
	int FIFO_fd;
	char *CAN_IF_name;
	unsigned char port;
	float Bus_util;
	float Bus_error_rate;
	unsigned short ws_port;
	//Electrics and last calibration date for Morfeas_Rpi_hat
	unsigned int Morfeas_RPi_Hat_last_cal;
	float Bus_voltage;
	float Bus_amperage;
	float Shunt_temp;
	auto_switch_off_var auto_switch_off_value;
	auto_switch_off_var auto_switch_off_cnt;
	struct UniNOx_sensor NOXs_data[2];
	struct NOx_values_avg_struct{
		unsigned char NOx_value_sample_cnt;
		float NOx_value_acc;
		float NOx_value_min;
		float NOx_value_max;
		unsigned char O2_value_sample_cnt;
		float O2_value_acc;
		float O2_value_min;
		float O2_value_max;
	} NOx_statistics[2];
	unsigned char dev_msg_cnt[2];
};

/*Structs for SDAQ_handler*/
//Morfeas_SDAQ-if stats struct, used in Morfeas_SDAQ_if
struct Morfeas_SDAQ_if_stats{
	char LogBook_file_path[100];
	int FIFO_fd;
	char *CAN_IF_name;
	unsigned char port;
	float Bus_util;
	float Bus_error_rate;
	//Electrics and last calibration date for Morfeas_Rpi_hat
	unsigned int Morfeas_RPi_Hat_last_cal;
	float Bus_voltage;
	float Bus_amperage;
	float Shunt_temp;
	unsigned char detected_SDAQs;// Amount of online SDAQ.
	unsigned char incomplete_SDAQs;// Amount of incomplete SDAQ.
	GSList *list_SDAQs;// List with SDAQ status, info and last seen timestamp.
	GSList *LogBook;//List of the LogBook file
};
// Data of a current_measurements node
struct Channel_curr_meas{
	float meas;
	unsigned char unit;
	unsigned char status;
};
// Data of a list_SDAQs node, used in Morfeas_SDAQ_if
struct SDAQ_info_entry{
	unsigned char SDAQ_address;
	short Timediff;
	unsigned short Last_Timestamp;
	sdaq_status SDAQ_status;
	sdaq_info SDAQ_info;
	unsigned char inp_mode;
	GSList *SDAQ_Channels_cal_dates;
	GSList *SDAQ_Channels_acc_meas;
	struct Channel_curr_meas *SDAQ_Channels_curr_meas;
	time_t last_seen;
	unsigned failed_reg_RX_CNT;
	unsigned reg_status:3;
};
// Data of a SDAQ_Channels_cal_dates node
struct Channel_date_entry{
	unsigned char Channel;
	sdaq_calibration_date CH_date;
};
// Data of a SDAQ_Channels_acc_meas node
struct Channel_acc_meas_entry{
	unsigned char Channel;
	unsigned char status;
	unsigned char unit_code;
	float last_meas;
	float meas_acc;
	float meas_min;
	float meas_max;
	unsigned short cnt;
};
// Data entry of a LogBook file, used in Morfeas_SDAQ_if
struct LogBook_entry{
	unsigned int SDAQ_sn;
	unsigned char SDAQ_address;
}__attribute__((packed, aligned(1)));
// struct of LogBook entry and it's Checksum, used in Morfeas_SDAQ_if
struct LogBook{
	struct LogBook_entry payload;
	unsigned char checksum;
}__attribute__((packed, aligned(1)));
//Data of the List Links, used in Morfeas_opc_ua
struct Link_entry{
	char ISO_channel_name[ISO_channel_name_size];
	unsigned char interface_type_num;
	unsigned int identifier;
	char *CAN_IF_name;//Used with NOX and CPAD handlers.
	unsigned char channel; //Address on NOX
	unsigned char rxNum_teleType_or_value;
	unsigned char tele_ID;
};
/*struct for system_stats*/
struct system_stats{
	float CPU_Util,RAM_Util,CPU_temp,Disk_Util;
	unsigned int Up_time;
};
