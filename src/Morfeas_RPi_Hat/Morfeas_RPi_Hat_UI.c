/*
Program: Morfeas_RPi_Hat_if.c  Implementation of supporting software for Morfeas_RPi_Hat.
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
#define VERSION "1.0" /*Release Version of Morfeas_RPi_Hat support software*/

/* Shell command buffer */
#define user_inp_buf_size 80
#define max_amount_of_user_arg 20
#define history_buffs_length 30

/* ncurses windows sizes definitions */
#define w_csa_stat_info_height 8
#define w_csa_stat_info_width 25
#define w_spacing 0
#define w_meas_width  w_stat_info_width
#define term_min_width  w_csa_stat_info_width*4 + w_spacing
#define term_min_height  w_csa_stat_info_height + 20

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <ncurses.h>
#include <glib.h>
#include <gmodule.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../Supplementary/Morfeas_run_check.h"
#include "Morfeas_RPi_Hat.h"

//Functions related to git logs info and compilation date. Implementation at Morfeas_info.c
char* Morfeas_get_release_date(void);
char* Morfeas_get_compile_date(void);
char* Morfeas_get_curr_git_hash(void);

//Structs def
struct app_data{
	unsigned char det_ports;
	unsigned char i2c_bus;
	unsigned char unsaved_fl[4];
	struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config Ports_config[4];
	struct Morfeas_RPi_Hat_Port_meas Ports_meas[4];
	WINDOW *Port_csa[4], *UI_term;
};
typedef struct{
	char usr_in_buff[user_inp_buf_size];
}history_buffer_entry;

//Global variables
pthread_mutex_t ncurses_access = PTHREAD_MUTEX_INITIALIZER;
volatile unsigned char running = 1, term_resize = 0;

/* --- Local Functions --- */
//slice free function for history_buffs_nodes
void history_buff_free_node(gpointer node)
{
	g_slice_free(history_buffer_entry, node);
}
//print usage function
void print_usage(char *prog_name);

//Function for initialiazation of ncurses windows
void w_init(struct app_data *arg);
//Function for decoding printing of last the last calibration date of a Port Surrent sense amplifier.
char *last_calibration_print(struct last_port_calibration_date);
//function for decode user input
int user_inp_dec(char **argv, char *usr_in_buff);
//UI_Shell Function
void *UI_shell(void *UI_term);
//function for execution of user's command input
void user_com(unsigned int argc, char **argv, struct app_data *arg);
//SDAQ_psim shell help, return 0 in success or 1 on failure
int shell_help();

int main(int argc, char *argv[])
{
	int i,c;
	//variables for threads
	pthread_t UI_shell_Thread_id;
	//Variables for ncurses
	int last_curx, last_cury;
	struct app_data stats = {.i2c_bus=I2C_BUS_NUM};
	struct winsize term_init_size;

	opterr = 1;
	while ((c = getopt (argc, argv, "hvb:")) != -1)
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
			case 'b':
				stats.i2c_bus=atoi(optarg);
				break;
			case '?':
				//print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	//Check if program already runs.
	if(check_already_run(argv[0]))
	{
		fprintf(stderr, "%s Already Running!!!\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	/*
	//Check if Morfeas_SDAQ_if running.
	if(check_already_run("Morfeas_SDAQ_if"))
	{
		fprintf(stderr, "Morfeas_SDAQ_if detected to run, %s can't run!!!\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	//Check if Morfeas_NOX_if running.
	if(check_already_run("Morfeas_NOX_if"))
	{
		fprintf(stderr, "Morfeas_NOX_if detected to run, %s can't run!!!\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	*/
	//Init and Detect amount of CSAs and get config for each
	stats.det_ports=0;
	for(i=0;i<4;i++)
	{
		if(!MAX9611_init(i,stats.i2c_bus))
		{
			stats.det_ports++;
			if(read_port_config(&(stats.Ports_config[i]), i, stats.i2c_bus))
			{
				stats.Ports_config[i].curr_meas_scaler = MAX9611_default_current_meas_scaler;
				stats.Ports_config[i].volt_meas_scaler = MAX9611_default_volt_meas_scaler;
			}
		}
	}
	//Exit if no SCA detected
	if(!stats.det_ports)
	{
		fprintf(stderr, "No Port Detected!!!\n");
		exit(EXIT_FAILURE);
	}

	//Check if the terminal have the minimum required size to run the application
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_init_size);// get current size of terminal window
	if(term_init_size.ws_col<term_min_width || term_init_size.ws_row<term_min_height)
	{
		printf("Terminal need to be at least %dX%d Characters\n",term_min_width,term_min_height);
		return EXIT_SUCCESS;
	}

	//Start ncurses
	initscr();
	//Init windows
	w_init(&stats);
	//Create thread to run function UI_shell
	pthread_create(&UI_shell_Thread_id, NULL, UI_shell, &stats);
	sleep(1);
	//Main loop:Read values from Port's SCAs (MAX9611)
	while(running)
	{
		pthread_mutex_lock(&ncurses_access);
			getyx(stats.UI_term, last_cury, last_curx);
			curs_set(0);//Hide curson
			for(i=0; i<stats.det_ports; i++)
			{
				mvwprintw(stats.Port_csa[i],1,6, "Port %u (can%u)", i, i);
				if(!get_port_meas(&(stats.Ports_meas[i]), i, stats.i2c_bus))
				{
					mvwprintw(stats.Port_csa[i],2,2, "Last_Cal = %s", last_calibration_print(stats.Ports_config[i].last_cal_date));
					mvwprintw(stats.Port_csa[i],3,2, "Voltage = %5.2fV ", (stats.Ports_meas[i].port_voltage - stats.Ports_config[i].volt_meas_offset) * stats.Ports_config[i].volt_meas_scaler);
					mvwprintw(stats.Port_csa[i],4,2, "Current = %5.3fA ", (stats.Ports_meas[i].port_current - stats.Ports_config[i].curr_meas_offset) * stats.Ports_config[i].curr_meas_scaler);
					mvwprintw(stats.Port_csa[i],5,2, "Shunt_Temp = %3.0f°F ", 32.0 + stats.Ports_meas[i].temperature * MAX9611_temp_scaler);
					if(stats.unsaved_fl[i])
						mvwprintw(stats.Port_csa[i],6,2, "\tUn-Saved");
					else
						mvwprintw(stats.Port_csa[i],6,2, "\t\t");
				}
				else
					mvwprintw(stats.Port_csa[i],2,2, "Error !!!");
				wrefresh(stats.Port_csa[i]);
			}
			wmove(stats.UI_term, last_cury, last_curx);
			curs_set(1);//show curson
			wrefresh(stats.UI_term);
		pthread_mutex_unlock(&ncurses_access);
		usleep(100000);
	}
	pthread_join(UI_shell_Thread_id, NULL);
	endwin();

	return EXIT_SUCCESS;
}



char *last_calibration_print(struct last_port_calibration_date last_date)
{
	static char buff[10];
	struct tm last_cal_tm = {0};

	if(!last_date.year &&
	   !last_date.month &&
	   !last_date.day)
	   return "UnCal";

	last_cal_tm.tm_year = last_date.year - 100;
	last_cal_tm.tm_mon = last_date.month - 1;
	last_cal_tm.tm_mday = last_date.day;
	strftime(buff, sizeof(buff), "%x", &last_cal_tm);
	return buff;
}

void last_calibration(struct last_port_calibration_date *last_date)
{
	time_t now = time(NULL);
	struct tm *cal_tm = gmtime(&now);

	last_date->year = cal_tm->tm_year - 100;
	last_date->month = cal_tm->tm_mon + 1;
	last_date->day = cal_tm->tm_mday;
	return;
}

void wclean_refresh(WINDOW *ptr)
{
	wclear(ptr);
	box(ptr,0,0);
	wrefresh(ptr);
	return;
}

void wclean_refresh_all(struct app_data *arg)
{
	for(int i=0; i < arg->det_ports; i++)
		wclean_refresh(arg->Port_csa[i]);
	wclean_refresh(arg->UI_term);
	return;
}

void w_init(struct app_data *arg)
{
	int term_col,term_row, offset, i;
	noecho();//disable echo
	raw();//getch without return
	keypad(stdscr, TRUE);
	scrollok(stdscr, TRUE);
	curs_set(1);//Show cursor
	getmaxyx(stdscr,term_row,term_col);
	offset = (term_col - w_csa_stat_info_width*arg->det_ports)/(2*arg->det_ports);
	//Create windows for CSA measurements
	for(i=0; i < arg->det_ports; i++)
	{
		arg->Port_csa[i] = newwin(w_csa_stat_info_height, w_csa_stat_info_width, 0, i*term_col/arg->det_ports+w_spacing+offset);
		scrollok(arg->Port_csa[i], TRUE);
	}
	arg->UI_term = newwin(term_row - w_csa_stat_info_height, term_col, w_csa_stat_info_height, 0);
	clear();
	refresh();
	for(i=0; i < arg->det_ports; i++)
		wclean_refresh(arg->Port_csa[i]);
	wclean_refresh(arg->UI_term);
	return;
}

int user_inp_dec(char **arg, char *usr_in_buff)
{
	unsigned char i=0;
	static char decode_buff[user_inp_buf_size];//assistance copy buffer, used instead of usr_in_buff to do not destroy the contents
	strcpy(decode_buff, usr_in_buff);
	arg[i] = strtok (decode_buff," ");
	while (arg[i] != NULL)
	{
		i++;
		arg[i] = strtok (NULL, " ");
	}
	return i;
}

const char shell_help_str[]={
	"\t      -----Morfeas_RPi_Hat Shell-----\n"
	" KEYS:\n"
	" \tKEY_UP    = Buffer up\n"
	"\tKEY_DOWN  = Buffer Down\n"
	"\tKEY_LEFT  = Cursor move left by 1\n"
	"\tKEY_RIGTH = Cursor move Right by 1\n"
	"\tCtrl + C  = Clear current buffer\n"
	"\tCtrl + L  = Clear screen\n"
	"\tCtrl + Q  = Quit\n"
	" COMMANDS:\n"
	"\tmeas p# = Print measurement of Port's CSA\n"
	"\tconfig p# = Print Port's Configuration\n"
	"\tset p# czero = Set port's current zero offset\n"
	"\tset p# vzero = Set port's voltage zero offset\n"
	"\tset p# vgain Ref_value = Calculate and set CSA's voltage gain at Reference value\n"
	"\tset p# cgain Ref_value = Calculate and set CSA's current gain at Reference value\n"
	"\tsave p# = Save Port's configuration to EEPROM\n"
};

//SDAQ_psim shell help
int shell_help()
{
	const int height = 22;
	const int width = 90;
	int starty = (LINES - height) / 2;
	int startx = (COLS - width) / 2;
	int key, scroll_lines=0;
	if(LINES>=height && COLS>=width)
	{
		WINDOW *help_win = newwin(height, width, starty, startx);
		keypad(help_win, TRUE);
		curs_set(0);//hide cursor
		scrollok(help_win, TRUE);
		mvwprintw(help_win, 1, 1, "%s", shell_help_str);
		mvwprintw(help_win, height-2, 1, " Press Ctrl+C to exit help");
		box(help_win, 0 , 0);
		wrefresh(help_win);
		do{
			key = getch();
			switch(key)
			{
				case KEY_UP:
					scroll_lines++;
					//wscrl(help_win, 1);
					wrefresh(help_win);
					break;
				case KEY_DOWN:
					scroll_lines--;
					//wscrl(help_win, -1);
					wrefresh(help_win);
					break;
			}
		}while(key!=3);
		wborder(help_win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
		wclear(help_win);
		wrefresh(help_win);
		delwin(help_win);
		curs_set(1);//Show cursor
		return 0;
	}
	return 1;
}

//Implementation of the user's Interface Shell function
void *UI_shell(void *pass_arg)
{
	struct app_data *app_arg = pass_arg;
	WINDOW *UI_shell_win = app_arg->UI_term;
	unsigned int end_index=0, cur_pos=0, key, argc, last_curx, last_cury, history_buffs_index=0, retval;
	char *argv[max_amount_of_user_arg] = {NULL};
	GQueue *hist_buffs = g_queue_new();
	g_queue_push_head(hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
	gpointer nth_node = NULL;
	char *usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(hist_buffs))->usr_in_buff;
	char temp_usr_in_buff[user_inp_buf_size];

	pthread_mutex_lock(&ncurses_access);
		//mvwprintw(help_win, 1, 1,
		mvwprintw(UI_shell_win, 1, 1, " press '?' for help.");
		mvwprintw(UI_shell_win, 2, 1, "][ ");
		wrefresh(UI_shell_win);
	pthread_mutex_unlock(&ncurses_access);
	while(running)
	{
		key = getch();// get the user's entrance
		pthread_mutex_lock(&ncurses_access);
			switch(key)
			{
				case 3 ://Ctrl + c clear buffer
					wmove(UI_shell_win, getcury(UI_shell_win),4);
					wclrtoeol(UI_shell_win);
					end_index = 0;
					cur_pos = 0;
					for(int i=0;i<user_inp_buf_size;i++)
						usr_in_buff[i] = '\0';
					break;
				case 17 ://Ctrl + q
					running = 0;
					break;
				case 12 : //Ctrl + l
					wclean_refresh_all(app_arg);
					mvwprintw(UI_shell_win, 1, 1, "][ %s",usr_in_buff);
					cur_pos = end_index;
					break;
				case '?' : //user request for help
					last_curx = getcurx(UI_shell_win);
					last_cury = getcury(UI_shell_win);
					retval = shell_help();
					if(retval)
					{
						wprintw(UI_shell_win, "Terminal size too small to print help!!!");
						mvwprintw(UI_shell_win, last_cury+1, 1, "][ %s",usr_in_buff);
						wmove(UI_shell_win, last_cury+retval, last_curx);
					}
					else
					{
						wclean_refresh_all(app_arg);//Re-Init windows
						mvwprintw(UI_shell_win, 1, 1, "][ %s",usr_in_buff);
						wmove(UI_shell_win, 1, last_curx);
					}
					break;
				case KEY_UP:
					if((nth_node = g_queue_peek_nth(hist_buffs,history_buffs_index+1)))
					{
						if(!history_buffs_index)
							memcpy(temp_usr_in_buff, usr_in_buff, sizeof(char)*user_inp_buf_size);
						memset(usr_in_buff, '\0', sizeof(char)*user_inp_buf_size);
						strcpy(usr_in_buff, ((history_buffer_entry *)nth_node)->usr_in_buff);
						history_buffs_index++;
						wmove(UI_shell_win, getcury(UI_shell_win), 4);
						wclrtoeol(UI_shell_win);
						wprintw(UI_shell_win, "%s",usr_in_buff);
						end_index = strlen(usr_in_buff);
						cur_pos = end_index;
					}
					break;
				case KEY_DOWN:
					if((nth_node = g_queue_peek_nth(hist_buffs,history_buffs_index-1)))
					{
						memset(usr_in_buff, '\0', sizeof(char)*user_inp_buf_size);
						strcpy(usr_in_buff, ((history_buffer_entry *)nth_node)->usr_in_buff);
						history_buffs_index--;
						wmove(UI_shell_win, getcury(UI_shell_win), 4);
						wclrtoeol(UI_shell_win);
						if(!history_buffs_index)
						{
							memcpy(usr_in_buff, temp_usr_in_buff, sizeof(char)*user_inp_buf_size);
							wmove(UI_shell_win, getcury(UI_shell_win), 4);
						}
						wprintw(UI_shell_win, "%s",usr_in_buff);
						end_index = strlen(usr_in_buff);
						cur_pos = end_index;
					}
					break;
				case KEY_LEFT:
					if(cur_pos)
					{
						wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)-1);
						cur_pos--;
					}
					break;
				case KEY_RIGHT:
					if(cur_pos<end_index)
					{
						wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)+1);
						cur_pos++;
					}
					break;
				case KEY_BACKSPACE :
					if(cur_pos)
					{
						for(unsigned int i=cur_pos-1;i<=end_index;i++)
							usr_in_buff[i] = usr_in_buff[i+1];
						wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)-1);//move cursor one left
						wclrtoeol(UI_shell_win); //clear from buffer to the end of line
						end_index--;
						cur_pos--;
						usr_in_buff[end_index] = '\0';
						wprintw(UI_shell_win, "%s", usr_in_buff + cur_pos);
						wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)-(end_index-cur_pos));
					}
					break;
				case KEY_DC ://Delete key
					if(cur_pos<end_index)
					{
						for(unsigned int i=cur_pos;i<=end_index;i++)
							usr_in_buff[i] = usr_in_buff[i+1];
						end_index--;
						wclrtoeol(UI_shell_win);
						wprintw(UI_shell_win, "%s", usr_in_buff + cur_pos);
						wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)-(end_index-cur_pos));
						usr_in_buff[end_index] = '\0';
					}
					break;
				case KEY_HOME ://Home key
					cur_pos = 0;
					wmove(UI_shell_win, getcury(UI_shell_win),4);
					break;
				case KEY_END ://End key
					cur_pos = end_index;
					wmove(UI_shell_win, getcury(UI_shell_win),4+end_index);
					break;
				case '\r' :
				case '\n' ://return or enter : Command decode and execution
					//clean window if is close to border
					if(getcury(UI_shell_win) >= getmaxy(UI_shell_win)-2)
					{
						wclean_refresh_all(app_arg);
						mvwprintw(UI_shell_win, 1, 1, "][ %s",usr_in_buff);
					}
					usr_in_buff[end_index] = '\0';
					wmove(UI_shell_win, getcury(UI_shell_win),getcurx(UI_shell_win)+(end_index-cur_pos));
					argc = user_inp_dec(argv, usr_in_buff);
					user_com(argc, argv, app_arg);
					mvwprintw(UI_shell_win, getcury(UI_shell_win)+1, 1, "][ ");
					end_index = 0;
					cur_pos = 0;
					if(*usr_in_buff)//make new entry in the history queue only if the current usr_in_buff is not empty
					{
						g_queue_push_head(hist_buffs, g_slice_alloc0(sizeof(history_buffer_entry)));
						usr_in_buff = ((history_buffer_entry *)g_queue_peek_head(hist_buffs))->usr_in_buff;
						if(g_queue_get_length(hist_buffs)>history_buffs_length)
							history_buff_free_node(g_queue_pop_tail(hist_buffs));
					}
					history_buffs_index = 0;
					break;
				default : //normal key press
					if(isprint(key))
					{
						if(end_index < user_inp_buf_size-2)//Stop buffering if the currsor is 1 char before the buffer limit
						{	//check if cursor has moved from the user
							if(cur_pos<end_index)
							{	//roll right side of the buffer by one position
								for(int i=end_index; i>=cur_pos && i>=0; i--)
									usr_in_buff[i+1] = usr_in_buff[i];
							}
							usr_in_buff[cur_pos] = key; // add new pressed key to the buffer
							end_index++;
							wprintw(UI_shell_win, "%s", usr_in_buff+cur_pos);
							cur_pos++;
							wmove(UI_shell_win, getcury(UI_shell_win), getcurx(UI_shell_win)-(end_index-cur_pos));
						}
					}
					break;
			}
			wrefresh(UI_shell_win);
		pthread_mutex_unlock(&ncurses_access);
	}
	g_queue_free_full(hist_buffs,history_buff_free_node);//free the allocated space of the history buffers
	return NULL;
}

//function for execution of user's command input
void user_com(unsigned int argc, char **argv, struct app_data *arg)
{
	char test_buff[user_inp_buf_size];
	unsigned char port;
	float val_arg;
	if(argc>=2)
	{
		if(strlen(argv[1]) == 2 &&(argv[1][0]=='p' || argv[1][0]=='P') && (argv[1][1]>='0' || argv[1][1]<='4'))
		{
			if((port = argv[1][1] - '0') < arg->det_ports)
			{
				switch(argc)
				{
					case 2:
						if(!strcmp(argv[0], "config"))
						{
							if(!arg->Ports_config[port].last_cal_date.year &&
							   !arg->Ports_config[port].last_cal_date.month &&
							   !arg->Ports_config[port].last_cal_date.day)
							{
								wprintw(arg->UI_term, " Default");
								return;
							}
							wprintw(arg->UI_term, " Volt_meas_offset=%hhu", arg->Ports_config[port].volt_meas_offset);
							wprintw(arg->UI_term, " Volt_meas_scale=%.2E", arg->Ports_config[port].volt_meas_scaler);
							wprintw(arg->UI_term, " Curr_meas_offset=%hhu", arg->Ports_config[port].curr_meas_offset);
							wprintw(arg->UI_term, " Curr_meas_scale=%.2E", arg->Ports_config[port].curr_meas_scaler);
							return;
						}
						else if(!strcmp(argv[0], "save"))
						{
							if(arg->unsaved_fl[port])
							{
								last_calibration(&(arg->Ports_config[port].last_cal_date));
								if(!erase_EEPROM(port, arg->i2c_bus))
								{
									if(!write_port_config(&(arg->Ports_config[port]), port, arg->i2c_bus))
									{
										wprintw(arg->UI_term, " Configuration saved");
										arg->unsaved_fl[port]=0;
									}
								}
								else
									wprintw(arg->UI_term, " EEPROM Erasing Failed!!!");
								return;
							}
						}
						else if(!strcmp(argv[0], "meas"))
						{
							wprintw(arg->UI_term, " Volt_meas=%.2fV", (arg->Ports_meas[port].port_voltage - arg->Ports_config[port].volt_meas_offset) * arg->Ports_config[port].volt_meas_scaler);
							wprintw(arg->UI_term, " Curr_meas=%.3fA", (arg->Ports_meas[port].port_current - arg->Ports_config[port].curr_meas_offset) * arg->Ports_config[port].curr_meas_scaler);
							wprintw(arg->UI_term, " CSA_out=%.2fV", (arg->Ports_meas[port].output - arg->Ports_config[port].volt_meas_offset) * arg->Ports_config[port].volt_meas_scaler);
							wprintw(arg->UI_term, " Comp_ref=%.2fmV", arg->Ports_meas[port].set_val * MAX9611_comp_scaler);
							wprintw(arg->UI_term, " Die_temp=%.1f°F", 32.0 + arg->Ports_meas[port].temperature * MAX9611_temp_scaler);
							return;
						}
						break;
					case 3:
						if(!strcmp(argv[0], "set"))
						{
							if(!strcmp(argv[2], "czero"))
							{
								arg->Ports_config[port].curr_meas_offset = arg->Ports_meas[port].port_current;
								arg->unsaved_fl[port]=1;
								return;
							}
							else if(!strcmp(argv[2], "vzero"))
							{
								arg->Ports_config[port].volt_meas_offset = arg->Ports_meas[port].port_voltage;
								arg->unsaved_fl[port]=1;
								return;
							}
						}
						break;
					case 4:
						if(!strcmp(argv[0], "set"))
						{
							val_arg = atof(argv[3]);
							sprintf(test_buff, "%.3f", val_arg);
							if(strstr(test_buff, argv[3]))//sanitize val_arg
							{
								if(!strcmp(argv[2], "vgain"))
								{
									arg->Ports_config[port].volt_meas_scaler = val_arg / (arg->Ports_meas[port].port_voltage - arg->Ports_config[port].volt_meas_offset);
									arg->unsaved_fl[port]=1;
									return;
								}
								else if(!strcmp(argv[2], "cgain"))
								{
									arg->Ports_config[port].curr_meas_scaler = val_arg / (arg->Ports_meas[port].port_current - arg->Ports_config[port].curr_meas_offset);
									arg->unsaved_fl[port]=1;
									return;
								}
							}
						}
						break;
				}
			}
		}
	}
	wprintw(arg->UI_term, " ????");
	return;
}


void print_usage(char *prog_name)
{
	const char preamp[] = {
	"Program: Morfeas_RPi_Hat  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "This program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "This is free software, and you are welcome to redistribute it\n"
    "under certain conditions; for details see LICENSE.\n"
	};
	const char exp[] = {
	"\tOptions:\n"
	"\t         -h : Print Help\n"
	"\t         -v : Print Version\n"
	"\t         -b : I2C Bus number (default 1)\n"
	};
	printf("%s\nUsage: %s [Options]\n\n%s\n%s\n", preamp, prog_name, exp, shell_help_str);
	return;
}

