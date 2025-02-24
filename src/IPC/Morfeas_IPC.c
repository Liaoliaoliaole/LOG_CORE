/*
File: Morfeas_IPC.c, Implementation of functions for IPC.
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
#include <stddef.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Morfeas_IPC.h"

const char *Morfeas_IPC_handler_type_name[] = {
	"SDAQ", "MDAQ", "IOBOX", "MTI", "NOX", "CPAD", NULL
};
	//----TX Functions----//
//function for TX, return the amount of bytes that transmitted through the FIFO, or 0 in failure
size_t IPC_msg_TX(int FIFO_fd, IPC_message *IPC_msg_ptr)
{
	return write(FIFO_fd, IPC_msg_ptr, sizeof(IPC_message));
}
//Function for construction of message for registration of a Handler
size_t IPC_Handler_reg_op(int FIFO_fd, unsigned char handler_type, char *Dev_or_Bus_name, unsigned char unreg)
{
	IPC_message IPC_reg_msg = {0};
	//Construct and send Handler registration msg
	IPC_reg_msg.Handler_reg.IPC_msg_type = unreg ? IPC_Handler_unregister : IPC_Handler_register;
	IPC_reg_msg.Handler_reg.handler_type = handler_type;
	memccpy(&(IPC_reg_msg.Handler_reg.Dev_or_Bus_name), Dev_or_Bus_name, '\0', Dev_or_Bus_name_str_size);
	IPC_reg_msg.Handler_reg.Dev_or_Bus_name[Dev_or_Bus_name_str_size-1] = '\0';
	return IPC_msg_TX(FIFO_fd, &IPC_reg_msg);
}
	//----RX Function----//
//function for RX, return the type of the received message or 0 in failure
unsigned char IPC_msg_RX(int FIFO_fd, IPC_message *IPC_msg_ptr)
{
	fd_set readCheck;
    fd_set errCheck;
    struct timeval timeout;
	unsigned char type;
	int select_ret;
	ssize_t read_bytes = -1;
	FD_ZERO(&readCheck);
    FD_ZERO(&errCheck);
	FD_SET(FIFO_fd, &readCheck);
	FD_SET(FIFO_fd, &errCheck);
	timeout.tv_sec = 0;
	timeout.tv_usec = 100000;
	select_ret = select(FIFO_fd+1, &readCheck, NULL, &errCheck, &timeout);
	if (select_ret < 0)
		perror("RX -> Select failed ");
	else if (FD_ISSET(FIFO_fd, &errCheck))
		perror("RX -> FD error ");
	else if (FD_ISSET(FIFO_fd, &readCheck))
	{
		read_bytes = read(FIFO_fd, IPC_msg_ptr, sizeof(IPC_message));
		if((type = ((unsigned char *)IPC_msg_ptr)[0]) <= Morfeas_IPC_MAX_type)
			return type;
	}
	if(read_bytes != -1)
		printf("Morfeas_IPC: Wrong amount of Bytes Received!!!\n");
	return 0;
}

int if_type_str_2_num(const char * if_type_str)
{
	if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[SDAQ]))
		return SDAQ;
	else if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[MDAQ]))
		return MDAQ;
	else if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[IOBOX]))
		return IOBOX;
	else if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[MTI]))
		return MTI;
	else if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[NOX]))
		return NOX;
	else if(!strcmp(if_type_str, Morfeas_IPC_handler_type_name[CPAD]))
		return CPAD;
	else
		return -1;
}
