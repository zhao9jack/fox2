/*
 *  plugin_admin.c
 *
 *  Console Program
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-8-31 10:24:34 Created.
 *
 *  Description: This file mainly includes the functions about 
 *
 */

#ifdef __WIN32__
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "server.h"
#include "connection.h"
#include "vdir.h"
#include "session.h"
#include "client.h"


static char* format_timeinterval( char* str, int t )
{
	int m, h, d, s;
	s = t % 60;
	t /= 60;
	m = t % 60;
	t /= 60;
	h = t % 24;
	t /= 24;
	d = t;
	sprintf( str, "%d day  %02d:%02d:%02d", d, h, m, s );
	return str;
}

static int admin_run( connection* conn )
{
	session * sess = conn->session;
	static char conn_state_str[][16]={"Initializing", "Ready", "Requesting", "Responsing", "End" };
	if( stricmp( conn->file_name, "server_info.xiaoxia" ) == 0 ){
		char timestr[64];
		int len = 0;
		webserver* server = conn->server;
		len += sprintf( conn->data_send + len, "<p><a href=\"http://xiaoxia.org/\" target=\"_blank\">Xiaoxia</a>'s Fast Multi-Threading WebServer:</p>\r\n" );
		len += sprintf( conn->data_send + len, "<table border=\"1\"><tr><td>server_name</td><td>%s</td></tr>\r\n", 
			server->server_name );
		len += sprintf( conn->data_send + len, "<tr><td>server_ip</td><td>%s</td></tr>\r\n", 
			server->server_ip );
		len += sprintf( conn->data_send + len, "<tr><td>server_port</td><td>%d</td></tr>\r\n", 
			server->server_port );
		len += sprintf( conn->data_send + len, "<tr><td>root_dir</td><td>%s</td></tr>\r\n", 
			server->root_dir );
		len += sprintf( conn->data_send + len, "<tr><td>max_clients</td><td>%d</td></tr>\r\n", 
			server->max_clients );
		len += sprintf( conn->data_send + len, "<tr><td>max_connections_per_client</td><td>%d</td></tr>\r\n", 
			server->max_onlines );
		len += sprintf( conn->data_send + len, "<tr><td>max_threads</td><td>%d</td></tr>\r\n", 
			server->max_threads );
		len += sprintf( conn->data_send + len, "<tr><td>conn_timeout</td><td>%d</td></tr>\r\n", 
			server->conn_timeout );
		len += sprintf( conn->data_send + len, "<tr><td>session_timeout</td><td>%d</td></tr>\r\n", 
			server->session_timeout );
		format_time( server->start_time, timestr );
		len += sprintf( conn->data_send + len, "<tr><td>start_time</td><td>%s</td></tr>\r\n", 
			timestr );
		int t = time(NULL) - server->start_time;
		len += sprintf( conn->data_send + len, "<tr><td>running_time</td><td>%s</td></tr>\r\n", 
			format_timeinterval( timestr, t) );
		len += sprintf( conn->data_send + len, "<tr><td>handled_requests</td><td>%d</td></tr>\r\n", 
			server->total_requests );
		len += sprintf( conn->data_send + len, "<tr><td>handled_connections</td><td>%d</td></tr>\r\n", 
			server->total_connections );
		len += sprintf( conn->data_send + len, "<tr><td>requests_per_second</td><td>%f</td></tr>\r\n", 
			(float)server->total_requests/t );
		len += sprintf( conn->data_send + len, "<tr><td>requests_per_day</td><td>%f</td></tr>\r\n", 
			(float)server->total_requests/(t/(3600*24)+1) );
		len += sprintf( conn->data_send + len, "<tr><td>client_num</td><td>%d</td></tr>\r\n", 
			server->client_num );
		len += sprintf( conn->data_send + len, "<tr><td>thread_num</td><td>%d</td></tr>\r\n", 
			server->thread_num );
		//lock the link, to avoid being deleted.
		pthread_mutex_lock( &server->mutex_client );
		client* c;
		for( c=server->first_client; c; c=c->next ){
			if( c->conn_num == 0 )
				continue;
			len += sprintf( conn->data_send + len, "<tr><td colspan=\"2\" style=\"padding:20px\"><table border=\"1\"><tr><td>ip_string</td><td>%s</td></tr>\r\n", 
				c->ip_string );
			format_time( c->time_create, timestr );
			len += sprintf( conn->data_send + len, "<tr><td>time_create</td><td>%s</td></tr>\r\n", 
				timestr );
			len += sprintf( conn->data_send + len, "<tr><td>conn_num</td><td>%d</td></tr>\r\n", 
				c->conn_num );
			
			pthread_mutex_lock( &c->mutex_conn );
			connection* cc;
			for( cc=c->first_conn ; cc; cc=cc->next ){
				len += sprintf( conn->data_send + len, "<tr><td colspan=\"2\" style=\"padding:20px\"><table border=\"1\"><tr><td>conn_id</td><td>%d</td></tr>\r\n", 
					cc->conn_id );
				format_time( cc->time_create, timestr );
				len += sprintf( conn->data_send + len, "<tr><td>time_create</td><td>%s</td></tr>\r\n", 
					timestr );
				format_time( cc->time_alive, timestr );
				len += sprintf( conn->data_send + len, "<tr><td>time_alive</td><td>%s</td></tr>\r\n", 
					timestr );
				len += sprintf( conn->data_send + len, "<tr><td>requests</td><td>%d</td></tr>\r\n", 
					cc->requests );
				len += sprintf( conn->data_send + len, "<tr><td>uri</td><td>%s %s %s</td></tr>\r\n", 
					cc->request_method, cc->uri, cc->http_version );
				len += sprintf( conn->data_send + len, "<tr><td>full_path</td><td>%s</td></tr>\r\n", 
					cc->full_path );
				len += sprintf( conn->data_send + len, "<tr><td>state</td><td>%s</td></tr>\r\n", 
					cc->state>=0&&cc->state<5?conn_state_str[cc->state]:"" );
				len += sprintf( conn->data_send + len, "</table></td></tr>\r\n" );
				//To prevent overflow!
				if( len > MAX_DATASEND - KB(16) )
					break;
			}
			len += sprintf( conn->data_send + len, "</table></td></tr>\r\n" );
			pthread_mutex_unlock( &c->mutex_conn );
		}
		pthread_mutex_unlock( &server->mutex_client );
		len += sprintf( conn->data_send + len, "</table>\r\n" );
	}else{
		return -1;
	}
	return 0;
}


#ifdef __WIN32__
#define EXPORT __declspec(dllexport) __stdcall 
#else
#define EXPORT
#endif
int EXPORT plugin_entry( webserver* srv )
{
	srv->vdir_create( srv, "/admin", (vdir_handler)admin_run );
	return 0;
}

int EXPORT plugin_cleanup( webserver* srv )
{
	return 0;
}
