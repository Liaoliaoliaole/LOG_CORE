/*
File: Morfeas_RPi_Hat.h  Declaration of functions related to Morfeas_RPi_Hat.
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
#define RED_LED 5
#define GREEN_LED 6
#define BLUE_LED 13

#define I2C_BUS_NUM 1
#define MAX9611_temp_scaler 0.864 //0.48 * 9/5
#define MAX9611_comp_scaler 0.268
#define MAX9611_default_volt_meas_scaler 0.014
#define MAX9611_default_current_meas_scaler 0.001222 //for 22 mohm shunt

extern int Morfeas_hat_error_num;

enum Morfeas_hat_error_enum{
	port_num_err,
//---- LEDs related ----//
	LED_no_support,
	GPIO_dir_error,
	GPIO_read_error,
	GPIO_read_file_error,
	GPIO_write_error,
	GPIO_write_file_error,
//---- I2C device related ----//
	i2c_bus_open_error,
	ioctl_error,
	i2c_write_err,
	unknown_slave,
	EEPROM_not_found,
	EEPROM_is_blank,
	EEPROM_Erase_error,
	EEPROM_verification_err,
	Checksum_error
};

#pragma pack(push, 1)//use pragma pack() to pack the following structs to 1 byte size (aka no zero padding)
//Struct for EEPROM(24AA08) data
struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config{
	struct last_port_calibration_date{
		unsigned char year;
		unsigned char month;
		unsigned char day;
	}last_cal_date;
	char volt_meas_offset;
	float volt_meas_scaler;
	char curr_meas_offset;
	float curr_meas_scaler;
	unsigned char checksum;
};
//Struct for current sense amplifier(MAX9611) data
struct Morfeas_RPi_Hat_Port_meas{
	unsigned short port_current;
	unsigned short port_voltage;
	unsigned short output;
	unsigned short set_val;
	short temperature;
};
//Structs for MAX9611 Control registers
struct MAX9611_config_1{
	unsigned mux : 3;
	unsigned shutdown : 1;
	unsigned LP : 1;
	unsigned mode : 3;
};
struct MAX9611_config_2{
	unsigned _NC : 4;
	unsigned DTIM : 1;
	unsigned RTIM : 1;
	unsigned _NC_ : 2;
};
#pragma pack(pop)//Disable packing

//Function that return a string for the last error related to Morfeas_RPi_Hat
char* Morfeas_hat_error();

//Decode string CAN_if_name to Port number. Return: Port's Number or -1 on failure
int get_port_num(char * CAN_if_name);
	//---- LEDs related ----//
//Init Morfeas_RPi_Hat LEDs, return 1 if sysfs files exist, 0 otherwise.
int LEDs_init();
//Write value to Morfeas_RPi_Hat LED, return 0 if write was success, -1 otherwise.
int LED_write(int LED_name, int value);
//Read value of Morfeas_RPi_Hat LED by name, return value of the LED, or -1 if read failed.
int LED_read(int LED_name);

//---- I2C device related ----//
/* All I2C related functions Return:
 * 0 in success,
 * -1 otherwise
 */
//Function to init the MAX9611, return 0 in success, -1 otherwise.
int MAX9611_init(unsigned char port, unsigned char i2c_dev_num);
//Function that read measurements for MAX9611, store them on memory pointed by meas.
int get_port_meas(struct Morfeas_RPi_Hat_Port_meas *meas, unsigned char port, unsigned char i2c_dev_num);

//Function that read data from EEPROM
int read_port_config(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config *config, unsigned char port, unsigned char i2c_dev_num);
//Function that write data to EEPROM, checksum calculated inside.
int write_port_config(struct Morfeas_RPi_Hat_EEPROM_SDAQnet_Port_config *config, unsigned char port, unsigned char i2c_dev_num);
//Function that erase EEPROM.
int erase_EEPROM(unsigned char port, unsigned char i2c_dev_num);



