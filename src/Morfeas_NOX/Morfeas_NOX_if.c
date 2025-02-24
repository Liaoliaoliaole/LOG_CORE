/*
File: Morfeas_NOX_if.c, Implementation of Morfeas NOX (CANBus) handler, Part of Morfeas_project.
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
#define VERSION "1.0" /*Release Version of Morfeas_NOX_if software*/
#define MAX_CANBus_FPS 1700.7 //Maximum amount of frames per sec for 250Kbaud
#define Configs_dir "/var/tmp/Morfeas_NOX_Configurations/" //Directory with NOX_handler's Configuration file

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/if_link.h>

#include <libsocketcan.h>

#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

//Include Functions implementation header
#include "../Supplementary/Morfeas_run_check.h"
#include "../Supplementary/Morfeas_JSON.h"
#include "../IPC/Morfeas_IPC.h" //<-#include -> "Morfeas_Types.h"
#include "../Supplementary/Morfeas_Logger.h"
#include "../Morfeas_RPi_Hat/Morfeas_RPi_Hat.h"

enum bitrate_check_return_values{
	bitrate_check_success,
	bitrate_check_error,
	bitrate_check_invalid
};

//Global varables
volatile unsigned char NOX_handler_run = 1;
static volatile struct Morfeas_NOX_if_flags flags = {0};
pthread_mutex_t NOX_access = PTHREAD_MUTEX_INITIALIZER;

/* Local function (declaration)
 * Return value: EXIT_FAILURE(1) of failure or EXIT_SUCCESS(0) on success. Except of other notice
 */
int CAN_if_bitrate_check(char *CAN_IF_name, int *bitrate);
void quit_signal_handler(int signum);//SIGINT handler function
void print_usage(char *prog_name);//print the usage manual
int NOX_handler_config_file(struct Morfeas_NOX_if_stats *stats, const char *mode);//Read and write NOX_handler configuration file

//--- D-Bus Listener function ---//
void * NOX_DBus_listener(void *varg_pt);//Thread function.

//--- WebSocket related functions ---//
void * Morfeas_NOX_ws_server(void *varg_pt);
void Morfeas_NOX_ws_server_send_meas(struct UniNOx_sensor *NOXs_data);

/*UniNOx functions*/
int NOx_heater(int socket_fd, unsigned char start_code);
unsigned char NOx_error_dec(unsigned char error_code);

int main(int argc, char *argv[])
{
	//Directory pointer variables
	DIR *dir;
	//Operational variables
	int c;
	char *logstat_path;
	unsigned char i2c_bus_num = I2C_BUS_NUM;
	unsigned long msg_cnt=0;
	time_t t_bfr=0, t_now=0;
	//Variables for CANBus Port Electric
	struct tm Morfeas_RPi_Hat_last_cal = {0};
	struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config port_meas_config = {0};
	struct Morfeas_RPi_Hat_Port_meas port_meas = {0};
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//Variables for Socket CAN and NOX_id decoder
	unsigned char sensor_index = -1;
	int RX_bytes, CAN_socket_num;
	struct timeval tv;
	struct ifreq ifr;
	struct sockaddr_can addr = {0};
	struct can_frame frame_rx;
	struct can_filter RX_filter;
	struct rtnl_link_stats64 link_stats = {0};
	NOx_can_id *NOx_id_dec;
	NOx_RX_frame *NOx_data = (NOx_RX_frame *)&(frame_rx.data);
	NOX_start_code startcode = {0};
	//Stats of Morfeas_NOX_IF
	struct Morfeas_NOX_if_stats stats = {.auto_switch_off_value = -1};
	//Variables for threads
	pthread_t DBus_listener_Thread_id, ws_server_Thread_id;
	struct NOX_DBus_thread_arguments_passer passer = {&startcode, &stats};

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(1);
	}
	opterr = 1;
	while((c = getopt(argc, argv, "hvb:")) != -1)
	{
		switch(c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'v'://Version
				printf("Release: %s (%s)\nCompile Date: %s\nVer: "VERSION"\n", Morfeas_get_curr_git_hash(),
																			   Morfeas_get_release_date(),
																			   Morfeas_get_compile_date());
				exit(EXIT_SUCCESS);
			case 'b':
				i2c_bus_num=atoi(optarg);
				break;
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	//Check if program already runs on same bus.
	if(check_already_run_with_same_arg(argv[0], argv[optind]))
	{
		fprintf(stderr, "%s for interface \"%s\" Already Running!!!\n", argv[0], argv[optind]);
		exit(EXIT_SUCCESS);
	}
	//Check if the size of the CAN-IF is bigger than the limit.
	stats.CAN_IF_name = argv[optind];
	if(strlen(stats.CAN_IF_name)>=Dev_or_Bus_name_str_size)
	{
		fprintf(stderr, "Interface name too big (>=%d)\n",Dev_or_Bus_name_str_size);
		exit(EXIT_FAILURE);
	}
	//Check bitrate of CAN-IF
	switch(CAN_if_bitrate_check(stats.CAN_IF_name, &CAN_socket_num))
	{
		case bitrate_check_invalid:
			fprintf(stderr, "Speed of CAN-IF(%s) is incompatible with UNI-NOx (%d != %d)!!!\n", stats.CAN_IF_name, CAN_socket_num, NOx_Bitrate);
			break;
		case bitrate_check_error:
			fprintf(stderr, "CAN_if_bitrate_check() failed !!!\n");
			break;
	}
	//CAN Socket Opening
	if((CAN_socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		exit(EXIT_FAILURE);
	}
	//Link interface name to socket
	strncpy(ifr.ifr_name, stats.CAN_IF_name, IFNAMSIZ-1); // get value from CAN-IF arguments
	if(ioctl(CAN_socket_num, SIOCGIFINDEX, &ifr))
	{
		char if_error[30];
		sprintf(if_error, "CAN-IF (%s)",ifr.ifr_name);
		perror(if_error);
		exit(EXIT_FAILURE);
	}
	//Logstat.json
	if(!(logstat_path = argv[optind+1]))
		fprintf(stderr,"No logstat_path argument. Running without logstat\n");
	else
	{
		if ((dir = opendir(logstat_path)))
			closedir(dir);
		else
		{
			fprintf(stderr,"logstat_path is invalid!\n");
			return EXIT_FAILURE;
		}
	}

	/*Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask*/
	//load filter's can_id member
	NOx_id_dec = (NOx_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(NOx_id_dec, 0, sizeof(NOx_can_id));
	NOx_id_dec->flags = 4;//set the EFF
	NOx_id_dec->NOx_addr = NOx_filter; // Received Messages from NOx Sensors only
	//load filter's can_mask member
	NOx_id_dec = (NOx_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(NOx_id_dec, 0, sizeof(NOx_can_id));
	NOx_id_dec->flags = 4;//Received only messages with extended ID (29bit)
	NOx_id_dec->NOx_addr = NOx_mask; // Constant field of NOx_addr marked for examination
	setsockopt(CAN_socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));

	// Add timeout option to the CAN Socket
	tv.tv_sec = 1;//Timeout after 1 second.
	tv.tv_usec = 0;
	setsockopt(CAN_socket_num, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	//Bind CAN Socket to address
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if(bind(CAN_socket_num, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("Error in socket bind");
		exit(EXIT_FAILURE);
	}

	//Link signal SIGINT, SIGTERM and SIGPIPE to quit_signal_handler
	signal(SIGINT, quit_signal_handler);
	signal(SIGTERM, quit_signal_handler);
	signal(SIGPIPE, quit_signal_handler);
	Logger("Morfeas_NOX_if (%s) Program Started\n",stats.CAN_IF_name);
	Logger("Release: %s (%s)\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date());
	Logger("Version: "VERSION", Compiled Date: %s\n", Morfeas_get_compile_date());
	//Get SDAQ_NET Port config
	stats.port = get_port_num(stats.CAN_IF_name);
	if(stats.port>=0 && stats.port<=3)
	{
		//Init SDAQ_NET Port's CSA
		if(!MAX9611_init(stats.port, i2c_bus_num))
		{
			flags.port_meas_exist = 1;
			//Read Port's CSA Configuration from EEPROM
			if(!read_port_config(&port_meas_config, stats.port, i2c_bus_num))
			{
				flags.port_meas_isCal = 1;
				Logger("Port's Last Calibration: %u/%u/%u\n", port_meas_config.last_cal_date.month,
															  port_meas_config.last_cal_date.day,
															  port_meas_config.last_cal_date.year+12000);
				//Convert date from port_meas_config to struct tm Morfeas_RPi_Hat_last_cal
				Morfeas_RPi_Hat_last_cal.tm_mon = port_meas_config.last_cal_date.month - 1;
				Morfeas_RPi_Hat_last_cal.tm_mday = port_meas_config.last_cal_date.day;
				Morfeas_RPi_Hat_last_cal.tm_year = port_meas_config.last_cal_date.year + 100;
				stats.Morfeas_RPi_Hat_last_cal = mktime(&Morfeas_RPi_Hat_last_cal);//Convert Morfeas_RPi_Hat_last_cal to time_t
			}
			else
				Logger(Morfeas_hat_error());
		}
		else
		{
			Logger(Morfeas_hat_error());
			Logger("SDAQnet Port CSA not found!!!\n");
		}
	}
	else
	{
		stats.Bus_voltage = NAN;
		stats.Bus_amperage = NAN;
		stats.Shunt_temp = NAN;
	}
	//Get stored configuration
	if(NOX_handler_config_file(&stats, "r"))
		Logger("Error at reading of the configuration file !!!\n");
	//----Make of FIFO file----//
	mkfifo(Data_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	//Register handler to Morfeas_OPC-UA Server
	Logger("Morfeas_NOX_if (%s) Send Registration message to OPC-UA via IPC....\n",stats.CAN_IF_name);
	//Open FIFO for Write
	stats.FIFO_fd = open(Data_FIFO, O_WRONLY);
	IPC_Handler_reg_op(stats.FIFO_fd, NOX, stats.CAN_IF_name, REG);
	Logger("Morfeas_NOX_if (%s) Registered on OPC-UA\n",stats.CAN_IF_name);

	//Start D-Bus listener and ws_server functions in separate threads
	pthread_create(&DBus_listener_Thread_id, NULL, NOX_DBus_listener, &passer);
	pthread_create(&ws_server_Thread_id, NULL, Morfeas_NOX_ws_server, &passer);

	//-----Actions on the bus-----//
	NOx_id_dec = (NOx_can_id *)&(frame_rx.can_id);
	sprintf(IPC_msg.NOX_BUS_info.Dev_or_Bus_name,"%s",stats.CAN_IF_name);//Load BUSName to IPC_msg
	while(NOX_handler_run)//FSM of Morfeas_NOX_if
	{
		RX_bytes=read(CAN_socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			msg_cnt++;//increase message counter
			switch(NOx_id_dec->NOx_addr)
			{
				case NOx_low_addr:
					sensor_index = 0;
					pthread_mutex_lock(&NOX_access);
						stats.NOXs_data[0].meas_state = startcode.fields.meas_low_addr;
					pthread_mutex_unlock(&NOX_access);
					break;
				case NOx_high_addr:
					sensor_index = 1;
					pthread_mutex_lock(&NOX_access);
						stats.NOXs_data[1].meas_state = startcode.fields.meas_high_addr;
					pthread_mutex_unlock(&NOX_access);
					break;
				default: sensor_index = -1;
			}
			if(sensor_index>=0 && sensor_index<=1)//Decode and Load NOx sensor frame
			{
				stats.NOXs_data[sensor_index].last_seen = time(NULL);
				stats.dev_msg_cnt[sensor_index]++;
				//Decode and Load Sensor's status
				stats.NOXs_data[sensor_index].status.supply_in_range = NOx_data->Supply_valid == 1;
				stats.NOXs_data[sensor_index].status.in_temperature = NOx_data->Heater_valid == 1;
				stats.NOXs_data[sensor_index].status.is_NOx_value_valid = NOx_data->NOx_valid == 1;
				stats.NOXs_data[sensor_index].status.is_O2_value_valid = NOx_data->Oxygen_valid == 1;
				stats.NOXs_data[sensor_index].status.heater_mode_state = NOx_data->Heater_mode;
				//Decode and Load Sensor's errors
				stats.NOXs_data[sensor_index].errors.heater = NOx_error_dec(NOx_data->Heater_error);
				stats.NOXs_data[sensor_index].errors.NOx = NOx_error_dec(NOx_data->NOx_error);
				stats.NOXs_data[sensor_index].errors.O2 = NOx_error_dec(NOx_data->O2_error);
				//Decode, check, scale and load values.
				if(stats.NOXs_data[sensor_index].status.is_NOx_value_valid)
				{
					stats.NOXs_data[sensor_index].NOx_value = NOx_val_scaling(NOx_data->NOx_value);
					if(!stats.NOx_statistics[sensor_index].NOx_value_sample_cnt)
					{
						stats.NOx_statistics[sensor_index].NOx_value_min = stats.NOXs_data[sensor_index].NOx_value;
						stats.NOx_statistics[sensor_index].NOx_value_max = stats.NOXs_data[sensor_index].NOx_value;
					}
					else
					{
						if(stats.NOXs_data[sensor_index].NOx_value < stats.NOx_statistics[sensor_index].NOx_value_min)//calculate NOx min
							stats.NOx_statistics[sensor_index].NOx_value_min = stats.NOXs_data[sensor_index].NOx_value;
						if(stats.NOXs_data[sensor_index].NOx_value > stats.NOx_statistics[sensor_index].NOx_value_max)//calculate NOx max
							stats.NOx_statistics[sensor_index].NOx_value_max = stats.NOXs_data[sensor_index].NOx_value;
					}
					stats.NOx_statistics[sensor_index].NOx_value_acc += stats.NOXs_data[sensor_index].NOx_value;
					stats.NOx_statistics[sensor_index].NOx_value_sample_cnt++;
				}
				else
				{
					stats.NOXs_data[sensor_index].NOx_value = NAN;
					stats.NOx_statistics[sensor_index].NOx_value_acc = NAN;
					stats.NOx_statistics[sensor_index].NOx_value_min = NAN;
					stats.NOx_statistics[sensor_index].NOx_value_max = NAN;
					stats.NOx_statistics[sensor_index].NOx_value_sample_cnt = 0;
				}
				if(stats.NOXs_data[sensor_index].status.is_O2_value_valid)
				{
					stats.NOXs_data[sensor_index].O2_value = O2_val_scaling(NOx_data->O2_value);
					if(!stats.NOx_statistics[sensor_index].O2_value_sample_cnt)
					{
						stats.NOx_statistics[sensor_index].O2_value_min = stats.NOXs_data[sensor_index].O2_value;
						stats.NOx_statistics[sensor_index].O2_value_max = stats.NOXs_data[sensor_index].O2_value;
					}
					else
					{
						if(stats.NOXs_data[sensor_index].O2_value < stats.NOx_statistics[sensor_index].O2_value_min)//calculate O2 min
							stats.NOx_statistics[sensor_index].O2_value_min = stats.NOXs_data[sensor_index].O2_value;
						if(stats.NOXs_data[sensor_index].O2_value > stats.NOx_statistics[sensor_index].O2_value_max)//calculate O2 max
							stats.NOx_statistics[sensor_index].O2_value_max = stats.NOXs_data[sensor_index].O2_value;
					}
					stats.NOx_statistics[sensor_index].O2_value_acc += stats.NOXs_data[sensor_index].O2_value;
					stats.NOx_statistics[sensor_index].O2_value_sample_cnt++;
				}
				else
				{
					stats.NOXs_data[sensor_index].O2_value = NAN;
					stats.NOx_statistics[sensor_index].O2_value_acc = NAN;
					stats.NOx_statistics[sensor_index].O2_value_min = NAN;
					stats.NOx_statistics[sensor_index].O2_value_max = NAN;
					stats.NOx_statistics[sensor_index].O2_value_sample_cnt = 0;
				}
				Morfeas_NOX_ws_server_send_meas(stats.NOXs_data);
				if(stats.dev_msg_cnt[sensor_index] >= 2)//Send Status and measurements of current UniNOx sensors to Morfeas_opc_ua via IPC, Approx every 100ms.
				{
					stats.dev_msg_cnt[sensor_index] = 0;
					pthread_mutex_lock(&NOX_access);
						IPC_msg.NOX_data.IPC_msg_type = IPC_NOX_data;
						//sprintf(IPC_msg.NOX_data.Dev_or_Bus_name, "%s", stats.CAN_IF_name);
						IPC_msg.NOX_data.sensor_addr = sensor_index;
						memcpy(&(IPC_msg.NOX_data.NOXs_data), &(stats.NOXs_data[sensor_index]), sizeof(struct UniNOx_sensor));
						IPC_msg_TX(stats.FIFO_fd, &IPC_msg);
					pthread_mutex_unlock(&NOX_access);
				}
				if((t_now = time(NULL)) - t_bfr)//Approx every second
				{
					pthread_mutex_lock(&NOX_access);
						if(stats.auto_switch_off_value && startcode.as_byte)
						{
							stats.auto_switch_off_cnt++;
							if(stats.auto_switch_off_cnt > stats.auto_switch_off_value)
							{
								startcode.as_byte = 0;
								stats.auto_switch_off_cnt = 0;
							}
						}
						else
							stats.auto_switch_off_cnt = 0;
						NOx_heater(CAN_socket_num, startcode.as_byte);
					pthread_mutex_unlock(&NOX_access);
					flags.export_logstat = 1;
					t_bfr = t_now;
				}
			}
		}
		else if(NOX_handler_run)
			flags.export_logstat = 1;
		if(flags.export_logstat)
		{
			flags.export_logstat = 0;
			pthread_mutex_lock(&NOX_access);
				//Attempt to get if stats.
				if(!can_get_link_stats(stats.CAN_IF_name, &link_stats) && link_stats.rx_packets)
					stats.Bus_error_rate = ((float)link_stats.rx_errors/link_stats.rx_packets)*100.0;
				//Calculate CANBus utilization
				stats.Bus_util = 100.0*msg_cnt/MAX_CANBus_FPS;
				msg_cnt = 0;
				if(flags.port_meas_exist)
				{
					if(!get_port_meas(&port_meas, stats.port, i2c_bus_num))
					{
						if(flags.port_meas_isCal)
						{
							stats.Bus_voltage = (port_meas.port_voltage - port_meas_config.volt_meas_offset) * port_meas_config.volt_meas_scaler;
							stats.Bus_amperage = (port_meas.port_current - port_meas_config.curr_meas_offset) * port_meas_config.curr_meas_scaler;
						}
						else
						{
							stats.Bus_voltage = port_meas.port_voltage * MAX9611_default_volt_meas_scaler;
							stats.Bus_amperage = port_meas.port_current * MAX9611_default_current_meas_scaler;
						}
						stats.Shunt_temp = 32.0 + port_meas.temperature * MAX9611_temp_scaler;
					}
					IPC_msg.NOX_BUS_info.Electrics = -1;
					IPC_msg.NOX_BUS_info.voltage = stats.Bus_voltage;
					IPC_msg.NOX_BUS_info.amperage = stats.Bus_amperage;
					IPC_msg.NOX_BUS_info.shunt_temp = stats.Shunt_temp;
				}
				else
					IPC_msg.NOX_BUS_info.Electrics = 0;
				//Transfer BUS utilization and SDAQnet port measurements to opc_ua
				IPC_msg.NOX_BUS_info.IPC_msg_type = IPC_NOX_CAN_BUS_info;
				IPC_msg.NOX_BUS_info.BUS_utilization = stats.Bus_util;
				IPC_msg.NOX_BUS_info.Bus_error_rate = stats.Bus_error_rate;
				IPC_msg.NOX_BUS_info.auto_switch_off_value = stats.auto_switch_off_value;
				IPC_msg.NOX_BUS_info.auto_switch_off_cnt = stats.auto_switch_off_cnt;
				IPC_msg.NOX_BUS_info.Dev_on_bus = 0;
				for(int i=0; i<2; i++)
				{
					if((time(NULL) - stats.NOXs_data[i].last_seen) < 5)//5 seconds
					{
						IPC_msg.NOX_BUS_info.Dev_on_bus++;
						IPC_msg.NOX_BUS_info.active_devs[i] = -1;
					}
					else
						IPC_msg.NOX_BUS_info.active_devs[i] = 0;
				}
				IPC_msg_TX(stats.FIFO_fd, &IPC_msg);
				//Write Stats to Logstat JSON file
				logstat_NOX(logstat_path, &stats);
			pthread_mutex_unlock(&NOX_access);
		}
	}
	Logger("Morfeas_NOX_if (%s) Exiting...\n",stats.CAN_IF_name);
	pthread_join(ws_server_Thread_id, NULL);//Wait ws_server thread to end.
	pthread_detach(ws_server_Thread_id);//Deallocate ws_server thread's memory.
	pthread_join(DBus_listener_Thread_id, NULL);//Wait DBus_listener thread to end.
	pthread_detach(DBus_listener_Thread_id);//Deallocate DBus_listener thread's memory.
	NOx_heater(CAN_socket_num, 0);//Stop any Measurement.
	close(CAN_socket_num);//Close CAN_socket
	//Remove handler from Morfeas_OPC_UA Server
	IPC_Handler_reg_op(stats.FIFO_fd, NOX, stats.CAN_IF_name, UNREG);
	Logger("Morfeas_NOX_if (%s) Removed from OPC-UA\n", stats.CAN_IF_name);
	close(stats.FIFO_fd);
	//Delete logstat file
	if(logstat_path)
		delete_logstat_NOX(logstat_path, &stats);
	return EXIT_SUCCESS;
}

inline void quit_signal_handler(int signum)
{
	if(signum == SIGPIPE)
		fprintf(stderr,"IPC: Force Termination!!!\n");
	else if(!NOX_handler_run && signum == SIGINT)
		raise(SIGABRT);
	NOX_handler_run = 0;
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: Morfeas_NOX_if  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details see LICENSE.\n"
	};
	const char exp[] = {
	"\tCAN-IF: The name of the CAN-Bus adapter\n"
	"\tOptions:\n"
	"\t         -h : Print Help\n"
	"\t         -v : Print Version\n"
	"\t         -b : I2C Bus number for Morfeas_RPI_hat (Default: 1)\n"
	};
	printf("%s\nUsage: %s [options] CAN-IF [/path/to/logstat/directory] \n\n%s\n", preamp, prog_name, exp);
	return;
}

int CAN_if_bitrate_check(char *CAN_IF_name, int *bitrate)
{
	struct can_bittiming bt={0};
	if(!CAN_IF_name)
		return bitrate_check_error;
	if(can_get_bittiming(CAN_IF_name, &bt))
		return bitrate_check_error;
	*bitrate = bt.bitrate;
	if(bt.bitrate!=NOx_Bitrate)
		return bitrate_check_invalid;
	return bitrate_check_success;
}

//Read and write NOX_handler config file;
int NOX_handler_config_file(struct Morfeas_NOX_if_stats *stats, const char *mode)
{
	struct NOX_config_file_struct{
		struct{
			auto_switch_off_var value;
		} payload;
		unsigned char checksum;
	} config_file_data;
	FILE *fp;
	DIR *dir;
	size_t read_bytes;
	char *config_file_path;
	unsigned char checksum;
	int retval = EXIT_SUCCESS;

	if(!stats || !mode)
		return EXIT_FAILURE;
	if(!(config_file_path = calloc(strlen(Configs_dir)+strlen(stats->CAN_IF_name)+strlen("Morfeas_NOXs_")+10, sizeof(char))))
	{
		fprintf(stderr, "Memory Error!!!\n");
		exit(EXIT_FAILURE);
	}
	sprintf(config_file_path, Configs_dir"Morfeas_NOXs_%s", stats->CAN_IF_name);
	if(!strcmp(mode, "r"))
	{
		if((fp = fopen(config_file_path, mode)))
		{
			read_bytes = fread(&config_file_data, 1, sizeof(struct NOX_config_file_struct), fp);
			if(read_bytes == sizeof(struct NOX_config_file_struct))
			{
				checksum = Checksum(&(config_file_data.payload), sizeof(config_file_data.payload));
				if(!(config_file_data.checksum ^ checksum))
					stats->auto_switch_off_value = config_file_data.payload.value;
			}
			else
				retval = EXIT_FAILURE;
			fclose(fp);
		}
		else
			retval = EXIT_FAILURE;
	}
	else if(!strcmp(mode, "w"))
	{
		//Check the existence of the Morfeas_NOX_configs directory
		if ((dir = opendir(Configs_dir)))
			closedir(dir);
		else
		{
			Logger("Making NOX_Configurations_dir \n");
			if(mkdir(Configs_dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
			{
				perror("Error at MTI_Configurations_dir creation!!!");
				exit(EXIT_FAILURE);
			}
		}
		if((fp = fopen(config_file_path, mode)))
		{
			config_file_data.payload.value = stats->auto_switch_off_value;
			config_file_data.checksum = Checksum(&(config_file_data.payload), sizeof(config_file_data.payload));
			read_bytes = fwrite(&config_file_data, 1, sizeof(struct NOX_config_file_struct), fp);
			if(read_bytes != sizeof(struct NOX_config_file_struct))
				retval = EXIT_FAILURE;
			fclose(fp);
		}
		else
			retval = EXIT_FAILURE;
	}
	free(config_file_path);
	return retval;
}
