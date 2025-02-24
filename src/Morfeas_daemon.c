/*
File: Morfeas_daemon.c, Implementation of Morfeas_daemon program, Part of Morfeas_project.
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
#define VERSION "0.91" /*Release Version of Morfeas_daemon*/
#define max_num_of_threads 16
#define max_lines_on_Logger 128

//Define Morfeas Components programs
#define Morfeas_opc_ua "Morfeas_opc_ua"
#define Morfeas_SDAQ_if "Morfeas_SDAQ_if"
#define Morfeas_MDAQ_if "Morfeas_MDAQ_if"
#define Morfeas_IOBOX_if "Morfeas_IOBOX_if"
#define Morfeas_MTI_if "Morfeas_MTI_if"
#define Morfeas_NOX_if "Morfeas_NOX_if"

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

#include "Morfeas_Types.h"
#include "Supplementary/Morfeas_run_check.h"
#include "Supplementary/Morfeas_XML.h"

typedef struct thread_arguments{
	xmlNode *component;
	char *configs_Dir_path;
	char *loggers_path;
	char *logstat_path;
}thread_arg;

//Global variables
static volatile unsigned char daemon_run = 1;
pthread_mutex_t thread_make_lock = PTHREAD_MUTEX_INITIALIZER;

//print the Usage manual
void print_usage(char *prog_name);
//Thread function
void * Morfeas_thread(void *varg_pt);
//File function that open the file from loggers_path, truncate it to limit_lines and append the buff
int tranc_file_and_wrire(char *loggers_path,char *out_str, unsigned int limit_lines);

static void stopHandler(int sign)
{
	daemon_run = 0;
}

int main(int argc, char *argv[])
{
	thread_arg t_arg = {0};
	unsigned char nodes_cnt = 0;
	char *config_path = NULL;
	DIR *loggers_dir;
	xmlDoc *doc;//XML DOC tree pointer
	xmlNode *Morfeas_component, *root_element; //XML root Node
	xmlChar* node_attr; //Value of Node's Attribute
	//variables for threads
	pthread_t Threads_ids[max_num_of_threads] = {0}, *Threads_ids_ind = Threads_ids;

	int c;
	//Get options
	while ((c = getopt (argc, argv, "hVc:")) != -1)
	{
		switch (c)
		{
			case 'h'://help
				print_usage(argv[0]);
				exit(EXIT_SUCCESS);
			case 'V'://Version
				printf(VERSION"\n");
				exit(EXIT_SUCCESS);
			case 'c'://configuration XML file
				config_path = optarg;
				break;
			case '?':
				print_usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	//Check if program already runs in other instance.
	if(check_already_run(argv[0]))
	{
		printf("%s Already running !!!\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	//Print start message
	printf("----- Morfeas_daemon Started -----\n");
	//If config_path is NULL try to get it from the environment variable "Morfeas_daemon_config_path"
	if(!config_path)
		config_path = getenv("Morfeas_daemon_config_path");
	if(!config_path || access(config_path, R_OK | F_OK ) || !strstr(config_path, ".xml"))
	{
		printf("Configuration File not defined or invalid!!!\n");
		exit(EXIT_FAILURE);
	}

	//Install stopHandler as the signal handler for SIGINT and SIGTERM signals.
	signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

	//Configuration XML parsing and validation check
	if(!Morfeas_XML_parsing(config_path, &doc))
	{
		root_element = xmlDocGetRootElement(doc);
		if(!Morfeas_daemon_config_valid(root_element))
		{
			//make Morfeas_Loggers_Directory
			t_arg.configs_Dir_path = XML_node_get_content(root_element, "CONFIGS_DIR");
			t_arg.logstat_path = XML_node_get_content(root_element, "LOGSTAT_DIR");
			t_arg.loggers_path = XML_node_get_content(root_element, "LOGGERS_DIR");
			mkdir(t_arg.loggers_path, 0777);
			if((loggers_dir = opendir(t_arg.loggers_path)))
			{
				closedir(loggers_dir);
				//Get Morfeas component from Configuration XML
				Morfeas_component = (get_XML_node(root_element, "COMPONENTS"))->children;
				while(Morfeas_component && nodes_cnt<max_num_of_threads)
				{
					if (Morfeas_component->type == XML_ELEMENT_NODE)
					{
						if((node_attr = xmlGetProp(Morfeas_component, BAD_CAST"Disable")))
						{
							if(!strcmp((char *)node_attr, "false"))
							{
								pthread_mutex_lock(&thread_make_lock);//Lock threading making
									t_arg.component = Morfeas_component;
									pthread_create(Threads_ids_ind, NULL, Morfeas_thread, &t_arg);
								pthread_mutex_unlock(&thread_make_lock);//Unlock threading making
								Threads_ids_ind++;
								usleep(100000);//100ms delay between thread creation
							}
							xmlFree(node_attr);
							nodes_cnt++;
						}
					}
					Morfeas_component = Morfeas_component->next;
				}
				if(nodes_cnt>=max_num_of_threads)
					printf("Max_amount(%d) of thread reached!!!\n",max_num_of_threads);
			}
			else
			{
				perror("Error on creation of Loggers directory");
				xmlFreeDoc(doc);//Free XML Doc
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			printf("Data Validation of The configuration XML file failed!!!\n");
			xmlFreeDoc(doc);//Free XML Doc
			exit(EXIT_FAILURE);
		}
		xmlFreeDoc(doc);//Free XML Doc
	}
	else
	{
		printf("XML Parsing of The configuration XML file failed!!!\n");
		exit(EXIT_FAILURE);
	}
	//sleep_loop
	while(daemon_run)
	{
		//TO-DO: check led status and light green
		sleep(1);
	}
	//Send SIGTERM to group
	kill(0,SIGTERM);
	//Wait until all threads ends
	for(int i=0; i<max_num_of_threads; i++)
	{
		pthread_join(Threads_ids[i], NULL);// wait for thread to finish
		pthread_detach(Threads_ids[i]);//de-allocate thread memory
	}
	xmlCleanupParser();
	xmlMemoryDump();
	return 0;
}

//print the Usage manual
void print_usage(char *prog_name)
{
	const char preamp[] = {
	"\tProgram: Morfeas_daemon  Copyright (C) 12019-12021  Sam Harry Tzavaras\n"
    "\tThis program comes with ABSOLUTELY NO WARRANTY; for details see LICENSE.\n"
    "\tThis is free software, and you are welcome to redistribute it\n"
    "\tunder certain conditions; for details see LICENSE.\n"
	};
	const char manual[] = {
		"Options:\n"
		"           -h : Print help.\n"
		"           -V : Version.\n"
		"           -c : Path to configuration XML file (see notes).\n"
		"\nNote_1: If '-c' option not used the Morfeas_daemon will try to\n"
		"          get config_path from env_var:\"Morfeas_deamon_config_path\"\n"
	};
	printf("%s\nUsage: %s [Options]\n\n%s",preamp, prog_name, manual);
	return;
}

//Thread Function, Decode varg_pt to xmlNode and start the Morfeas Component program.
void * Morfeas_thread(void *varg_pt)
{
	FILE *cmd_fd;
	char out_str[256] = {0}, system_call_str[512] = {0}, Logger_name[128] = {0}, i2cbus_num_arg[50] = {0},
		 *loggers_path, *i2cbus_num_str;
	int i2cbus_num;
	thread_arg *t_arg = varg_pt;

	pthread_mutex_lock(&thread_make_lock);//Lock threading making
		loggers_path = calloc(strlen(t_arg->loggers_path)+2, sizeof(*loggers_path));
		if(!loggers_path)
		{
			fprintf(stderr,"Memory Error!!!\n");
			exit(EXIT_FAILURE);
		}
		strcpy(loggers_path, t_arg->loggers_path);
		if(!strcmp((char *)(t_arg->component->name), "OPC_UA_SERVER"))
		{
			sprintf(Logger_name,"%s.log",Morfeas_opc_ua);
			sprintf(system_call_str,"%s -a '%s' -c '%s%s%s' -l '%s' 2>&1", Morfeas_opc_ua,
													  XML_node_get_content(t_arg->component, "APP_NAME"),
													  t_arg->configs_Dir_path,
													  t_arg->configs_Dir_path[strlen(t_arg->configs_Dir_path)-1]=='/'?"":"/",
													  "OPC_UA_Config.xml",// XML_node_get_content(t_arg->component, "CONFIG_FILE")
													  t_arg->logstat_path);
		}
		else if(!strcmp((char *)(t_arg->component->name), "SDAQ_HANDLER"))
		{
			if((i2cbus_num_str=XML_node_get_content(t_arg->component, "I2CBUS_NUM"))&&(i2cbus_num=atoi(i2cbus_num_str)))
				sprintf(i2cbus_num_arg, "-b %u", i2cbus_num);
			sprintf(Logger_name,"%s_%s.log",Morfeas_SDAQ_if, XML_node_get_content(t_arg->component, "CANBUS_IF"));
			sprintf(system_call_str,"%s %s %s %s 2>&1", Morfeas_SDAQ_if,
													    i2cbus_num_arg,
													    XML_node_get_content(t_arg->component, "CANBUS_IF"),
													    t_arg->logstat_path);
		}
		else if(!strcmp((char *)(t_arg->component->name), "NOX_HANDLER"))
		{
			if((i2cbus_num_str=XML_node_get_content(t_arg->component, "I2CBUS_NUM"))&&(i2cbus_num=atoi(i2cbus_num_str)))
				sprintf(i2cbus_num_arg, "-b %u", i2cbus_num);
			sprintf(Logger_name,"%s_%s.log",Morfeas_NOX_if, XML_node_get_content(t_arg->component, "CANBUS_IF"));
			sprintf(system_call_str,"%s %s %s %s 2>&1", Morfeas_NOX_if,
													 	i2cbus_num_arg,
														XML_node_get_content(t_arg->component, "CANBUS_IF"),
														t_arg->logstat_path);
		}
		else if(!strcmp((char *)(t_arg->component->name), "MDAQ_HANDLER"))
		{
			sprintf(Logger_name,"%s_%s.log",Morfeas_MDAQ_if, XML_node_get_content(t_arg->component, "DEV_NAME"));
			sprintf(system_call_str,"%s %s %s %s 2>&1", Morfeas_MDAQ_if,
														XML_node_get_content(t_arg->component, "IPv4_ADDR"),
														XML_node_get_content(t_arg->component, "DEV_NAME"),
														t_arg->logstat_path);
		}
		else if(!strcmp((char *)(t_arg->component->name), "IOBOX_HANDLER"))
		{
			sprintf(Logger_name,"%s_%s.log",Morfeas_IOBOX_if, XML_node_get_content(t_arg->component, "DEV_NAME"));
			sprintf(system_call_str,"%s %s %s %s 2>&1", Morfeas_IOBOX_if,
												     	XML_node_get_content(t_arg->component, "IPv4_ADDR"),
														 XML_node_get_content(t_arg->component, "DEV_NAME"),
												     	t_arg->logstat_path);
		}
		else if(!strcmp((char *)(t_arg->component->name), "MTI_HANDLER"))
		{
			sprintf(Logger_name,"%s_%s.log",Morfeas_MTI_if, XML_node_get_content(t_arg->component, "DEV_NAME"));
			sprintf(system_call_str,"%s %s %s %s 2>&1", Morfeas_MTI_if,
														XML_node_get_content(t_arg->component, "IPv4_ADDR"),
														XML_node_get_content(t_arg->component, "DEV_NAME"),
														t_arg->logstat_path);
		}
		else
		{
			printf("Unknown Component type (%s)!!!\n",t_arg->component->name);
			pthread_mutex_unlock(&thread_make_lock);//Unlock threading making
			free(loggers_path);
			return NULL;
		}
	pthread_mutex_unlock(&thread_make_lock);//Unlock threading making

	//Report Thread call
	printf("New Thread for: %s\n",system_call_str);
	//Make correction of loggers_path
	if(loggers_path[strlen(loggers_path)-1]!='/')
		loggers_path[strlen(loggers_path)] = '/';

	//Enlarge loggers_path table to fit logger_name and copy logger_name to it
	loggers_path = realloc(loggers_path, strlen(loggers_path)+strlen(Logger_name)+2);
	strcat(loggers_path, Logger_name);

 	//Build New Logger file
	if((cmd_fd = fopen(loggers_path, "w")))
		fclose(cmd_fd);

	//Fork command in system_call_str to thread
	cmd_fd = popen(system_call_str, "re");
	if(!cmd_fd)
    {
        printf("Couldn't start command\n");
        free(loggers_path);
		return NULL;
    }

    //Read from stdout/err of forked command and write it to Log file
	while(fgets(out_str, sizeof(out_str), cmd_fd))
		tranc_file_and_wrire(loggers_path, out_str, max_lines_on_Logger);

	//Check exit status of forked command
	if(pclose(cmd_fd) == 256)
		printf("Command \"%s\" Exit with Error !!!\n", system_call_str);
	else if(daemon_run)
		printf("Thread for command \"%s\" Exit unexpectedly !!!\n", system_call_str);
	else
		unlink(loggers_path); //Delete Logger file
	//free allocated memory
	free(loggers_path);
	return NULL;
}

int tranc_file_and_wrire(char *loggers_path,char *buff, unsigned int limit_lines)
{
	FILE *Log_fd;
	char *file_cont;
	int c=0, flag=0;
	unsigned int i, lines=0, str_len;
	Log_fd = fopen(loggers_path, "r+");
	if(Log_fd)
	{
		fseek(Log_fd, 0, SEEK_END);
		if(ftell(Log_fd))
		{
			//allocate memory with length the file size
			if(!(file_cont = malloc(ftell(Log_fd)+1)))
			{
				fprintf(stderr, "Memory Error!!!!");
				exit(EXIT_FAILURE);
			}
			//Load file to buffer and Count the lines
			rewind(Log_fd);
			for(i=0; (c = fgetc(Log_fd)) != EOF;)
			{
				if(flag)
				{
					file_cont[i] = c;
					i++;
				}
				if(c == '\n')
				{
					lines++;
					flag = 1;
				}
			}
			file_cont[i] = '\0';
			//Check for amount of lines
			if(lines >= limit_lines)
			{
				Log_fd = freopen(loggers_path, "w", Log_fd);
				str_len = strlen(file_cont);
				for(i=0; i<str_len; i++)
					fputc(file_cont[i], Log_fd);
			}
			else
				fseek(Log_fd, 0, SEEK_END);
			free(file_cont);
		}
		fprintf(Log_fd, "%s",buff);
		fclose(Log_fd);
		return EXIT_SUCCESS;
	}
	else
	{	//Build New Logger file
		Log_fd = fopen(loggers_path, "w");
		if(Log_fd)
			fclose(Log_fd);
		else
			perror("fopen_error");
	}
	return EXIT_FAILURE;
}
