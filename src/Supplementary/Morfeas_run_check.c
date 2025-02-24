/*
File: Morfeas_run_check.c Implementation of check_run function.
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
#include <string.h>
#include <arpa/inet.h>

int check_already_run(const char *prog_name)
{
	char out_str[128]={0}, cmd[128], *tok, i=1;

	sprintf(cmd, "pidof %s",prog_name);
	FILE *out = popen(cmd, "r");
	fgets(out_str, sizeof(out_str), out);
	pclose(out);
	if(i==1)
	{
		tok = strtok(out_str, " ");
		while((tok = strtok(NULL, " ")))
			i++;
	}
	return i>1? EXIT_FAILURE : EXIT_SUCCESS;
}

int check_already_run_with_same_arg(const char *called_prog_name,const char *arg_check)
{
	const char *prog_name = called_prog_name;
	char out_str[1024] = {0}, *cmd, *buff, *tok, *word;
	unsigned int i=0;
	
	if(!called_prog_name || !arg_check)
		return EXIT_FAILURE;
	//Allocate memory for buff, where is the size of called_prog_name
	if(!(buff = calloc(strlen(called_prog_name)+1, sizeof(*buff))))
	{
		fprintf(stderr, "Memory error!!!\n");
		exit(EXIT_FAILURE);
	}
	//copy called_prog_name to buff and split it with delimiter "/"
	strcpy(buff, called_prog_name);
	if((tok = strtok(buff, "/")))
	{
		while((tok = strtok(NULL, "/")))
			prog_name = tok;
	}
	//Allocate memory for cmd, where is sizes of "prog_name" + 50 chars -> for string "ps aux | grep --color=none -E '([0-9] |/)%s'"
	if(!(cmd = calloc(strlen(prog_name)+50, sizeof(*cmd))))
	{
		fprintf(stderr, "Memory error!!!\n");
		free(buff);
		exit(EXIT_FAILURE);
	}
	//Construct cmd
	sprintf(cmd, "ps aux | grep --color=none -E '([0-9] |/)%s'", prog_name);
	//fork cmd and get stdout through pipe to out_str
	FILE *out = popen(cmd, "r");
	fread(out_str, sizeof(out_str), sizeof(*out_str), out);
	pclose(out);
	// split out_str with delimiter "\n"
	if((tok = strtok(out_str, "\n")))
	{
		do{
			if((word = strstr(tok, arg_check)))//check if arg_check is in line
			{
				if(*(word+-1)==' ' && (*(word+strlen(arg_check))==' ' || *(word+strlen(arg_check))=='\0'))//Check if the founded is a wholly word
					i++;
			}
		}while((tok = strtok(NULL, "\n")));
	}
	free(cmd);
	free(buff);
	return i>1 ? EXIT_FAILURE : EXIT_SUCCESS;
}

unsigned char Checksum(void *data, size_t data_size)
{
    unsigned char checksum = 0;
	unsigned char *data_dec = (unsigned char *)data;
    for (size_t i = 0; i < data_size; i++, data_dec++)
    {
        checksum = (checksum >> 1) + ((checksum & 1) << 7);
        checksum += *(data_dec);
    }
    return checksum;
}

//Return 0 if ipv4_str is not valid. Using inet_pton to validate
int is_valid_IPv4(const char* ipv4_str)
{
	unsigned char buf[sizeof(struct in6_addr)];
    return inet_pton(AF_INET, ipv4_str, buf)>0;
}
