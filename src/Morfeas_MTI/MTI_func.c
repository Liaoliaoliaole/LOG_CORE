/*
File: MTI_func.c, Implementation of functions for MTI (MODBus), Part of Morfeas_project.
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
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <modbus.h>

#include "../Morfeas_Types.h"

const char *MTI_charger_state_str[]={"Discharging", "Full", "", "Charging"};
const char *MTI_Data_rate_str[]={"250kbps", "1Mbps", "2Mbps"};
const char *MTI_Tele_dev_type_str[]={"Disabled", "Disabled", "TC16", "TC8", "RMSW/MUX", "QUAD", "TC4", NULL};
const char *MTI_RM_dev_type_str[]={"", "RMSW", "MUX", "Mini_RMSW", NULL};
const char *MTI_RMSW_SW_names[]={"Main_SW","SW_1","SW_2"};
const char *MTI_MUX_Sel_names[]={"Sel_1","Sel_2","Sel_3","Sel_4"};


int get_MTI_status(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats)
{
	struct MTI_dev_status cur_status;

	if(modbus_read_input_registers(ctx, MTI_STATUS_OFFSET, sizeof(cur_status)/sizeof(short), (unsigned short*)&cur_status)<=0)
	{
		stats->error = errno;
		return EXIT_FAILURE;
	}
	//Convert and load MTI_status to stats struct
	stats->MTI_status.MTI_batt_volt = cur_status.batt_volt;
	stats->MTI_status.MTI_batt_capacity = cur_status.batt_cap;
	stats->MTI_status.MTI_charge_status = (unsigned char)cur_status.batt_state;
	stats->MTI_status.MTI_CPU_temp = cur_status.CPU_temp;
	stats->MTI_status.buttons_state.pb1 = cur_status.Button_state==4.0;
	stats->MTI_status.buttons_state.pb2 = cur_status.Button_state==2.0;
	stats->MTI_status.buttons_state.pb3 = cur_status.Button_state==1.0;
	stats->MTI_status.PWM_gen_out_freq = cur_status.PWM_freq;
	for(unsigned char i=0; i<4;i++)
		stats->MTI_status.PWM_outDuty_CHs[i] = cur_status.PWM_Channels[i];
	return EXIT_SUCCESS;
}

int get_MTI_Radio_config(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats)
{
	struct MTI_RTX_config_struct cur_RX_config;

	if(modbus_read_registers(ctx, MTI_CONFIG_OFFSET, sizeof(cur_RX_config)/sizeof(short), (unsigned short*)&cur_RX_config)<=0)
	{
		stats->error = errno;
		return EXIT_FAILURE;
	}
	//Sanitization of status values
	cur_RX_config.RF_channel = cur_RX_config.RF_channel>127?0:cur_RX_config.RF_channel;//Sanitize Radio channel frequency.
	cur_RX_config.Tele_dev_type = cur_RX_config.Tele_dev_type>Tele_TC4||cur_RX_config.Tele_dev_type<Tele_TC16?0:cur_RX_config.Tele_dev_type;//Sanitize Telemetry device type
	//Convert and load values to stats struct
	stats->MTI_Radio_config.RF_channel = cur_RX_config.RF_channel;
	stats->MTI_Radio_config.Data_rate = cur_RX_config.Data_rate;
	stats->MTI_Radio_config.Tele_dev_type = cur_RX_config.Tele_dev_type;
	switch(cur_RX_config.Tele_dev_type)
	{
		case Tele_TC8:
		case Tele_TC16:
		case Tele_TC4:
			if(cur_RX_config.Specific_reg[0]==49)//Check if Message validation is enable. From MTI's Documentation.
			{
				stats->MTI_Radio_config.sRegs.for_temp_tele.StV = cur_RX_config.Specific_reg[1];
				stats->MTI_Radio_config.sRegs.for_temp_tele.StF = cur_RX_config.Specific_reg[2];
			}
			else
				for(int i=0; i<sizeof(stats->MTI_Radio_config.sRegs.as_array);i++)
					stats->MTI_Radio_config.sRegs.as_array[i]=0;
			break;
		case RMSW_MUX:
			stats->MTI_Radio_config.sRegs.as_array[0] = cur_RX_config.Specific_reg[0];
			stats->MTI_Radio_config.sRegs.as_array[1] = cur_RX_config.Specific_reg[21];
			stats->MTI_Radio_config.sRegs.for_rmsw_dev.res_1 = 0;
			stats->MTI_Radio_config.sRegs.for_rmsw_dev.res_2 = 0;//reset reserved bits
			break;
	}
	return EXIT_SUCCESS;
}

int get_MTI_Tele_data(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats)
{
	int remain_words, i, j, pos;
	struct MTI_PWM_config_struct Pulse_gen_conf;
	union MTI_Tele_data_union{
		struct MTI_16_temp_tele as_TC16;
		struct MTI_4_temp_tele as_TC4;
		struct MTI_quad_tele as_QUAD;
		struct MTI_mux_rmsw_tele as_MUXs_RMSWs[32];
	}cur_MTI_Tele_data;

	switch(stats->MTI_Radio_config.Tele_dev_type)
	{
		case Tele_TC8:
		case Tele_TC16:
			if(modbus_read_input_registers(ctx, MTI_TELE_DATA_OFFSET, sizeof(cur_MTI_Tele_data.as_TC16)/sizeof(short), (unsigned short*)&cur_MTI_Tele_data)<=0)
			{
				stats->error = errno;
				return EXIT_FAILURE;
			}
			stats->Tele_data.as_TC16.packet_index = cur_MTI_Tele_data.as_TC16.index;
			stats->Tele_data.as_TC16.RX_status = cur_MTI_Tele_data.as_TC16.rx_status;
			stats->Tele_data.as_TC16.RX_Success_ratio = cur_MTI_Tele_data.as_TC16.success;
			stats->Tele_data.as_TC16.Data_isValid = cur_MTI_Tele_data.as_TC16.valid_data;
			memcpy(&(stats->Tele_data.as_TC16.CHs), &(cur_MTI_Tele_data.as_TC16.channels), sizeof(stats->Tele_data.as_TC16.CHs));
			break;
		case Tele_TC4:
			if(modbus_read_input_registers(ctx, MTI_TELE_DATA_OFFSET, sizeof(cur_MTI_Tele_data.as_TC4)/sizeof(short), (unsigned short*)&cur_MTI_Tele_data)<=0)
			{
				stats->error = errno;
				return EXIT_FAILURE;
			}
			stats->Tele_data.as_TC4.packet_index = cur_MTI_Tele_data.as_TC4.index;
			stats->Tele_data.as_TC4.RX_status = cur_MTI_Tele_data.as_TC4.rx_status;
			stats->Tele_data.as_TC4.RX_Success_ratio = cur_MTI_Tele_data.as_TC4.success;
			stats->Tele_data.as_TC4.Data_isValid = cur_MTI_Tele_data.as_TC4.valid_data;
			memcpy(&(stats->Tele_data.as_TC4.CHs), &(cur_MTI_Tele_data.as_TC4.channels), sizeof(cur_MTI_Tele_data.as_TC4.channels));
			break;
		case Tele_quad:
			//Get Quadrature telemetry data
			if(modbus_read_input_registers(ctx, MTI_2CH_QUAD_DATA_OFFSET, sizeof(cur_MTI_Tele_data.as_QUAD)/sizeof(short), (unsigned short*)&cur_MTI_Tele_data)<=0)
			{
				stats->error = errno;
				return EXIT_FAILURE;
			}
			//Convert data and load them to stats
			stats->Tele_data.as_QUAD.Data_isValid = (unsigned short)cur_MTI_Tele_data.as_QUAD.index!=stats->Tele_data.as_QUAD.packet_index?1:0;
			stats->Tele_data.as_QUAD.packet_index = cur_MTI_Tele_data.as_QUAD.index;
			stats->Tele_data.as_QUAD.RX_status = cur_MTI_Tele_data.as_QUAD.rx_status;
			stats->Tele_data.as_QUAD.RX_Success_ratio = cur_MTI_Tele_data.as_QUAD.success;
			stats->Tele_data.as_QUAD.CNTs[0] = *cur_MTI_Tele_data.as_QUAD.Channel_1;
			stats->Tele_data.as_QUAD.CNTs[1] = *cur_MTI_Tele_data.as_QUAD.Channel_2;
			for(i=0; i<2; i++)
			{
				//Load scalers from User_config
				stats->Tele_data.as_QUAD.gen_config[i].scaler = stats->user_config.gen_config[i].scaler;
				stats->Tele_data.as_QUAD.CHs[i] = stats->Tele_data.as_QUAD.CNTs[i] * stats->Tele_data.as_QUAD.gen_config[i].scaler;
			}
			//Get Pulse Generators Configuration
			if(modbus_read_registers(ctx, MTI_PULSE_GEN_OFFSET, sizeof(Pulse_gen_conf)/sizeof(short), (unsigned short*)&Pulse_gen_conf)<=0)
			{
				stats->error = errno;
				return EXIT_FAILURE;
			}
			//Convert pulse generators config and load it to stats
			for(i=0; i<sizeof(stats->Tele_data.as_QUAD.gen_config)/sizeof(stats->Tele_data.as_QUAD.gen_config[0]); i++)
			{
				stats->Tele_data.as_QUAD.gen_config[i].max = Pulse_gen_conf.CHs[i].cnt_max;
				stats->Tele_data.as_QUAD.gen_config[i].min = Pulse_gen_conf.CHs[i].cnt_min;
				stats->Tele_data.as_QUAD.gen_config[i].pwm_mode.as_byte = Pulse_gen_conf.CHs[i].cnt_mode;
			}
			break;
		case RMSW_MUX:
			//Loop that Getting The Remote controlling devices data, and store them to the cur_MTI_Tele_data struct.
			for(i=0, remain_words = sizeof(cur_MTI_Tele_data.as_MUXs_RMSWs)/(sizeof(short)); remain_words>0; remain_words -= MTI_MODBUS_MAX_READ_REGISTERS, i++)
			{
				if(modbus_read_input_registers(ctx, i*MTI_MODBUS_MAX_READ_REGISTERS + MTI_RMSWs_DATA_OFFSET,
											  (remain_words>MTI_MODBUS_MAX_READ_REGISTERS?MTI_MODBUS_MAX_READ_REGISTERS:remain_words),
											  ((unsigned short*)&cur_MTI_Tele_data)+i*MTI_MODBUS_MAX_READ_REGISTERS)<=0)
				{
					stats->error = errno;
					return EXIT_FAILURE;
				}
			}
			stats->Tele_data.as_RMSWs.amount_to_be_remove = 0;
			//Loop that detecting removed devices
			if(stats->Tele_data.as_RMSWs.amount_of_devices && stats->Tele_data.as_RMSWs.amount_of_devices <= MAX_RMSW_DEVs)
			{
				for(i=0; i<stats->Tele_data.as_RMSWs.amount_of_devices; i++)
				{
					for(j=0; j<MAX_RMSW_DEVs && stats->Tele_data.as_RMSWs.det_devs_data[i].dev_id != cur_MTI_Tele_data.as_MUXs_RMSWs[j].dev_id; j++);
					if(j==MAX_RMSW_DEVs)
					{
						stats->Tele_data.as_RMSWs.IDs_to_be_removed[stats->Tele_data.as_RMSWs.amount_to_be_remove] = stats->Tele_data.as_RMSWs.det_devs_data[i].dev_id;
						stats->Tele_data.as_RMSWs.amount_to_be_remove++;
					}
				}
			}
			//Loop that find the detected devices and load them to the stats.
			stats->Tele_data.as_RMSWs.amount_of_devices = 0;
			for(i=0, pos=0; i<MAX_RMSW_DEVs; i++)
			{
				if(cur_MTI_Tele_data.as_MUXs_RMSWs[i].dev_type)
				{
					stats->Tele_data.as_RMSWs.amount_of_devices++;
					//Convert and load data of the Detected controlling telemetry device
					stats->Tele_data.as_RMSWs.det_devs_data[pos].pos_offset = i;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].dev_type = cur_MTI_Tele_data.as_MUXs_RMSWs[i].dev_type;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].dev_id = cur_MTI_Tele_data.as_MUXs_RMSWs[i].dev_id;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].time_from_last_mesg = 120 - cur_MTI_Tele_data.as_MUXs_RMSWs[i].time_from_last_mesg;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].dev_temp = ((short)cur_MTI_Tele_data.as_MUXs_RMSWs[i].temp)/128.0;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].input_voltage = cur_MTI_Tele_data.as_MUXs_RMSWs[i].input_voltage/1000.0;
					stats->Tele_data.as_RMSWs.det_devs_data[pos].switch_status.as_byte = cur_MTI_Tele_data.as_MUXs_RMSWs[i].switch_status;
					switch(stats->Tele_data.as_RMSWs.det_devs_data[pos].dev_type)
					{
						case RMSW_2CH:
							for(int j=0;j<4; j+=2)
							{
								stats->Tele_data.as_RMSWs.det_devs_data[pos].meas_data[j] = cur_MTI_Tele_data.as_MUXs_RMSWs[i].meas_data[j]/1000.0;
								stats->Tele_data.as_RMSWs.det_devs_data[pos].meas_data[j+1] = cur_MTI_Tele_data.as_MUXs_RMSWs[i].meas_data[j+1]/1000.0;
							}
							break;
						case Mini_RMSW:
							for(int j=0;j<4; j++)
								stats->Tele_data.as_RMSWs.det_devs_data[pos].meas_data[j] = ((short)cur_MTI_Tele_data.as_MUXs_RMSWs[i].meas_data[j])/16.0;
							break;
					}
					pos++;
				}
			}
			break;
		default:
			return -EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

								//----- MTI write functions -----//
int set_MTI_Radio_config(modbus_t *ctx, unsigned char new_RF_CH, unsigned char new_mode, union MTI_specific_regs *new_sregs)
{
	struct MTI_RTX_config_struct new_Radio_config = {.RF_channel=new_RF_CH, .Tele_dev_type=new_mode};
	unsigned char amount = 3;//default amount of configuration registers
	//Disable MTI's Transceiver
	if(modbus_write_register(ctx, TRX_MODE_REG, Disabled)<=0)
		return errno;
	//Preparing specific registers for new MTI config
	switch(new_mode)
	{
		case Tele_quad:
			amount = 6;
			break;
		case Tele_TC4:
		case Tele_TC8:
		case Tele_TC16:
			if(new_sregs->for_temp_tele.StV == 0xff || new_sregs->for_temp_tele.StF == 0xff)//In case of both arg are -1, Disable the validation mechanism
				amount = 6;
			else if(new_sregs->for_temp_tele.StV && new_sregs->for_temp_tele.StF)
			{
				new_Radio_config.Specific_reg[0] = 49;//Enable the MTI's validation mechanism
				new_Radio_config.Specific_reg[1] = new_sregs->for_temp_tele.StV;
				new_Radio_config.Specific_reg[2] = new_sregs->for_temp_tele.StF;
				amount = 6;//Including configuration for MTI's validation mechanism
			}
			break;
		case RMSW_MUX:
			new_Radio_config.RF_channel = 0;//Remote controlling device always listening at channel 0 (2400MHz)
			new_Radio_config.Specific_reg[0] = new_sregs->as_array[0];//Device Specific Register 0, Global switches configuration
			amount = 6;//Including Global control register
			break;
	}
	if(modbus_write_registers(ctx, MTI_CONFIG_OFFSET, amount, (unsigned short*)&new_Radio_config)<=0)
		return errno;
	return EXIT_SUCCESS;
}

int set_MTI_Global_switches(modbus_t *ctx, bool global_power, bool global_sleep)
{
	unsigned short global_reg = global_power | global_sleep<<1;

	if(modbus_write_register(ctx, GLOBAL_SW_REG, global_reg)<=0)
		return errno;
	return EXIT_SUCCESS;
}

int ctrl_tele_switch(modbus_t *ctx, unsigned char mem_pos, unsigned char tele_type, unsigned char sw_name, bool new_state)
{
	struct MTI_RMSW_cnrl_mem{
		unsigned short MTI_tele_type;
		unsigned short Tele_dev_ID;
		unsigned short reserved;
		union state_register{
			union switch_status_dec enc;
			unsigned short as_short;
		}new_status;
	}mem;

	//Read tele_type and current switches states registers
	if(modbus_read_registers(ctx, MTI_RMSWs_DATA_OFFSET + mem_pos * RMSW_MEM_SIZE, sizeof(mem)/sizeof(short), (unsigned short*)&mem)<=0)
		return errno;
	if(mem.MTI_tele_type != tele_type)
		return MTI_TELE_MODE_ERROR;

	switch(tele_type)
	{
		case RMSW_2CH:
			mem.new_status.enc.rmsw_dec.reserved = 0;//Clean unused bits
			mem.new_status.enc.rmsw_dec.Rep_rate = 1;//Set rep_rate to high speed;
			switch(sw_name)
			{
				case Main_SW:
					mem.new_status.enc.rmsw_dec.Main = new_state; break;
				case SW_1:
					mem.new_status.enc.rmsw_dec.CH1 = new_state; break;
				case SW_2:
					mem.new_status.enc.rmsw_dec.CH2 = new_state; break;
				default:
					return EXIT_FAILURE;
			}
			break;
		case MUX:
			mem.new_status.enc.mux_dec.reserved = 0;//Clean unused bits
			mem.new_status.enc.mux_dec.Rep_rate = 1;//Set rep_rate to high speed;
			switch(sw_name)
			{
				case Sel_1:
					mem.new_status.enc.mux_dec.CH1 = new_state; break;
				case Sel_2:
					mem.new_status.enc.mux_dec.CH2 = new_state; break;
				case Sel_3:
					mem.new_status.enc.mux_dec.CH3 = new_state; break;
				case Sel_4:
					mem.new_status.enc.mux_dec.CH4 = new_state; break;
				default:
					return EXIT_FAILURE;
			}
			break;
		case Mini_RMSW:
			mem.new_status.enc.mini_dec.reserved = 0;//Clean unused bits
			mem.new_status.enc.mini_dec.Rep_rate = 1;//Set rep_rate to high speed;
			mem.new_status.enc.mini_dec.Main = new_state;
			break;
		default:
			return EXIT_FAILURE;
	}
	if(modbus_write_register(ctx, MTI_RMSWs_SWITCH_OFFSET + mem_pos * RMSW_MEM_SIZE, mem.new_status.as_short)<=0)
		return errno;
	return EXIT_SUCCESS;
}

int set_MTI_PWM_gens(modbus_t *ctx, struct Gen_config_struct *new_Config)
{
	struct MTI_PWM_config_struct new_PWM_config = {.PWM_out_freq=10000};
	for(int i=0; i<Amount_OF_GENS; i++)
	{
		new_PWM_config.CHs[i].cnt_max = new_Config[i].max;
		new_PWM_config.CHs[i].cnt_min = new_Config[i].min;
		new_PWM_config.CHs[i].middle_val = new_Config[i].min + (new_Config[i].max - new_Config[i].min)/2;
		new_PWM_config.CHs[i].cnt_mode = new_Config[i].pwm_mode.dec.saturation;
		new_PWM_config.CHs[i].cnt_mode |= 0x82;//Or Mask to set fixed_freq and mid_val_use
	}
	if(modbus_write_registers(ctx, MTI_PULSE_GEN_OFFSET, sizeof(new_PWM_config)/sizeof(short), (unsigned short*)&new_PWM_config)<=0)
		return errno;
	return EXIT_SUCCESS;
}

int MTI_set_user_config(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats)
{
	union MTI_specific_regs sRegs={.as_short = stats->user_config.sRegs.as_short};
	int ret=set_MTI_Radio_config(ctx, stats->user_config.RF_channel,
									  stats->user_config.Tele_dev_type,
									  &sRegs);
	if(!ret)
	{
		if(stats->user_config.Tele_dev_type == Tele_quad)
			ret=set_MTI_PWM_gens(ctx, stats->user_config.gen_config);
		else if(stats->user_config.Tele_dev_type==RMSW_MUX&&
			    stats->MTI_Radio_config.Tele_dev_type!=stats->user_config.Tele_dev_type)
			stats->Tele_data.as_RMSWs.amount_of_devices=0;
	}
	return ret;
}
