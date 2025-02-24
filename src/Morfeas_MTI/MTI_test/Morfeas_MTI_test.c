/*
File: Morfeas_MTI_test.c, Implementation of Morfeas MTI (MODBus) handler, Part of Morfeas_project.
Copyright (C) 12019-12020  Sam harry Tzavaras

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
#define VERSION "1.0" /*Release Version of Morfeas_MTI_test*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <arpa/inet.h>

#include <modbus.h>

#include "../../IPC/Morfeas_IPC.h" //<- #include "Morfeas_Types.h"
#include "../../Supplementary/Morfeas_run_check.h"
#include "../../Supplementary/Morfeas_JSON.h"
#include "../../Supplementary/Morfeas_Logger.h"

//Global variables
static volatile unsigned char handler_run = 1;

//Print the Usage manual
void print_usage(char *prog_name);

//Signal Handler Function
static void stopHandler(int signum)
{
	if(signum == SIGPIPE)
		fprintf(stderr,"IPC: Force Termination!!!\n");
	handler_run = 0;
}

	//-- MTI Functions --//
//MTI function that request the MTI's status and load them to stats, return 0 on success
int get_MTI_status(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);
//MTI function that request the MTI's RX configuration. Load configuration status stats and return "telemetry type". 
int get_MTI_Radio_config(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);
//MTI function that request from MTI the telemetry data. Load this data to stats. Return 0 in success
int get_MTI_Tele_data(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);

int main(int argc, char *argv[])
{
	//MODBus related variables
	modbus_t *ctx;
	//Apps variables
	char *path_to_logstat_dir;
	struct Morfeas_MTI_if_stats stats = {0};
	
	//Check for call without arguments
	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	//Get options
	int c;
	while ((c = getopt (argc, argv, "hV")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'V'://Version
				printf(VERSION"\n");
				exit(EXIT_SUCCESS);
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	//Get arguments
	stats.MTI_IPv4_addr = argv[optind];
	stats.dev_name = argv[++optind];
	path_to_logstat_dir = argv[++optind];
	//Check arguments
	if(!stats.MTI_IPv4_addr || !stats.dev_name)
	{
		fprintf(stderr, "Mandatory Argument(s) missing!!!\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if(!is_valid_IPv4(stats.MTI_IPv4_addr))
	{
		fprintf(stderr, "Argument for IPv4 is invalid!!!\n");
		exit(EXIT_FAILURE);
	}
	//Check if other instance of this program already runs with same MTI_IPv4_addr
	if(check_already_run_with_same_arg(argv[0], stats.MTI_IPv4_addr))
	{
		fprintf(stderr, "%s for IPv4:%s Already Running!!!\n", argv[0], stats.MTI_IPv4_addr);
		exit(EXIT_SUCCESS);
	}
	//Install stopHandler as the signal handler for SIGINT, SIGTERM and SIGPIPE signals.
	signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    signal(SIGPIPE, stopHandler);

	//Print welcome message
	Logger("---- Morfeas_ΜΤΙ_if Started ----\n");
	Logger("libMODBus Version: %s\n",LIBMODBUS_VERSION_STRING);
	if(!path_to_logstat_dir)
		Logger("Argument for path to logstat directory Missing, %s will run in Compatible mode !!!\n",argv[0]);
	
	//Make MODBus socket for connection
	ctx = modbus_new_tcp(stats.MTI_IPv4_addr, MODBUS_TCP_DEFAULT_PORT);
	//Set Slave address
	if(modbus_set_slave(ctx, default_slave_address))
	{
		fprintf(stderr, "Can't set slave address!!!\n");
		modbus_free(ctx);
		return EXIT_FAILURE;
	}
	//Attempt connection to MTI
	while(modbus_connect(ctx) && handler_run)
	{
		sleep(1);
		stats.error = errno;
		//MTI_status_to_IPC(FIFO_fd, &stats);
		Logger("Connection Error (%d): %s\n", errno, modbus_strerror(errno));
		logstat_MTI(path_to_logstat_dir, &stats);
	}
	stats.error = 0;//load no error on stats
	
	while(handler_run)//MTI printing of status and telemetry device(s)
	{
		printf("\n-------------------------------------------------------------------------------------\n");
		Logger("New read status request\n");
		if(!get_MTI_status(ctx, &stats))
		{
			printf("====== MTI Status =====\n");
			printf("MTI batt=%.2fV\n",stats.MTI_status.MTI_batt_volt);
			printf("MTI batt cap=%.2f%%\n",stats.MTI_status.MTI_batt_capacity);
			printf("MTI batt state=%s\n",MTI_charger_state_str[stats.MTI_status.MTI_charge_status]);
			printf("MTI cpu_temp=%.2f°C\n",stats.MTI_status.MTI_CPU_temp);
			printf("Bt1=%d\tBt2=%d\tBt3=%d\n",
				stats.MTI_status.buttons_state.pb1,
				stats.MTI_status.buttons_state.pb2,
				stats.MTI_status.buttons_state.pb3);
			printf("MTI PWM_gen_out_freq=%.2fHz\n",stats.MTI_status.PWM_gen_out_freq);
			for(int i=0; i<4; i++)
				printf("MTI PWM_outDuty_CH[%d]=%.2f%%\n",i,stats.MTI_status.PWM_outDuty_CHs[i]);
			printf("=======================\n");
		}
		else
			Logger("get_MTI_status request Failed!!!\n");
		
		Logger("New get_MTI_Radio_config request\n");
		if(get_MTI_Radio_config(ctx, &stats)>=0)
		{
			printf("=== RX configuration ==\n");
			printf("RX Frequency=%.3fGHz\n",(2400+stats.MTI_Radio_config.RF_channel)/1000.0);
			printf("Data_rate=%s\n",MTI_Data_rate_str[stats.MTI_Radio_config.Data_rate]);
			printf("Tele_dev_type=%s\n",MTI_Tele_dev_type_str[stats.MTI_Radio_config.Tele_dev_type]);
			switch(stats.MTI_Radio_config.Tele_dev_type)
			{
				case Tele_TC4:
				case Tele_TC8:
				case Tele_TC16:
					printf("Samples until set valid = %d\n", stats.MTI_Radio_config.sreg.for_temp_tele.StV);
					printf("Samples until reset valid = %d\n", stats.MTI_Radio_config.sreg.for_temp_tele.StF);
					break;
				case RM_SW_MUX:
					printf("Manual_button = %d\n", stats.MTI_Radio_config.sreg.for_rmsw_dev.manual_button);
					printf("Sleep_button = %d\n", stats.MTI_Radio_config.sreg.for_rmsw_dev.sleep_button);
					printf("Global_switch = %d\n", stats.MTI_Radio_config.sreg.for_rmsw_dev.global_switch);
					printf("Global_speed = %d\n", stats.MTI_Radio_config.sreg.for_rmsw_dev.global_speed);
					break;
			}
			/*
			for(int i=0; i<sizeof(stats.MTI_Radio_config.Specific_reg)/sizeof(short); i++)
				printf("SFR[%d]=%d(0x%x)\n", i, stats.MTI_Radio_config.Specific_reg[i], stats.MTI_Radio_config.Specific_reg[i]);
			*/
			printf("=======================\n");
			if(stats.MTI_Radio_config.Tele_dev_type)
			{
				Logger("New get_MTI_Tele_data request\n");
				if(!get_MTI_Tele_data(ctx, &stats))
				{
					if(stats.MTI_Radio_config.Tele_dev_type!=RM_SW_MUX)
					{
						printf("\n===== Tele data =====\n");
						printf("Telemetry data is%s valid\n", stats.Tele_data.as_TC4.Data_isValid?"":" NOT");
						printf("Packet Index=%d\n", stats.Tele_data.as_TC4.packet_index);
						printf("RX Status=");
						switch(stats.Tele_data.as_TC4.RX_status)
						{
							case 0: printf("No signal"); break;
							case 1: 
							case 2: printf("RX %d",stats.Tele_data.as_TC4.RX_status); break;
							case 3: printf("Both RXs"); break;
						}
						printf("\n");
						printf("RX success Ratio=%d%%\n", stats.Tele_data.as_TC4.RX_Success_ratio);
					}
					else
					{
						printf("\n===== Remote Switches and Multiplexers =====\n");
						printf("Amount of detected devices = %d\n", stats.Tele_data.as_RMSWs.amount_of_devices);
					}
					printf("\n===== Data =====\n");
					switch(stats.MTI_Radio_config.Tele_dev_type)
					{
						case Tele_TC4:
							for(int i=0; i<sizeof(stats.Tele_data.as_TC4.CHs)/sizeof(*stats.Tele_data.as_TC4.CHs); i++)
								printf("CH%2d -> %.3f\n",i,stats.Tele_data.as_TC4.CHs[i]);
							for(int i=0; i<sizeof(stats.Tele_data.as_TC4.Refs)/sizeof(*stats.Tele_data.as_TC4.Refs); i++)
								printf("REF%2d -> %.3f\n",i,stats.Tele_data.as_TC4.Refs[i]);
							break;
						case Tele_TC8:
							for(int i=0; i<sizeof(stats.Tele_data.as_TC8.CHs)/sizeof(*stats.Tele_data.as_TC8.CHs); i++)
								printf("CH%2d -> %.3f\n",i,stats.Tele_data.as_TC8.CHs[i]);
							for(int i=0; i<sizeof(stats.Tele_data.as_TC8.Refs)/sizeof(*stats.Tele_data.as_TC8.Refs); i++)
								printf("REF%2d -> %.3f\n",i,stats.Tele_data.as_TC8.Refs[i]);
							break;
						case Tele_TC16:
							for(int i=0; i<sizeof(stats.Tele_data.as_TC16.CHs)/sizeof(*stats.Tele_data.as_TC16.CHs); i++)
								printf("CH%2d -> %.3f\n",i,stats.Tele_data.as_TC16.CHs[i]);
							break;
						case Tele_quad:
							for(int i=0; i<sizeof(stats.Tele_data.as_QUAD.CHs)/sizeof(*stats.Tele_data.as_QUAD.CHs); i++)
							{
								printf("\tCH%2d\n",i);
								printf("PWM_max = %d\n", stats.Tele_data.as_QUAD.gen_config[i].max);
								printf("PWM_min = %d\n", stats.Tele_data.as_QUAD.gen_config[i].min);
								printf("Saturation_mode = %d\n", stats.Tele_data.as_QUAD.gen_config[i].pwm_mode.dec.saturation);
								printf("Fixed_freq = %d\n", stats.Tele_data.as_QUAD.gen_config[i].pwm_mode.dec.fixed_freq);
								printf("Cnt_Value -> %.3f\n", stats.Tele_data.as_QUAD.CHs[i]);
							}
							break;
						case RM_SW_MUX:
							for(int i=0; i<stats.Tele_data.as_RMSWs.amount_of_devices; i++)
							{
								printf("Device %d:\n",i+1);
								printf("\tMemory offset=%u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].pos_offset);
								printf("\tDev_type = %s\n",MTI_RM_dev_type_str[stats.Tele_data.as_RMSWs.det_devs_data[i].dev_type]);
								printf("\tDev_ID = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].dev_id);
								printf("\tLast_msg_before = %d sec\n",stats.Tele_data.as_RMSWs.det_devs_data[i].time_from_last_mesg);
								printf("\tDev temp = %.2f°C\n",stats.Tele_data.as_RMSWs.det_devs_data[i].dev_temp);
								printf("\tDev input_voltage = %.2fV\n",stats.Tele_data.as_RMSWs.det_devs_data[i].input_voltage);
								switch(stats.Tele_data.as_RMSWs.det_devs_data[i].dev_type)
								{
									case RMSW_2CH:
										printf("\tCH1 = %.2fV\n",stats.Tele_data.as_RMSWs.det_devs_data[i].meas_data[0]);
										printf("\tCH1 = %.3fA\n",stats.Tele_data.as_RMSWs.det_devs_data[i].meas_data[1]);
										printf("\tCH2 = %.2fV\n",stats.Tele_data.as_RMSWs.det_devs_data[i].meas_data[2]);
										printf("\tCH2 = %.3fA\n",stats.Tele_data.as_RMSWs.det_devs_data[i].meas_data[3]);
										break;
									case Mini_RMSW:
										for(int j=0;j<4; j++)
											printf("\tCH%u = %.2f°C\n",j,stats.Tele_data.as_RMSWs.det_devs_data[i].meas_data[j]);
										break;
								}
								printf("\tControl byte = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.as_byte);
								switch(stats.Tele_data.as_RMSWs.det_devs_data[i].dev_type)
								{
									case RMSW_2CH:
										printf("\t\tTX_rate = %d sec\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.Rep_rate?2:20);
										printf("\t\tMain_SW = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.Main);
										printf("\t\t CH1_SW = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.CH1);
										printf("\t\t CH2_SW = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.rmsw_dec.CH2);
										break;
									case MUX:
										printf("\t\tTX_rate = %d sec\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.Rep_rate?2:20);
										printf("\t\t CH1_SW -> %c\n",'A'+stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH1);
										printf("\t\t CH2_SW -> %c\n",'A'+stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH2);
										printf("\t\t CH3_SW -> %c\n",'A'+stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH3);
										printf("\t\t CH4_SW -> %c\n",'A'+stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mux_dec.CH4);
										break;
									case Mini_RMSW:
										printf("\t\tTX_rate = ");
										switch(stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mini_dec.Rep_rate)
										{
											case 0: printf("20"); break;
											case 2: printf("2"); break;
											case 3:
											case 1: printf(".2"); break;
										}
										printf(" sec\n");
										printf("\t\tMain_SW = %u\n",stats.Tele_data.as_RMSWs.det_devs_data[i].switch_status.mini_dec.Main);
										break;
								}
								printf("\n");
							}
							break;
					}
					printf("=======================\n");
					if(stats.counter >= 10)
						logstat_MTI(path_to_logstat_dir, &stats);
				}
				else
					Logger("get_MTI_Tele_data request Failed!!!\n");
			}
		}
		else
			Logger("get_MTI_Radio_config request Failed!!!\n");
		stats.counter++;
		usleep(100000);
	}
	
	//Close MODBus connection and De-allocate memory
	modbus_close(ctx);
	modbus_free(ctx);
	//Delete logstat file
	if(path_to_logstat_dir)
		delete_logstat_MTI(path_to_logstat_dir, &stats);
	return EXIT_SUCCESS;
}

//print the Usage manual
void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: Morfeas_MTI_test  Copyright (C) 12019-12020  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"\tDev_name: A string that related to the configuration of the MTI\n\n"
		"\t    IPv4: The IPv4 address of MTI\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -V : Version.\n"
	};
	printf("%s\nUsage: %s IPv4 Dev_name [/path/to/logstat/directory] [Options]\n\n%s",preamp, prog_name, manual);
	return;
}