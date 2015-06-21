/*
 *  connection.c
 *
 *  connection
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-10 13:31:57 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  Connection
 *  2009-10-26 cookie_set added expire entry.
 *
 */

#ifdef __WIN32__
#include <windows.h>
#include <io.h>
#define SHUT_RDWR SD_BOTH
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "memory.h"
#include "server.h"
#include "client.h"
#include "session.h"
#include "http.h"
#include "connection.h"
#include "vdir.h"
#include "util.h"
#include "proxy.h"
#include "listdir.h"
#include "regex.h"

#ifndef __WIN32__
#define strnicmp strncasecmp
#endif
static unsigned short conn_id = 1;

connection* connection_create( struct cclient* c, int socket )
{
	connection* conn;
	if( c->conn_num >= c->server->max_onlines )
	{
		DBG("failed c->conn_num: %d", c->conn_num );
		return NULL;
	}
	NEW( conn, sizeof(connection) );
	memset( conn, 0, sizeof(connection) );
	LINK_APPEND( conn, c->first_conn );
	c->conn_num ++;
	conn->conn_id = conn_id++;
	conn->server = c->server;
	conn->client = c;
	conn->time_create = conn->time_alive = time( NULL );
	conn->state = C_READY;
	conn->socket = socket;
	conn->header_equal = header_equal;
	conn->header_value = header_value;
	conn->cookie_get = cookie_get;
	conn->cookie_set = cookie_set;
	conn->form_get = form_get;
	conn->param_get = param_get;
	conn->write_buf = write_buf;
	conn->add_header = add_header;
	conn->proxy_socket = 0;
	
	return conn;
}

static int exec_asp( connection* conn )
{
/*
	char key[20];
	key[16] = '\0';
	cookie_get( conn, "SESSIONKEY", key, 16 );
	conn->session = session_get( conn->client, key );
	if( !conn->session ){
		conn->code = 500;
	}else{
		if( !(*key) || strcmp( conn->session->key, key ) != 0 )
			cookie_set( conn, "SESSIONKEY", "/", -1, conn->session->key );
*/
		NEW( conn->data_send, MAX_DATASEND+4 );
		conn->data_send[0] = '\0';
		conn->data_size = 0;
		if( vdir_exec( conn->server, conn->current_dir, conn )==0 )
			return 0;
		if( !conn->data_size )
			conn->data_size = strlen( conn->data_send );
/*
	}
*/
	return 1;
}

static int vhost_searcher(const void *v, const void *p)
{
	virtual_host* host;
	host = (virtual_host*) v;
	if( stricmp( host->name, p ) ==0 )
		return 1;
	if( host->alias[0] ){
		char * q = strstr( host->alias, p );
		if( q && q>host->alias && *(q-1)=='|' )
			return 1;
	}
	return 0;
}

//091105 by HG
static int loop_rewrite_match( const void* p, const void *q )
{
	connection* conn = (connection*)q;
	rewrite_rule* rule = (rewrite_rule*)p;
	const int nmatch = 9; //why is nine? enough?
	char* newname;
	int i;
	char* v;
	regmatch_t pm[nmatch]; 
	if( conn->uri[0] != '/' ){ //bad luck!!
		NEW( newname, PATH_LEN+4 );
		if( strcmp( conn->current_dir, "/") == 0 )
			sprintf( newname, "/%s", conn->uri );
		else
			sprintf( newname, "%s/%s", conn->current_dir, conn->uri );
		strcpy( conn->uri, newname );
		DEL( newname );
	}
	if( strncmp( conn->uri, rule->base, rule->base_len ) )
		return 0; //search next rule
	if( regexec( &rule->compiled_pattern, conn->uri, nmatch, pm, 0 )  ==REG_NOMATCH )
		return 0; //search next rule
	NEW( newname, FILENAME_LEN+4 );
	for( v=rule->result, i=0; *v; v++ ){
		if( *v=='$' && *(v+1)>='0'&& *(v+1)<='9' ){
			int k = *(++v)-'0', j;
			if( pm[k].rm_so == -1 )
				continue;
			for( j=pm[k].rm_so; i<FILENAME_LEN && j<pm[k].rm_eo; j++ )
				newname[i++]=conn->uri[j];
		}else if( i<FILENAME_LEN ){
			newname[i++]=*v;
		}
	}
	newname[i]='\0';
	strcpy( conn->uri, newname );
	DEL( newname );
//	DBG("new uri: %s", conn->uri );
	return 1; //finished.
}

static void work_http( connection* conn )
{
	int i;
	virtual_host * host;
	//check if the connection cannot work.
	if( conn->state != C_READY ){
		DBG("error wrong connection state");
		return;
	}
	conn->state = C_REQUESTING;
	http_request( conn );
	if( conn->state != C_REQUESTING ){
		return;
	}
	++ conn->requests;
	++ conn->server->total_requests;
	if( conn->requests >= conn->server->max_requests_per_conn || strcmp(conn->http_version, "HTTP/1.0")==0 )
		conn->keep_alive = 0;
	//response
	conn->code = 200;
	conn->state = C_RESPONSING;
	/* Check Host and then set root directory. */
	host = loop_search( &conn->server->loop_vhost, conn->host, vhost_searcher );
	if( !host )
		host = loop_search( &conn->server->loop_vhost, "*", vhost_searcher );
	/* Server Busy */
	if( conn->server->thread_num > conn->server->max_threads ){
		http_error( conn, 503, "<h1>Server is too busy! Try it later.</h1>" );
		conn->keep_alive = 0;
	}else if( host ){
		//read root
		conn->root_dir = host->root_dir;
		if( !loop_is_empty( &host->loop_rewrite ) )
			loop_search( &host->loop_rewrite, (void*)conn, loop_rewrite_match );
		http_parse_uri( conn );
		DBG("[%s]%d:%s %s \t%s", conn->client->ip_string, conn->conn_id, conn->request_method, conn->uri, conn->host );
_RESPONSE:	
		if( host->redirect ){
			char* tmp;
			NEW( tmp, PATH_LEN+32 );
			if( conn->uri[0] == '/' )
				sprintf( tmp, "http://%s%s", host->redirect_host, conn->uri );  
			else
				sprintf( tmp, "http://%s/%s", host->redirect_host, conn->uri );  
			http_redirect( conn, tmp );
			DEL( tmp );
		}else{
			http_parse_path( conn );
			if( host->proxy && ( !host->proxy_exts[0] ||
				strstr( host->proxy_exts, conn->extension ) ) )
				{
				// uses proxy server
				if( proxy_request( conn, host->proxy_ip, host->proxy_port ) < 0 )
					http_error( conn, 503, "<h1>Failed to connect proxy server! Try it later.</h1>" );
			}else{
				conn->document_type = http_doctype( conn->server, conn->extension );
				
				if( (conn->server->asp_exts[0]=='*') ){
					vdir_exec( conn->server, conn->current_dir, conn );
				}else if( !conn->document_type ){
					http_error( conn, 404, "<h1>File not found.</h1>" );
				}else if( conn->extension[0] && strstr( conn->server->asp_exts, conn->extension ) ){
					//php  do ...
					exec_asp( conn );
				}else if( access(conn->full_path, 0)==0 ){
					if( is_dir(conn->full_path) ){
						char* tmp;
						NEW( tmp, PATH_LEN+32 );
						if( conn->script_name[strlen(conn->script_name)-1] != '/' ){
							//Are you sure that script starts with '/'?
							sprintf( tmp, "http://%s%s/", conn->host, conn->script_name );  
							http_redirect( conn, tmp );
						}else{
							if( tmp ){
								for( i = 0; i<MAX_DEFAULT_PAGES; i++ )
								{
									if( !conn->server->default_pages[i][0] ) {
										i=MAX_DEFAULT_PAGES;
										break;
									}
									sprintf( tmp, "%s/%s", conn->full_path, conn->server->default_pages[i] );
									if( access( tmp, 0 ) == 0 )
									{
										//091004 by Huang Guan.
										sprintf( conn->script_name, "%s%s", conn->script_name, 
											conn->server->default_pages[i] );
										DEL( tmp );
										goto _RESPONSE;
									}
								}
							}
							if( i == MAX_DEFAULT_PAGES ){
								// List Directory
								if( host->list ){
									int ret;
									NEW( conn->data_send, MAX_DATASEND+4 );
									conn->data_send[0] = '\0';
									strcpy( conn->extension, "html" );
									conn->document_type = http_doctype( conn->server, conn->extension );
									ret = listdir( conn->data_send, MAX_DATASEND, conn->full_path, 
										conn->script_name );
									conn->data_size = ret;
								}else{
									http_error( conn, 403, "<h1>Forbidden</h1>" );
								}
							}
						}
						DEL( tmp );
					}else{
						http_sendfile( conn, conn->full_path );
					}
				}else if( strncmp(conn->current_dir, "/system", 7)==0 && conn->root_dir==host->root_dir ){
					strcpy(conn->script_name, conn->script_name+7);
					conn->root_dir = conn->client->server->root_dir;
					goto _RESPONSE;
				}else{
					http_error( conn, 404, "<h1>File not found.</h1>" );
				}
			}
		}
	}else{
		http_error( conn, 403, "<h1>Unknown host name.</h1>" );
	}
	
	
	if( conn->state == C_RESPONSING )
		http_response( conn );
	if( conn->form_data ){
		DEL( conn->form_data );
	}
	if( conn->data_send ){
		DEL( conn->data_send );
	}
	
	if( conn->session )
		conn->session->reference --;
	conn->session = NULL;
		
	//next request
	if( conn->keep_alive ){
		conn->state = C_READY;
	}else{
		conn->state = C_END;
	}
}

int connection_start( connection *conn )
{
	do{
		conn->time_alive = time( NULL );
		work_http( conn );
	}while( conn->server->state == S_RUNNING && conn->state == C_READY );
	// fix bug 091105 by HG SD_SEND
	pthread_mutex_lock( &conn->client->mutex_conn );
	if( conn->socket ){
		shutdown( conn->socket, SHUT_RDWR );
		closesocket( conn->socket );
	}
	if( conn->proxy_socket ){
		shutdown( conn->proxy_socket, SHUT_RDWR );
		closesocket( conn->proxy_socket );
	}
	conn->socket = conn->proxy_socket = 0;
	pthread_mutex_unlock( &conn->client->mutex_conn );
	DBG("End connection %s %d", conn->uri, conn->conn_id );
	conn->state = C_END;
	return 0;
}


void connection_stop( connection* conn )
{
	conn->state = C_INIT;
	DBG("connection_stop called url %s", conn->uri );
}


int header_equal( connection* conn, char* name, char* value )
{
	char* v = header_value( conn, name );
	if( !value[0]&&v[0] )
		return 0;
	if( strnicmp( value, v, strlen(value) ) == 0 )
		return 1;
	return 0;
}

char* header_value( connection* conn, char* name )
{
	int i;
	for( i=0; i<conn->header_num; i++ )
		if( stricmp( conn->headers[i].name, name ) == 0 )
			return conn->headers[i].value;
	return "";
}

void cookie_get( connection* conn, char* name, char* value, int len )
{
	int j = 0;
	char tmp[64];
	strcpy( tmp, name );
	strcat( tmp, "=" );
	if( !conn->cookie_data )
		return;
	char *p = strstr( conn->cookie_data, tmp );
	if( p == NULL ){
		strcpy( value, "" );
		return ;
	}
	p += strlen( tmp );
	while( *p && *p != ';' ){
		value[j++] = *p;
		if( j>=len )	return;
		p ++;
	}
	value[j] = '\0';
}


void cookie_set( connection* conn, char* name, char* path, int age, char* value )
{
	char* buf;
	NEW( buf, KB(2) );
	if( age==-1 )
		sprintf( buf, "Set-Cookie: %.32s=%.1024s; Path=%.256s\r\n", name, value, path );
	else
		sprintf( buf, "Set-Cookie: %.32s=%.1024s; Max-Age=%d; Path=%.256s\r\n", name, value, age, path );
	add_header( conn, buf );
	DEL( buf );
}


void form_get( connection* conn, char* name, char* value, int len )
{
	int j = 0;
//	int l = conn->form_size;
	char tmp[64+4];
	strcpy( tmp, name );
	strcat( tmp, "=" );
	/* Bug: fixed by hg 090911 */
	if( !conn->form_data ){
		strcpy( value, "" );
		return;
	}
	char *p = strstr( conn->form_data, tmp );
	if( p == NULL ){
		strcpy( value, "" );
		return ;
	}
	p += strlen( tmp );
	while( *p && *p != '&' ){
		value[j++] = *p;
		if( j>=len )	return;
		p ++;
	}
	value[j] = '\0';
}

void param_get( connection* conn, char* name, char* value, int len )
{
	int j = 0;
	char tmp[64+4];
	strcpy( tmp, name );
	strcat( tmp, "=" );
	if( !conn->query_params )
		return;
	char *p = strstr( conn->query_params, tmp );
	if( p == NULL ){
		strcpy( value, "" );
		return ;
	}
	p += strlen( tmp );
	while( *p && *p != '&' ){
		value[j++] = *p;
		if( j>=len )	return;
		p ++;
	}
	value[j] = '\0';
}

int write_buf( connection* conn, void* buf, int len )
{
	uchar* p = (uchar*)conn->data_send + conn->data_size;
	conn->data_size += len;
	if( len > MAX_DATASEND ){
		conn->data_size -= len;
		DBG("Error: out of buffer! data_size: %d conn_uri: %s\n", conn->data_size, conn->uri);
		return 0;
	}
	memcpy( p, buf, len );
	p[len]=0;
	return len;
}

int add_header( connection* conn, char* str )
{
	int len = strlen(str);
	conn->header_send_len += len;
	if( conn->header_send_len >= KB(1) ){
		conn->header_send_len -= len;
		return -1;
	}
	strcat( conn->header_send, str );
	return len;
}
