#ifndef _PROXY_H_


struct cconnection;
int proxy_request( struct cconnection* conn, const char* ip, int port );

#endif
