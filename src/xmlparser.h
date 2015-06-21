// Warning: 此版本不支持多线程环境下操作同一个XML数据

#ifndef _XML_PARSER_
#define _XML_PARSER_

#define XML_NODE_NAME_LEN 32
#define XML_NODE_VALUE_LEN 256
#define XML_ENCODING_LEN 16

struct XML_NODE{
	struct XML_NODE	*node_attr;	//attributes
	struct XML_NODE *node_next;	//next sibling
	struct XML_NODE *node_pre;	//previous sibling
	struct XML_NODE *node_child; //children
	struct XML_NODE *node_parent;	//parent
	char	name[XML_NODE_NAME_LEN];
	char*	value;
	int		value_len;	//length of value, not including the '\0'
};

typedef struct XML_DATA{
	struct XML_NODE	*node_root;		//Document Root
	struct XML_NODE *node_cur;	//Current Node
	unsigned short	version;	//XML Version
	char encoding[XML_ENCODING_LEN];	//XML encoding
}XML;

//加载接口
struct XML_DATA* xml_load( const char* filename );	//load xml data from a file
struct XML_DATA* xml_parse( const char* mem );	//load xml data from memory buffer
void xml_save( struct XML_DATA* xml, const char* filename );	//save xml data to a file
int xml_build( struct XML_DATA* xml, char* mem, int mem_size );	//build xml into memory
void xml_free( struct XML_DATA* xml );	//free xml data
//操作接口
int xml_redirect( struct XML_DATA* xml, const char* path, int create_if_not_exist );	//change the current path
char* xml_readstr( struct XML_DATA* xml, const char* path );	//read a string
int xml_readnum( struct XML_DATA* xml, const char* path );	//read a number
void xml_writenum( struct XML_DATA* xml, const char* path, int num );	//write a number
void xml_writestr( struct XML_DATA* xml, const char* path, const char* str );	//write a string
int xml_movenext( struct XML_DATA* xml );	//读取下一个项目
//
const char* xml_lasterr();

#endif	//_XML_PARSER_

