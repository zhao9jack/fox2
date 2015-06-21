#ifdef __WIN32__
#include <windows.h>
#include <winsock.h>
#include <wininet.h>
#else
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "server.h"
#include "debug.h"
#include "memory.h"
#include "util.h"

//091009 from myqq
int mkdir_recursive( char* path )
{
	char *p;
	if( access( path, 0 ) == 0 )
		return 0;
	for( p=path; *p; p++ ){
		if( p>path && *p == '/' ){
			*p = 0;
			if( access( path, 0 ) != 0 ){
#ifdef __WIN32__
				mkdir( path );
#else
				if( mkdir( path, S_IRWXU ) != 0 )
					return -1;
#endif
			}
			*p = '/';
		}
	}
#ifdef __WIN32__
	return mkdir( path );
#else
	return mkdir( path, S_IRWXU );
#endif
}

int is_dir( const char* path )
{
	char isdir = 0;
#ifdef __WIN32__
	// check if the file/directory exists
	unsigned int dwAttributes = GetFileAttributes( path );
	isdir = ((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat stat_buf;
	if( lstat( path, &stat_buf ) >=0 ){
		isdir = S_ISDIR(stat_buf.st_mode);
	}
#endif
	return isdir;
}

int parse_path( char* path )
{
	int i, j, len;
	len = strlen( path );
	//和谐/
	for( i=0; i<len; i++ ){
		if( path[i]=='\\' )
			path[i]='/';
	}
	for( i=j=0; i<len; i++ ){
		if( (i==0 || (i>0 && path[i-1]=='/') ) && 
			(i+2==len || path[i+2]=='/' ) && 
			path[i]=='.' && i<len-1 && path[i+1]=='.' 
			)
		{
			i+=2;
			if( j>0 ){
				if( path[j-1] == '/' ) j--;
				while( j>0 && path[j-1]!='/' ) j--;
			}
		}else{
			if( path[i] == '/' && ( j==0 || path[j-1]=='/') ) continue;
			path[j++] = path[i];
		}
	}
	/* bugfix j>0 by HG 20091030 */
#ifdef __WIN32__
	while( j>0 && ( path[j-1]=='/' || path[j-1]=='.' || path[j-1]==' ' )  )
#else
	while( j>0 && ( path[j-1]=='/' || path[j-1]=='.' || path[j-1]==' ' )  )
#endif
		j--;
	path[j] = 0;
	return j;
}

int get_line( char* buf, int size, int* pos, char* line, int max )
{
	int i = *pos, j=0;
	char ok = 0;
	if( i>size ) ok=1;
	while( !ok )
	{
		char c = buf[i];
		switch( c )
		{
		case '\r':	//ignore
			break;
		case '\n':
			ok = 1;
			break;
		default:
			line[j++] = c;
			if( j>max-1 ) ok = 1;
			break;
		}
		i++;
		if( i>size ) break;
	}
	if( ok )
	{
		*pos = i;
		line[j] = 0;
		return j;
	}
	return -1;
	
}


void format_time(time_t t, char* buf)
{
	if( !t ) t= time(NULL);
	if( gmtime(&t) )
		strftime( buf, 64, "%a, %d %b %Y %X", gmtime(&t) );
	else 
		strcpy( buf, "[Incorrect time]");
}


char* http_code_string(int no)
{
	int i;
	static struct STATUS_CODE{
		int no;
		char* desc;
	}status_string[] = {
        { 0, "0 NULL" },
        { 200, "200 OK" },
        { 201, "201 Created" },
        { 202, "202 Accepted" },
        { 204, "204 No Content" },
        { 206, "206 Partial Content" },
        { 301, "301 Moved Permanently" },
        { 302, "302 Moved Temporarily" },
        { 304, "304 Not Modified" },
        { 400, "400 Bad Request" },
        { 401, "401 Unauthorized" },
        { 403, "403 Forbidden" },
        { 404, "404 Not Found" },
        { 500, "500 Internal Server Error" },
        { 501, "501 Not Implemented" },
        { 502, "502 Bad Gateway" },
        { 503, "503 Service Unavailable" }
    };
    #define COUNT (sizeof(status_string)/sizeof(struct STATUS_CODE))
    for( i=0; i<COUNT; i++ )
    {
        if( no == status_string[i].no )
            return status_string[i].desc;
    }
    // print an error
    printf("Error in http_code_string(%d).", no);
    return status_string[0].desc;
}


// 解析服务器列表
void read_network_addr( networkaddress* srv, char* s, int* count, int max  )
{
	char ip[32], port[10], read_name = 1, *p;
	int j = 0;
	*count = 0;
	for( p=s; ; p++ ){
		if( *p == ':' ){
			ip[j]=0;
			j=0; read_name = 0;
		}else if( *p=='|' || *p=='\0' ){
			port[j]=0;
			j=0; read_name = 1;
			if( *count < max ){
				strncpy( srv[*count].ip, ip, 31 );
				srv[*count].port = atoi( port );
				(*count) ++;
			}
			if( *p=='\0' )
				break;
		}else if(*p==' '){
			continue;
		}else{
			if( read_name ){
				if( j<31 )	ip[j++] = *p;
			}else{
				if( j<9 ) port[j++] = *p;
			}
		}
	}
}

int netaddr_set( const char* name, struct sockaddr_in* addr )
{
	if( (addr->sin_addr.s_addr = inet_addr( name ) ) == -1 )
	{
		//it's not an IP.
		//not an ip, maybe a domain
		struct hostent *host;
		host = gethostbyname( name );
		if( host )
			addr->sin_addr.s_addr = *(size_t*) host->h_addr_list[0];
		else
			return -1;
	}
	return 0;
}

int send_and_wait( int sock, char* p, int len )
{
	// I guess if you do the following things, you need not shutdown. Right?  Maybe wrong!  2010-05-30
	// 091105 by HG
	
	int ret, rest, bc;
	rest = len;
	while( rest > 0 )
	{
		bc = KB(64);
		if( bc > rest )
			bc = rest;
		ret = send( sock, p, bc, 0 );
		if( ret < 0 )
			return ret;
		rest -= ret;
		p += ret;
	}
	return len-rest;
}

int set_socket_nonblocking(int sock)
{
#ifdef __WIN32__
	unsigned long ul = 1;
	ioctlsocket(sock,FIONBIO,&ul); 
#else
	int opts;
	opts = fcntl(sock, F_GETFL);
	if(opts < 0){
		perror("[FoxProject]fcntl(sock,GETFL)");
		return(-1);
	}
	opts = opts | O_NONBLOCK;
	if(fcntl(sock, F_SETFL, opts) < 0){
		perror("[FoxProject]fcntl(sock,SETFL,opts)");
		return(-1);
	}
#endif
	return 0;
}
