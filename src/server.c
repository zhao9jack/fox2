/*
 *  main.c
 *
 *  Console Program
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-9 10:23:51 Created.
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

#include "debug.h"
#include "memory.h"
#include "config.h"
#include "server.h"
#include "connection.h"
#include "http.h"
#include "vdir.h"
#include "plugin.h"
#include "xmlparser.h"

static void delete_host( const void* v )
{
	loop_cleanup( &((virtual_host *)v)->loop_rewrite );
	DEL( v );
}

static void delete_rules( const void* v )
{
	regfree( &((rewrite_rule *)v)->compiled_pattern );
	DEL( v );
}

void server_reload( webserver* server )
{
	server_stop( server );
	server_end( server );
	server_create( server, server->config_path );
	server_start( server );
}

int server_create( webserver* server, char* confile )
{
	if( !server )
		return -1;
	memset( server, 0, sizeof(webserver) );
	server->config_path = confile;
	//load configuration
	XML* conf;
	conf = xml_load( server->config_path );
	if( !conf )
		return -2;
	server->config = conf;
	xml_redirect( conf, "/", 1 );
	strncpy( server->server_name, xml_readstr(conf, "serverName"), 127 );
	strncpy( server->root_dir, xml_readstr(conf, "root"), DIRNAME_LEN-1 );
	server->max_clients = xml_readnum(conf, "maxClient");
	server->max_onlines = xml_readnum(conf, "maxOnline"); //In old version config file.
	if( server->max_onlines==0 )
		server->max_onlines = xml_readnum(conf, "maxConnectionPerClient");
	server->max_requests_per_conn = xml_readnum(conf, "maxRequestsPerConnection");
	if( server->max_requests_per_conn<=0 )
		server->max_requests_per_conn = 1;
	server->server_busy_threads = xml_readnum(conf, "serverBusyThreads");
	if( server->server_busy_threads <= 0 )
		server->server_busy_threads = 2;
	server->max_threads = xml_readnum(conf, "maxThread");
	NEW( server->thread_list, sizeof(pthread_t)*(server->max_threads+server->server_busy_threads) ); 
	server->conn_timeout = xml_readnum(conf, "connectionTimeout");
	server->session_timeout = xml_readnum(conf, "sessionTimeout");
	// Log
	server->terminal_log = xml_readnum(conf, "terminalLog");
	server->file_log = xml_readnum(conf, "fileLog");
	debug_set_dir( xml_readstr(conf, "fileLog:directory") );
	if( server->terminal_log )
		debug_term_on();
	else
		debug_term_off();
	if( server->file_log )
		debug_file_on();
	else
		debug_file_off();
	//default pages
	char* defaults = xml_readstr(conf, "defaultPages");
	strncpy( server->asp_exts, xml_readstr(conf, "pluginExtensions"), 255 );
	strncpy( server->server_ip, xml_readstr(conf, "serverIP"), 31 );
	server->server_port = xml_readnum(conf, "serverPort");
	//deal with default pages
	int len = strlen( defaults );
	int i, j=0;
	for( i=0; i<len; i++ ) if( defaults[i] == '|' ) defaults[i]=0;
	for( i=0; i<len; i++ ) if( i==0 ) strncpy( server->default_pages[j++], &defaults[i], 31 ); 
		else if( defaults[i] == 0 ){ strncpy( server->default_pages[j++], &defaults[++i], 31 );  } 
	//load error pages
	loop_create( &server->loop_epage, MAX_ERROR_PAGES, NULL );
	if( xml_redirect( conf, "/errorPages/page", 0 ) ){
		do{
			error_page *page;
			NEW( page, sizeof(error_page) );
			loop_push_to_head( &server->loop_epage, (void*)page );
			strncpy( page->path, xml_readstr( conf, "." ), 127 );
			page->code = xml_readnum( conf, ":code" );
		}while( xml_movenext( conf ) );
	}
	//load doc types
	loop_create( &server->loop_doctype, MAX_DOCUMENT_TYPES, NULL );
	if( xml_redirect( conf, "/documentTypes/type", 0 ) ){
		do{
			doc_type *type;
			NEW( type, sizeof(doc_type) );
			loop_push_to_head( &server->loop_doctype, (void*)type );
			strncpy( type->extension, xml_readstr( conf, ":extension" ), 31 );
			strncpy( type->value, xml_readstr( conf, ":value" ), 63 );
		}while( xml_movenext( conf ) );
	}
	//load virtual hosts
	loop_create( &server->loop_vhost, MAX_HOSTS, delete_host );
	if( xml_redirect( conf, "/virtualHost", 0 ) ){
		do{
			virtual_host *host;
			char tmp[70];
			NEW( host, sizeof(virtual_host) );
			loop_push_to_head( &server->loop_vhost, (void*)host );
			strncpy( host->name, xml_readstr( conf, ":name" ), 63 );
			strncpy( host->root_dir, xml_readstr( conf, "root" ), DIRNAME_LEN-1 );
			host->list = xml_readnum( conf, "list" );
			strncpy( host->proxy_ip, xml_readstr( conf, "proxy/serverAddress" ), 31 );
			strncpy( host->proxy_exts, xml_readstr( conf, "proxy/extensions" ), 31 );
			host->proxy_port = xml_readnum( conf, "proxy/serverPort" );
			strncpy( &host->alias[1], xml_readstr( conf, "alias" ), 1022 );
			if( host->alias[1] )
				host->alias[0] = '|';	//for we search for "|host"
			else
				host->alias[0] = '\0';
			strncpy( host->redirect_host, xml_readstr( conf, "redirect" ), 63 );
			if( host->redirect_host[0] )
				host->redirect = 1;
			else
				host->redirect = 0;
			if( host->proxy_ip[0] )
				host->proxy = 1;
			else
				host->proxy = 0;
			//load rewrite information
			loop_create( &host->loop_rewrite, MAX_REWRITE_RULES, delete_rules );
			if( xml_redirect( conf, "rewrite", 0 ) ){
				do{
					char base[64];
					strncpy( base, xml_readstr(conf, ":base"), 63 );
					int base_len = strlen(base);
					//load rules
					if( xml_redirect( conf, "rule", 0 ) ){
						do{
							int ret;
							rewrite_rule * rule;
							NEW( rule, sizeof(rewrite_rule) );  //base
							loop_push_to_head( &host->loop_rewrite, (void*)rule );
							strcpy( rule->base, base );
							rule->base_len = base_len;
							strncpy( rule->result, xml_readstr( conf, "result" ), 127 );
							const char* pattern = xml_readstr( conf, "pattern" );
							if( (ret=regcomp( &rule->compiled_pattern, pattern, REG_EXTENDED ) )!=0 ){
								regerror ( ret, &rule->compiled_pattern, tmp, 64 );
								DBG("rewrite error: %d: %s  pattern:%s host:%s", ret, tmp, pattern, host->name );
							}
						}while( xml_movenext( conf ) );
						//do next rewrite
						sprintf(tmp, "/virtualHost?name=%s/rewrite?base=%s", host->name, base );
						if(!xml_redirect( conf, tmp, 0 ))
							DBG("##Impossible.");
					}
				//	printf("read rewrite rules at base=%s\n", base);
				}while( xml_movenext( conf ) );
				//do next virtual host
				sprintf(tmp, "/virtualHost?name=%s", host->name );
				xml_redirect( conf, tmp, 0 );
			}
		}while( xml_movenext( conf ) );
	}
	//init mutex
	pthread_mutex_init( &server->mutex_client, NULL );
	//init vdir
	vdir_init( server );
	//init plugin
	plugin_init( server );
	server->state = S_STOPPED;
	return 0; 
}

#define INTERNAL 1
static void* server_guard( void* data )
{
	DBG("ok");
	webserver* server = (webserver*)data;
	int counter = 1;
	while( server->state == S_RUNNING ) // || server->first_client )
	{
		//lock the link, to avoid being deleted.
		pthread_mutex_lock( &server->mutex_client );
		client* c;
		for( c=server->first_client; c; ){
			client* next = c->next;	//avoid c being deleted.
			client_live( c );
			c = next;
		}
		pthread_mutex_unlock( &server->mutex_client );
		SLEEP(1);
		counter ++;
	}
	DBG("end.");
	return NULL; //Bugfix by Huang Guan 0905 pthread_join exception
}

static void* server_listen( void* data )
{
	webserver* server = (webserver*)data;
	int counter = 0, sock;
	while( server->state == S_RUNNING )
	{
		sock = accept( server->sock_listen, NULL, NULL);
		if( sock <= 0 || server->state != S_RUNNING )
		{
			break;
		}
		struct sockaddr_in sa;
		int len = sizeof(sa);
		uint ip;
		getpeername(sock, (struct sockaddr *)&sa, &len);
		ip = ntohl(sa.sin_addr.s_addr);
//		DBG("IP：%s:%d", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
		//search client
		pthread_mutex_lock( &server->mutex_client );
		client *c;
		connection* conn = NULL;
		for( c = server->first_client; c; c = c->next )
			if( c->ip == ip ){
				//get it
				break;
			}
		if( c == NULL ) //create a client
			c = client_create( server, ip );
		if( c ){
			//create a connection to deal with it
			pthread_mutex_lock( &c->mutex_conn );
			conn = connection_create( c, sock );
			if( conn ) /* fixed bug: HG 20091016 */
				conn->thread = pthread_self();
			pthread_mutex_unlock( &c->mutex_conn );
		}
		pthread_mutex_unlock( &server->mutex_client );
		if( conn ){
			++ server->thread_num;
			++ server->total_connections;
			connection_start( conn );
			pthread_mutex_lock( &c->mutex_conn );
			LINK_DELETE( conn, c->first_conn );
			pthread_mutex_unlock( &c->mutex_conn );
			DEL( conn );
			-- c->conn_num;
			-- server->thread_num;
		}
		counter ++;
	}
	return NULL;
}

#define END_LISTEN(err) { DBG(err); server->state = S_STOPPED; return -1; }
int server_start( webserver* server )
{
	int i, ret;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 256*1024); //256KB enough??
	server->state = S_RUNNING;
	//try to bind socket
	if ( (server->sock_listen = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP )) <=0 )
		END_LISTEN("Init Socket Error.");
	//端口复用
	i = 1;
	setsockopt(server->sock_listen, SOL_SOCKET, SO_REUSEADDR, (void*)&i, sizeof(i));
	struct sockaddr_in m_sockaddr;
	m_sockaddr.sin_family = AF_INET;
	if( !server->server_ip[0] )
		m_sockaddr.sin_addr.s_addr = INADDR_ANY;
	else
		m_sockaddr.sin_addr.s_addr = inet_addr( server->server_ip );
	m_sockaddr.sin_port = htons( server->server_port );
	if ( bind( server->sock_listen, (struct sockaddr*)&m_sockaddr, sizeof( m_sockaddr ) ) <0 )
		END_LISTEN("Bind Socket Error.");
	//try to listen
	if( listen( server->sock_listen, server->max_threads ) <0 )
		END_LISTEN("Listen Socket Error.");
	server->start_time = time(NULL);
	DBG("listening on port %d", server->server_port );
	server->client_num = 0;
	server->first_client = NULL;
	//create threads
	ret = pthread_create( &server->thread_guard, &attr, (void*)server_guard, (void*)server );
	server->thread_num = 0;
	//multiple listen threads
	for( i=0; i<server->max_threads + server->server_busy_threads; i++ ){
		ret = pthread_create( &server->thread_list[i], &attr, (void*)server_listen, (void*)server );
		if( ret != 0 ){
			DBG("failed to create thread[%d]", i );
			perror("pthread_create");
		}
	}
	pthread_attr_destroy(&attr);
	DBG("server started");
	return 0;
}

int server_stop( webserver* server )
{
	int i, ret;
	server->state = S_STOPPED;
	shutdown( server->sock_listen, SHUT_RDWR );
	//close current clients
	pthread_join( server->thread_guard, (void**)&ret );
#ifdef __WIN32__
	closesocket( server->sock_listen );
#else
	close( server->sock_listen );
#endif
	pthread_mutex_lock( &server->mutex_client );
	client* c;
	for( c=server->first_client; c; c=c->next ){
		client_end( c );
	}
	pthread_mutex_unlock( &server->mutex_client );
	
	DBG("awaiting threads to exit...");
	for( i=0; i<server->max_threads+server->server_busy_threads; i++ )
		pthread_join( server->thread_list[i], (void**)0 );
		
	for( c=server->first_client; c;  ){
		client* next = c->next;
		//close all sessions
		pthread_mutex_lock( &c->mutex_session );
		session* s;
		for( s=c->first_session; s; ){
			session* next = s->next;
			if( s->reference > 0 ){
				DBG("##Fatal Error: session->reference = %d", s->reference );
			}
			LINK_DELETE( s, c->first_session );
			DEL( s );
			s = next;
		}
		pthread_mutex_unlock( &c->mutex_session );
		pthread_mutex_destroy( &c->mutex_session );
		pthread_mutex_destroy( &c->mutex_conn );
		DEL( c );
		c = next;
	}
	DBG("Server is stopped.");
	return 0;
}

int server_clear( webserver* server )
{
	//close current clients
	pthread_mutex_lock( &server->mutex_client );
	client* c;
	for( c=server->first_client; c;  ){
		client* next = c->next;
		client_end( c );	//this client would be deleted from the link.
		c = next;
	}
	pthread_mutex_unlock( &server->mutex_client );
	DBG("Server is cleared.");
	return 0;
}

void server_print( webserver* server )
{
	DBG("ServerName: %s", server->server_name );
	DBG("State: %d", server->state );
	//lock the link, to avoid being deleted.
	pthread_mutex_lock( &server->mutex_client );
	client* c;
	for( c=server->first_client; c; c=c->next ){
		client_print( c );
	}
	pthread_mutex_unlock( &server->mutex_client );
}

void server_end( webserver* server )
{
	if( server->state == S_RUNNING )
		server_stop( server );
	DEL( server->thread_list );
	xml_free( server->config );
	plugin_cleanup(server);
	vdir_cleanup(server);
	loop_cleanup(&server->loop_vhost);
	loop_cleanup(&server->loop_epage);
	loop_cleanup(&server->loop_doctype);
	pthread_mutex_destroy( &server->mutex_client );
}
