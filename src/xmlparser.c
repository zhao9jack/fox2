/*
 *		XML Parser
 *	  Huang Guan (gdxxhg@gmail.com)
 *	2009年7月26日 星期日 Created.
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmlparser.h"

static char _err_str[32];
#define MIN(a,b) (a>b?b:a)

void seterr( const char* str )
{
	strncpy( _err_str, str, 32-1 );
	puts(str);
}

const char* xml_lasterr()
{
	return _err_str;
}

//load xml data from a file
struct XML_DATA* xml_load( const char* filename )
{
	unsigned int filesize;
	char* buf;
	struct XML_DATA* xml;
	FILE* fp;
	fp = fopen( filename, "rb" );
	if( fp == NULL ){
		seterr( "file could not open." );
		return NULL;
	}
	//get file size
	fseek( fp, 0L, SEEK_END );
	filesize = ftell( fp );
	fseek( fp, 0L, SEEK_SET );	//recover the cursor
	buf = (char*)malloc( filesize+1 );
	if( !buf ){
		seterr( "no memory to load file data.");
		fclose( fp );
		return NULL;
	}
	if( fread( buf, filesize, 1, fp ) < 1 ){
		seterr( "read file error.");
		free( buf );
		fclose( fp );
		return NULL;
	}
	buf[filesize]='\0';
	xml = xml_parse( buf );
	free( buf );
	fclose( fp );
	return xml;
}

static struct XML_NODE* create_node( struct XML_DATA* xml )
{
	struct XML_NODE* nod;
	nod = (struct XML_NODE*) malloc( sizeof(struct XML_NODE) );
	if( !nod ){
		seterr( "no memory to allocate node.");
		return NULL;
	}
	memset( (void*)nod, 0, sizeof(struct XML_NODE) );
	if( xml && xml->node_root == NULL )
		xml->node_root = nod;
	return nod;
}

static struct XML_NODE* create_childnode( struct XML_DATA* xml, struct XML_NODE* cur )
{
	struct XML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		nod->node_next = cur->node_child;
		//建立链表关系
		if( cur->node_child ){	//如果已经有儿子，插入到儿子前面
			cur->node_child->node_pre = nod;
		}
		nod->node_parent = cur;
		cur->node_child = nod;
	}
	return nod;
}

static struct XML_NODE* create_attrnode( struct XML_DATA* xml, struct XML_NODE* cur )
{
	struct XML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		//建立链表关系
		nod->node_next = cur->node_attr;
		if( cur->node_attr )	//如果已经有属性，插入到属性前面
			cur->node_attr->node_pre = nod;
		nod->node_parent = cur;
		cur->node_attr = nod;
	}
	return nod;
}

static struct XML_NODE* create_nextnode( struct XML_DATA* xml, struct XML_NODE* cur )
{
	struct XML_NODE* nod;
	nod = create_node( xml );
	if( !nod )
		return NULL;
	if( cur ) {
		//建立链表关系
		nod->node_pre = cur;
		if( cur->node_next )	//已经有下一个，则插入
			cur->node_next->node_pre = nod;
		cur->node_next = nod;
		nod->node_parent = cur->node_parent;
	}
	return nod;
}

// XML语法分析，长过程
static void parse_node( struct XML_DATA* xml, const char* mem )
{
	enum PARSE_STATE{
		OutsideTag,
		TagName,
		TagEnd,
		Text,
		AttributeName,
		AttributeValue,
		Header,
		Comment
	};
	enum PARSE_STATE state;
	const char* p, *v;
	struct XML_NODE* cur;
	cur = NULL;
	state = OutsideTag;
	for( p=mem; *p; p++ ){
		switch( state ){
		case OutsideTag:	//
			switch(*p){
			case '<':	//'<' a new node starts
				if( *(p+1)=='/' ){	//end tag
					cur = cur->node_parent;
					state = TagEnd;
				}else if( *(p+1)=='?' ){	//xml header
					state = Header;
				}else if( *(p+1)=='!' ){	//xml comment
					state = Comment;
				}else{
					state = TagName;
					cur= create_nextnode( xml, cur );
					if( !cur ) return;
				}
				v = p+1;
				break;
			case ' ':	case '\r':	case '\n':	case '\t':
				break;
			default:
				printf("## incorrect character: %c\n", *p );
				break;
			}
			break;
		case Header:
			if(*p=='>')
				state = OutsideTag;
			break;
		case Comment:
			if(*p=='>')
				state = OutsideTag;
			break;
		case TagName:	//<tag
			switch(*p){
			case '>':
			case ' ':	case '\r':	case '\n':	case '\t':	case '/':
				if( (int)(p-v)>0 ){	//Tag Name
					memcpy( cur->name, v, MIN(XML_NODE_NAME_LEN-1, (int)(p-v)) );
				}
				v = p+1;
				if( *p == '>' ){
					state = Text;
				}else{
					state = AttributeName;
				}
				break;
			}
			break;
		case Text:	//<tag>Text</tag>
			if( *p == '<' ){
				if( *(p+1) == '/' ){//end tag??
					int len = (p-v);
					if( len > 0 ){
						cur->value_len = len;
						cur->value = (char*)malloc( cur->value_len+1 );
						if( cur->value ){
							memset( cur->value, 0, cur->value_len+1 );
							memcpy( cur->value, v, len );
						}
					}
					state = TagEnd;
				}else{	//child tag
					state = TagName;
					cur= create_childnode( xml, cur );
					if( !cur ) return;
				}
				v = p+1;
			}
			break;
		case TagEnd:	//</tag>
			if( *p == '>' ){
				if( *v == '/' && strncmp( v+1, cur->name, MIN(XML_NODE_NAME_LEN-1, (int)(p-v-1)) )==0 ){
					state = OutsideTag;
				}else{
					printf("## incorrect end tag for <%s>\n", cur->name );
				}
//				printf("node name:%s  value:%s\n", cur->name, cur->value );
			}
			break;
		case AttributeName:	//name=""
			switch(*p){
			case ' ':	case '\r':	case '\n':	case '\t':	case '=':
				if( (int)(p-v)>1 ){ //a name is ok
					cur = create_attrnode( xml, cur );
					if( !cur ) return;
					memcpy( cur->name, v, MIN(XML_NODE_NAME_LEN-1, (int)(p-v)) );
				}
				v = p+1;
				if( *p == '=' ){
					state = AttributeValue;
				}
				break;
			case '>':
				state = Text;
				v = p+1;
				break;
			case '/':	//short end tag
				if( *(++p) == '>' ){
					state = OutsideTag;
				}else{
					printf("## no > after /\n" );
				}
				break;
			}
			break;
		case AttributeValue:
			if( *p=='"' ){
				if( *(v-1)=='"' ){ //a value is ok
					int len = (p-v);
					cur->value_len = len;
					cur->value = (char*)malloc( cur->value_len+1 );
					if( cur->value ){
						memset( cur->value, 0, cur->value_len+1 );
						memcpy( cur->value, v, len );
					}
					state = AttributeName;
//					printf("attr name:%s  value:%s\n", cur->name, cur->value );
					//attribute is ok.
					cur = cur->node_parent;
				}
				v = p+1;
			}
			break;
		}
	}
}

//load xml data from memory buffer
struct XML_DATA* xml_parse( const char* mem )
{
	struct XML_DATA* xml;
	xml = (struct XML_DATA*) malloc( sizeof(struct XML_DATA) );
	if( !xml ){
		seterr( "no memory to allocate for xml");
		return NULL;
	}
	memset( (void*)xml, 0, sizeof(struct XML_DATA) );
	parse_node( xml, mem );
	xml->node_cur = xml->node_root;
	return xml;
}

//save xml data to a file
void xml_save( struct XML_DATA* xml, const char* filename )
{
	unsigned int size;
	char* buf;
	FILE* fp;
	int i, ret;
	size = 1024*4;	//4K
	fp = fopen( filename, "wb" );
	if( fp == NULL ){
		seterr( "file could not open." );
		return;
	}
	for( i=0; i<5; i++ ){
		buf = (char*)malloc( size+1 );
		if( !buf ){
			seterr( "no memory to allocate buf.");
			fclose( fp );
			return;
		}
		ret = xml_build( xml, buf, size );
		if( ret<0 ){
			free( buf );
			buf = NULL;
		}else{
			break;
		}
	}
	if( !buf ){
		seterr( "cannot allocate buf. buf == NULL.");
		return ;
	}
	if( fputs( buf, fp ) < 0 ){
		seterr( "write file error.");
	}
	free( buf );
	fclose( fp );
}

static int build_node( struct XML_NODE* nod, int level, char* mem, int mem_size )
{
	struct XML_NODE* p;
	int i, len;
	len = mem_size;
	len -= strlen(nod->name)+1+level*1;
	//tag name
	if( len>0 ){
		for( i=0;i<level;i++ )
			strcat( mem, "\t" );
		strcat( mem, "<" );
		strcat( mem, nod->name );
	}
//	printf("building %s\n", nod->name );
	//attributes
	for( p=nod->node_attr; p; p=p->node_next ){
		len -= strlen(p->name)+p->value_len+4;
		if( len>0 )
			sprintf( mem, "%s %s=\"%s\"", mem, p->name, p->value );
	}
	//body
	if( nod->node_child ){
		len -= 2;
		if(len>0) strcat( mem, ">\n" );
		len = build_node( nod->node_child, level+1, mem+strlen(mem), len );
		len -= strlen(nod->name)+4+level*1;
		if( len>0 ){
			for( i=0;i<level;i++ )
				strcat( mem, "\t" );
			sprintf( mem, "%s</%s>\n", mem, nod->name );
		}
	}else if( !nod->node_attr || nod->value_len>0 ){
		len -= 1;
		if(len>0) strcat( mem, ">" );
		len -= nod->value_len;
		if( len>0 && nod->value )
			strcat( mem, nod->value );
		len -= strlen(nod->name)+4;
		if( len>0 )
			sprintf( mem, "%s</%s>\n", mem, nod->name );
	}else{ //short tag
		len -=4;
		if( len>0 )
			strcat( mem, " />\n" );
	}
	if( nod->node_next ){
		len = build_node( nod->node_next, level, mem+strlen(mem), len );
	}
	return len;
}

//build xml into memory
int xml_build( struct XML_DATA* xml, char* mem, int mem_size )	
{
	int len;
	memset( mem, 0, mem_size );
	//build xml header
	//not implemented ...
	//build xml body
	len = build_node( xml->node_root, 0, mem, mem_size );
	return (mem_size-len);
}

//free all nodes recursively
static void free_node( struct XML_NODE* nod )
{
	while( nod ){
		if( nod->node_attr )
			free_node( nod->node_attr );
		if( nod->node_child )
			free_node( nod->node_child );
		if( nod->value )
			free( nod->value );
		if( nod->node_next ){
			nod = nod->node_next;
			free( nod->node_pre );
		}else{
			free( nod );
			nod = NULL;
		}
	}
}

//free xml data
void xml_free( struct XML_DATA* xml )
{
	free_node( xml->node_root );
	free( xml );
}

static struct XML_NODE* get_attrnode( struct XML_DATA* xml, struct XML_NODE* parent, 
	const char* name, int create )
{
	struct XML_NODE* nod;
	nod = parent->node_attr;
	while( nod ){
		if( strcmp( nod->name, name )==0 )
			return nod;
		nod = nod->node_next;
	}
	//not found.
	if( create ){
		nod = create_attrnode( xml, parent );
		strncpy( nod->name, name, XML_NODE_NAME_LEN-1 ); 
		return nod;
	}else{
		return NULL;
	}
}

static struct XML_NODE* get_childnode( struct XML_DATA* xml, struct XML_NODE* parent, 
	char* name, char* condition, int create )
{
	struct XML_NODE* nod;
	nod = parent->node_child;
	if( !condition ){
		//upper  specail one
		if( strcmp( name, ".." )==0 ){
			if( parent->node_parent )
				return parent->node_parent;
			else
				return parent;
		}else if( strcmp( name, "." )==0 ){
			return parent;
		}
		//search nodes
		while( nod ){
			if( strcmp( nod->name, name )==0 )
				return nod;
			nod = nod->node_next;
		}
		//not found.
		if( create ){
			nod = create_childnode( xml, parent );
			strncpy( nod->name, name, XML_NODE_NAME_LEN-1 ); 
			return nod;
		}
	}else{	//得意之作
		char *p, *m, *v;
		char* conds[16][2];
		int i, j, end;
		struct XML_NODE* nod_attr;
		for( p=condition, i=0, end=0, v=condition; !end && i<16; p++ ){
			if( *p=='&' || *p=='\0' ){
				if(*p=='\0')	end=1;
				*p = '\0';
				m = strchr(v, '=' );
				if( m==NULL ){
					printf("## syntax error: lack '='.\n");
				}else{
					*m = '\0';
					conds[i][0] = v;
					conds[i][1] = m+1;
					i++;
				}
				v = p+1;
			}
		}
		//search nodes
		while( nod ){
			if( strcmp( nod->name, name )==0 ){
				//meet all conditions
				for( j=0; j<i; j++ ){
					nod_attr = get_attrnode( xml, nod, conds[j][0], 0 );
					if( !nod_attr || !nod_attr->value )
						break;
					if( strcmp( nod_attr->value, conds[j][1] )!=0 )
						break;
				}
				if( j==i ){	//均符合要求
					return nod;
				}
			}
			nod = nod->node_next;
		}
		//not found
		if( create ){
			nod = create_childnode( xml, parent );
			strncpy( nod->name, name, XML_NODE_NAME_LEN-1 ); 
			//meet all conditions
			for( j=0; j<i; j++ ){
				nod_attr = get_attrnode( xml, nod, conds[j][0], 1 );
				if( !nod_attr )
					break;
				nod_attr->value_len = strlen(conds[j][1]);
				nod_attr->value = (char*)malloc(nod_attr->value_len+1);
				strcpy( nod_attr->value, conds[j][1] );
			}
			return nod;
		}
	}
	return NULL;
}

//change path
// "/a/b/c"
// "/a?id="3434"/b/c
static struct XML_NODE* parse_path( struct XML_DATA *xml, const char* path, int create )
{
	struct XML_NODE* nod;
	const char* p, *v;
	char tmp[256];
	int len;
	p = path;
	//root 
	if( *p == '/' ){
		nod = xml->node_root;
		p++;
	}else{
		nod = xml->node_cur;
	}
	//
	v = p;
	for(;nod;p++){
		if( ( *p=='/'&& !(p>path&&*(p-1)=='=')  ) || *p=='\0' ){//进入一级目录
			char* m;
			len = (int)(p-v);
			if( len>0 && len<256 ){
				memcpy( tmp, v, len );
				tmp[len] = '\0';
				m = strchr( tmp, '?' );	
				if( m ){	//need search
					*m = '\0';
					if( *tmp ){
						nod = get_childnode( xml, nod, tmp, m+1, create );
					}else if( nod->node_parent ){
						nod = get_childnode( xml, nod->node_parent, nod->name, m+1, create );
					}
				}else{
					m = strchr( tmp, ':' );
					if( m ){
						*m = '\0';
						if( *tmp )
							nod = get_childnode( xml, nod, tmp, NULL, create );
						if( nod )
							nod = get_attrnode( xml, nod, m+1, create );
					}else{
						nod = get_childnode( xml, nod, tmp, NULL, create );
					}
				}
			}
			v = p+1;
			if( *p=='\0' )
				break;
		}
	}
	return nod;
}

//change the current path
//return 1:success  0:failed.
//
int xml_redirect( struct XML_DATA* xml, const char* path, int create_if_not_exist )	
{
	struct XML_NODE* nod;
	nod = parse_path( xml, path, create_if_not_exist );
	if( nod ){
		xml->node_cur = nod;
		return 1;
	}else{
		return 0;
	}
}

//read a string
char* xml_readstr( struct XML_DATA* xml, const char* path )
{
	struct XML_NODE* nod;
	nod = parse_path( xml, path, 0 );
	if( nod && nod->value ){
		return nod->value;
	}else{
		return "";	//temporary do this.
	}
}

//read a number	
int xml_readnum( struct XML_DATA* xml, const char* path )
{
	struct XML_NODE* nod;
	nod = parse_path( xml, path, 0 );
	if( nod && nod->value ){
		return atol(nod->value);
	}else{
		return 0;	//temporary do thi.
	}
}

//write a number
void xml_writenum( struct XML_DATA* xml, const char* path, int num )
{
	struct XML_NODE* nod;
	char tmp[16];
	nod = parse_path( xml, path, 1 );
	if( nod ){
		sprintf( tmp, "%d", num );
		if( nod->value )
			free( nod->value );
		nod->value_len = strlen(tmp);
		nod->value = (char*)malloc(nod->value_len+1);
		strcpy( nod->value, tmp );
	}
}

//write a string
void xml_writestr( struct XML_DATA* xml, const char* path, const char* str )	
{
	struct XML_NODE* nod;
	nod = parse_path( xml, path, 1 );
	if( nod ){
		if( nod->value )
			free( nod->value );
		nod->value_len = strlen(str);
		nod->value = (char*)malloc(nod->value_len+1);
		strcpy( nod->value, str );
	}
}

//move to the next node.
//return 1:succ 0:failed
int xml_movenext( struct XML_DATA* xml )
{
	struct XML_NODE* nod;
	nod = xml->node_cur;
	if( nod ) 
		nod = nod->node_next;
	if( nod ){
		xml->node_cur = nod;
		return 1;
	}else{
		return 0;
	}
}
