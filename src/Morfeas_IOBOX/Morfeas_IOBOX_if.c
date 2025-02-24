/*
File: Morfeas_IOBOX_if.c, Implementation of Morfeas IOBOX (MODBus) handler, Part of Morfeas_project.
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
#define VERSION "1.1" /*Release Version of Morfeas_IOBOX_if*/

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

#include "../IPC/Morfeas_IPC.h" //<-#include -> "Morfeas_Types.h"
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
//--- Local Functions ---//
// IOBOX_status_to_IPC function. Send Status of IOBOX to Morfeas_opc_ua via IPC
void IOBOX_status_to_IPC(int FIFO_fd, struct Morfeas_IOBOX_if_stats *stats);
// Function that register IOBOX Channels to Morfeas_opc_ua via IPC
void IPC_Channels_reg(int FIFO_fd, struct Morfeas_IOBOX_if_stats *stats);

int main(int argc, char *argv[])
{
	clock_t t0, t1, t_wait; //Time measure variables.
	//MODBus related variables
	modbus_t *ctx;
	int rc, RX_mem;
	//Apps variables
	char *path_to_logstat_dir;
	//Variables for IPC
	IPC_message IPC_msg = {0};
	struct Morfeas_IOBOX_if_stats stats = {0};
	unsigned short IOBOX_regs[IOBOX_imp_reg], old_indexes[IOBOX_Amount_of_All_RXs] = {0};
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
				printf("Release: %s (%s)\nCompile Date: %s\nVer: "VERSION"\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date(), Morfeas_get_compile_date());
				exit(EXIT_SUCCESS);
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	//Get arguments
	stats.IOBOX_IPv4_addr = argv[optind];
	stats.dev_name = argv[++optind];
	path_to_logstat_dir = argv[++optind];
	//Check arguments
	if(!stats.IOBOX_IPv4_addr || !stats.dev_name)
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
	if(!is_valid_IPv4(stats.IOBOX_IPv4_addr))
	{
		fprintf(stderr, "Argument for IPv4 is invalid!!!\n");
		exit(EXIT_FAILURE);
	}
	//Check if other instance of this program already runs with same IOBOX_IPv4_addr
	if(check_already_run_with_same_arg(argv[0], stats.IOBOX_IPv4_addr))
	{
		fprintf(stderr, "%s for IPv4:%s Already Running!!!\n", argv[0], stats.IOBOX_IPv4_addr);
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
	Logger("---- Morfeas_IOBOX_if Started ----\n");
	Logger("Release: %s (%s)\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date());
	Logger("Version: "VERSION", Compiled Date: %s\n", Morfeas_get_compile_date());
	Logger("libMODBus Version: %s\n",LIBMODBUS_VERSION_STRING);
	if(!path_to_logstat_dir)
		Logger("Argument for path to logstat directory Missing, %s will run in Compatible mode !!!\n",argv[0]);

	//----Make of FIFO file----//
	mkfifo(Data_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	//Register handler to Morfeas_OPC-UA Server
	Logger("Morfeas_IOBOX_if(%s) Send Registration message to OPC-UA via IPC....\n",stats.dev_name);
	//Open FIFO for Write
	FIFO_fd = open(Data_FIFO, O_WRONLY);
	IPC_Handler_reg_op(FIFO_fd, IOBOX, stats.dev_name, REG);
	Logger("Morfeas_IOBOX_if(%s) Registered on OPC-UA\n",stats.dev_name);

	//Make MODBus socket for connection
	ctx = modbus_new_tcp(stats.IOBOX_IPv4_addr, MODBUS_TCP_DEFAULT_PORT);
	//Set Slave address
	if(modbus_set_slave(ctx, default_slave_address))
	{
		fprintf(stderr, "Can't set slave address !!!\n");
		modbus_free(ctx);
		return EXIT_FAILURE;
	}
	//Attempt connection to IOBOX
	while(modbus_connect(ctx) && handler_run)
	{
		sleep(1);
		stats.error = errno;
		IOBOX_status_to_IPC(FIFO_fd, &stats);
		Logger("Connection Error (%d): %s\n", errno, modbus_strerror(errno));
		logstat_IOBOX(path_to_logstat_dir, &stats);
	}
	if(!handler_run)
		goto Exit;
	stats.error = OK_status;
	IOBOX_status_to_IPC(FIFO_fd, &stats);//send status report to Morfeas_opc_ua via IPC
	//Print Connection success message
	Logger("Connected to IOBOX %s(%s)\n", stats.IOBOX_IPv4_addr, stats.dev_name);
		//--- main application loop ---//
	//Register channels on Morfeas_opc_ua via IPC
	IPC_Channels_reg(FIFO_fd, &stats);
	//Load dev name and IPv4 address to IPC_msg
	IPC_msg.IOBOX_data.IPC_msg_type = IPC_IOBOX_data;
	memccpy(IPC_msg.IOBOX_data.Dev_or_Bus_name, stats.dev_name,'\0',Dev_or_Bus_name_str_size);
	IPC_msg.IOBOX_data.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	inet_pton(AF_INET, stats.IOBOX_IPv4_addr, &(IPC_msg.IOBOX_data.IOBOX_IPv4));
	while(handler_run)
	{
		t0 = clock();
		rc = modbus_read_input_registers(ctx, IOBOX_start_reg, IOBOX_Max_reg_read, IOBOX_regs);
		if(rc <= 0)
		{
			Logger("Error (%d) on MODBus Register read: %s\n", errno, modbus_strerror((stats.error = errno)));//load errno to stats and report to Logger
			IOBOX_status_to_IPC(FIFO_fd, &stats);//send status report to Morfeas_opc_ua via IPC
			while(modbus_connect(ctx) && handler_run)//Attempt to reconnection
			{
				logstat_IOBOX(path_to_logstat_dir, &stats);//Report error on logstat
				sleep(1);
			}
			if(handler_run)
			{
				Logger("Recover from last Error\n");
				stats.error = OK_status;
				IOBOX_status_to_IPC(FIFO_fd, &stats);
			}
		}
		else
		{	// --- Scale measurements and send them to Morfeas_opc_ua via IPC --- //
			//Load Data for "Wireless Inductive Power Supply"
			IPC_msg.IOBOX_data.Supply_Vin = IOBOX_regs[0]/100.0;
			for(int i=0, j=1; i<IOBOX_Amount_of_STD_RXs; i++)
			{
				IPC_msg.IOBOX_data.Supply_meas[i].Vout = IOBOX_regs[j++]/100.0;
				IPC_msg.IOBOX_data.Supply_meas[i].Iout = IOBOX_regs[j++]/100.0;
				j++;
			}
			//Check if IOBOX is 6CH and no inductive supply and attempt to read extra RX Channels.
			if(!IPC_msg.IOBOX_data.Supply_Vin)
				modbus_read_input_registers(ctx, IOBOX_Max_reg_read, IOBOX_imp_reg-IOBOX_Max_reg_read, IOBOX_regs+IOBOX_Max_reg_read);
			//Load Data for RXs
			RX_mem = IOBOX_RXs_mem_offset;
			for(int i=0; i<IOBOX_Amount_of_All_RXs; i++)
			{
				if(old_indexes[i]!=IOBOX_regs[IOBOX_Index_reg_pos+RX_mem])//Check if message in valid, by compering Message index with the old
				{
					for(int j=0; j<IOBOX_Amount_of_channels; j++)
						IPC_msg.IOBOX_data.RX[i].CH_value[j] = ((short)IOBOX_regs[j+RX_mem])/16.0;//Recast the value to signed
					IPC_msg.IOBOX_data.RX[i].index = IOBOX_regs[IOBOX_Index_reg_pos+RX_mem];
					IPC_msg.IOBOX_data.RX[i].status = IOBOX_regs[IOBOX_Status_reg_pos+RX_mem];
					IPC_msg.IOBOX_data.RX[i].success = IOBOX_regs[IOBOX_Success_reg_pos+RX_mem];
				}
				else
				{
					IPC_msg.IOBOX_data.RX[i].status = 0;
					IPC_msg.IOBOX_data.RX[i].success = 0;
				}
				old_indexes[i] = IOBOX_regs[IOBOX_Index_reg_pos+RX_mem];
				RX_mem += IOBOX_RXs_mem_offset;
			}

			//Send measurements
			IPC_msg_TX(FIFO_fd, &IPC_msg);

				//--- Accumulate values for averaging ---//
			//Load Wireless Inductive Power Supply data to stats
			stats.Supply_Vin += IPC_msg.IOBOX_data.Supply_Vin;
			for(int i=0; i<IOBOX_Amount_of_STD_RXs; i++)
			{
				stats.Supply_meas[i].Vout += IPC_msg.IOBOX_data.Supply_meas[i].Vout;
				stats.Supply_meas[i].Iout += IPC_msg.IOBOX_data.Supply_meas[i].Iout;
			}
			//Load RXs Data to stats
			RX_mem = IOBOX_RXs_mem_offset;
			for(int i=0; i<IOBOX_Amount_of_All_RXs; i++)
			{
				for(int j=0; j<IOBOX_Amount_of_channels; j++)
					stats.RX[i].CH_value[j] += IPC_msg.IOBOX_data.RX[i].CH_value[j];
				stats.RX[i].index = IPC_msg.IOBOX_data.RX[i].index;
				stats.RX[i].status = IPC_msg.IOBOX_data.RX[i].status;
				stats.RX[i].success = IPC_msg.IOBOX_data.RX[i].success;
				RX_mem += IOBOX_RXs_mem_offset;
			}
			stats.counter++;
			if(stats.counter > 10)
			{
				logstat_IOBOX(path_to_logstat_dir, &stats);
				stats.counter = 0;
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
	//Remove registered handler from Morfeas_OPC_UA Server
	IPC_Handler_reg_op(FIFO_fd, IOBOX, stats.dev_name, UNREG);
	Logger("Morfeas_IOBOX_if (%s) Removed from OPC-UA\n",stats.dev_name);
	close(FIFO_fd);
	//Delete logstat file
	if(path_to_logstat_dir)
		delete_logstat_IOBOX(path_to_logstat_dir, &stats);
	return EXIT_SUCCESS;
}

//print the Usage manual
void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: Morfeas_IOBOX_if  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"\tDev_name: A string that related to the configuration of the IOBOX\n\n"
		"\t    IPv4: The IPv4 address of IOBOX\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -v : Version.\n"
	};
	printf("%s\nUsage: %s IPv4 Dev_name [/path/to/logstat/directory] [Options]\n\n%s",preamp, prog_name, manual);
	return;
}

//IOBOX_status function. Send Status of IOBOX to Morfeas_opc_ua via IPC
void IOBOX_status_to_IPC(int FIFO_fd, struct Morfeas_IOBOX_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//Load status to IPC_message
	IPC_msg.IOBOX_report.IPC_msg_type = IPC_IOBOX_report;
	memccpy(IPC_msg.IOBOX_report.Dev_or_Bus_name, stats->dev_name,'\0',Dev_or_Bus_name_str_size);
	IPC_msg.IOBOX_report.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load IOBOX IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->IOBOX_IPv4_addr, &(IPC_msg.IOBOX_report.IOBOX_IPv4));
	//Load error code to report IPC_msg
	IPC_msg.IOBOX_report.status = stats->error;
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that register IOBOX Channels to Morfeas_opc_ua via IPC
void IPC_Channels_reg(int FIFO_fd, struct Morfeas_IOBOX_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//--- Load necessary message data to IPC_message ---/
	IPC_msg.IOBOX_channels_reg.IPC_msg_type = IPC_IOBOX_channels_reg; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.IOBOX_channels_reg.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.IOBOX_channels_reg.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load IOBOX IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->IOBOX_IPv4_addr, &(IPC_msg.IOBOX_channels_reg.IOBOX_IPv4));
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}
