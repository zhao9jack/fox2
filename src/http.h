#ifndef _HTTP_H
#define _HTTP_H

#include "connection.h"

int http_request( connection* conn );
int http_response( connection* conn );
int http_redirect( connection*conn, char* path );
int http_sendfile( connection*conn, char* file );
int http_sendheader( connection* conn );
void http_error( connection* conn, int code, const char* desc );
int http_senddata( connection* conn, char* p, int len );
void http_parse_path( connection * conn );
void http_parse_uri( connection* conn );
void format_time(time_t t, char* buf);
char* http_doctype( struct cweb_server* srv, char* ext );

#endif
