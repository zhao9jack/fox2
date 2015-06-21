/*
 *  http.c
 *
 *  HTTP
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-10 14:32:50 Created.
 *  2009-1-8  15:32:40 Updated. Range is avalible.
 *
 *  Description: This file mainly includes the functions about 
 *  HTTP
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>//get file time
#ifdef __WIN32__
#include <winsock.h>
#include <wininet.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif


#include "debug.h"
#include "memory.h"
#include "server.h"
#include "utf8.h"
#include "util.h"
#include "http.h"

#define MIN(a,b) (a<b?a:b)

enum HTTP_STEP{
	HTTP_URI,
	HTTP_HEADER,
	HTTP_BODY,
	HTTP_DONE
};

void http_parse_uri( connection* conn )
{
	char* p = strchr( conn->uri, '?' );
	if( p ){
		strncpy( conn->script_name, conn->uri, MIN( FILENAME_LEN, (int)(p-conn->uri) ) );
		conn->script_name[(int)(p-conn->uri)] = 0;
		strcpy( conn->query_params, p+1 );
	}else{
		strncpy( conn->script_name, conn->uri, FILENAME_LEN );
		strcpy( conn->query_params, "" );
	}
	p = conn->script_name;
	if( p[0]=='h' && p[1]=='t' && p[2]=='t' && p[3]=='p' && p[4]==':' && p[5]=='/' && p[6]=='/' ){
		p = strchr( &p[7], '/' );
		if( p ){
			*p = '\0';
			strcpy( conn->proxy_host, &conn->script_name[7] );
			*p = '/';
			strcpy( conn->script_name, p );
		}else{
			strcpy( conn->script_name, "/" );
		}
	}
//	utf8 = if_UTF8( conn->script_name );
	decode_uri( conn->script_name );
//	if( !utf8 ){
//		DBG("WARNING: not utf8, translated to.");
//		gb_to_utf8( conn->script_name, conn->script_name, PATH_LEN );
//	}
#ifdef __WIN32__
	// fixed bug: 091105 by HG
	utf8_to_gb( conn->script_name, conn->script_name, PATH_LEN );
#endif
	decode_uri( conn->query_params );
//	DBG("%s %s params: %s", conn->request_method, conn->script_name, 
//		conn->query_params, conn->http_version );
//	DBG("conn->script_name: %s  conn->current_dir: %s", conn->script_name, conn->current_dir );
}

static void parse_http_head( connection* conn, char* line, int len )
{
	int i, j;
//	int utf8=0;
	for( i=0, j=0; i<len && line[i]!=' ' && j<16; i++ )
		conn->request_method[j++] = line[i];
	conn->request_method[j] = 0;
	for( i=i+1, j=0; i<len && line[i]!=' ' && j<PATH_LEN; i++ )
		conn->uri[j++] = line[i];
	conn->uri[j]=0;
	for( i=i+1, j=0; i<len && line[i]!=' ' && j<16; i++ )
		conn->http_version[j++] = line[i];
	conn->http_version[j]=0;
}

static void parse_header( connection* conn, char* line, int len )
{
	char* p;
	if( conn->header_num >= MAX_HEADER ){
		DBG("conn->header_num: %d", conn->header_num );
		return;
	}
	p = strstr( line, ": " );
	if( !p )
		return;
	int l = MIN(32, (int)(p-line) );
	strncpy( conn->headers[conn->header_num].name, line, l );
	conn->headers[conn->header_num].name[l] = 0;
	l = MIN(1024, len-(int)(p-line)-1 );
	strncpy( conn->headers[conn->header_num].value, p+2, l );
	conn->headers[conn->header_num].value[l] = 0;
//	DBG("header %s: %s", conn->headers[conn->header_num].name, 
//		conn->headers[conn->header_num].value );
	conn->header_num ++;
}

static void http_read( connection* conn )
{
	int step;
	char *buf = conn->http_read_buf, *form_raw = NULL;	//8 KB temporary buffer
	char *line;
	NEW( line, KB(4)+4 );
	int ret, pos, len, size, body_pos = 0, body_len = 0;
	step = HTTP_URI;
	pos = size = 0;
	while( (ret = recv( conn->socket, buf+size, MAX_HTTP_READ_BUFFER-size, 0 ) ) >= 0 )
	{
		if( !ret ){
			conn->state = C_END;
			break;
		}
		size += ret;
PARSE:
		switch( step )
		{
		case HTTP_URI:
			len = get_line( buf, size, &pos, line, KB(4) );
			if( len>0 ){
				parse_http_head( conn, line, len );
				step = HTTP_HEADER;
				goto PARSE;
			}
			break;
		case HTTP_HEADER:
			len = get_line( buf, size, &pos, line, KB(4) );
			if( len>0 ){
				parse_header( conn, line, len );
				goto PARSE;
			}else if( len == 0 ){
				if ( header_equal( conn, "Content-Length", "" ) )	//no body sent
				{
					step = HTTP_DONE;
				}else{
					step = HTTP_BODY;
					body_len = atoi( header_value( conn, "Content-Length" ) );
					//check the body_len size.
					if( body_len > MAX_POST_SIZE ){
						DBG("Warning: body_len: %d", body_len );
						step = HTTP_DONE;
					}else{
						conn->form_size = body_len;
						NEW( form_raw, conn->form_size );
					}
				}
				goto PARSE;
			}
			break;
		case HTTP_BODY:
			len = size - pos;
			/* Bugfix: 2010-05-29  by HG. Overflow */
			if( len + body_pos > body_len ){ 
				DBG("Warning: Too much content data, expected: %d but it's %d", body_len, len+body_pos);
				len = body_len - body_pos;
			}
			memcpy( form_raw+body_pos, buf+pos, len );
			body_pos += len;
			pos = size = 0;
			if( body_pos >= body_len )
			{
				step = HTTP_DONE;
				goto PARSE;
			}
			break;
		case HTTP_DONE:
			if( form_raw )
				conn->form_data = form_raw;
			DEL( line );
			conn->http_read_pos = pos;
			conn->http_read_size = size;
			return;
		}
	}
	if( form_raw )
		DEL( form_raw );
	DEL( line );
	conn->state = C_END;
}

void http_parse_path( connection * conn )
{	
	//** build current dir
	int len;
	char tmp[PATH_LEN];
	if( conn->script_name[0] != '/' ) {	//相对目录
		if( strcmp( conn->current_dir, "/") == 0 )
		{
			sprintf( tmp, "/%s", conn->script_name );
			strncpy( conn->script_name, tmp, FILENAME_LEN );
		}else{
			sprintf( tmp, "%s/%s", conn->current_dir, conn->script_name );
			strncpy( conn->script_name, tmp, FILENAME_LEN );
		}
	}
	char* pos = strrchr( conn->script_name, '/' );
	if( pos == NULL )
	{
		strcpy( conn->current_dir, "/" );
//		strcpy( conn->file_name, conn->script_name+1 );	//这里为什么要加1？是不是有问题？
		strcpy( conn->file_name, conn->script_name );	//2009-1-8 0:00:27 Huang Guan
	}else if ( pos > conn->script_name )
	{
		*pos = 0;
		strcpy( conn->current_dir, conn->script_name );
		*pos = '/';
		strcpy( conn->file_name, pos+1 );
	}else{
		strcpy( conn->current_dir, "/" );
		strcpy( conn->file_name, conn->script_name+1 ); //这里要+1 091006 By Huang Guan
	}
	//** get full path
	strcpy( tmp, conn->script_name );
	//erase ..
	len = parse_path( tmp );
	//** get extension  bugfixed by Huang Guan 091205 don't get the dot if we find the slash /
	for( pos = tmp + len - 1; pos >= tmp && *pos != '.' && *pos != '/'; pos-- );
	if( *pos == '.' ){
		/* 感谢气泡熊44670提出这个bug */
		strncpy( conn->extension, pos+1, 32 );
	}
	strcpy( conn->full_path, conn->root_dir );
	if( tmp[0] ){
		strcat( conn->full_path, "/" );
		strcat( conn->full_path, tmp );
	}
//	DBG("full_path: %s  extension: %s  query_params: %s", conn->full_path, conn->extension
//		, conn->query_params );
}

int http_request( connection* conn )
{
	conn->header_num = 0;
	conn->responsed = 0;
	conn->data_size = 0;
	conn->data_send = 0;
	conn->form_data = 0;
	conn->form_size = 0;
	conn->host = 0;
	conn->header_send_len = 0;
	conn->extension[0] = 0;
	conn->document_type = NULL;
	conn->proxy_host[0] = 0;
	conn->keep_alive = 0;
	strcpy( conn->header_send, "\r\n" );
	http_read( conn );
	if( header_equal( conn, "Connection", "keep-alive" ) || header_equal( conn, "Proxy-Connection", "keep-alive" ) )
		conn->keep_alive = 1;
	conn->cookie_data = header_value( conn, "Cookie" );
	conn->host = header_value( conn, "Host" );
	return 0;
}

int http_redirect( connection*conn, char* path )
{
	conn->code = 302;
	char buf[KB(1)];
	sprintf( buf, "Location: %s\r\n", path );
	add_header( conn, buf );
	return 0;
}

int http_sendfile( connection*conn, char* file )
{
	char not_modified = 0;
	char timestr[64];
	int file_size;
	// get file modified time
	FILE *fp = fopen( file, "rb");
	if(fp!=NULL)	//file is existed!
	{
		struct stat statbuf;
		//   get   information   about   the   file
		fstat(fileno(fp), &statbuf);
		file_size = statbuf.st_size;
		format_time( statbuf.st_mtime, timestr );
	
		// get If-Modified-Since time
		char* str;
		str = header_value( conn, "If-Modified-Since");
		char* p = strstr( str, " GMT" );
		if( p )
		{
			*p = 0;
			if( strcmp( str, timestr ) == 0 )	//比较时间。
			{
				p = strstr( p+1, "length=");
				if( p )
				{
					p += 7;
					if( atoi(p) == file_size )	//比较大小
					{
						not_modified = 1;
					}
				}else{
					not_modified = 1;
				}
			}
		}
		if( not_modified )
		{
			conn->code = 304;
		}else{
			sprintf( conn->header_send, "%sLast-Modified: %s GMT\r\n", 
				conn->header_send, timestr );
			conn->file_size = file_size;
			//Warning: header_send_len should be inc!!
			//支持续传，设置起点。  2009-1-8 15:32 
			sprintf( conn->header_send, "%sAccept-Ranges: bytes\r\n", conn->header_send );
			//判断是否需要续传 Range: bytes=5000-
			str = header_value( conn, "Range");
			conn->range_begin = 0;
			conn->range_end = file_size - 1;
			if( str ){
				char tmp[32];
				strncpy( tmp, str, 31 );
				if( (p=strstr( tmp, "bytes=" )) ){
					p+=6;
					char* t = strchr( tmp, '-' );
					*t = '\0';
					if( (t++) == p ){ //读取尾部 
						conn->range_begin = file_size - atol( t );
					}else{
						conn->range_begin = atol( p );
						if( *t != '\0' && *t != ',' )	//has end pos
							conn->range_end = atol( t );
					}
					sprintf( conn->header_send, "%sContent-Range: bytes %d-%d/%d\r\n", 
						conn->header_send, conn->range_begin, conn->range_end,
						conn->file_size );
					if( conn->range_end - conn->range_begin + 1 != conn->file_size )
						conn->code = 206;
				}
			}
			//准备传输 
			conn->data_size = conn->range_end - conn->range_begin + 1;
			//Send HTTP Header
			http_sendheader( conn );
			NEW( conn->data_send, KB(64) );
			int data_sent = 0, bytes_once;
			fseek( fp, conn->range_begin, SEEK_SET );
			while( data_sent < conn->data_size ){
				conn->time_alive = time( NULL );
				bytes_once = conn->data_size - data_sent;
				if( bytes_once > KB(64) )
					bytes_once = KB(64);
				if( fread( conn->data_send, bytes_once, 1, fp ) < 1 ){
					DBG("read file error: %s", file );
					break;
				}
				send( conn->socket, conn->data_send, bytes_once, 0 );
				data_sent += bytes_once;
			}
			DEL( conn->data_send );
			conn->responsed = 1;
		}
		fclose(fp);
	}else{
		conn->code = 404;
//		DBG("%x File %s is not found.", conn, file );
		return -1;
	}
	return 0;
}

static int epage_searcher(const void *v, const void *p)
{
	error_page* page;
	page = (error_page*) v;
	if( page->code ==(int)p )
		return 1;
	return 0;
}

void http_error( connection* conn, int code, const char* desc )
{
	error_page* page;
	conn->code = code;
	page = loop_search( &conn->server->loop_epage, (void*)code, epage_searcher );
	if( page && access(page->path, 0)==0 ){
		char* p;
		p = strchr( page->path, '.' );
		if( p )
			strncpy( conn->extension, p+1, 32 );
		strcpy( conn->extension, "html" );
		conn->document_type = http_doctype( conn->server, conn->extension );
		/* For user defined error page, we use normal status code, right? */
		conn->code = 200; 
		http_sendfile( conn, page->path );
	}else{
		if( !conn->data_send )
			NEW( conn->data_send, MAX_DATASEND );
		conn->write_buf( conn, (char*)desc, strlen(desc) );
		strcpy( conn->extension, "html" );
		conn->document_type = http_doctype( conn->server, conn->extension );
	}
}

static int doctype_searcher(const void *v, const void *p)
{
	doc_type* type;
	type = (doc_type*) v;
	if( stricmp(type->extension, (char*)p)==0 )
		return 1;
	return 0;
}

char* http_doctype( webserver* srv, char* ext )
{
	doc_type* type;
	type = loop_search( &srv->loop_doctype, (void*)ext, doctype_searcher );
	if( type )
		return type->value;
	return NULL;
}

int http_sendheader( connection* conn )
{
	char *header;
	char tmp[64];
	int len;
	NEW( header, KB(4) );
	sprintf( header, "%s %s\r\n", conn->http_version, http_code_string( conn->code ) );
	strcat( header, "Server: Xiaoxia's WebServer (http://home.xxsyzx.com)\r\n");
	format_time( 0, tmp );
	sprintf( header, "%sDate: %s GMT\r\n", header, tmp );
	if( !conn->document_type ){
		if( conn->extension[0] )
			conn->document_type = http_doctype( conn->server, conn->extension );
	}
	if( conn->data_size > 0 ){
		if( conn->document_type && strstr( conn->header_send, "Content-type" )==NULL && 
			strstr( conn->header_send, "Content-Type" )==NULL )
			sprintf( header, "%sContent-Type: %s\r\n", header, conn->document_type );
		sprintf( header, "%sContent-Length: %d\r\n", header, conn->data_size );
	}else{
		strcat( header, "Content-Length: 0\r\n" );
	}
	if( conn->keep_alive )
		strcat( header, "Connection: keep-alive" );
	else
		strcat( header, "Connection: close" );
//	strcat( header, "Cache-control: private" );	//haha, why no \r\n?? Think about it!
	strcat( header, conn->header_send );
	strcat( header, "\r\n" );
	len = strlen( header );
//	puts( header );
	send( conn->socket, header, len, 0 );
	DEL( header );
	return 0;
}

int http_response( connection* conn )
{
	if( conn->responsed ){
		return -1;
	}
	//send response http
	conn->responsed = 1;
	http_sendheader( conn );
	if( conn->data_size > 0 )
		send( conn->socket, conn->data_send, conn->data_size, 0 );
	return 0;
}

