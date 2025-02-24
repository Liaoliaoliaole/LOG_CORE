/*
File "Morfeas_JSON.h" part of Morfeas project, contains declaration of JSON exporting functions.
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

//Converting and exporting function for struct stats (Type Morfeas_sys_stats). Convert it to JSON format and save it to logstat_path
int logstat_sys(char *logstat_path, void *stats_arg);
//Delete logstat file for opc_ua
int delete_logstat_sys(char *logstat_path);

//Converting and exporting function for struct stats (Type Morfeas_SDAQ_if_stats). Convert it to JSON format and save it to logstat_path
int logstat_SDAQ(char *logstat_path, void *stats_arg);
//Delete logstat file for SDAQnet_Handler
int delete_logstat_SDAQ(char *logstat_path, void *stats_arg);

//Converting and exporting function for MDAQ Modbus register. Convert it to JSON format and save it to logstat_path
int logstat_MDAQ(char *logstat_path, void *stats_arg);
//Delete logstat file for IOBOX_handler
int delete_logstat_MDAQ(char *logstat_path, void *stats_arg);

//Converting and exporting function for IOBOX Modbus register. Convert it to JSON format and save it to logstat_path
int logstat_IOBOX(char *logstat_path, void *stats_arg);
//Delete logstat file for IOBOX_handler
int delete_logstat_IOBOX(char *logstat_path, void *stats_arg);

//Converting and exporting function for MTI's stats struct. Convert it to JSON format and save it to logstat_path
int logstat_MTI(char *logstat_path, void *stats_arg);
//Delete logstat file for MTI_handler
int delete_logstat_MTI(char *logstat_path, void *stats_arg);

//Converting and exporting function for NOx's stats struct. Convert it to JSON format and save it to logstat_path
int logstat_NOX(char *logstat_path, void *stats_arg);
//Delete logstat file for NOX_handler
int delete_logstat_NOX(char *logstat_path, void *stats_arg);