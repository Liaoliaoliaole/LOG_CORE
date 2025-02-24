/*
File: Morfeas_Logger.h Declaration of Morfeas_Logger Functions.
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

void Logger(const char *fmt, ...);

//Functions related to git logs info and compilation date. Implementation at Morfeas_info.c
char* Morfeas_get_release_date(void);
char* Morfeas_get_compile_date(void);
char* Morfeas_get_curr_git_hash(void);
