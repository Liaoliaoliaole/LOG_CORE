/*
File: NOX_func.c, Implementation of functions for NOX (CANBus), Part of Morfeas_project.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include <arpa/inet.h>
#include <linux/can.h>
#include <linux/can/raw.h>

#include "../Morfeas_Types.h"
#include "../Supplementary/Morfeas_Logger.h"

const char *Heater_mode_str[]={
	"Automatic mode",
	"Heating up (3 to 4)",
	"Heating up (1 to 2)",
	"Heater off"
};
const char *Errors_dec_str[]={
	"No error",
	"Open Wire",
	"Short circuit"
};
				/*TX Functions*/
//Heater control function for UniNOx sensor
int NOx_heater(int socket_fd, unsigned char start_code)
{
	struct can_frame frame_tx = {0};
	NOx_can_id *NOX_id_ptr = (NOx_can_id *)&(frame_tx.can_id);
	NOx_TX_frame *NOX_tx_data_ptr = (NOx_TX_frame *)&(frame_tx.data);

	//construct identifier for synchronization measure message
	NOX_id_ptr->flags = 4;//set the EFF
	NOX_id_ptr->NOx_addr = NOx_TX_addr;
	frame_tx.can_dlc = sizeof(NOx_TX_frame);//Payload size
	memset(NOX_tx_data_ptr->tbd, -1, sizeof(NOX_tx_data_ptr->tbd));
	NOX_tx_data_ptr->start_code = start_code;
	if(write(socket_fd, &frame_tx, sizeof(struct can_frame))>0)
		return 1;
	return 0;
}
//Error Decode Function
unsigned char NOx_error_dec(unsigned char error_code)
{
	switch(error_code)
	{
		case NOx_No_error: return 0;
		case NOx_Open_wire_error: return 1;
		case NOx_Short_circuit_error: return 2;
	}
	return 0;
}