#ifndef _CONNECTION_H
#define _CONNECTION_H

#include <time.h>
#include <pthread.h>
#include "config.h"
#include "vdir.h"

enum CONN_STATE{
	C_INIT,
	C_READY,
	C_REQUESTING,
	C_RESPONSING,
	C_END,
};
struct cweb_server;
struct cclient;
struct csession;

typedef struct cheader{
	char name[32+4];
	char value[1024+4];
}header;

#define MAX_HEADER 24
#define MAX_DATASEND MB(1)
#define MAX_POST_SIZE MB(20)
#define MAX_HTTP_READ_BUFFER KB(10)

typedef struct cconnection{
	struct cconnection*	next;
	struct cconnection*	pre;
	struct cweb_server *	server;
	struct cclient *	client;
	struct csession*	session;
	unsigned short		conn_id; //2009-10-3 by Huang Guan.
	pthread_t		thread;	//2009-1-8 by Huang Guan.
	time_t 			time_create;
	time_t 			time_alive;
	int			state;
	int			requests;
	char			current_dir[DIRNAME_LEN+4];
	int			socket;
	char			request_method[16+4];
	char			uri[PATH_LEN+4];
	char			http_version[16+4];
	char			keep_alive;
	int			header_num;
	header			headers[MAX_HEADER];
	char*			form_data;
	int			form_size;
	char*			cookie_data;	//in header
	char*			host;	//in header
	char			script_name[FILENAME_LEN+4];
	char			query_params[PATH_LEN+4];
	char			file_name[FILENAME_LEN+4];
	char			full_path[PATH_LEN+4];
	char*			root_dir;
	char			extension[32+4];
	int			code;	//reply code
	char			header_send[KB(4)+4];
	int			header_send_len;
	char*			data_send;
	int			data_size;
	char			responsed;
	/// 续传
	unsigned int 		range_begin;
	unsigned int		range_end;
	unsigned int		file_size;
	int 			(*header_equal)( struct cconnection* conn, char* name, char* value );
	char* 			(*header_value)( struct cconnection* conn, char* name );
	void 			(*cookie_get)( struct cconnection* conn, char* name, char* value, int len );
	void 			(*cookie_set)( struct cconnection* conn, char* name, char* path, int age, char* value );
	void 			(*form_get)( struct cconnection* conn, char* name, char* value, int len );
	void 			(*param_get)( struct cconnection* conn, char* name, char* value, int len );
	int 			(*write_buf)( struct cconnection* conn, void* buf, int len );
	int 			(*add_header)( struct cconnection* conn, char* str );
	char*			document_type;
	int			proxy_socket;	//For proxy use. Close this socket if it has been used when closing the connection
	char			proxy_host[FILENAME_LEN+4];
	int			http_read_pos;
	int			http_read_size;
	char			http_read_buf[MAX_HTTP_READ_BUFFER+4];
}connection;

int header_equal( connection* conn, char* name, char* value );
char* header_value( connection* conn, char* name );
void cookie_get( connection* conn, char* name, char* value, int len );
void cookie_set( connection* conn, char* name, char* path, int age, char* value );
void form_get( connection* conn, char* name, char* value, int len );
void param_get( connection* conn, char* name, char* value, int len );
connection* connection_create( struct cclient* c, int socket );
int connection_start( connection *c );
void connection_stop( connection* c );
int write_buf( connection* conn, void* buf, int len );
int add_header( connection* conn, char* str );

//from another file
void asp_run( connection* c );

#endif
