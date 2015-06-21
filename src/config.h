//config.h
#ifndef _CONFIG_H
#define _CONFIG_H

#define CONFIG_NAME_LEN 64
#define CONFIG_VALUE_LEN 256

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef struct config_item{
	char name[CONFIG_NAME_LEN];
	char value[CONFIG_VALUE_LEN];
}config_item;

typedef struct cconfig{
	int item_count;
	config_item *items[1024];
}config;

int config_open( config* c, char* filename );
int config_readint( config*c, char* name );
char* config_readstr( config*c, char* name );
void config_close( config* c );

#endif
