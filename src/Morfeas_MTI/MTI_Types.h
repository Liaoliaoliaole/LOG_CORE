/*
File: MTI_types.h, Contain the Declaration for data types that related to MTI, Part of Morfeas_project.
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
#include <modbus.h>

#define MTI_MODBUS_MAX_READ_REGISTERS (MODBUS_MAX_READ_REGISTERS-1) //Correction for the wrong MTI's MODBus implementation

#define MTI_TELE_MODE_ERROR MODBUS_ENOBASE-1

/*MTI's ModBus regions Offsets*/
#define RMSW_MEM_SIZE 10
//Holding registers region
#define TRX_MODE_REG 2
#define GLOBAL_SW_REG 24
#define MTI_RMSWs_SWITCH_OFFSET 28
#define MTI_CONFIG_OFFSET 0
#define MTI_PULSE_GEN_OFFSET 1050 //int registers
//Read registers region
#define MTI_RMSWs_DATA_OFFSET 25 //short registers
#define MTI_STATUS_OFFSET 2000 //float registers
#define MTI_TELE_DATA_OFFSET 2050 //float registers
#define MTI_2CH_QUAD_DATA_OFFSET 1050 //int registers

//Defined value that temperature telemetry sent if input is open (aka No sensor)
#define NO_SENSOR_VALUE 2000.0

//Define amount of Pulse Generators
#define Amount_OF_GENS 2

//--- Enumerators for MTI Telemetry types---//
enum MTI_Telemetry_Dev_type_enum{
	Disabled = 1,
	Tele_TC16,
	Tele_TC8,
	RMSW_MUX,
	Tele_quad,
	Tele_TC4,
	//Limits
	Dev_type_min = Tele_TC16,
	Dev_type_max = Tele_TC4,
};
//--- Enumerator for Controlling telemetry device ---//
enum MTI_Controlling_Dev_type_enum{
	RMSW_2CH = 1,
	MUX,
	Mini_RMSW,
	//Limits
	Tele_type_min = RMSW_2CH,
	Tele_type_max = Mini_RMSW,
};
//--- Enumerators for switch names ---//
//For RMSW_2CH
enum RMSW_switches_names{
	Main_SW,
	SW_1,
	SW_2
};
//For Multiplexers
enum MUX_switches_names{
	Sel_1,
	Sel_2,
	Sel_3,
	Sel_4,
};

//--- From MODBus Input Registers(Read only) 32001... ---//
struct MTI_dev_status{
	float batt_volt;
	float batt_cap;
	float batt_state;
	float CPU_temp;
	float Button_state;
	float PWM_clock;
	float PWM_freq;
	float PWM_Channels[4];
};
struct MTI_16_temp_tele{
	float index;
	float rx_status;
	float success;
	float valid_data;
	float valid_data_cnt;
	float reserved[5];
	float channels[16];
};
struct MTI_4_temp_tele{
	float index;
	float rx_status;
	float success;
	float valid_data;
	float valid_data_cnt;
	float reserved[5];
	float channels[6];
};
struct MTI_quad_tele{
	unsigned int index;
	unsigned int rx_status;
	unsigned int success;
	unsigned int samplerate;
	unsigned int drift_index;
	unsigned int reserved[5];
	int Channel_1[5];
	int Channel_2[5];
};
struct MTI_mux_rmsw_tele{
	unsigned short dev_type;
	unsigned short dev_id;
	unsigned short time_from_last_mesg;
	unsigned short switch_status;
	unsigned short temp;
	unsigned short input_voltage;
	unsigned short meas_data[4];
};

//--- From MODBus Holding Registers(R/W) 40001-40008 ---//
struct MTI_RTX_config_struct{
	unsigned short RF_channel;
	unsigned short Data_rate;
	unsigned short Tele_dev_type;
	unsigned short Specific_reg[22];
};

//--- From MODBus Holding Registers(R/W) 41051-41071 ---//
struct MTI_PWM_config_struct{
	unsigned int PWM_out_freq;
	int PLL_Control_high;
	int PLL_Control_low;
	struct PWM_Channels_control_struct{
		unsigned int cnt_max;
		unsigned int cnt_min;
		unsigned int middle_val;
		unsigned int cnt_mode;
	}CHs[2];
};

//Struct used to passing arguments to D-Bus listener
struct MTI_DBus_thread_arguments_passer
{
	modbus_t **ctx;
	struct Morfeas_MTI_if_stats *stats;
};
