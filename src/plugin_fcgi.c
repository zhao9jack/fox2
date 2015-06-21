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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __WIN32__
#include <winsock.h>
#include <wininet.h>
#define SHUT_RDWR SD_BOTH
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "config.h"
#include "debug.h"
#include "server.h"
#include "connection.h"
#include "vdir.h"
#include "session.h"
#include "client.h"
#include "fcgi.h"
#include "xmlparser.h"
#include "loop.h"

#ifndef __WIN32__
#define closesocket close
#endif

#define MAX_FCGI_PROGRAM 128
typedef struct fcgi_program{
	char		shell[128];
	char		extension[64];
	char		server_addr[64];
	int		server_port;
	FILE*		process_fp;
	struct sockaddr_in dest_addr;
}fcgi_program;

static loop loop_program;

static int fcgi_write( int fd, void* buf, size_t size )
{
	return send( fd, (char*)buf, size, 0);
}

int fcgi_readln( int fd, char* buf, int size  )
{
	int i;
	char ch;
	for( i=ch=0; ch!='\n'; )
	{
		if( recv( fd, &ch, 1, 0 )<=0 )
			return -1;
		buf[i++] = ch;
	}
	buf[i] = '\0';
	return i;
}


// build fcgi header
static void build_header( FCGI_Header* hdr, int type, int requestId, int contentLength, int paddingLength )
{
	hdr->version = FCGI_VERSION_1;
	hdr->type = type;
	hdr->requestIdB0 = (unsigned char)requestId;
	hdr->requestIdB1 = (unsigned char)(requestId>>8);
	hdr->contentLengthB0 = (unsigned char)contentLength;
	hdr->contentLengthB1 = (unsigned char)(contentLength>>8);
	hdr->paddingLength = (unsigned char)paddingLength;
	hdr->reserved = 0;
}

static int begin_request( int sock_fcgi, int requestId )
{
	FCGI_BeginRequestRecord rp;
	int ret;
	build_header( &rp.header, FCGI_BEGIN_REQUEST, requestId, sizeof(rp.body), 0 );
	rp.body.roleB0 = FCGI_RESPONDER;
	rp.body.roleB1 = 0;
	rp.body.flags = 0;
	memset( rp.body.reserved, 0, sizeof(rp.body.reserved) );
	ret = fcgi_write( sock_fcgi, (void*)&rp, sizeof(FCGI_BeginRequestRecord) );
	return ret;
}

static int add_param( char* buf, char* name, char* value, int pos, int max )
{
	if(!value)
		return pos;
	int name_len = strlen(name), value_len = strlen(value);
	if( name_len + value_len + 8 + pos < max ){
		if( name_len >= 0x80 ){
			buf[pos++] = (name_len>>24)|0x80;
			buf[pos++] = name_len>>16;
			buf[pos++] = name_len>>8;
			buf[pos++] = name_len;
		}else{
			buf[pos++] = name_len;
		}
		if( value_len >= 0x80 ){
			buf[pos++] = (value_len>>24)|0x80;
			buf[pos++] = value_len>>16;
			buf[pos++] = value_len>>8;
			buf[pos++] = value_len;
		}else{
			buf[pos++] = value_len;
		}
		memcpy( &buf[pos], name, name_len );
		pos += name_len;
		memcpy( &buf[pos], value, value_len );
		pos += value_len;
//		printf("%s=%s\n", name , value );
	}
	return pos;
}

static int build_params( connection* conn, int sock_fcgi )
{
	FCGI_Header header;
	char* buf;
	int pos=0, ret;
	buf = (char*)malloc( KB(64)+8 );
	char tmp[10];
	pos = add_param( buf, "SCRIPT_FILENAME", conn->full_path, pos, KB(64) );
	pos = add_param( buf, "QUERY_STRING", conn->query_params, pos, KB(64) );
	pos = add_param( buf, "REQUEST_METHOD", conn->request_method, pos, KB(64) );
	pos = add_param( buf, "CONTENT_TYPE", conn->header_value(conn, "CONTENT-TYPE"), pos, KB(64) );
	pos = add_param( buf, "CONTENT_LENGTH", conn->header_value(conn, "CONTENT-LENGTH"), pos, KB(64) );
	pos = add_param( buf, "SCRIPT_NAME", conn->script_name, pos, KB(64) );
	pos = add_param( buf, "REQUEST_URI", conn->uri, pos, KB(64) );
	pos = add_param( buf, "DOCUMENT_URI", conn->uri, pos, KB(64) );
	pos = add_param( buf, "DOCUMENT_ROOT", conn->server->root_dir, pos, KB(64) );
	pos = add_param( buf, "SERVER_PROTOCOL", conn->http_version, pos, KB(64) );
	pos = add_param( buf, "GATEWAY_INTERFACE", "CGI/1.1", pos, KB(64) );
	pos = add_param( buf, "SERVER_SOFTWARE", "Xiaoxia's WebServer(gdxxhg@gmail.com)", pos, KB(64) );
	pos = add_param( buf, "REMOTE_ADDR", conn->client->ip_string, pos, KB(64) );
	pos = add_param( buf, "SERVER_ADDR", conn->server->server_ip, pos, KB(64) );
	sprintf( tmp, "%d", conn->server->server_port );
	pos = add_param( buf, "SERVER_PORT", tmp, pos, KB(64) );
	pos = add_param( buf, "SERVER_NAME", conn->server->server_name, pos, KB(64) );
	pos = add_param( buf, "REDIRECT_STATUS", "200", pos, KB(64) );
	pos = add_param( buf, "HTTP_ACCEPT", conn->header_value(conn, "ACCEPT"), pos, KB(64) );
	pos = add_param( buf, "HTTP_ACCEPT_LANGUAGE", conn->header_value(conn, "ACCEPT-LANGUAGE"), pos, KB(64) );
	pos = add_param( buf, "HTTP_ACCEPT_ENCODING", conn->header_value(conn, "ACCEPT-ENCODING"), pos, KB(64) );
	pos = add_param( buf, "HTTP_USER_AGENT", conn->header_value(conn, "USER-AGENT"), pos, KB(64) );
	pos = add_param( buf, "HTTP_HOST", conn->header_value(conn, "HOST"), pos, KB(64) );
	pos = add_param( buf, "HTTP_CONNECTION", conn->header_value(conn, "CONNECTION"), pos, KB(64) );
	pos = add_param( buf, "HTTP_CONTENT_TYPE", conn->header_value(conn, "CONTENT_TYPE"), pos, KB(64) );
	pos = add_param( buf, "HTTP_CONTENT_LENGTH", conn->header_value(conn, "CONTENT_LENGTH"), pos, KB(64) );
	pos = add_param( buf, "HTTP_CACHE_CONTROL", conn->header_value(conn, "CACHE-CONTROL"), pos, KB(64) );
	pos = add_param( buf, "HTTP_COOKIE", conn->cookie_data, pos, KB(64) );
	pos = add_param( buf, "HTTP_REFERER", conn->header_value(conn, "REFERER"), pos, KB(64) );
	pos = add_param( buf, "HTTP_X_FORWARDED_FOR", conn->header_value(conn, "X-Forwarded-For"), pos, KB(64) );

	build_header( &header, FCGI_PARAMS, conn->conn_id, pos, pos%8?8-(pos%8):0 );
	while( pos%8 )
		buf[pos++]=0;
	ret = fcgi_write( sock_fcgi, (void*)&header, sizeof(header) );
	ret = fcgi_write( sock_fcgi, (void*)buf, pos );
	build_header( &header, FCGI_PARAMS, conn->conn_id, 0, 0 );
	ret = fcgi_write( sock_fcgi, (void*)&header, sizeof(header) );
	free( buf );
	return ret;
}

static int build_post( connection* conn, int sock_fcgi )
{
	FCGI_Header header;
	int ret, pos, size;
	char zeros[8] = {0,};
	pos = 0;
	while( conn->form_size>pos ){
		size = conn->form_size - pos;
		if( size > KB(32) )
			size = KB(32);
		build_header( &header, FCGI_STDIN, conn->conn_id, size, size%8?8-(size%8):0 );
		ret = fcgi_write( sock_fcgi, (void*)&header, sizeof(header) );
		ret = fcgi_write( sock_fcgi, (void*)(((char*)conn->form_data)+pos), size );
		if( size%8 )
			ret = fcgi_write( sock_fcgi, (void*)zeros, 8-(size%8) );
		pos += size;
	}
	build_header( &header, FCGI_STDIN, conn->conn_id, 0, 0 );
	ret = fcgi_write( sock_fcgi, (void*)&header, sizeof(header) );
	return ret;
}

static int do_response( connection* conn, int sock_fcgi )
{	
	int ret, pos;
	void* buf;
	FCGI_Header header;
	int body=0;
	for(;;){
		ret = recv( sock_fcgi, (void*)&header, sizeof(FCGI_Header), 0 );
		if( ret <= 0 ){
			break;
		}
		if( ret != sizeof(FCGI_Header) ){
			printf("[fcgi]wrong response header.\n");
			return -1;
		}
		if( header.type == FCGI_STDOUT ){
			unsigned short data_len;
			char line[KB(4)];
			data_len = ((int)header.contentLengthB1)<<8 | header.contentLengthB0;
			if( data_len == 0 )
				continue;
			if( !body ){
				do{
					static char status[] = "Status: "; 
					ret = fcgi_readln( sock_fcgi, line, KB(4) );
					if( ret > 2 ){
						if( strncmp( status, line, 8 ) == 0 ){
							line[8+3] = '\0';
							conn->code = atoi(&line[8]);
						}else{
							conn->add_header( conn, line );
						}
					}
					data_len -= ret;
				}while( ret > 2 );
				body = 1;
			}
			while( data_len > 0 ){
				buf = malloc( data_len );
				if( !buf ) break;
				ret = recv( sock_fcgi, buf, data_len, 0 );
				if( ret <=0 ){
					printf("[fcgi]recv error.ret=%d\n", ret );
					free(buf);
					break;
				}
				data_len -= ret;
				conn->write_buf( conn, buf, ret );
				free(buf);
			}
			//padding
			if( header.paddingLength>0 )
				recv( sock_fcgi, (void*)&header, header.paddingLength, 0 );
		}else if( header.type==FCGI_END_REQUEST ){
			FCGI_EndRequestBody body;
			unsigned int appStatus;
			ret = recv( sock_fcgi, (void*)&body, sizeof(FCGI_EndRequestBody), 0 );
			if( ret!=sizeof(FCGI_EndRequestBody) )
				return -1;
			appStatus = ((unsigned)body.appStatusB3<<24) |
				((unsigned)body.appStatusB2<<16) |
				((unsigned)body.appStatusB1<<8) |
				((unsigned)body.appStatusB0);
//			printf("end with %d  protStatus:%d\n", appStatus, body.protocolStatus);
			return 0;
		}
	}
}

static int program_searcher(const void *v, const void *p)
{
	fcgi_program* fp;
	fp = (fcgi_program*) v;
	if( stricmp( fp->extension, p ) ==0 )
		return 1;
	return 0;
}

static int fcgi_run( connection* conn )
{
	int ret;
	fcgi_program* fcgip;
	fcgip = loop_search( &loop_program, conn->extension, program_searcher );
	if( !fcgip )
		return -1;
	printf("[fcgi] fcgi_run %s\n", conn->uri );
	if( conn->proxy_socket <= 0 ){
		if ( (conn->proxy_socket = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP )) <=0 ){
			perror("[fcgi] Init Socket Error.");
			return -1;
		}
		if( connect( conn->proxy_socket, (struct sockaddr*)&fcgip->dest_addr, sizeof(struct sockaddr_in)) < 0 )
		{
			perror("[fcgi] connect failed.");
			return -2;
		}
	}
	//提交一个请求
	begin_request( conn->proxy_socket, conn->conn_id );
	//发送参数
	build_params( conn, conn->proxy_socket );
	//Post
	build_post( conn, conn->proxy_socket );
	//Read
	do_response( conn, conn->proxy_socket );
	//
	pthread_mutex_lock( &conn->client->mutex_conn );
	closesocket( conn->proxy_socket );
	conn->proxy_socket = 0;
	pthread_mutex_unlock( &conn->client->mutex_conn );
	return 0;
}


static void delete_program( const void* v )
{
	fcgi_program * fp = (fcgi_program*)v;
	if( fp->process_fp ){
		printf("terminating %s\n", fp->shell );
		pclose( fp->process_fp );
	}
	free( fp );
}

#ifdef __WIN32__
#define EXPORT __declspec(dllexport) __stdcall 
#else
#define EXPORT
#endif
int EXPORT plugin_entry( webserver* srv )
{
	//load fcgi program information
	loop_create( &loop_program, MAX_FCGI_PROGRAM, delete_program );
	if( xml_redirect( srv->config, "/fastCGI/program", 0 ) ){
		do{
			fcgi_program * fcgip;
			fcgip = (fcgi_program*) malloc( sizeof(fcgi_program) );
			memset( fcgip, 0, sizeof(fcgi_program) );
			loop_push_to_head( &loop_program, (void*)fcgip );
			strncpy( fcgip->extension, xml_readstr(srv->config, ":extension"), 63 );
			strncpy( fcgip->shell, xml_readstr( srv->config, "shell" ), 127 );
			strncpy( fcgip->server_addr, xml_readstr( srv->config, "serverAddress" ), 63 );
			fcgip->server_port = xml_readnum(srv->config, "serverPort");
			printf("fcgi for %s:%d extension:%s shell:%s\n", fcgip->server_addr, fcgip->server_port, 
				fcgip->extension, fcgip->shell );
			/* fixme: It cannot work!
			if( fcgip->shell[0] ){
				fcgip->process_fp = popen( fcgip->shell, "r" );
				if( !fcgip->process_fp )
					perror("failed to open process.");
			}
			*/
			fcgip->dest_addr.sin_family = PF_INET;
			fcgip->dest_addr.sin_addr.s_addr = inet_addr( fcgip->server_addr );
			fcgip->dest_addr.sin_port = htons( fcgip->server_port );
		}while( xml_movenext( srv->config ) );
	}
	srv->vdir_create( srv, "/", (vdir_handler)fcgi_run );
	return 0;
}

int EXPORT plugin_cleanup( webserver* srv )
{
	loop_cleanup( &loop_program );
	return 0;
}
