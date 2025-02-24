/*
Program: Morfeas_SDAQ_if. A controlling software for SDAQ-CAN Devices, part of morfeas_core project.
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
#define VERSION "1.0" /*Release Version of Morfeas_SDAQ_if software*/

#define LogBooks_dir "/var/tmp/Morfeas_LogBooks/"
#define SYNC_INTERVAL 10//seconds
#define LIFE_TIME 50// Value In seconds, define the time that a SDAQ_info_entry node defined as off-line and removed from the list
#define DEV_INFO_FAILED_RXs 2//Maximum amount of allowed failed receptions before re-query.
#define MAX_CANBus_FPS 3401.4 //Maximum amount of frames per sec for 500Kbaud
#define Ready_to_reg_mask 0x85 //Mask for SDAQ state: standby, no error and normal mode
#define SDAQ_ERROR_mask (1<<2) //Mask for SDAQ Error bit check

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

//Global variables
static volatile struct Morfeas_SDAQ_if_flags{
	unsigned run : 1;
	unsigned led_existent :1;
	unsigned port_meas_exist :1;
	unsigned port_meas_isCal :1;
	unsigned Clean_flag :1;
	unsigned bus_info :1;
	unsigned is_meas_started :1;
}flags = {.run=1,0};
static struct timespec tstart;
static int CAN_socket_num;


/* Local function (declaration)
 * Return value: EXIT_FAILURE(1) of failure or EXIT_SUCCESS(0) on success. Except of other notice
 */
void quit_signal_handler(int signum);//SIGINT handler function
void CAN_if_timer_handler(int signum);//sync timer handler function
void print_usage(char *prog_name);//print the usage manual

	/*Morfeas_SDAQ-if functions*/
//Function for Status LEDs
void led_stat(struct Morfeas_SDAQ_if_stats *stats);
//Logbook read and write from file;
int LogBook_file(struct Morfeas_SDAQ_if_stats *stats, const char *mode);
//Function to clean-up list_SDAQs from non active SDAQ, also send IPC msg to opc_ua for each dead SDAQ
int clean_up_list_SDAQs(struct Morfeas_SDAQ_if_stats *stats);
//Function that found and return the status of a node from the list_SDAQ with SDAQ_address == address. Used in FSM
struct SDAQ_info_entry * find_SDAQ(unsigned char address, struct Morfeas_SDAQ_if_stats *stats);
//Function that add or refresh SDAQ to lists list_SDAQ and LogBook, Return the data of node or NULL. Used in FSM
struct SDAQ_info_entry * add_or_refresh_SDAQ_to_lists(int socket_fd, sdaq_can_id *sdaq_id_dec, sdaq_status *status_dec, struct Morfeas_SDAQ_if_stats *stats);
//Function for Updating "Device Info" of a SDAQ. Used in FSM
int update_info(unsigned char address, sdaq_info *info_dec, struct Morfeas_SDAQ_if_stats *stats);
//Function for Updating Input mode of a SDAQ. Used in FSM
int update_input_mode(unsigned char address, sdaq_sysvar *sysvar_dec, struct Morfeas_SDAQ_if_stats *stats);
//Function for Updating "Calibration Date" of a SDAQ's channel. Used in FSM
int add_update_channel_date(unsigned char address, unsigned char channel, sdaq_calibration_date *date_dec, struct Morfeas_SDAQ_if_stats *stats);
//Function that add current meas to channel's accumulator of a SDAQ's channel. Used in FSM
int acc_meas(unsigned char channel, sdaq_meas *meas_dec, struct SDAQ_info_entry *sdaq_node);
//Function that find and return the amount of incomplete (with out all info and dates) nodes.
int incomplete_SDAQs(struct Morfeas_SDAQ_if_stats *stats);
//Function for Updating Time_diff (from debugging message) of a SDAQ. Used in FSM, also send IPC msg t opc_ua.
int update_Timediff(unsigned char address, sdaq_sync_debug_data *ts_dec, struct Morfeas_SDAQ_if_stats *stats);
//Function for construction of message for registration or update of a SDAQ
int IPC_SDAQ_reg_update(int FIFO_fd, char *CANBus_if_name, unsigned char address, sdaq_status *SDAQ_status, unsigned char reg_status, unsigned char amount);

	/*GSList related functions*/
void free_SDAQ_info_entry(gpointer node);//used with g_slist_free_full to free the data of each node of list_SDAQs
void free_LogBook_entry(gpointer node);//used with g_slist_free_full to free the data of each node of list LogBook

int main(int argc, char *argv[])
{
	DIR *dir;//Directory pointer variables
	//Operational variables
	int c;
	char *logstat_path;
	unsigned char i2c_bus_num = I2C_BUS_NUM;
	unsigned long msg_cnt=0;
	struct SDAQ_info_entry *SDAQ_data;
	//Variables for CANBus Port Electric
	struct tm Morfeas_RPi_Hat_last_cal = {0};
	struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config port_meas_config = {0};
	struct Morfeas_RPi_Hat_Port_meas port_meas = {0};
	//Variables for IPC
	IPC_message IPC_msg = {0};
	//Variables for Socket CAN and SDAQ_decoders
	int RX_bytes;
	struct timeval tv;
	struct ifreq ifr;
	struct sockaddr_can addr = {0};
	struct can_frame frame_rx;
	struct can_filter RX_filter;
	struct rtnl_link_stats64 link_stats = {0};
	sdaq_can_id *sdaq_id_dec;
	sdaq_status *status_dec = (sdaq_status *)frame_rx.data;
	sdaq_info *info_dec = (sdaq_info *)frame_rx.data;
	sdaq_sync_debug_data *ts_dec = (sdaq_sync_debug_data *)frame_rx.data;
	sdaq_calibration_date *date_dec = (sdaq_calibration_date *)frame_rx.data;
	sdaq_meas *meas_dec = (sdaq_meas *)frame_rx.data;
	sdaq_sysvar *sysvar_dec = (sdaq_sysvar *)frame_rx.data;
	//data_and stats of the Morfeas-SDAQ_IF
	struct Morfeas_SDAQ_if_stats stats = {0};
	//Timers related Variables
	struct itimerval timer;

	if(argc == 1)
	{
		print_usage(argv[0]);
		exit(1);
	}
	opterr = 1;
	while((c = getopt (argc, argv, "hvb:")) != -1)
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
	//Check if program already runs on same CAN-if.
	if(check_already_run_with_same_arg(argv[0], argv[optind]))
	{
		fprintf(stderr, "%s for interface \"%s\" Already Running!!!\n", argv[0], argv[optind]);
		exit(EXIT_SUCCESS);
	}

	//Check the existence of the LogBooks directory
	dir = opendir(LogBooks_dir);
	if (dir)
		closedir(dir);
	else
	{
		Logger("Making LogBooks_dir \n");
		if(mkdir(LogBooks_dir, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
		{
			perror("Error at LogBook file creation!!!");
			exit(EXIT_FAILURE);
		}
	}
	//Check if the size of the CAN-IF is bigger than the limit.
	if(strlen(argv[optind])>=Dev_or_Bus_name_str_size)
	{
		fprintf(stderr, "Interface name too big (>=%d)\n",Dev_or_Bus_name_str_size);
		exit(EXIT_FAILURE);
	}
	//CAN Socket Opening
	if((CAN_socket_num = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0)
	{
		perror("Error while opening socket");
		exit(EXIT_FAILURE);
	}
	//Link interface name to socket
	strncpy(ifr.ifr_name, argv[optind], IFNAMSIZ-1); // get value from CAN-IF arguments
	if(ioctl(CAN_socket_num, SIOCGIFINDEX, &ifr))
	{
		char if_error[30];
		sprintf(if_error, "CAN-IF (%s)",ifr.ifr_name);
		perror(if_error);
		exit(EXIT_FAILURE);
	}
	stats.CAN_IF_name = argv[optind];
	//Logstat.json
	if(!(logstat_path = argv[optind+1]))
		fprintf(stderr,"No logstat_path argument. Running without logstat\n");
	else
	{
		dir = opendir(logstat_path);
		if (dir)
			closedir(dir);
		else
		{
			fprintf(stderr,"logstat_path is invalid!\n");
			return EXIT_FAILURE;
		}
	}

	/*Filter for CAN messages	-- SocketCAN Filters act as: <received_can_id> & mask == can_id & mask*/
	//load filter's can_id member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_id;//Set encoder to filter.can_id
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//set the EFF, Received only messages with extended ID (29bit).
	sdaq_id_dec->protocol_id = PROTOCOL_ID;//Received Messages with protocol_id == PROTOCOL_ID
	sdaq_id_dec->payload_type = 0x80;//Received Messages with payload_type & 0x80 == TRUE, aka Master <- SDAQ.
	//load filter's can_mask member
	sdaq_id_dec = (sdaq_can_id *)&RX_filter.can_mask; //Set encoder to filter.can_mask
	memset(sdaq_id_dec, 0, sizeof(sdaq_can_id));
	sdaq_id_dec->flags = 4;//Flags field marked for examination.
	sdaq_id_dec->protocol_id = -1;//Protocol_id field marked for examination.
	sdaq_id_dec->payload_type = 0x80;//+ The most significant bit of Payload_type field marked for examination.
	setsockopt(CAN_socket_num, SOL_CAN_RAW, CAN_RAW_FILTER, &RX_filter, sizeof(RX_filter));

	// Add timeout option to the CAN Socket
	tv.tv_sec = 10;//interval time that a SDAQ send a Status/ID frame.
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

	//Link signal SIGALRM to timer's handler
	signal(SIGALRM, CAN_if_timer_handler);
	//Link signal SIGINT, SIGTERM and SIGPIPE to quit_signal_handler
	signal(SIGINT, quit_signal_handler);
	signal(SIGTERM, quit_signal_handler);
	signal(SIGPIPE, quit_signal_handler);
	Logger("Morfeas_SDAQ_if (%s) Program Started\n",stats.CAN_IF_name);
	Logger("Release: %s (%s)\n", Morfeas_get_curr_git_hash(), Morfeas_get_release_date());
	Logger("Version: "VERSION", Compiled Date: %s\n", Morfeas_get_compile_date());
	//Initialize the indication LEDs of the Morfeas-proto (sysfs implementation)
	if(!(flags.led_existent = LEDs_init(stats.CAN_IF_name)))
		Logger(Morfeas_hat_error());
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
				Logger("SDAQnet Last Calibration: %u/%u/%u\n", port_meas_config.last_cal_date.month,
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
	//Load the LogBook file to LogBook List
	Logger("Morfeas_SDAQ_if (%s) Read of LogBook file\n", stats.CAN_IF_name);
	sprintf(stats.LogBook_file_path, "%sMorfeas_SDAQ_if_%s_LogBook", LogBooks_dir, stats.CAN_IF_name);
	RX_bytes = LogBook_file(&stats, "r");
	if(RX_bytes>0)
		Logger("IO Error on LogBook File!!!\n");
	else if(RX_bytes<0)
		Logger("Checksum Error on LogBook File!!!\n");
	//----Make of FIFO file----//
	mkfifo(Data_FIFO, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
	//Register handler to Morfeas_OPC-UA Server
	Logger("Morfeas_SDAQ_if (%s) Send Registration message to OPC-UA via IPC....\n", stats.CAN_IF_name);
	//Open FIFO for Write
	stats.FIFO_fd = open(Data_FIFO, O_WRONLY);
	IPC_Handler_reg_op(stats.FIFO_fd, SDAQ, stats.CAN_IF_name, 0);
	Logger("Morfeas_SDAQ_if (%s) Registered on OPC-UA\n", stats.CAN_IF_name);
	//Initialize Sync timer expired time
	memset (&timer, 0, sizeof(struct itimerval));
	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = timer.it_interval.tv_sec;
	timer.it_value.tv_usec = timer.it_interval.tv_usec;
	//Get start time
	clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
	//Start timer
	setitimer(ITIMER_REAL, &timer, NULL);

	//-----Init Actions-----//
	memccpy(IPC_msg.SDAQ_meas.Dev_or_Bus_name, stats.CAN_IF_name,'\0', Dev_or_Bus_name_str_size);//Init Bus_name field of IPC_msg.
	IPC_msg.SDAQ_meas.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	sdaq_id_dec = (sdaq_can_id *)&(frame_rx.can_id);//Point ID decoder to ID field from frame_rx
	Stop(CAN_socket_num, Broadcast);//Stop any measuring activity on the bus
	while(flags.run)//FSM of Morfeas_SDAQ_if
	{
		RX_bytes=read(CAN_socket_num, &frame_rx, sizeof(frame_rx));
		if(RX_bytes==sizeof(frame_rx))
		{
			switch(sdaq_id_dec->payload_type)
			{
				case Measurement_value:
					if(frame_rx.can_dlc == sizeof(sdaq_meas))
					{
						if(!flags.is_meas_started)//Check if SDAQ remain in meas after of a Stop.
							Stop(CAN_socket_num, sdaq_id_dec->device_addr);
						else if((SDAQ_data = find_SDAQ(sdaq_id_dec->device_addr, &stats)))
						{
							time(&(SDAQ_data->last_seen));//Update last seen time
							if(logstat_path)//Add current measurements to Average Accumulator
								acc_meas(sdaq_id_dec->channel_num, meas_dec, SDAQ_data);//Add meas to acc
							if(SDAQ_data->SDAQ_Channels_curr_meas && sdaq_id_dec->channel_num <= SDAQ_data->SDAQ_info.num_of_ch)
							{	//Load meas to Current meas buffer
								memcpy(&(SDAQ_data->SDAQ_Channels_curr_meas[sdaq_id_dec->channel_num-1]), meas_dec, sizeof(struct Channel_curr_meas));
								if(sdaq_id_dec->channel_num == SDAQ_data->SDAQ_info.num_of_ch)
								{	//Send measurement through IPC
									IPC_msg.SDAQ_meas.IPC_msg_type = IPC_SDAQ_meas;
									IPC_msg.SDAQ_meas.SDAQ_serial_number = SDAQ_data->SDAQ_status.dev_sn;
									IPC_msg.SDAQ_meas.Amount_of_channels = SDAQ_data->SDAQ_info.num_of_ch;
									IPC_msg.SDAQ_meas.Last_Timestamp = meas_dec->timestamp;
									memcpy(&(IPC_msg.SDAQ_meas.SDAQ_channel_meas),
											 SDAQ_data->SDAQ_Channels_curr_meas,
											 sizeof(struct Channel_curr_meas)*SDAQ_data->SDAQ_info.num_of_ch);
									IPC_msg_TX(stats.FIFO_fd, &IPC_msg);
								}
							}
						}
					}
					break;
				case Sync_Info:
					update_Timediff(sdaq_id_dec->device_addr, ts_dec, &stats);
					break;
				case Device_status:
					if(frame_rx.can_dlc != sizeof(sdaq_status))
						break;
					if((SDAQ_data = add_or_refresh_SDAQ_to_lists(CAN_socket_num, sdaq_id_dec, status_dec, &stats)))
					{
						if(!(status_dec->status & Ready_to_reg_mask))
						{	//Check if current SDAQ is not registered.
							if(!SDAQ_data->reg_status)
							{
								Logger("Register new SDAQ (%s) with S/N: %010u(0x%08x) -> Address: %02hhu\n", dev_type_str[status_dec->dev_type],
																											  status_dec->dev_sn,
																											  status_dec->dev_sn,
																											  SDAQ_data->SDAQ_address);
								SDAQ_data->reg_status = Registered;
								if(flags.is_meas_started)
								{
									Stop(CAN_socket_num, Broadcast);
									flags.is_meas_started = 0;
									SDAQ_data->failed_reg_RX_CNT = -1;
								}
								else
								{
									QueryDeviceInfo(CAN_socket_num, SDAQ_data->SDAQ_address);
									SDAQ_data->failed_reg_RX_CNT = 0;
								}
							}
							else if(SDAQ_INFO_PENDING_CHECK(SDAQ_data->reg_status))//Check if current SDAQ's send status and reg_status isn't "Ready".
							{
								if(flags.is_meas_started)
								{
									Stop(CAN_socket_num, Broadcast);
									flags.is_meas_started = 0;
								}
								else
								{
									if(SDAQ_data->failed_reg_RX_CNT >= DEV_INFO_FAILED_RXs)
									{
										SDAQ_data->failed_reg_RX_CNT = 0;
										switch(SDAQ_data->reg_status)
										{
											case Registered:
											case Pending_Calibration_data:
												QueryDeviceInfo(CAN_socket_num, SDAQ_data->SDAQ_address);
												break;
											case Pending_input_mode:
												QuerySystemVariables(CAN_socket_num, SDAQ_data->SDAQ_address);
												break;
										}
									}
									else
										SDAQ_data->failed_reg_RX_CNT++;
								}
							}//Check if all SDAQ is registered, and if yes put the current one in measure mode
							else if(SDAQ_data->reg_status == Ready && !(stats.incomplete_SDAQs = incomplete_SDAQs(&stats)))
							{
								Start(CAN_socket_num, sdaq_id_dec->device_addr);
								flags.is_meas_started = 1;
								Logger("Start %s -> Address: %02hhu\n", dev_type_str[status_dec->dev_type],
																		SDAQ_data->SDAQ_address);
							}
						}
						else if(status_dec->status & SDAQ_ERROR_mask)//Error flag in status is set.
							Logger("SDAQ (%s) with S/N: %010u(0x%08x) -> Address: %02hhu Report ERROR!!!\n", dev_type_str[status_dec->dev_type],
																											 status_dec->dev_sn,
																											 status_dec->dev_sn,
																											 SDAQ_data->SDAQ_address);
						stats.incomplete_SDAQs = incomplete_SDAQs(&stats);
						IPC_SDAQ_reg_update(stats.FIFO_fd, stats.CAN_IF_name, SDAQ_data->SDAQ_address, status_dec, SDAQ_data->reg_status, stats.detected_SDAQs);
					}
					else
						Logger("Maximum amount of addresses reached!!!!\n");
					break;
				case Device_info:
					if(frame_rx.can_dlc == sizeof(sdaq_info))
						update_info(sdaq_id_dec->device_addr, info_dec, &stats);
					break;
				case System_variable:
					if(frame_rx.can_dlc == sizeof(sdaq_sysvar))
						update_input_mode(sdaq_id_dec->device_addr, sysvar_dec, &stats);
					break;
				case Calibration_Date:
					if(frame_rx.can_dlc == sizeof(sdaq_calibration_date))
						add_update_channel_date(sdaq_id_dec->device_addr, sdaq_id_dec->channel_num, date_dec, &stats);
					break;
			}
			msg_cnt++;//Increase message counter
		}
		if(flags.Clean_flag)
		{
			clean_up_list_SDAQs(&stats);
			flags.Clean_flag = 0;
			if(!stats.detected_SDAQs)
				flags.is_meas_started = 0;
		}
		if(flags.bus_info)
		{	//Attempt to get if stats.
			if(!can_get_link_stats(stats.CAN_IF_name, &link_stats) && link_stats.rx_packets)
				stats.Bus_error_rate = ((float)link_stats.rx_errors/link_stats.rx_packets)*100.0;
			//Calculate CANBus utilization
			stats.Bus_util = 100.0*msg_cnt/MAX_CANBus_FPS;
			msg_cnt = 0;
			flags.bus_info = 0;
			//Get Electrics for BUS port
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
				IPC_msg.SDAQ_BUS_info.Electrics = -1;
				IPC_msg.SDAQ_BUS_info.voltage = stats.Bus_voltage;
				IPC_msg.SDAQ_BUS_info.amperage = stats.Bus_amperage;
				IPC_msg.SDAQ_BUS_info.shunt_temp = stats.Shunt_temp;
			}
			else
				IPC_msg.SDAQ_BUS_info.Electrics = 0;
			//Transfer bus utilization to opc_ua
			IPC_msg.SDAQ_BUS_info.IPC_msg_type = IPC_SDAQ_CAN_BUS_info;
			IPC_msg.SDAQ_BUS_info.BUS_utilization = stats.Bus_util;
			IPC_msg.SDAQ_BUS_info.Bus_error_rate = stats.Bus_error_rate;
			IPC_msg_TX(stats.FIFO_fd, &IPC_msg);
			//Write Stats to Logstat JSON file
			logstat_SDAQ(logstat_path, &stats);
		}
		led_stat(&stats);
	}
	Logger("Morfeas_SDAQ_if (%s) Exiting...\n",stats.CAN_IF_name);
	//Save LogBook list to a file before destroy it
	Logger("Save LogBook list to LogBook file\n");
	LogBook_file(&stats,"w");
	//Free all lists
	Logger("Free allocated memory...\n");
	g_slist_free_full(stats.list_SDAQs, free_SDAQ_info_entry);
	g_slist_free_full(stats.LogBook, free_LogBook_entry);
	//Stop any measuring activity on the bus
	Logger("Send Stop measure to SDAQs...\n");
	Stop(CAN_socket_num,Broadcast);
	close(CAN_socket_num);//Close CAN_socket
	//Remove Registeration handler to Morfeas_OPC_UA Server
	IPC_Handler_reg_op(stats.FIFO_fd, SDAQ, stats.CAN_IF_name, 1);
	Logger("Morfeas_SDAQ_if (%s) Removed from OPC-UA\n",stats.CAN_IF_name);
	close(stats.FIFO_fd);
	//Delete logstat file
	if(logstat_path)
		delete_logstat_SDAQ(logstat_path, &stats);
	return EXIT_SUCCESS;
}

inline void quit_signal_handler(int signum)
{
	if(signum == SIGPIPE)
		fprintf(stderr,"IPC: Force Termination!!!\n");
	else if(!flags.run && signum == SIGINT)
		raise(SIGABRT);
	flags.run = 0;
}

inline void CAN_if_timer_handler (int signum)
{
	static unsigned char timer_ring_cnt = SYNC_INTERVAL, cleanup_trig_cnt = 20/SYNC_INTERVAL;
	unsigned short time_seed;
	struct timespec time_rep = {0,0};
	timer_ring_cnt--;
	if(!timer_ring_cnt)
	{
		clock_gettime(CLOCK_MONOTONIC_RAW, &time_rep);
		time_seed = (time_rep.tv_nsec - tstart.tv_nsec)/1000000;
		time_seed += (time_rep.tv_sec - tstart.tv_sec)*1000;
		if(time_seed>=60000)
		{
			clock_gettime(CLOCK_MONOTONIC_RAW, &tstart);
			time_seed -= 60000;
		}
		//Check if is time for Clean up
		if(!cleanup_trig_cnt)
		{	//Clean up cycle trig
			flags.Clean_flag=1;//Trig a clean up of list_SDAQ.
			cleanup_trig_cnt=20/SYNC_INTERVAL;//Approximately every 20 sec
		}
		cleanup_trig_cnt--;
		if(flags.is_meas_started)
			Sync(CAN_socket_num, time_seed);//Send Synchronization with time_seed to all SDAQs
		timer_ring_cnt = SYNC_INTERVAL;//Reset timer_ring_cnt
	}
	flags.bus_info = 1;
	return;
}

void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: Morfeas_SDAQ_if  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
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

void led_stat(struct Morfeas_SDAQ_if_stats *stats)
{
	static struct{
		unsigned Bus_util : 1;
		unsigned Max_dev_num : 1;
	}leds_status = {0};

	if(flags.led_existent)
	{
		if(stats->Bus_util>95.0)
		{
			LED_write(BLUE_LED, 1);
			if(!leds_status.Bus_util)
				Logger("Bus Utilization is in High Level(>95%%) !!!!\n");
			leds_status.Bus_util = 1;
		}
		else if(stats->Bus_util<=80.0 && leds_status.Bus_util)
		{
			LED_write(BLUE_LED, 0);
			leds_status.Bus_util = 0;
			Logger("Bus Utilization restore to Normal Level(<80%%)!!!!\n");
		}

		if(stats->detected_SDAQs>=60)
		{
			LED_write(RED_LED, 1);
			leds_status.Max_dev_num = 1;
		}
		else
		{
			if(leds_status.Max_dev_num)
			{
				LED_write(RED_LED, 0);
				leds_status.Max_dev_num = 0;
				Logger("Amount of SDAQs restored to Normal (<60)!!!!\n");
			}
		}
	}
}
	/*Lists related function implementation*/
//SDAQ_info_entry allocator
struct SDAQ_info_entry* new_SDAQ_info_entry()
{
    struct SDAQ_info_entry *new_node = g_slice_new0(struct SDAQ_info_entry);
    return new_node;
}
//Channel_date_entry allocator
struct Channel_date_entry* new_SDAQ_Channel_date_entry()
{
    struct Channel_date_entry *new_node = g_slice_new0(struct Channel_date_entry);
    return new_node;
}
//Channel_acc_meas_entry allocator
struct Channel_acc_meas_entry* new_SDAQ_Channel_acc_meas_entry()
{
    struct Channel_acc_meas_entry *new_node = g_slice_new0(struct Channel_acc_meas_entry);
    return new_node;
}
//LogBook_entry allocator
struct LogBook_entry* new_LogBook_entry()
{
    struct LogBook_entry *new_node = g_slice_new0(struct LogBook_entry);
    return new_node;
}

//Free a node from list SDAQ_Channels_cal_dates
void free_channel_cal_dates_entry(gpointer node)
{
	g_slice_free(struct Channel_date_entry, node);
}
//Free a node from list SDAQ_Channels_acc_meas
void free_channel_acc_meas_entry(gpointer node)
{
	g_slice_free(struct Channel_acc_meas_entry, node);
}
//Free a node from list SDAQ_info
void free_SDAQ_info_entry(gpointer node)
{
	struct SDAQ_info_entry *node_dec = node;
	g_slist_free_full(node_dec->SDAQ_Channels_cal_dates, free_channel_cal_dates_entry);
	node_dec->SDAQ_Channels_cal_dates = NULL;
	g_slist_free_full(node_dec->SDAQ_Channels_acc_meas, free_channel_acc_meas_entry);
	node_dec->SDAQ_Channels_acc_meas = NULL;
	if(node_dec->SDAQ_Channels_curr_meas)
	{
		free(node_dec->SDAQ_Channels_curr_meas);
		node_dec->SDAQ_Channels_curr_meas = NULL;
	}
	g_slice_free(struct SDAQ_info_entry, node);
}
//Free a node from List LogBook
void free_LogBook_entry(gpointer node)
{
	g_slice_free(struct LogBook_entry, node);
}

/*
	Comparing function used in g_slist_find_custom, comp arg address to node's Address.
*/
gint SDAQ_info_entry_find_address (gconstpointer node, gconstpointer arg)
{
	const unsigned char *arg_t = arg;
	struct SDAQ_info_entry *node_dec = (struct SDAQ_info_entry *) node;
	return node_dec->SDAQ_address == *arg_t ? 0 : 1;
}

/*
	Comparing function used in g_slist_find_custom, comp arg S/N to node's S/N.
*/
gint SDAQ_info_entry_find_serial_number (gconstpointer node, gconstpointer arg)
{
	const unsigned int *arg_t = arg;
	struct SDAQ_info_entry *node_dec = (struct SDAQ_info_entry *) node;
	return node_dec->SDAQ_status.dev_sn == *arg_t ? 0 : 1;
}

/*
	Comparing function used in g_slist_find_custom, comp arg S/N to node's S/N.
*/
gint LogBook_entry_find_serial_number (gconstpointer node, gconstpointer arg)
{
	const unsigned int *arg_t = arg;
	struct LogBook_entry *node_dec = (struct LogBook_entry *) node;
	return node_dec->SDAQ_sn == *arg_t ? 0 : 1;
}

/*
	Comparing function used in g_slist_find_custom, comp arg address to node's Address.
*/
gint LogBook_entry_find_address (gconstpointer node, gconstpointer arg)
{
	const unsigned char *arg_t = arg;
	struct SDAQ_info_entry *node_dec = (struct SDAQ_info_entry *) node;
	return node_dec->SDAQ_address == *arg_t ? 0 : 1;
}

/*
	Comparing function used in g_slist_insert_sorted,
*/
gint SDAQ_info_entry_cmp (gconstpointer a, gconstpointer b)
{
	if(((struct SDAQ_info_entry *)a)->SDAQ_address != ((struct SDAQ_info_entry *)b)->SDAQ_address)
		return (((struct SDAQ_info_entry *)a)->SDAQ_address <= ((struct SDAQ_info_entry *)b)->SDAQ_address) ?  0 : 1;
	else
		return (((struct SDAQ_info_entry *)a)->SDAQ_status.dev_sn <= ((struct SDAQ_info_entry *)b)->SDAQ_status.dev_sn) ?  0 : 1;
}

//Logbook read and write from file;
int LogBook_file(struct Morfeas_SDAQ_if_stats *stats, const char *mode)
{
	FILE *fp;
	GSList *LogBook_node = stats->LogBook;
	struct LogBook_entry *node_data;
	struct LogBook data;
	size_t read_bytes;
	unsigned char checksum;

	if(!strcmp(mode, "r"))
	{
		if(stats->LogBook)
		{
			g_slist_free_full(stats->LogBook, free_LogBook_entry);
			stats->LogBook = NULL;
		}
		fp=fopen(stats->LogBook_file_path, mode);
		if(fp)
		{
			do{
				read_bytes = fread(&data, 1, sizeof(data), fp);
				if(read_bytes == sizeof(struct LogBook))
				{
					checksum = Checksum(&(data.payload), sizeof(data.payload));
					if(!(data.checksum ^ checksum))
					{
						if(!(node_data = new_LogBook_entry()))
						{
							fprintf(stderr,"Memory Error!!!\n");
							exit(EXIT_FAILURE);
						}
						memcpy(node_data, &(data.payload), sizeof(data.payload));
						stats->LogBook = g_slist_append(stats->LogBook, node_data);
					}
					else
					{
						g_slist_free_full(stats->LogBook, free_LogBook_entry);
						stats->LogBook = NULL;
						if((fp = freopen(stats->LogBook_file_path, "w", fp)))
							fclose(fp);
						return -1;
					}
				}
			}while(read_bytes == sizeof(struct LogBook));
			fclose(fp);
		}
		else
			return EXIT_FAILURE;
	}
	else if(!strcmp(mode, "w"))
	{	//Check if list LogBook have elements
		if(LogBook_node)
		{
			fp=fopen(stats->LogBook_file_path, mode);
			if(fp)
			{	//Store all the nodes of list LogBook in file
				while(LogBook_node)
				{
					node_data = LogBook_node->data;
					if(node_data)
					{
						checksum = Checksum(node_data, sizeof(struct LogBook_entry));
						fwrite (node_data, 1, sizeof(struct LogBook_entry), fp);
						fwrite (&checksum, 1, sizeof(checksum), fp);
					}
					LogBook_node = LogBook_node -> next;//Next node
				}
				fclose(fp);
			}
			else
			{
				Logger("Error on LogBook file Write !!!!\n");
				return EXIT_FAILURE;
			}
		}
	}
	else if(!strcmp(mode, "a"))
	{	//Check if list_SDAQs have elements
		if(stats->list_SDAQs)
		{
			while(LogBook_node->next)//Find last node
				LogBook_node = LogBook_node->next;//Next node
			fp=fopen(stats->LogBook_file_path, mode);
			if(fp)
			{
				node_data = LogBook_node->data;
				checksum = Checksum(node_data, sizeof(struct LogBook_entry));
				//Store last node of list LogBook in file
				fwrite (node_data, 1, sizeof(struct LogBook_entry), fp);
				fwrite (&checksum, 1, sizeof(checksum), fp);
				fclose(fp);
			}
			else
			{
				Logger("Error on LogBook file Appending !!!!\n");
				return EXIT_FAILURE;
			}
		}
		else
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
//Function that find and return the amount of incomplete (incomplete info and/or dates) nodes.
int incomplete_SDAQs(struct Morfeas_SDAQ_if_stats *stats)
{
	unsigned int incomp_amount=0;
	GSList *list_SDAQs = stats->list_SDAQs;
	struct SDAQ_info_entry *node_data;
	while(list_SDAQs)
	{
		node_data = list_SDAQs->data;
		if(node_data->reg_status < Ready)
			incomp_amount++;
		list_SDAQs = list_SDAQs->next;
	}
	return incomp_amount;
}

short time_diff_cal(unsigned short dev_time, unsigned short ref_time)
{
	short ret = dev_time > ref_time ? dev_time - ref_time : ref_time - dev_time;
	if(ret<0)
		ret = 60000 - dev_time - ref_time;
	return ret;
}
/*Function for Updating Time_diff (from debugging message) of a SDAQ. Used in FSM*/
int update_Timediff(unsigned char address, sdaq_sync_debug_data *ts_dec, struct Morfeas_SDAQ_if_stats *stats)
{
	IPC_message IPC_msg = {0};
	GSList *list_node;
	struct SDAQ_info_entry *sdaq_node;
	if (stats->list_SDAQs)
	{
		list_node = g_slist_find_custom(stats->list_SDAQs, &address, SDAQ_info_entry_find_address);
		if(list_node)
		{
			sdaq_node = list_node->data;
			sdaq_node->Timediff = time_diff_cal(ts_dec->dev_time, ts_dec->ref_time);
			time(&(sdaq_node->last_seen));
			//Send timediff over IPC
			IPC_msg.SDAQ_timediff.IPC_msg_type = IPC_SDAQ_timediff;
			sprintf(IPC_msg.SDAQ_timediff.Dev_or_Bus_name,"%s",stats->CAN_IF_name);
			IPC_msg.SDAQ_timediff.SDAQ_serial_number = sdaq_node->SDAQ_status.dev_sn;
			IPC_msg.SDAQ_timediff.Timediff = sdaq_node->Timediff;
			IPC_msg_TX(stats->FIFO_fd, &IPC_msg);
		}
		else
			return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
/*
	Comparing function used in g_slist_find_custom, comp arg channel to Channel_date_entry node's channel.
*/
gint SDAQ_Channels_cal_dates_entry_find_channel (gconstpointer node, gconstpointer arg)
{
	const unsigned char *arg_t = arg;
	struct Channel_date_entry *node_dec = (struct Channel_date_entry *) node;
	return node_dec->Channel == *arg_t ? 0 : 1;
}
/*
	Comparing function used in g_slist_find_custom, comp arg channel to Channel_acc_meas_entry node's channel.
*/
gint SDAQ_Channels_acc_meas_entry_find_channel (gconstpointer node, gconstpointer arg)
{
	const unsigned char *arg_t = arg;
	struct Channel_acc_meas_entry *node_dec = (struct Channel_acc_meas_entry *) node;
	return node_dec->Channel == *arg_t ? 0 : 1;
}
/*
	Comparing function used in g_slist_insert_sorted,
*/
gint SDAQ_Channels_cal_dates_entry_cmp (gconstpointer a, gconstpointer b)
{
	return (((struct Channel_date_entry *)a)->Channel <= ((struct Channel_date_entry *)b)->Channel) ?  0 : 1;
}
/*
	Comparing function used in g_slist_insert_sorted,
*/
gint SDAQ_Channels_acc_meas_entry_cmp (gconstpointer a, gconstpointer b)
{
	return (((struct Channel_acc_meas_entry *)a)->Channel <= ((struct Channel_acc_meas_entry *)b)->Channel) ?  0 : 1;
}
/*Function for Updating "Device Info" of a SDAQ. Used in FSM*/
int update_info(unsigned char address, sdaq_info *info_dec, struct Morfeas_SDAQ_if_stats *stats)
{
	IPC_message IPC_msg = {0};
	GSList *list_node;
	struct SDAQ_info_entry *sdaq_node;

	if(!info_dec->num_of_ch || !info_dec->sample_rate || !info_dec->max_cal_point)
		return EXIT_FAILURE;
	if(stats->list_SDAQs)
	{
		list_node = g_slist_find_custom(stats->list_SDAQs, &address, SDAQ_info_entry_find_address);
		if(list_node)
		{
			sdaq_node = list_node->data;
			memcpy(&(sdaq_node->SDAQ_info), info_dec, sizeof(sdaq_info));
			time(&(sdaq_node->last_seen));
			if(sdaq_node->reg_status < Pending_Calibration_data)
			{
				sdaq_node->reg_status = Pending_Calibration_data;
				sdaq_node->failed_reg_RX_CNT = 0;
			}
			//Release and Allocate memory for the Channels_current_meas
			if(sdaq_node->SDAQ_Channels_curr_meas)
				free(sdaq_node->SDAQ_Channels_curr_meas);
			if(!(sdaq_node->SDAQ_Channels_curr_meas = calloc(info_dec->num_of_ch, sizeof(struct Channel_curr_meas))))
			{
				fprintf(stderr,"Memory error!!!\n");
				exit(EXIT_FAILURE);
			}
			//Send info through IPC
			IPC_msg.SDAQ_info.IPC_msg_type = IPC_SDAQ_info;
			sprintf(IPC_msg.SDAQ_info.Dev_or_Bus_name,"%s",stats->CAN_IF_name);
			IPC_msg.SDAQ_info.SDAQ_serial_number = sdaq_node->SDAQ_status.dev_sn;
			memcpy(&(IPC_msg.SDAQ_info.SDAQ_info_data), info_dec, sizeof(sdaq_info));
			IPC_msg_TX(stats->FIFO_fd, &IPC_msg);
		}
		else
			return EXIT_FAILURE;
	}
	else
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
/*Function for Updating Input mode of a SDAQ. Used in FSM*/
int update_input_mode(unsigned char address, sdaq_sysvar *sysvar_dec, struct Morfeas_SDAQ_if_stats *stats)
{
	IPC_message IPC_msg = {0};
	GSList *list_node;
	struct SDAQ_info_entry *sdaq_node;

	if(sysvar_dec->type)//Return if SDAQ's sysvar msg have types >0.
		return EXIT_SUCCESS;
	if(stats->list_SDAQs)
	{
		list_node = g_slist_find_custom(stats->list_SDAQs, &address, SDAQ_info_entry_find_address);
		if(list_node)
		{
			sdaq_node = list_node->data;
			time(&(sdaq_node->last_seen));
			if(*dev_input_mode_str[sdaq_node->SDAQ_info.dev_type])//Check if selected dev_type of sdaq_node have available input mode.
			{	//Send SDAQ's input mode through IPC
				IPC_msg.SDAQ_inpMode.IPC_msg_type = IPC_SDAQ_inpMode;
				sprintf(IPC_msg.SDAQ_inpMode.Dev_or_Bus_name,"%s",stats->CAN_IF_name);
				IPC_msg.SDAQ_inpMode.SDAQ_serial_number = sdaq_node->SDAQ_status.dev_sn;
				IPC_msg.SDAQ_inpMode.Dev_type = sdaq_node->SDAQ_info.dev_type;
				IPC_msg.SDAQ_inpMode.Input_mode = sysvar_dec->var_val.as_uint32;
				IPC_msg_TX(stats->FIFO_fd, &IPC_msg);
				//Set or Update sdaq_node's inp_mode.
				sdaq_node->inp_mode = sysvar_dec->var_val.as_uint32;
				//Check reg_status, and start meas if needed.
				if(sdaq_node->reg_status < Ready)
				{
					sdaq_node->reg_status = Ready;
					sdaq_node->failed_reg_RX_CNT = 0;
				}
			}
			else
				return EXIT_SUCCESS;
		}
		else
			return EXIT_FAILURE;
	}
	else
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
//Function that found and return the status of a node from the list_SDAQ with SDAQ_address == address, Used in FSM
struct SDAQ_info_entry * find_SDAQ(unsigned char address, struct Morfeas_SDAQ_if_stats *stats)
{
	GSList *list_node = g_slist_find_custom(stats->list_SDAQs, &address, SDAQ_info_entry_find_address);
	if(list_node)
		return (struct SDAQ_info_entry*)(list_node->data);
	return NULL;
}
/*Function for Updating "Calibration Date" of a SDAQ's channel. Used in FSM*/
int add_update_channel_date(unsigned char address, unsigned char channel, sdaq_calibration_date *date_dec, struct Morfeas_SDAQ_if_stats *stats)
{
	IPC_message IPC_msg = {0};
	GSList *list_node, *date_list_node;
	struct SDAQ_info_entry *sdaq_node;
	struct Channel_date_entry *sdaq_Channels_cal_dates_node;

	if(stats->list_SDAQs)
	{
		list_node = g_slist_find_custom(stats->list_SDAQs, &address, SDAQ_info_entry_find_address);
		if(list_node)
		{
			sdaq_node = list_node->data;
			time(&(sdaq_node->last_seen));
			if(sdaq_node->reg_status < Pending_Calibration_data)//Check, if "cal_date" msg comes before "Dev_info".
				return EXIT_FAILURE;
			date_list_node = g_slist_find_custom(sdaq_node->SDAQ_Channels_cal_dates, &channel, SDAQ_Channels_cal_dates_entry_find_channel);
			if(date_list_node)//Channel is already in to SDAQ_Channels_cal_dates list: Update CH_date.
			{
				sdaq_Channels_cal_dates_node = date_list_node->data;
				memcpy(&(sdaq_Channels_cal_dates_node->CH_date), date_dec, sizeof(sdaq_calibration_date));
			}
			else//Channel is not in SDAQ_Channels_cal_dates list: Create CH_date entry.
			{
				sdaq_Channels_cal_dates_node = new_SDAQ_Channel_date_entry();
				if(sdaq_Channels_cal_dates_node)
				{
					sdaq_Channels_cal_dates_node->Channel = channel;
					memcpy(&(sdaq_Channels_cal_dates_node->CH_date), date_dec, sizeof(sdaq_calibration_date));
					sdaq_node->SDAQ_Channels_cal_dates = g_slist_insert_sorted(sdaq_node->SDAQ_Channels_cal_dates,
																			   sdaq_Channels_cal_dates_node,
																			   SDAQ_Channels_cal_dates_entry_cmp);
				}
				else
				{
					fprintf(stderr,"Memory error!!!\n");
					exit(EXIT_FAILURE);
				}
			}
			//Send calibration data via IPC.
			IPC_msg.SDAQ_cal_date.IPC_msg_type = IPC_SDAQ_cal_date;
			sprintf(IPC_msg.SDAQ_cal_date.Dev_or_Bus_name,"%s",stats->CAN_IF_name);
			IPC_msg.SDAQ_cal_date.SDAQ_serial_number = sdaq_node->SDAQ_status.dev_sn;
			IPC_msg.SDAQ_cal_date.channel = channel;
			memcpy(&(IPC_msg.SDAQ_cal_date.SDAQ_cal_date), date_dec, sizeof(sdaq_calibration_date));
			IPC_msg_TX(stats->FIFO_fd, &IPC_msg);
			//Check, if this cal_date message is for the last channel.
			if(channel == sdaq_node->SDAQ_info.num_of_ch)
			{	//Check if all channels have cal_dates.
				if(g_slist_length(sdaq_node->SDAQ_Channels_cal_dates) == sdaq_node->SDAQ_info.num_of_ch)
				{	//Check if current SDAQ have support "input modes".
					if(*dev_input_mode_str[sdaq_node->SDAQ_info.dev_type] && sdaq_node->reg_status < Pending_input_mode)
					{
						sdaq_node->reg_status = Pending_input_mode;
						sdaq_node->failed_reg_RX_CNT = 0;
						QuerySystemVariables(CAN_socket_num, sdaq_node->SDAQ_address);
					}
					else if(sdaq_node->reg_status < Ready)//Otherwise mark SDAQ's reg_status as "Ready".
						sdaq_node->reg_status = Ready;
					return EXIT_SUCCESS;
				}
				else//Otherwise repeat QueryDeviceInfo().
				{
					//Logger("Unordered calibration dates reception, QueryDeviceInfo for SDAQ:%d will be repeated!!!\n",sdaq_node->SDAQ_address);
					sdaq_node->failed_reg_RX_CNT = 0;
					QuerySystemVariables(CAN_socket_num, sdaq_node->SDAQ_address);
					return EXIT_FAILURE;
				}
			}
			return EXIT_SUCCESS;
		}
		else
			return EXIT_FAILURE;
	}
	return EXIT_FAILURE;
}
/*Function that add current meas to channel's accumulator of a SDAQ's channel. Used in FSM*/
int acc_meas(unsigned char channel, sdaq_meas *meas_dec, struct SDAQ_info_entry *sdaq_node)
{
	GSList *acc_meas_list_node;
	struct Channel_acc_meas_entry *sdaq_Channels_acc_meas_node;

	acc_meas_list_node = g_slist_find_custom(sdaq_node->SDAQ_Channels_acc_meas, &channel, SDAQ_Channels_acc_meas_entry_find_channel);
	if(acc_meas_list_node)//Channel is already in to the SDAQ_Channels_acc_meas list: Add meas to acc.
	{
		sdaq_Channels_acc_meas_node = acc_meas_list_node->data;
		sdaq_Channels_acc_meas_node->unit_code = meas_dec->unit;
		sdaq_Channels_acc_meas_node->status = meas_dec->status;
		if(!sdaq_Channels_acc_meas_node->cnt)
			sdaq_Channels_acc_meas_node->meas_acc = 0;
		sdaq_Channels_acc_meas_node->meas_acc += meas_dec->meas;
		if(!sdaq_Channels_acc_meas_node->cnt || meas_dec->meas > sdaq_Channels_acc_meas_node->meas_max)
			sdaq_Channels_acc_meas_node->meas_max = meas_dec->meas;
		if(!sdaq_Channels_acc_meas_node->cnt || meas_dec->meas < sdaq_Channels_acc_meas_node->meas_min)
			sdaq_Channels_acc_meas_node->meas_min = meas_dec->meas;
		sdaq_Channels_acc_meas_node->cnt++;
		return EXIT_SUCCESS;
	}
	else//Channel is not in the list: Create entry and add meas.
	{
		sdaq_Channels_acc_meas_node = new_SDAQ_Channel_acc_meas_entry();
		if(sdaq_Channels_acc_meas_node)
		{
			sdaq_Channels_acc_meas_node->Channel = channel;
			sdaq_Channels_acc_meas_node->unit_code = meas_dec->unit;
			sdaq_Channels_acc_meas_node->status = meas_dec->status;
			if(!(meas_dec->status & (1<<No_sensor)))
			{
				sdaq_Channels_acc_meas_node->meas_acc += meas_dec->meas;
				sdaq_Channels_acc_meas_node->meas_max = meas_dec->meas;
				sdaq_Channels_acc_meas_node->meas_min = meas_dec->meas;
				sdaq_Channels_acc_meas_node->cnt++;
			}
			else
			{
				sdaq_Channels_acc_meas_node->meas_acc = 0;
				sdaq_Channels_acc_meas_node->cnt = 0;
			}
			sdaq_node->SDAQ_Channels_acc_meas = g_slist_insert_sorted(sdaq_node->SDAQ_Channels_acc_meas,
																		  sdaq_Channels_acc_meas_node,
																	      SDAQ_Channels_acc_meas_entry_cmp);
			return EXIT_SUCCESS;
		}
		else
		{
			fprintf(stderr,"Memory error!!!\n");
			exit(EXIT_FAILURE);
		}
	}
	return EXIT_FAILURE;
}

//Function that add or refresh SDAQ to lists list_SDAQ and LogBook, called if status message received. Used in FSM
struct SDAQ_info_entry * add_or_refresh_SDAQ_to_lists(int socket_fd, sdaq_can_id *sdaq_id_dec, sdaq_status *status_dec, struct Morfeas_SDAQ_if_stats *stats)
{
	unsigned char address_test;
	struct SDAQ_info_entry *list_SDAQ_node_data;
	struct LogBook_entry *LogBook_node_data;
	GSList *check_is_in_list_SDAQ, *check_is_in_LogBook;

	check_is_in_LogBook = g_slist_find_custom(stats->LogBook, &(status_dec->dev_sn), LogBook_entry_find_serial_number);
	check_is_in_list_SDAQ = g_slist_find_custom(stats->list_SDAQs, &(status_dec->dev_sn), SDAQ_info_entry_find_serial_number);
	if(check_is_in_list_SDAQ)//SDAQ is in list_SDAQ
	{
		list_SDAQ_node_data = check_is_in_list_SDAQ->data;
		time(&(list_SDAQ_node_data->last_seen));//Update last_seen for the SDAQ entry
		memcpy(&(list_SDAQ_node_data->SDAQ_status), status_dec, sizeof(sdaq_status)); //Update SDAQ's status value
		if(list_SDAQ_node_data->SDAQ_address != sdaq_id_dec->device_addr)//If TRUE, set back to the node_data->SDAQ_address
			SetDeviceAddress(socket_fd, list_SDAQ_node_data->SDAQ_status.dev_sn, list_SDAQ_node_data->SDAQ_address);
		return list_SDAQ_node_data;
	}
	else if(check_is_in_LogBook)//SDAQ is not in list_SDAQ, but is recorded in LogBook (Old Known entry)
	{
		LogBook_node_data = check_is_in_LogBook->data;
		check_is_in_list_SDAQ = g_slist_find_custom(stats->list_SDAQs, &(LogBook_node_data->SDAQ_address), SDAQ_info_entry_find_address);
		if(!check_is_in_list_SDAQ)//If TRUE, make new entry to list_SDAQ with address from LogBook and then configured SDAQ
		{	//Make new entry to list_SDAQ with address from LogBook
			list_SDAQ_node_data = new_SDAQ_info_entry();
			if(list_SDAQ_node_data)
			{
				list_SDAQ_node_data->SDAQ_address = LogBook_node_data->SDAQ_address;
				memcpy(&(list_SDAQ_node_data->SDAQ_status), status_dec, sizeof(sdaq_status));
				time(&(list_SDAQ_node_data->last_seen));
				stats->list_SDAQs = g_slist_insert_sorted(stats->list_SDAQs, list_SDAQ_node_data, SDAQ_info_entry_cmp);
				stats->detected_SDAQs++;
				if(sdaq_id_dec->device_addr != LogBook_node_data->SDAQ_address)//Check and configure SDAQ with Address from LogBook
					SetDeviceAddress(socket_fd, status_dec->dev_sn, LogBook_node_data->SDAQ_address);
				return list_SDAQ_node_data;
			}
			else
			{
				fprintf(stderr,"Memory error!\n");
				exit(EXIT_FAILURE);
			}
		}
		else//Address from record on LogBook is currently used
		{	//Try to find an available address
			for(address_test=1;address_test<Parking_address;address_test++)
			{
				if(!g_slist_find_custom(stats->list_SDAQs, &address_test, SDAQ_info_entry_find_address))
				{
					list_SDAQ_node_data = new_SDAQ_info_entry();
					if(list_SDAQ_node_data)
					{	//Load SDAQ data on new list_SDAQ entry
						list_SDAQ_node_data->SDAQ_address = address_test;
						memcpy(&(list_SDAQ_node_data->SDAQ_status), status_dec, sizeof(sdaq_status));
						time(&(list_SDAQ_node_data->last_seen));
						stats->list_SDAQs = g_slist_insert_sorted(stats->list_SDAQs, list_SDAQ_node_data, SDAQ_info_entry_cmp);
						//Update LogBook with new address
						LogBook_node_data->SDAQ_address = address_test;
						SetDeviceAddress(socket_fd, status_dec->dev_sn, address_test);
						LogBook_file(stats, "w");
						stats->detected_SDAQs++;
						return list_SDAQ_node_data;
					}
					else
					{
						fprintf(stderr,"Memory error!\n");
						exit(EXIT_FAILURE);
					}
				}
			}
			//If not any address available set SDAQ to park
			if(sdaq_id_dec->device_addr != Parking_address)
				SetDeviceAddress(socket_fd, status_dec->dev_sn, Parking_address);
			return NULL;
		}
	}
	else //Completely unknown SDAQ
	{
		//Check if the current address of the SDAQ is not conflict with any other in list_SDAQ, if not use it as it's pre addressed
		address_test = sdaq_id_dec->device_addr;
		if(g_slist_find_custom(stats->list_SDAQs, &address_test, SDAQ_info_entry_find_address) || address_test==Parking_address)
		{
			if(g_slist_length(stats->list_SDAQs)<Parking_address)
			{	//Try to find an available address
				for(address_test=1;address_test<Parking_address;address_test++)
				{
					if(!g_slist_find_custom(stats->list_SDAQs, &address_test, SDAQ_info_entry_find_address))
					{
						list_SDAQ_node_data = new_SDAQ_info_entry();
						LogBook_node_data = new_LogBook_entry();
						if(list_SDAQ_node_data && LogBook_node_data)
						{	//Load SDAQ data on new list_SDAQ entry
							list_SDAQ_node_data->SDAQ_address = address_test;
							memcpy(&(list_SDAQ_node_data->SDAQ_status), status_dec, sizeof(sdaq_status));
							time(&(list_SDAQ_node_data->last_seen));
							stats->list_SDAQs = g_slist_insert_sorted(stats->list_SDAQs, list_SDAQ_node_data, SDAQ_info_entry_cmp);
							//Update LogBook with new address
							LogBook_node_data->SDAQ_address = address_test;
							LogBook_node_data->SDAQ_sn = status_dec->dev_sn;
							stats->LogBook = g_slist_append(stats->LogBook, LogBook_node_data);
							LogBook_file(stats, "a");
							SetDeviceAddress(socket_fd, status_dec->dev_sn, address_test);
							stats->detected_SDAQs++;
							return list_SDAQ_node_data;
						}
						else
						{
							fprintf(stderr,"Memory error!\n");
							exit(EXIT_FAILURE);
						}
					}
				}
			}
			//If not any address available set SDAQ to park
			if(sdaq_id_dec->device_addr != Parking_address)
				SetDeviceAddress(socket_fd, status_dec->dev_sn, Parking_address);
			return NULL;
		}
		else //Register the pre-address SDAQ.
		{
			list_SDAQ_node_data = new_SDAQ_info_entry();
			LogBook_node_data = new_LogBook_entry();
			if(list_SDAQ_node_data && LogBook_node_data)
			{	//Load SDAQ data on new list_SDAQ entry
				list_SDAQ_node_data->SDAQ_address = address_test;
				memcpy(&(list_SDAQ_node_data->SDAQ_status), status_dec, sizeof(sdaq_status));
				time(&(list_SDAQ_node_data->last_seen));
				stats->list_SDAQs = g_slist_insert_sorted(stats->list_SDAQs, list_SDAQ_node_data, SDAQ_info_entry_cmp);
				//Update LogBook with new address
				LogBook_node_data->SDAQ_address = address_test;
				LogBook_node_data->SDAQ_sn = status_dec->dev_sn;
				stats->LogBook = g_slist_append(stats->LogBook, LogBook_node_data);
				LogBook_file(stats, "a");
				stats->detected_SDAQs++;
				return list_SDAQ_node_data;
			}
			else
			{
				fprintf(stderr,"Memory error!\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	return NULL;
}
//Function that cleaning the list_SDAQ from dead entries.
int clean_up_list_SDAQs(struct Morfeas_SDAQ_if_stats *stats)
{
	IPC_message IPC_msg = {0};
	struct SDAQ_info_entry *sdaq_node;
	GSList *check_node;
	time_t now=time(NULL);
	if(stats->list_SDAQs)//Check if list_SDAQs have elements
	{	//Check for dead SDAQs
		check_node = stats->list_SDAQs;
		while(check_node)
		{
			if(check_node->data)
			{
				sdaq_node = check_node->data;
				if((now - sdaq_node->last_seen) > LIFE_TIME)
				{
					stats->detected_SDAQs--;
					Logger("%s:%hhu (S/N:%u) removed from Device list\n", dev_type_str[sdaq_node->SDAQ_status.dev_type],
																		  sdaq_node->SDAQ_address,
																		  sdaq_node->SDAQ_status.dev_sn);
					//Send info of the removed SDAQ through IPC
					IPC_msg.SDAQ_clean.IPC_msg_type = IPC_SDAQ_clean_up;
					sprintf(IPC_msg.SDAQ_clean.Dev_or_Bus_name,"%s",stats->CAN_IF_name);
					IPC_msg.SDAQ_clean.SDAQ_serial_number = sdaq_node->SDAQ_status.dev_sn;
					IPC_msg.SDAQ_clean.t_amount = stats->detected_SDAQs;
					IPC_msg_TX(stats->FIFO_fd, &IPC_msg);
					//SDAQ free allocated memory operation
					free_SDAQ_info_entry(check_node->data);
					check_node->data = NULL;
				}
			}
			check_node = check_node -> next;//next node
		}
		//Delete empty nodes from the list
		stats->list_SDAQs = g_slist_remove_all(stats->list_SDAQs, NULL);
	}
	else
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

//Function for construction of message for registration or update of a SDAQ
int IPC_SDAQ_reg_update(int FIFO_fd, char *CANBus_if_name, unsigned char address, sdaq_status *SDAQ_status, unsigned char reg_status, unsigned char amount)
{
	IPC_message IPC_reg_msg = {0};
	//Send SDAQ registration over IPC
	IPC_reg_msg.SDAQ_reg_update.IPC_msg_type = IPC_SDAQ_register_or_update;
	memccpy(&(IPC_reg_msg.SDAQ_reg_update.Dev_or_Bus_name), CANBus_if_name, '\0', Dev_or_Bus_name_str_size);
	IPC_reg_msg.SDAQ_reg_update.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	IPC_reg_msg.SDAQ_reg_update.address = address;
	memcpy(&(IPC_reg_msg.SDAQ_reg_update.SDAQ_status), SDAQ_status,  sizeof(sdaq_status));
	IPC_reg_msg.SDAQ_reg_update.reg_status = reg_status;
	IPC_reg_msg.SDAQ_reg_update.t_amount = amount;
	return IPC_msg_TX(FIFO_fd, &IPC_reg_msg);
}
