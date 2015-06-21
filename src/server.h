//Server.h
#ifndef _SERVER_H
#define _SERVER_H

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define KB(a) a*1024
#define MB(a) KB(1024)*a
#define PATH_LEN 1024
#define DIRNAME_LEN 256
#define FILENAME_LEN 256
#define MAX_CLIENT 100
#define MAX_VDIR 100
#define MAX_PLUGIN 64


#ifdef __WIN32__
#define SLEEP(a) _sleep(a*1000)
#else
#define closesocket close
#define stricmp strcasecmp
#define SLEEP(a) sleep(a)
#endif

#include <pthread.h>
#include "config.h"
#include "client.h"
#include "loop.h"
#include "xmlparser.h"
#include "regex.h"

#define MAX_HOSTS 256
#define MAX_REWRITE_RULES 128
#define MAX_ERROR_PAGES 100
#define MAX_DOCUMENT_TYPES 256
#define MAX_DEFAULT_PAGES 10

enum SERVER_STATUS{
	S_INIT,
	S_RUNNING,
	S_STOPPED,
	S_ERROR
};

typedef struct rewrite_rule{
	char		base[64];
	char		base_len;
	char		result[128];
	regex_t		compiled_pattern;
}rewrite_rule;

typedef struct virtual_host{
	char		name[64];
	char		root_dir[DIRNAME_LEN];
	char		list;	//是否允许列表文件
	char		proxy;	//是否使用代理
	char		proxy_exts[32];	//代理文件扩展名
	char		proxy_ip[32]; //代理ip
	unsigned short	proxy_port;	//代理端口
	loop		loop_rewrite;	//rewrite rules
	char		redirect;
	char		redirect_host[64];
	char		alias[1024];
}virtual_host;

typedef struct error_page{
	int		code;
	char		path[128];
}error_page;

typedef struct doc_type{
	char		extension[32];
	char		value[64];
}doc_type;

typedef struct cweb_server{
	char 		state;
	char 		server_name[128];
	int 		server_port;
	char 		root_dir[DIRNAME_LEN];
	int 		max_clients;
	int 		max_onlines;
	char 		default_pages[MAX_DEFAULT_PAGES][32];
	char		terminal_log;
	char 		file_log;
	int 		conn_timeout;
	int 		session_timeout;
	XML* 		config;
	pthread_t* 	thread_list;
	pthread_t 	thread_guard;
	int 		sock_listen;
	//client link table
	int 		client_num;
	client* 	first_client;
	pthread_mutex_t mutex_client;
	int		max_threads;
	//vdir
	loop		loop_vdir;
	loop		loop_plugin;
	int 		(*vdir_create)( struct cweb_server* srv, char* name, vdir_handler handler );
	int 		(*vdir_delete)( struct cweb_server* srv, char* name );
	char		asp_exts[256];
	loop		loop_vhost;
	char*		config_path;
	char		server_ip[32];
	loop		loop_epage;
	loop		loop_doctype;
	int		thread_num;
	int		server_busy_threads;
	int		max_requests_per_conn;
	int		total_requests;
	int		total_connections;
	time_t		start_time;
}webserver;

int server_create( webserver* server, char* config );
int server_start( webserver* server );
int server_stop( webserver* server );
void server_print( webserver* server );
void server_end( webserver* server );
int server_clear( webserver* server );
void server_reload( webserver* server );

#endif


