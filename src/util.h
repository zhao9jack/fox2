#ifndef _UTIL_H
#define _UTIL_H

typedef struct networkaddress{
	char		ip[32];
	unsigned short	port;
}networkaddress;

int parse_path( char* path );
int get_line( char* buf, int size, int* pos, char* line, int max );
char* http_ext_desc( char* ext );
char* http_code_string(int no);
void format_time(time_t t, char* buf);
int is_dir( const char* path );
int mkdir_recursive( char* path );
void read_network_addr( networkaddress* srv, char* s, int* count, int max  );
struct sockaddr_in;
int netaddr_set( const char* name, struct sockaddr_in* addr );
int send_and_wait( int sock, char* p, int len );
int set_socket_nonblocking(int sock);

#endif
