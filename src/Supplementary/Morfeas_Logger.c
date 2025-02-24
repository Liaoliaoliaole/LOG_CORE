/*
File: Morfeas_Logger.h Implementation of Morfeas_Logger Functions.
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
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
 #include <unistd.h>

void Logger(const char *fmt, ...)
{
	char time_buff[30];
	struct timespec now;
	struct tm * timeinfo;

	clock_gettime(CLOCK_REALTIME, &now);
	timeinfo = localtime(&(now.tv_sec));
	strftime(time_buff,sizeof(time_buff),"%F %a %T",timeinfo);
	fprintf(stdout,"(%s.%04lu): ", time_buff, now.tv_nsec/100000);
	va_list arg;
    va_start(arg, fmt);
    	vfprintf(stdout, fmt, arg);
    va_end(arg);
	fflush(stdout);
    fsync(fileno(stdout));
}
