/*
File: Morfeas_MDAQ_if.c, Implementation of Morfeas MDAQ (MODBus) handler, Part of Morfeas_project.
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
#define VERSION "1.0" /*Release Version of Morfeas_MDAQ_if*/

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

#include "../IPC/Morfeas_IPC.h" //<- #include "Morfeas_Types.h"
#include "../Supplementary/Morfeas_run_check.h"
#include "../Supplementary/Morfeas_JSON.h"
#include "../Supplementary/Morfeas_Logger.h"

//Global variables
static volatile unsigned char handler_run = 1;

//Print the Usage manual
void print_usage(char *prog_name);

//Signal Handler Function
static void stopHandler(int signum)
{
	if(signum == SIGPIPE)
		fprintf(stderr,"IPC: Force Termination!!!\n");
	else if(!handler_run && signum == SIGINT)
		raise(SIGABRT);
	handler_run = 0;
}

//--- Local functions ---//
// MDAQ_status_to_IPC function. Send Status of MDAQ to Morfeas_opc_ua via IPC
void MDAQ_status_to_IPC(int FIFO_fd, struct Morfeas_MDAQ_if_stats *stats);
//Function that register MDAQ Channels to Morfeas_opc_ua via IPC
void IPC_Channels_reg(int FIFO_fd, struct Morfeas_MDAQ_if_stats *stats);

int main(int argc, char *argv[])
{
	clock_t t0, t1, t_wait; //Time measure variables.
	//MODBus related variables
	modbus_t *ctx;
	int rc, offset;
	//Apps variables
	char *path_to_logstat_dir;
	//Variables for IPC
	IPC_message IPC_msg = {0};
	struct Morfeas_MDAQ_if_stats stats = {0};
	unsigned short MDAQ_regs[MDAQ_imp_reg]; float *index_dec;
	//FIFO file descriptor
	int FIFO_fd;
	//Check for call without arguments
	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	//Get options
	int c;
	while ((c = getopt (argc, argv, "hv")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'v'://Version
				printf("Release: %s (%s)\nCompile Date: %s\nVer: "VERSION"\n", Morfeas_get_curr_git_hash(),
																			   Morfeas_get_release_date(),
																			   Morfeas_get_compile_date());
				exit(EXIT_SUCCESS);
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	//Get arguments
	stats.MDAQ_IPv4_addr = argv[optind];
	stats.dev_name = argv[++optind];
	path_to_logstat_dir = argv[++optind];
	//Check arguments
	if(!stats.MDAQ_IPv4_addr || !stats.dev_name)
	{
		fprintf(stderr, "Mandatory Argument(s) missing!!!\n");
		print_usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if(strlen(stats.dev_name)>=Dev_or_Bus_name_str_size)
	{
		fprintf(stderr, "Dev_name too big (>=%d)\n",Dev_or_Bus_name_str_size);
		exit(EXIT_FAILURE);
	}
	if(!is_valid_IPv4(stats.MDAQ_IPv4_addr))
	{
		fprintf(stderr, "Argument for IPv4 is invalid!!!\n");
		exit(EXIT_FAILURE);
	}
	//Check if other instance of this program already runs with same MDAQ_IPv4_addr
	if(check_already_run_with_same_arg(argv[0], stats.MDAQ_IPv4_addr))
	{
		fprintf(stderr, "%s for IPv4:%s Already Running!!!\n", argv[0], stats.MDAQ_IPv4_addr);
		exit(EXIT_SUCCESS);
	}
	//Check if other instance of this program already runs with same Device Name
	if(check_already_run_with_same_arg(argv[0], stats.dev_name))
	{
		fprintf(stderr, "%s with Dev_name:%s Already Running!!!\n", argv[0], stats.dev_name);
		exit(EXIT_SUCCESS);
	}
	//Install stopHandler as the signal handler for SIGINT, SIGTERM and SIGPIPE signals.
	signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    signal(SIGPIPE, stopHandler);

	//Print welcome message
	Logger("---- Morfeas_MDAQ_if Started ----\n");
	Logger("Release: %s (%s)\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date());
	Logger("Version: "VERSION", Compiled Date: %s\n", Morfeas_get_compile_date());
	Logger("libMODBus Version: %s\n",LIBMODBUS_VERSION_STRING);
	if(!path_to_logstat_dir)
		Logger("Argument for path to logstat directory Missing, %s will run in Compatible mode !!!\n",argv[0]);

	//----Make of FIFO file----//
	mkfifo(Data_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	//Register handler to Morfeas_OPC-UA Server
	Logger("Morfeas_MDAQ_if(%s) Send Registration message to OPC-UA via IPC....\n",stats.dev_name);
	//Open FIFO for Write
	FIFO_fd = open(Data_FIFO, O_WRONLY);
	IPC_Handler_reg_op(FIFO_fd, MDAQ, stats.dev_name, REG);
	Logger("Morfeas_MDAQ_if(%s) Registered on OPC-UA\n",stats.dev_name);

	//Make MODBus socket for connection
	ctx = modbus_new_tcp(stats.MDAQ_IPv4_addr, MODBUS_TCP_DEFAULT_PORT);
	//Set Slave address
	if(modbus_set_slave(ctx, default_slave_address))
	{
		fprintf(stderr, "Can't set slave address !!!\n");
		modbus_close(ctx);
		modbus_free(ctx);
		return EXIT_FAILURE;
	}
	//Attempt connection to MDAQ
	while(modbus_connect(ctx) && handler_run)
	{
		stats.error = errno;
		MDAQ_status_to_IPC(FIFO_fd, &stats);
		Logger("Connection Error (%d): %s\n", errno, modbus_strerror(errno));
		logstat_MDAQ(path_to_logstat_dir, &stats);
		sleep(1);
	}
	if(!handler_run)
		goto Exit;
	stats.error = OK_status;
	MDAQ_status_to_IPC(FIFO_fd, &stats);//send status report to Morfeas_opc_ua via IPC
	//Print Connection success message
	Logger("Connected to MDAQ %s(%s)\n", stats.MDAQ_IPv4_addr, stats.dev_name);
		//--- Main Application Loop ---//
	//Register channels on Morfeas_opc_ua via IPC
	IPC_Channels_reg(FIFO_fd, &stats);
	//Load dev name and IPv4 address to IPC_msg
	IPC_msg.MDAQ_data.IPC_msg_type = IPC_MDAQ_data;
	memccpy(IPC_msg.MDAQ_data.Dev_or_Bus_name, stats.dev_name,'\0',Dev_or_Bus_name_str_size);
	IPC_msg.MDAQ_data.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	inet_pton(AF_INET, stats.MDAQ_IPv4_addr, &(IPC_msg.MDAQ_data.MDAQ_IPv4));
	while(handler_run)
	{
		t0 = clock();
		rc = modbus_read_input_registers(ctx, MDAQ_start_reg, MDAQ_imp_reg, MDAQ_regs);
		if (rc <= 0)
		{
			Logger("Error (%d) on MODBus Register read: %s\n",errno, modbus_strerror((stats.error = errno)));//load errno to stats and report to Logger
			MDAQ_status_to_IPC(FIFO_fd, &stats);//send status report to Morfeas_opc_ua via IPC
			while(modbus_connect(ctx) && handler_run)//Attempt to reconnect
			{
				logstat_MDAQ(path_to_logstat_dir, &stats);//Report error on logstat.
				sleep(1);
			}
			if(handler_run)
			{
				Logger("Recover from last Error\n");
				stats.error = OK_status;
				MDAQ_status_to_IPC(FIFO_fd, &stats);
			}
		}
		else
		{
			// --- Scale measurements and send them to Morfeas_opc_ua via IPC --- //
			//Load MDAQ Board Data
			index_dec = (float*)MDAQ_regs;
			IPC_msg.MDAQ_data.meas_index = (unsigned int)*index_dec;//30101
			IPC_msg.MDAQ_data.board_temp = *((float*)(MDAQ_regs+6));//30107

			//Load MDAQ Channels Data
			offset = 10;
			for(int i=0; i<8; i++)
			{
				for(int j=0; j<4; j++)
				{
					IPC_msg.MDAQ_data.meas[i].value[j] = *((float*)(MDAQ_regs+offset));
					offset+=2;
				}
				IPC_msg.MDAQ_data.meas[i].warnings = (unsigned char)*((float*)(MDAQ_regs+offset));
				offset+=2;
			}
			//Send measurements to Morfeas_opc_ua
			IPC_msg_TX(FIFO_fd, &IPC_msg);

			if(stats.counter >= 10)
			{
				logstat_MDAQ(path_to_logstat_dir, &stats);
				stats.counter = 0;
			}
			else
			{
				//Load MDAQ Board Data to stats
				stats.meas_index = IPC_msg.MDAQ_data.meas_index;
				stats.board_temp += IPC_msg.MDAQ_data.board_temp;
				//Load MDAQ Channels Data to stats
				for(int i=0; i<8; i++)
				{
					stats.meas[i].warnings = IPC_msg.MDAQ_data.meas[i].warnings;
					for(int j=0; j<4; j++)
					{
						//Check if value is valid before add it to the accumulator.
						if(!((stats.meas[i].warnings>>j)&1))
							stats.meas[i].value[j] += IPC_msg.MDAQ_data.meas[i].value[j];
						else
							stats.meas[i].value[j] += stats.meas[i].value[j];
					}
				}
				stats.counter++;
			}
		}
		t1 = clock();
		if((t_wait = 100000-(t1-t0))>0)
			usleep(t_wait);
	}
Exit:
	//Close MODBus connection and De-allocate memory
	modbus_close(ctx);
	modbus_free(ctx);
	//Remove Registeration handler to Morfeas_OPC_UA Server
	IPC_Handler_reg_op(FIFO_fd, MDAQ, stats.dev_name, UNREG);
	Logger("Morfeas_MDAQ_if (%s) Removed from OPC-UA\n",stats.dev_name);
	close(FIFO_fd);
	//Delete logstat file
	if(path_to_logstat_dir)
		delete_logstat_MDAQ(path_to_logstat_dir, &stats);
	return EXIT_SUCCESS;
}

//Print the Usage manual
void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: Morfeas_MDAQ_if  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"\tDev_name: A string that related to the configuration of the MDAQ\n\n"
		"\t    IPv4: The IPv4 address of MDAQ\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -v : Version.\n"
	};
	printf("%s\nUsage: %s IPv4 Dev_name [/path/to/logstat/directory] [Options]\n\n%s",preamp, prog_name, manual);
	return;
}

//MDAQ_status function. Send Status of MDAQ to Morfeas_opc_ua via IPC
void MDAQ_status_to_IPC(int FIFO_fd, struct Morfeas_MDAQ_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//scale measurements and send them to Morfeas_opc_ua via IPC
	IPC_msg.MDAQ_report.IPC_msg_type = IPC_MDAQ_report;
	memccpy(IPC_msg.MDAQ_report.Dev_or_Bus_name, stats->dev_name,'\0',Dev_or_Bus_name_str_size);
	IPC_msg.MDAQ_report.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MDAQ IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MDAQ_IPv4_addr, &(IPC_msg.MDAQ_report.MDAQ_IPv4));
	//Load error code to report IPC_msg
	IPC_msg.MDAQ_report.status = stats->error;
	//Send status report
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that register MDAQ Channels to Morfeas_opc_ua via IPC
void IPC_Channels_reg(int FIFO_fd, struct Morfeas_MDAQ_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//--- Load necessary message data to IPC_message ---/
	IPC_msg.MDAQ_channels_reg.IPC_msg_type = IPC_MDAQ_channels_reg; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MDAQ_channels_reg.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MDAQ_channels_reg.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MDAQ IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MDAQ_IPv4_addr, &(IPC_msg.MDAQ_channels_reg.MDAQ_IPv4));
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}
