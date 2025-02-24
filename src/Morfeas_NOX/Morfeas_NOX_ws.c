/*
File: Morfeas_NOX_ws.c, Implementation of WebSocket functionality for Morfeas_NOX_if, Part of Morfeas_project.
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
#define DEBUG false

#define PORT_init 8081
#define PORT_pool_size 4
#define MAX_AMOUNT_OF_CLIENTS 8

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#include <nopoll.h>
#include <cjson/cJSON.h>

#include "../Morfeas_Types.h"
#include "../Supplementary/Morfeas_Logger.h"

struct WS_send_frame_struct{
	unsigned long long timestamp;
	float NOx_val[2];
	float O2_val[2];
}static WS_NOX_sensors_data={0};

//External variables
extern volatile unsigned char NOX_handler_run;
extern pthread_mutex_t NOX_access;

//Global static constants
static const char Morfeas_NOX_ws_server_accepted_protocol[] = "Morfeas_NOX_WS_if";

//Global static variables
static volatile unsigned char amount_of_clients;
static pthread_mutex_t WS_NOX_sensors_data_access = PTHREAD_MUTEX_INITIALIZER;

//Callback function
static nopoll_bool Morfeas_NOX_ws_server_on_open(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data);
static nopoll_bool Morfeas_NOX_ws_server_on_ready(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data);
static void Morfeas_NOX_ws_server_on_msg(noPollCtx *ctx, noPollConn *conn, noPollMsg *msg, noPollPtr user_data);

//Shared function, Importing meas from NOXs_data to WS_NOX_sensors_data.
void Morfeas_NOX_ws_server_send_meas(struct UniNOx_sensor *NOXs_data)
{
	int i;
	struct timespec now;

	if(!NOXs_data)
		return;
	clock_gettime(CLOCK_REALTIME, &now);
	pthread_mutex_lock(&WS_NOX_sensors_data_access);
		WS_NOX_sensors_data.timestamp = now.tv_sec;
		WS_NOX_sensors_data.timestamp *= 1000;//Convert to milliseconds.
		WS_NOX_sensors_data.timestamp += now.tv_nsec/1000000;//Convert to milliseconds and add.
		for(i=0; i<2; i++)
		{
			WS_NOX_sensors_data.NOx_val[i] = NOXs_data[i].NOx_value;
			WS_NOX_sensors_data.O2_val[i] = NOXs_data[i].O2_value;
		}
	pthread_mutex_unlock(&WS_NOX_sensors_data_access);
}

//WebSocket listener Thread function
void * Morfeas_NOX_ws_server(void *varg_pt)
{
	struct Morfeas_NOX_if_stats *stats;
	int port;
	char p_buff[10];
	noPollCtx *master_ctx;
	noPollConn *WS_serv;

	if(!varg_pt)//Immediately return if varg_pt is NULL.
		return NULL;
	stats = ((struct NOX_DBus_thread_arguments_passer *)varg_pt)->stats;//Decoded variables from passer
	master_ctx = nopoll_ctx_new();
	amount_of_clients = 0;
#if DEBUG
	nopoll_log_enable(master_ctx, nopoll_true); nopoll_log_color_enable(master_ctx, true);
#endif
	for(port = PORT_init; port<(PORT_init+PORT_pool_size); port++)
	{
		sprintf(p_buff, "%hu", port);
		if(nopoll_conn_is_ok((WS_serv = nopoll_listener_new(master_ctx, "0.0.0.0", p_buff))))
			break;
		usleep(100000);
	}
	if(port>=(PORT_init+PORT_pool_size))
	{
		Logger("ERROR: nopoll_listener_new() Failed, WebSocket server will terminate\n");
		return NULL;
	}
	//Set ws_port with server's TCP port.
	pthread_mutex_lock(&NOX_access);
		stats->ws_port = port;
	pthread_mutex_unlock(&NOX_access);

	Logger("WebSocket server started and listening@%s on ref:%d\n", nopoll_conn_port(WS_serv), nopoll_conn_ref_count(WS_serv));
	//Set Callback functions
	nopoll_ctx_set_on_open(master_ctx, Morfeas_NOX_ws_server_on_open, NULL);
	nopoll_ctx_set_on_ready(master_ctx, Morfeas_NOX_ws_server_on_ready, NULL);
	nopoll_ctx_set_on_msg(master_ctx, Morfeas_NOX_ws_server_on_msg, NULL);
	//Server's main loop
	while(NOX_handler_run)
	{
		nopoll_loop_wait(master_ctx, 100000);//Process WebSocket events
		//Logger("amount_of_clients=%d\n", amount_of_clients);
	}
	nopoll_conn_close(WS_serv);
	Logger("Listener: finishing references: %d\n", nopoll_ctx_ref_count(master_ctx));
	//Clean up
	nopoll_ctx_unref(master_ctx);
	nopoll_cleanup_library();
	return NULL;
}

static void _Morfeas_NOX_ws_server_conn_on_close(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
	amount_of_clients--;
	Logger("Close connection with ID:%d\n", nopoll_conn_get_id(conn));
	return;
}

static nopoll_bool Morfeas_NOX_ws_server_on_open(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
	if(amount_of_clients>=MAX_AMOUNT_OF_CLIENTS)
	{
		Logger("Max amount of Clients reached!!!\n");
		return nopoll_false;
	}
	//Set and check requested protocol.
	nopoll_conn_set_accepted_protocol(conn, Morfeas_NOX_ws_server_accepted_protocol);
	if(!nopoll_cmp(nopoll_conn_get_requested_protocol(conn), nopoll_conn_get_accepted_protocol(conn)))
	{
		Logger("New connection from %s rejected due to invalid protocol!!!\n", nopoll_conn_host(conn));
		return nopoll_false;
	}
	else
		Logger("New Connection request from %s get ID:%d\n", nopoll_conn_host(conn), nopoll_conn_get_id(conn));
	return nopoll_true;
}

static nopoll_bool Morfeas_NOX_ws_server_on_ready(noPollCtx *ctx, noPollConn *conn, noPollPtr user_data)
{
	nopoll_conn_set_on_close(conn, _Morfeas_NOX_ws_server_conn_on_close, NULL);
	Logger("Connection from %s with ID:%d is ready\n", nopoll_conn_host(conn), nopoll_conn_get_id(conn));
	amount_of_clients++;
	return nopoll_true;
}

static void Morfeas_NOX_ws_server_on_msg(noPollCtx *ctx, noPollConn *conn, noPollMsg *msg, noPollPtr user_data)
{
	//cJSON related variables
	cJSON *root, *meas_tbl, *meas_JSON;

	char JSON_str[256];
	const char *msg_cont, close_resp[]="Unknown Request";

	if(nopoll_msg_is_final(msg))
	{
		msg_cont = (const char *) nopoll_msg_get_payload(msg);
		if(nopoll_cmp(msg_cont, "getMeasRAW"))
		{
			pthread_mutex_lock(&WS_NOX_sensors_data_access);
				nopoll_conn_send_binary(conn, (char *)&WS_NOX_sensors_data, sizeof(WS_NOX_sensors_data));
			pthread_mutex_unlock(&WS_NOX_sensors_data_access);
		}
		else if(nopoll_cmp(msg_cont, "getMeasJSON"))
		{
			root = cJSON_CreateObject();
			meas_tbl = cJSON_CreateArray();
			pthread_mutex_lock(&WS_NOX_sensors_data_access);
				cJSON_AddNumberToObject(root, "Samples_timestamp", WS_NOX_sensors_data.timestamp);
				for(unsigned char i=0; i<2; i++)
				{
					meas_JSON = cJSON_CreateObject();
					cJSON_AddNumberToObject(meas_JSON, "NOx_val", WS_NOX_sensors_data.NOx_val[i]);
					cJSON_AddNumberToObject(meas_JSON, "O2_val", WS_NOX_sensors_data.O2_val[i]);
					cJSON_AddItemToArray(meas_tbl, meas_JSON);
				}
			pthread_mutex_unlock(&WS_NOX_sensors_data_access);
			cJSON_AddItemToObject(root, "meas", meas_tbl);
			cJSON_PrintPreallocated(root, JSON_str, sizeof(JSON_str), 0);
			cJSON_Delete(root);
			nopoll_conn_send_text(conn, JSON_str, -1);
		}
		else
			nopoll_conn_close_ext(conn, 1011, close_resp, strlen(close_resp));
			//nopoll_conn_send_binary(conn, msg_cont, nopoll_msg_get_payload_size(msg));
	}
	return;
}
