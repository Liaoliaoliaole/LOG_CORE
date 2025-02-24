/*
File: Morfeas_RPi_Hat.c  Implementation of functions related to Morfeas_RPi_Hat.
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
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>

#include <arpa/inet.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include "Morfeas_RPi_Hat.h"
#include "../Morfeas_Types.h"
#include "../Supplementary/Morfeas_run_check.h"

int Morfeas_hat_error_num = -1;

//Function that return error message
char* Morfeas_hat_error()
{
	static char error_str[256] = {0};
	switch(Morfeas_hat_error_num)
	{
		case port_num_err: return "Port Number is out of range (0..3)\n";
			//---- I2C device related ----//
		case i2c_bus_open_error: sprintf(error_str, "Error on I2C open: %s\n",strerror(errno)); return error_str;
		case ioctl_error: sprintf(error_str, "Error on ioctl: %s\n",strerror(errno)); return error_str;
		case i2c_write_err: return "Error I2C_Write!!!\n";
		case EEPROM_not_found: return "Configuration EEPROM(24AA08) Not found!!!\n";
		case EEPROM_is_blank: return "EEPROM is Blank!!!\n";
		case EEPROM_Erase_error: return "EEPROM erase error: EEPROM is not Blank!!!\n";
		case EEPROM_verification_err: return "Write_port_config: Verification Failed!!!\n";
		case Checksum_error: return "Configuration EEPROM(24AA08) Checksum Error!!!\n";
		case unknown_slave: return "Given I2C slave address is not of any Morfeas_RPI_Hat's Devices\n";
			//---- LEDs related ----//
		case LED_no_support: return "LEDs are Not supported!\n";
		case GPIO_dir_error: return "Failed to set GPIO direction!\n";
		case GPIO_read_error: return "Failed to read GPIO value!!!\n";
		case GPIO_read_file_error: return "Failed to open GPIO value file for reading!!!\n";
		case GPIO_write_error: return "Failed to write GPIO value!!!\n";
		case GPIO_write_file_error: return "Failed to open GPIO value file for writing!!!\n";
		default :
			sprintf(error_str, "Unknown Error (%d)!!!\n",Morfeas_hat_error_num);
			return error_str;
	}
}

//Decode string CAN_if_name to Port number. Return: Port's Number or -1 on failure
int get_port_num(char * CAN_if_name)
{
	if(!strcmp(CAN_if_name, "can0"))
		return 0;
	else if(!strcmp(CAN_if_name, "can1"))
		return 1;
	else if(!strcmp(CAN_if_name, "can2"))
		return 2;
	else if(!strcmp(CAN_if_name, "can3"))
		return 3;
	else
	{
		Morfeas_hat_error_num = port_num_err;
		return -1;
	}
}
	//---- LEDs related ----//
//Init Morfeas_RPi_Hat LEDs, return 1 if sysfs files exist, 0 otherwise.
int LEDs_init(char *CAN_IF_name)
{
	char path[35];
	char buffer[3];
	ssize_t bytes_written;
	int sysfs_fd, i, pin, port;

	port = get_port_num(CAN_IF_name);
	if(port>=0 && port<=3)
	{	//init GPIO on sysfs
		for(i=0; i<2; i++)
		{
			sysfs_fd = open("/sys/class/gpio/export", O_WRONLY);
			if(sysfs_fd < 0)
			{
				Morfeas_hat_error_num = LED_no_support;
				return 0;
			}
			pin = i ? BLUE_LED : RED_LED;
			bytes_written = snprintf(buffer, 3, "%d", pin);
			write(sysfs_fd, buffer, bytes_written);
			close(sysfs_fd);
		}
		sleep(1);
		//Set direction of GPIOs
		for(i=0; i<2; i++)
		{
			pin = i ? BLUE_LED : RED_LED;
			snprintf(path, 35, "/sys/class/gpio/gpio%d/direction", pin);
			sysfs_fd = open(path, O_WRONLY);
			if(sysfs_fd < 0)
			{
				Morfeas_hat_error_num = LED_no_support;
				return 0;
			}
			if (write(sysfs_fd, "out", 3)<0)
			{
				Morfeas_hat_error_num = GPIO_dir_error;
				return 0;
			}
			close(sysfs_fd);

		}
	}
	return 1;
}
//Write value to Morfeas_RPi_Hat LED, return 0 if write was success, -1 otherwise.
int LED_write(int LED_name, int value)
{
	static const char s_values_str[] = "01";
	char path[50];
	int fd;
	snprintf(path, 30, "/sys/class/gpio/gpio%d/value", LED_name);
	fd = open(path, O_WRONLY);
	if(-1 == fd)
	{
		Morfeas_hat_error_num = GPIO_write_file_error;
		return -1;
	}
	if(1 != write(fd, &s_values_str[!value ? 0 : 1], 1))
	{
		Morfeas_hat_error_num = GPIO_write_error;
		return -1;
	}
	close(fd);
	return(0);
}
//Read value of Morfeas_RPi_Hat LED by name, return value of the LED, or -1 if read failed.
int LED_read(int LED_name)
{
	char read_val[30] = {0};
	char path[50];
	int fd;
	snprintf(path, 30, "/sys/class/gpio/gpio%d/value", LED_name);
	fd = open(path, O_WRONLY);
	if(-1 == fd)
	{
		Morfeas_hat_error_num = GPIO_read_file_error;
		return -1;
	}
	if(read(fd, &read_val, 1) != 1)
	{
		Morfeas_hat_error_num = GPIO_read_error;
		return -1;
	}
	close(fd);
	return(atoi(read_val));
}

	//---- I2C device related ----//
/*
//Function that write block "data" to I2C device with address "dev_addr" on I2C bus "i2c_dev_num". Return: 0 on success, -1 on failure.
int I2C_write_block(unsigned char i2c_dev_num, unsigned char dev_addr, unsigned char reg, void *data, unsigned char len)
{
	char filename[30];//Path to sysfs I2C-dev
	int i2c_fd;//I2C file descriptor
	int write_bytes;
	unsigned char *data_w_reg;
	//Open I2C-bus
	sprintf(filename, "/dev/i2c-%u", i2c_dev_num);
	i2c_fd = open(filename, O_RDWR);
	if(i2c_fd < 0)
	{
	  Morfeas_hat_error_num = i2c_bus_open_error;
	  return -1;
	}
	if(ioctl(i2c_fd, I2C_SLAVE, dev_addr) < 0)
	{
	  Morfeas_hat_error_num = ioctl_error;
	  close(i2c_fd);
	  return -1;
	}
	if(!(data_w_reg = calloc(len+1, sizeof(*data_w_reg))))
	{
	  fprintf(stderr, "Memory error!!!\n");
	  exit(EXIT_FAILURE);
	}
	data_w_reg[0] = reg;
	memcpy(data_w_reg+1, data, len);
	write_bytes = write(i2c_fd, data_w_reg, len+1);
	free(data_w_reg);
	close(i2c_fd);
	return write_bytes == len+1 ? 0 : -1;
}
*/
//Function that read a block "data" from an I2C device with address "dev_addr" on I2C bus "i2c_dev_num". Return: 0 on success, -1 on failure.
int I2C_read_block(unsigned char i2c_dev_num, unsigned char dev_addr, unsigned char reg, void *data, unsigned char len)
{
	char filename[30];//Path to sysfs I2C-dev
	int ret_val, i2c_fd;//I2C file descriptor
	struct i2c_rdwr_ioctl_data msgset;
	//Open I2C-bus
	sprintf(filename, "/dev/i2c-%u", i2c_dev_num);
	i2c_fd = open(filename, O_RDWR);
	if(i2c_fd < 0)
	{
	  Morfeas_hat_error_num = i2c_bus_open_error;
	  return -1;
	}
	//Allocate memory for the messages
	msgset.nmsgs = 2;
	if(!(msgset.msgs = calloc(msgset.nmsgs, sizeof(struct i2c_msg))))
	{
	  fprintf(stderr, "Memory error!!!\n");
	  exit(EXIT_FAILURE);
	}
	//Build message for Write reg
	msgset.msgs[0].addr = dev_addr;
	msgset.msgs[0].flags = 0; //Write
	msgset.msgs[0].len = 1;
	msgset.msgs[0].buf = &reg;
	//Build message for Read *data
	msgset.msgs[1].addr = dev_addr;
	msgset.msgs[1].flags = I2C_M_RD;//Read flag
	msgset.msgs[1].len = len;
	msgset.msgs[1].buf = data;
	//write reg and read the measurements
	if((ret_val = ioctl(i2c_fd, I2C_RDWR, &msgset)) < 0)
		Morfeas_hat_error_num = ioctl_error;
	close(i2c_fd);
	free(msgset.msgs);
	return ret_val==msgset.nmsgs ? 0 : -1;
}

//Function to init the MAX9611
int MAX9611_init(unsigned char port, unsigned char i2c_dev_num)
{
	struct MAX9611_config_1 conf_word1={0};
	struct MAX9611_config_2 conf_word2={0};
	unsigned char send_data[3];
	char filename[30];//Path to sysfs I2C-dev
	int i2c_fd;//I2C file descriptor
	int addr;// Address for MAX9611 connected to port
	//Get address of MAX9611
	switch(port)
	{
		case 0: addr=0x70; break;
		case 1: addr=0x73; break;
		case 2: addr=0x7c; break;
		case 3: addr=0x7f; break;
		default: Morfeas_hat_error_num = unknown_slave; return -1;
	}
	//Open I2C-bus
	sprintf(filename, "/dev/i2c-%u", i2c_dev_num);
	i2c_fd = open(filename, O_RDWR);
	if(i2c_fd < 0)
	{
	  Morfeas_hat_error_num = i2c_bus_open_error;
	  return EXIT_FAILURE;
	}
	if(ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
	{
	  Morfeas_hat_error_num = ioctl_error;
	  close(i2c_fd);
	  return EXIT_FAILURE;
	}

	//Prepare config word 2 for CSA with Gain x4
	conf_word1.mode = 0b111;
	conf_word1.mux = 1;

	//load register address and config words to data, and send them.
	send_data[0] = 0x0A;//register addres for configuration word 1
	send_data[1] = *((unsigned char*)&conf_word1);
	send_data[2] = *((unsigned char*)&conf_word2);
	if(write(i2c_fd, send_data, sizeof(send_data)) != sizeof(send_data))
	{
		  Morfeas_hat_error_num = i2c_write_err;
		  close(i2c_fd);
		  return EXIT_FAILURE;
	}

	//Prepare config word 2 for CSA mode to "Read all channels sequentially every 2ms".
	conf_word1.mux = 0b111;

	//load register address and config words to data, and send them.
	send_data[0] = 0x0A;//register addres for configuration word 1
	send_data[1] = *((unsigned char*)&conf_word1);
	if(write(i2c_fd, send_data, 2) != 2)
	{
		  Morfeas_hat_error_num = i2c_write_err;
		  close(i2c_fd);
		  return EXIT_FAILURE;
	}
	close(i2c_fd);
	return EXIT_SUCCESS;
}
//Function that read measurements for MAX9611, store them on memory pointed by meas.
int get_port_meas(struct Morfeas_RPi_Hat_Port_meas *meas, unsigned char port, unsigned char i2c_dev_num)
{
	int ret_val, addr, i, port_meas_size;// Address for MAX9611 connected to port
	unsigned short *meas_dec = (unsigned short *)meas;

	switch(port)
	{
		case 0: addr=0x70; break;
		case 1: addr=0x73; break;
		case 2: addr=0x7c; break;
		case 3: addr=0x7f; break;
		default: Morfeas_hat_error_num = unknown_slave; return -1;
	}
	if(!(ret_val = I2C_read_block(i2c_dev_num, addr, 0, meas, sizeof(struct Morfeas_RPi_Hat_Port_meas))))
	{
		port_meas_size = sizeof(struct Morfeas_RPi_Hat_Port_meas)/sizeof(unsigned short);
		for(i=0; i<port_meas_size; i++)
		{
			*(meas_dec+i) = htons(*(meas_dec+i));
			*(meas_dec+i) >>= i<port_meas_size-1 ? 4 : 7;//Shift right 4 for all meas except temp that shift right 7. From MAX9611 datasheet
		}
	}
	return ret_val;
}
//Function that read data from EEPROM(24AA08)
int read_port_config(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config *config, unsigned char port, unsigned char i2c_dev_num)
{
	struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config config_read;
	unsigned int blank_check=0;
	int addr;// Address for 24AA08 EEPROM

	switch(port)
	{
		case 0: addr=0x50; break;
		case 1: addr=0x51; break;
		case 2: addr=0x52; break;
		case 3: addr=0x53; break;
		default: Morfeas_hat_error_num = unknown_slave; return -1;
	}
	//Get data from EEPROM
	if(I2C_read_block(i2c_dev_num, addr, 0, &config_read, sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config)))
	{
		Morfeas_hat_error_num = EEPROM_not_found;
		return -1;
	}
	//Check if EEPROM is blank
	for(int i=0; i<sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config); i++)
	{
		if(((unsigned char*)&config_read)[i]==0xff)
			blank_check++;
	}
	if(blank_check == sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config))
	{
		Morfeas_hat_error_num = EEPROM_is_blank;
		return 2;
	}
	//Calculate and compare Checksum
	if(config_read.checksum ^ Checksum(&config_read, sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config)-1))
	{
		Morfeas_hat_error_num = Checksum_error;
		return 1;
	}
	memcpy(config, &config_read, sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config));
	return 0;
}
//Function that write data to EEPROM(24AA08). Checksum calculated inside.
int write_port_config(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config *config, unsigned char port, unsigned char i2c_dev_num)
{
	unsigned char zero=0;
	struct i2c_rdwr_ioctl_data msgset;
	struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config read_config;
	char filename[30];//Path to sysfs I2C-dev
	int addr;// Address for 24AA08 EEPROM
	int i2c_fd;//I2C file descriptor
	unsigned char data_w_reg[17];//16 data bytes + 1 register byte
	int len, reg=0;

	//Calc addr for port argument
	switch(port)
	{
		case 0: addr=0x50; break;
		case 1: addr=0x51; break;
		case 2: addr=0x52; break;
		case 3: addr=0x53; break;
		default: Morfeas_hat_error_num = unknown_slave; return -1;
	}
	//Open I2C-bus
	sprintf(filename, "/dev/i2c-%u", i2c_dev_num);
	i2c_fd = open(filename, O_RDWR);
	if(i2c_fd < 0)
	{
	  Morfeas_hat_error_num = i2c_bus_open_error;
	  return 3;
	}
	//Set addr as I2C_SLAVE address
	if(ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
	{
	  Morfeas_hat_error_num = ioctl_error;
	  close(i2c_fd);
	  return 4;
	}
	//Check device existence.
	if(i2c_smbus_write_quick(i2c_fd, 0))
	{
		Morfeas_hat_error_num = EEPROM_not_found;
		close(i2c_fd);
		return -1;
	}
	//Calculate checksum
	config->checksum = Checksum(config, sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config)-1);
	//Write data blocks to EEPROM. TO-DO Check
	while(reg<sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config))
	{
		data_w_reg[0] = reg;
		if(sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config)-reg<16)
			len = sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config) - reg;
		else
			len = 16;
		memcpy(data_w_reg+1, ((void *)config)+reg, len);
		if(write(i2c_fd, data_w_reg, len+1) != len+1)
		{
		  Morfeas_hat_error_num = i2c_write_err;
		  close(i2c_fd);
		  return -1;
		}
		usleep(10000);//Sleep for 10msec
		reg += len;
	}
	//Read data block from EEPROM.
	//Allocate memory for the messages
	msgset.nmsgs = 2;
	if(!(msgset.msgs = calloc(msgset.nmsgs, sizeof(struct i2c_msg))))
	{
	  fprintf(stderr, "Memory error!!!\n");
	  exit(EXIT_FAILURE);
	}
	//Build message for Write reg at 0
	msgset.msgs[0].addr = addr;
	msgset.msgs[0].flags = 0; //Write
	msgset.msgs[0].len = 1;
	msgset.msgs[0].buf = &zero;
	//Build message for Read *data
	msgset.msgs[1].addr = addr;
	msgset.msgs[1].flags = I2C_M_RD;//Read flag
	msgset.msgs[1].len = sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config);
	msgset.msgs[1].buf = (void *)&read_config;
	//write reg and read the measurements
	if(ioctl(i2c_fd, I2C_RDWR, &msgset) < 0)
	{
		Morfeas_hat_error_num = ioctl_error;
		close(i2c_fd);
		free(msgset.msgs);
		return -1;
	}
	close(i2c_fd);
	free(msgset.msgs);
	//Verification
	for(int i=0; i<sizeof(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config); i++)
	{
		if(((unsigned char*)&read_config)[i]!=((unsigned char*)config)[i])
		{
			Morfeas_hat_error_num = EEPROM_verification_err;
			return -1;
		}
	}
	return 0;
}
#define block_size 16
//Function that erase EEPROM.
int erase_EEPROM(unsigned char port, unsigned char i2c_dev_num)
{
	unsigned char zero=0, read_buff[0xff];
	struct i2c_rdwr_ioctl_data msgset;
	char filename[30];//Path to sysfs I2C-dev
	int addr;// Address for 24AA08 EEPROM
	int i2c_fd;//I2C file descriptor
	unsigned char Blank_w_reg[17];//16 data bytes + 1 register byte
	unsigned short reg=0;

	//Calc addr for port argument
	switch(port)
	{
		case 0: addr=0x50; break;
		case 1: addr=0x51; break;
		case 2: addr=0x52; break;
		case 3: addr=0x53; break;
		default: Morfeas_hat_error_num = unknown_slave; return -1;
	}
	//Open I2C-bus
	sprintf(filename, "/dev/i2c-%u", i2c_dev_num);
	i2c_fd = open(filename, O_RDWR);
	if(i2c_fd < 0)
	{
	  Morfeas_hat_error_num = i2c_bus_open_error;
	  return 3;
	}
	//Set addr as I2C_SLAVE address
	if(ioctl(i2c_fd, I2C_SLAVE, addr) < 0)
	{
	  Morfeas_hat_error_num = ioctl_error;
	  close(i2c_fd);
	  return 4;
	}
	//Check device existence.
	if(i2c_smbus_write_quick(i2c_fd, 0))
	{
		Morfeas_hat_error_num = EEPROM_not_found;
		close(i2c_fd);
		return -1;
	}
	//Init Blank_w_reg to 0xFF (EEPROM blank data)
	memset(Blank_w_reg+1, 0xFF, block_size);
	//Write data blocks to EEPROM.
	while(reg<0xff)
	{
		Blank_w_reg[0] = reg;
		if(write(i2c_fd, Blank_w_reg, block_size+1) != block_size+1)
		{
		  Morfeas_hat_error_num = i2c_write_err;
		  close(i2c_fd);
		  return -1;
		}
		usleep(10000);//Sleep for 10msec
		reg += block_size;
	}
	//Read data block from EEPROM.
	//Allocate memory for the messages
	msgset.nmsgs = 2;
	if(!(msgset.msgs = calloc(msgset.nmsgs, sizeof(struct i2c_msg))))
	{
	  fprintf(stderr, "Memory error!!!\n");
	  exit(EXIT_FAILURE);
	}
	//Build message for Write reg at 0
	msgset.msgs[0].addr = addr;
	msgset.msgs[0].flags = 0; //Write
	msgset.msgs[0].len = 1;
	msgset.msgs[0].buf = &zero;
	//Build message for Read *data
	msgset.msgs[1].addr = addr;
	msgset.msgs[1].flags = I2C_M_RD;//Read flag
	msgset.msgs[1].len = 0xFF;//EEPROM size
	msgset.msgs[1].buf = (void *)&read_buff;
	//write reg and read the measurements
	if(ioctl(i2c_fd, I2C_RDWR, &msgset) < 0)
	{
		Morfeas_hat_error_num = ioctl_error;
		close(i2c_fd);
		free(msgset.msgs);
		return -1;
	}
	close(i2c_fd);
	free(msgset.msgs);
	//Verification
	for(int i=0; i<sizeof(read_buff); i++)
	{
		if(read_buff[i]!=0xFF)
		{
			Morfeas_hat_error_num = EEPROM_Erase_error;
			return -1;
		}
	}
	return 0;
}

