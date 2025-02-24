/*
File: Morfeas_MTI_if.c, Implementation of Morfeas MTI (MODBus) handler, Part of Morfeas_project.
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
#define Configs_dir "/var/tmp/Morfeas_MTI_Configurations/"
#define VERSION "1.0" /*Release Version of Morfeas_MTI_if*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
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

//Enumerator for the FSM's state
enum Morfeas_MTI_FSM_States{
	wait,
	get_config,
	get_data,
	get_status,
	error
};

//Global variables
unsigned char handler_run = 1;
pthread_mutex_t MTI_access = PTHREAD_MUTEX_INITIALIZER;

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

	//--- File related functions ---//
//Function that get or store the users_config to file. Return 0 on success.
int user_config(struct Morfeas_MTI_if_stats *stats, const char *mode);

	//--- MTI's Read Functions ---//
//MTI function that request the MTI's status and load them to stats. Return 0 on success.
int get_MTI_status(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);
//MTI function that request the MTI's RX configuration and load it to stats. Return 0 in success.
int get_MTI_Radio_config(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);
//MTI function that request from MTI the telemetry data and load them to stats. Return 0 in success.
int get_MTI_Tele_data(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);

	//--- MTI's User config function ---//
//Function that write user_config to MTI. Return 0 on success.
int MTI_set_user_config(modbus_t *ctx, struct Morfeas_MTI_if_stats *stats);

//--- D-Bus Listener function ---//
void * MTI_DBus_listener(void *varg_pt);//Thread function.

//--- Local Morfeas_IPC functions ---//
// MTI_status_to_IPC function. Send Status of MTI to Morfeas_opc_ua via IPC
void MTI_status_to_IPC(int FIFO_fd, struct Morfeas_MTI_if_stats *stats);
//Function that register MTI methods and properties tree to Morfeas_opc_ua via IPC
void IPC_reg_MTI_tree(int FIFO_fd, struct Morfeas_MTI_if_stats *stats);
//Function that send Health status of MTI to Morfeas_opc_ua via IPC
void IPC_Update_Health_status(int FIFO_fd, struct Morfeas_MTI_if_stats *stats);
//Function that send Radio configuration to Morfeas_opc_ua via IPC
void IPC_Update_Radio_status(int FIFO_fd, struct Morfeas_MTI_if_stats *stats, unsigned char new_config);
//Function that send Telemetry data(not RMSWs and MUXs) to Morfeas_opc_ua via IPC
void IPC_Telemetry_data(int FIFO_fd, struct Morfeas_MTI_if_stats *stats);
//Function that send data from RMSWs and MUXs to Morfeas_opc_ua via IPC
void IPC_RMSW_MUX_data(int FIFO_fd, struct Morfeas_MTI_if_stats *stats);

int main(int argc, char *argv[])
{
	//MODBus related variables
	modbus_t *ctx;
	//Apps variables
	int ret, tcp_port = MODBUS_TCP_DEFAULT_PORT, modbus_addr = default_slave_address;
	char *path_to_logstat_dir;
	unsigned char state = get_config, prev_RF_CH=-1, prev_dev_type=-1;
	unsigned short prev_sRegs=-1;
	struct Morfeas_MTI_if_stats stats = {0};
	//Directory pointer variables
	DIR *dir;
	//Variables for threads
	pthread_t DBus_listener_Thread_id;
	struct MTI_DBus_thread_arguments_passer passer = {&ctx, &stats};
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
	while ((c = getopt (argc, argv, "hvp:a:")) != -1)
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
			case 'p':
				if(!(tcp_port = atoi(optarg)))
				{
					fprintf(stderr, "-p argument is invalid!!!!\n");
					exit(1);
				}
				break;
			case 'a':
				if(!(modbus_addr = atoi(optarg)))
				{
					fprintf(stderr, "-a argument is invalid!!!!\n");
					exit(1);
				}
				break;
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
	if(strlen(stats.dev_name)>Dev_or_Bus_name_str_size)
	{
		fprintf(stderr, "Dev_name too big (>=%d)\n",Dev_or_Bus_name_str_size);
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
	//Check if other instance of this program already runs with same Device Name
	if(check_already_run_with_same_arg(argv[0], stats.dev_name))
	{
		fprintf(stderr, "%s with Dev_name:%s Already Running!!!\n", argv[0], stats.dev_name);
		exit(EXIT_SUCCESS);
	}

	//Check the existence of the Morfeas_MTI_configs directory
	dir = opendir(Configs_dir);
	if (dir)
		closedir(dir);
	else
	{
		Logger("Making MTI_Configurations_dir \n");
		if(mkdir(Configs_dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
		{
			perror("Error at MTI_Configurations_dir creation!!!");
			exit(EXIT_FAILURE);
		}
	}

	//Install stopHandler as the signal handler for SIGINT, SIGTERM and SIGPIPE signals.
	signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    signal(SIGPIPE, stopHandler);

	//Print welcome message
	Logger("---- Morfeas_ΜΤΙ_if Started ----\n");
	Logger("Release: %s (%s)\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date());
	Logger("Version: "VERSION", Compiled Date: %s\n", Morfeas_get_compile_date());
	Logger("libMODBus Version: %s\n",LIBMODBUS_VERSION_STRING);
	if(!path_to_logstat_dir)
		Logger("Argument for path to logstat directory Missing, %s will run in Compatible mode !!!\n",argv[0]);

	//---- Make of FIFO file ----//
	mkfifo(Data_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	//Register handler to Morfeas_OPC-UA Server
	Logger("Morfeas_MTI_if(%s) Send Registration message to OPC-UA via IPC....\n",stats.dev_name);
	//Open FIFO for Write
	FIFO_fd = open(Data_FIFO, O_WRONLY);
	IPC_Handler_reg_op(FIFO_fd, MTI, stats.dev_name, 0);
	Logger("Morfeas_MTI_if(%s) Registered on OPC-UA\n",stats.dev_name);

	//Make MODBus socket for connection
	ctx = modbus_new_tcp(stats.MTI_IPv4_addr, tcp_port);
	//Set Slave address
	if(modbus_set_slave(ctx, modbus_addr))
	{
		fprintf(stderr, "Can't set slave address!!!\n");
		modbus_free(ctx);
		return EXIT_FAILURE;
	}

	//Attempt connection to MTI
	while(modbus_connect(ctx) && handler_run)
	{
		stats.error = errno;
		MTI_status_to_IPC(FIFO_fd, &stats);
		Logger("Connection Error (%d): %s\n", errno, modbus_strerror(errno));
		logstat_MTI(path_to_logstat_dir, &stats);
		sleep(1);
	}
	if(!handler_run)
		goto Exit;
	//Print Connection success message
	Logger("Connected to %s(%s:%d@%d)\n", stats.dev_name, stats.MTI_IPv4_addr, tcp_port, modbus_addr);
	IPC_reg_MTI_tree(FIFO_fd, &stats);//Send MTI tree registration message to Morfeas_OPC_UA via IPC
	stats.error = 0;//load no error on stats
	MTI_status_to_IPC(FIFO_fd, &stats);//send status report to Morfeas_opc_ua via IPC

	//Get old user_config from config_file
	if(user_config(&stats, "r"))//Check for failure
	{
		for(int i=0; i<Amount_OF_GENS; i++)
		{
			stats.user_config.gen_config[i].scaler=1.0;
			stats.user_config.gen_config[i].max=100;
			stats.user_config.gen_config[i].min=0;
			stats.user_config.gen_config[i].pwm_mode.as_byte = 0;
		}
	}
	//Set MTI to User_config
	if(MTI_set_user_config(ctx, &stats))
		state = error;

	//Start D-Bus listener function in a thread
	pthread_create(&DBus_listener_Thread_id, NULL, MTI_DBus_listener, &passer);

	while(handler_run)//Application's FSM
	{
		switch(state)
		{
			case error:
				pthread_mutex_lock(&MTI_access);
					Logger("MODBus Error(%d): %s\n",errno, modbus_strerror((stats.error = errno)));
					MTI_status_to_IPC(FIFO_fd, &stats);//Send Error status report to Morfeas_opc_ua via IPC
				pthread_mutex_unlock(&MTI_access);
				do{
					pthread_mutex_lock(&MTI_access);
						ret = modbus_connect(ctx);//Attempt to reconnect
						stats.error = ret?errno:OK_status;//Load errno to stats at error or OK_status at success
						logstat_MTI(path_to_logstat_dir, &stats);//report error on logstat
					pthread_mutex_unlock(&MTI_access);
					if(ret)//Sleep 1sec at failure
						sleep(1);
				}while(ret && handler_run);
				if(!handler_run)//Check if exit by handler stop request
					break;
				Logger("Recover from last Error\n");
				pthread_mutex_lock(&MTI_access);
					MTI_status_to_IPC(FIFO_fd, &stats);//Send Okay status report to Morfeas_opc_ua via IPC
					stats.counter = 0;//Reset sample counter
					//Restore MTI to User_config
					state = MTI_set_user_config(ctx, &stats)?error:get_config;
				pthread_mutex_unlock(&MTI_access);
				break;
			case get_config:
				pthread_mutex_lock(&MTI_access);
					if(!get_MTI_Radio_config(ctx, &stats))
					{	//Check for change on configuration
						if(prev_RF_CH^stats.MTI_Radio_config.RF_channel||
						   prev_dev_type^stats.MTI_Radio_config.Tele_dev_type||
						   prev_sRegs^stats.MTI_Radio_config.sRegs.as_short)
						{
							IPC_Update_Radio_status(FIFO_fd, &stats, prev_dev_type^stats.MTI_Radio_config.Tele_dev_type?1:0);
							prev_RF_CH = stats.MTI_Radio_config.RF_channel;
							prev_dev_type = stats.MTI_Radio_config.Tele_dev_type;
							prev_sRegs = stats.MTI_Radio_config.sRegs.as_short;
						}
						if(stats.counter < 10)//approximately every second
						{	//Check transceiver state; if ON, next state is get_data, otherwise wait.
							state = (stats.MTI_Radio_config.Tele_dev_type>=Dev_type_min&&
									 stats.MTI_Radio_config.Tele_dev_type<=Dev_type_max) ? get_data : wait;
							stats.counter++;
						}
						else
							state = get_status;
					}
					else
						state = error;
				pthread_mutex_unlock(&MTI_access);
				break;
			case get_data:
				pthread_mutex_lock(&MTI_access);
					state = (ret = get_MTI_Tele_data(ctx, &stats)) ? error : wait;
					if(!ret)
					{
						switch(stats.MTI_Radio_config.Tele_dev_type)
						{
							case Tele_TC16:
							case Tele_TC8:
							case Tele_TC4:
							case Tele_quad:
								IPC_Telemetry_data(FIFO_fd, &stats);
								break;
							case RMSW_MUX:
								IPC_RMSW_MUX_data(FIFO_fd, &stats);
								break;
						}
					}
				pthread_mutex_unlock(&MTI_access);
				break;
			case get_status:
				pthread_mutex_lock(&MTI_access);
					if(!get_MTI_status(ctx, &stats))
					{
						IPC_Update_Health_status(FIFO_fd, &stats);
						logstat_MTI(path_to_logstat_dir, &stats);//Export logstat
						stats.counter = 0;
						state = wait;
					}
					else
						state = error;
				pthread_mutex_unlock(&MTI_access);
				break;
			case wait:
				if(stats.MTI_Radio_config.Tele_dev_type == RMSW_MUX)
					usleep(200000);
				else
					usleep(100000);
				state = get_config;
				break;
		}
	}

	pthread_join(DBus_listener_Thread_id, NULL);// wait DBus_listener thread to end
	pthread_detach(DBus_listener_Thread_id);//deallocate DBus_listener thread's memory
Exit:
	//Close MODBus connection and deallocate memory
	modbus_close(ctx);
	modbus_free(ctx);
	//Remove Registered handler from Morfeas_OPC_UA Server
	IPC_Handler_reg_op(FIFO_fd, MTI, stats.dev_name, 1);
	Logger("Morfeas_MTI_if (%s) Removed from OPC-UA\n",stats.dev_name);
	close(FIFO_fd);
	//Delete logstat file
	if(path_to_logstat_dir)
		delete_logstat_MTI(path_to_logstat_dir, &stats);
	return EXIT_SUCCESS;
}

//print the Usage manual
void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: Morfeas_MTI_if  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"\tDev_name: A string that related to the configuration of the MTI\n\n"
		"\t    IPv4: The IPv4 address of MTI\n\n"
		"Options:\n"
		"           -h : Print help.\n"
		"           -v : Version.\n"
		"           -p : TCP port(default:502)\n"
		"           -a : Modbus Slave address(default:10)\n"
	};
	printf("%s\nUsage: %s IPv4 Dev_name [/path/to/logstat/directory] [Options]\n\n%s",preamp, prog_name, manual);
	return;
}

int user_config(struct Morfeas_MTI_if_stats *stats, const char *mode)
{
	char config_file_path[60];
	FILE *fp;
	struct{
		MTI_stored_config user_config;
		unsigned char checksum;
	}file_config;

	if(strcmp(mode, "r") && strcmp(mode, "w"))
		return EXIT_FAILURE;
	sprintf(config_file_path, "%sMorfeas_MTI_%s", Configs_dir, stats->dev_name);
	if(!strcmp(mode, "r"))
	{
		fp=fopen(config_file_path, mode);
		if(fp)
		{
			fread(&file_config, 1, sizeof(file_config), fp);
			fclose(fp);
			if(!(file_config.checksum ^ Checksum(&(file_config.user_config), sizeof(MTI_stored_config))))
				memcpy(&(stats->user_config), &(file_config.user_config), sizeof(MTI_stored_config));
			else
			{
				Logger("MTI configuration file Checksum Error!!!\n");
				return EXIT_FAILURE;
			}
		}
		else
			return EXIT_FAILURE;
	}
	else if(!strcmp(mode, "w"))
	{
		memcpy(&(file_config.user_config), &(stats->user_config), sizeof(MTI_stored_config));
		file_config.checksum = Checksum(&(file_config.user_config), sizeof(MTI_stored_config));
		fp=fopen(config_file_path, mode);
		if(fp)
		{
			fwrite(&file_config, 1, sizeof(file_config), fp);
			fclose(fp);
		}
		else
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

// MTI_status function. Send Status of MTI to Morfeas_opc_ua via IPC
void MTI_status_to_IPC(int FIFO_fd, struct Morfeas_MTI_if_stats *stats)
{
	unsigned char i;
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_report.IPC_msg_type = IPC_MTI_report;
	memccpy(IPC_msg.MTI_report.Dev_or_Bus_name, stats->dev_name,'\0',Dev_or_Bus_name_str_size);
	IPC_msg.MTI_report.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MTI IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &(IPC_msg.MTI_report.MTI_IPv4));
	//Load current Radio Telemetry type
	IPC_msg.MTI_report.Tele_dev_type = stats->MTI_Radio_config.Tele_dev_type;
	//Load MiniRMSW amount and them IDs to IPC_msg, if MTI's radio configured for RMSW/MUX. Otherwise amount is 1
	if(stats->MTI_Radio_config.Tele_dev_type == RMSW_MUX && stats->Tele_data.as_RMSWs.amount_of_devices)
	{
		for(i=0; i<stats->Tele_data.as_RMSWs.amount_of_devices; i++)
		{
			if(stats->Tele_data.as_RMSWs.det_devs_data[i].dev_type == Mini_RMSW)
			{
				IPC_msg.MTI_report.IDs_of_MiniRMSWs[IPC_msg.MTI_report.amount_of_Linkable_tele] = stats->Tele_data.as_RMSWs.det_devs_data[i].dev_id;
				IPC_msg.MTI_report.amount_of_Linkable_tele++;
			}
		}
	}
	else if(stats->MTI_Radio_config.Tele_dev_type>=Dev_type_min && stats->MTI_Radio_config.Tele_dev_type<=Dev_type_max)
		IPC_msg.MTI_report.amount_of_Linkable_tele = 1;
	//Load error code to report IPC_msg
	IPC_msg.MTI_report.status = stats->error;
	//Send status report
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that register MTI tree to Morfeas_opc_ua via IPC
void IPC_reg_MTI_tree(int FIFO_fd, struct Morfeas_MTI_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_tree_reg.IPC_msg_type = IPC_MTI_tree_reg; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MTI_tree_reg.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MTI_tree_reg.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MTI's IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &(IPC_msg.MTI_tree_reg.MTI_IPv4));
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that send MTI's Health status to Morfeas_opc_ua via IPC
void IPC_Update_Health_status(int FIFO_fd, struct Morfeas_MTI_if_stats *stats)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_Update_Health.IPC_msg_type = IPC_MTI_Update_Health; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MTI_Update_Health.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MTI_Update_Health.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load Health status data to IPC_msg
	IPC_msg.MTI_Update_Health.cpu_temp = stats->MTI_status.MTI_CPU_temp;
	IPC_msg.MTI_Update_Health.batt_capacity = stats->MTI_status.MTI_batt_capacity;
	IPC_msg.MTI_Update_Health.batt_voltage = stats->MTI_status.MTI_batt_volt;
	IPC_msg.MTI_Update_Health.batt_state = stats->MTI_status.MTI_charge_status;
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that send Radio configuration to Morfeas_opc_ua via IPC
void IPC_Update_Radio_status(int FIFO_fd, struct Morfeas_MTI_if_stats *stats, unsigned char new_config)
{
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_Update_Radio.IPC_msg_type = IPC_MTI_Update_Radio; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MTI_Update_Radio.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MTI_Update_Radio.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MTI's IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &(IPC_msg.MTI_Update_Radio.MTI_IPv4));
	//Load Health status data to IPC_msg
	IPC_msg.MTI_Update_Radio.RF_channel = stats->MTI_Radio_config.RF_channel;
	IPC_msg.MTI_Update_Radio.Data_rate = stats->MTI_Radio_config.Data_rate;
	IPC_msg.MTI_Update_Radio.Tele_dev_type = stats->MTI_Radio_config.Tele_dev_type;
	memcpy(&(IPC_msg.MTI_Update_Radio.sRegs), &(stats->MTI_Radio_config.sRegs), sizeof(IPC_msg.MTI_Update_Radio.sRegs));
	IPC_msg.MTI_Update_Radio.isNew_config = new_config?1:0;
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that send Telemetry data(not RMSWs and MUXs) to Morfeas_opc_ua via IPC
void IPC_Telemetry_data(int FIFO_fd, struct Morfeas_MTI_if_stats *stats)
{
	if(stats->MTI_Radio_config.Tele_dev_type<Dev_type_min||
	   stats->MTI_Radio_config.Tele_dev_type>Dev_type_max||
	   stats->MTI_Radio_config.Tele_dev_type == RMSW_MUX)
		return;
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_tele_data.IPC_msg_type = IPC_MTI_Tele_data; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MTI_tele_data.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MTI_tele_data.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MTI's IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &(IPC_msg.MTI_tele_data.MTI_IPv4));
	//Load Telemetry device type
	IPC_msg.MTI_tele_data.Tele_dev_type = stats->MTI_Radio_config.Tele_dev_type;
	//Load Telemetry device data
	switch(stats->MTI_Radio_config.Tele_dev_type)
	{
		case Tele_TC16:
			memcpy(&(IPC_msg.MTI_tele_data.data), &(stats->Tele_data), sizeof(IPC_msg.MTI_tele_data.data.as_TC16));
			break;
		case Tele_TC8:
			memcpy(&(IPC_msg.MTI_tele_data.data), &(stats->Tele_data), sizeof(IPC_msg.MTI_tele_data.data.as_TC8));
			break;
		case Tele_TC4:
			memcpy(&(IPC_msg.MTI_tele_data.data), &(stats->Tele_data), sizeof(IPC_msg.MTI_tele_data.data.as_TC4));
			break;
		case Tele_quad:
			memcpy(&(IPC_msg.MTI_tele_data.data), &(stats->Tele_data), sizeof(IPC_msg.MTI_tele_data.data.as_QUAD));
			break;
	}
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}

//Function that send data from RMSWs and MUXs to Morfeas_opc_ua via IPC
void IPC_RMSW_MUX_data(int FIFO_fd, struct Morfeas_MTI_if_stats *stats)
{
	if(stats->MTI_Radio_config.Tele_dev_type<Dev_type_min||
	   stats->MTI_Radio_config.Tele_dev_type>Dev_type_max||
	   stats->MTI_Radio_config.Tele_dev_type != RMSW_MUX)
		return;
	//Variables for IPC
	IPC_message IPC_msg = {0};

		//--- Load data to IPC_message ---//
	IPC_msg.MTI_RMSW_MUX_data.IPC_msg_type = IPC_MTI_RMSW_MUX_data; //Message type
	//Load Device name to IPC_message
	memccpy(IPC_msg.MTI_RMSW_MUX_data.Dev_or_Bus_name, stats->dev_name, '\0', Dev_or_Bus_name_str_size);
	IPC_msg.MTI_RMSW_MUX_data.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	//Load MTI's IPv4 by converting from string to unsigned integer
	inet_pton(AF_INET, stats->MTI_IPv4_addr, &(IPC_msg.MTI_RMSW_MUX_data.MTI_IPv4));
	//Load RMSW/MUX data to IPC_msg
	IPC_msg.MTI_RMSW_MUX_data.Devs_data.amount_of_devices = stats->Tele_data.as_RMSWs.amount_of_devices;
	IPC_msg.MTI_RMSW_MUX_data.Devs_data.amount_to_be_remove = stats->Tele_data.as_RMSWs.amount_to_be_remove;
	memcpy(IPC_msg.MTI_RMSW_MUX_data.Devs_data.IDs_to_be_removed,
		   stats->Tele_data.as_RMSWs.IDs_to_be_removed,
		   stats->Tele_data.as_RMSWs.amount_to_be_remove);
	memcpy(IPC_msg.MTI_RMSW_MUX_data.Devs_data.det_devs_data,
		   stats->Tele_data.as_RMSWs.det_devs_data,
		   stats->Tele_data.as_RMSWs.amount_of_devices * sizeof(struct RMSW_MUX_Mini_data_struct));
	//Send status report to Morfeas_opc_ua
	IPC_msg_TX(FIFO_fd, &IPC_msg);
}
