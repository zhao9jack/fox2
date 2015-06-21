// Bugfix for back to parent folder. 091205 by HuangGuan
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#ifdef __WIN32__
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <dirent.h>
#endif
#include "server.h"
#include "debug.h"
#include "memory.h"
#include "listdir.h"
#include "loop.h"
#include "util.h"

#define ATTR_DIR 1
typedef struct finfo_t{
	char name[128+4];
	time_t mtime;
	unsigned int size;
	char attr;
}finfo_t;
struct _retval{
	char* buf;
	int siz;
	int pos;
};

#ifndef __WIN32__
static char* strlwr( char* p )
{
	char* q = p;
	while( *q ){
		*q = tolower(*q); 
		++q;
	}
	return p;
}
#endif

static void loop_delfunc( const void* p )
{
	DEL(p);
}

static void search_dir( const char* path, loop *lp )
{
	char *tmp;
	NEW( tmp, PATH_LEN + 128+4 );
#ifdef __WIN32__
	int fhandle;
	struct _finddata_t filesx;
	sprintf( tmp, "%s/*", path );
	fhandle = _findfirst( tmp, &filesx);
	if(fhandle == -1)
	{
		DBG("fhandle error for %s\n", path);
		DEL(tmp);
		return ;
	}
	do{
		if (strcmp(filesx.name, ".")==0 || strcmp(filesx.name, "..")==0)
			continue;
		finfo_t *fi;
		NEW( fi, sizeof(finfo_t) );
		strncpy( fi->name, filesx.name, 128 );
		fi->size = filesx.size;
		fi->mtime = filesx.time_write;
		fi->attr = filesx.attrib&_A_SUBDIR ? 1:0;
		loop_push_to_tail( lp, fi );
	}while(!_findnext(fhandle,&filesx));
	_findclose(fhandle);
#else
	DIR* dir_info;
	struct dirent* dir_entry;
	dir_info = opendir(path);
	int last_slash = 0;
	if( dir_info ){
		if (path[strlen(path)-1] == '/')
			last_slash = 1;
		while ( (dir_entry = readdir(dir_info)) != NULL)
		{
			struct stat s;
			finfo_t *fi;
			if (strcmp(dir_entry->d_name, ".")==0 || strcmp(dir_entry->d_name, "..")==0)
				continue;
			if( last_slash )
				snprintf(tmp, PATH_LEN, "%s%s", path, dir_entry->d_name);
			else
				snprintf(tmp, PATH_LEN, "%s/%s", path, dir_entry->d_name);
			if (stat(tmp, &s) == 0){
				NEW( fi, sizeof(finfo_t) );
				strncpy( fi->name, dir_entry->d_name, 128 );
				fi->size = s.st_size;
				fi->mtime = s.st_mtime;
				fi->attr = (s.st_mode&S_IFMT) == S_IFDIR ? 1:0;
				loop_push_to_tail( lp, fi );
			}
		} // while
		closedir(dir_info);
	}else{
		DBG("failed opening dir ./plugins");
	}
#endif
	DEL( tmp );
}


static const char* icon_file(char* fname)
{
	/* 文件图标类型描述     */
	static struct ftype_t
	{
		char* extension;
		char* file;
	}file_types[] = {
		{ "*" ,		"0.gif"  },
		{ "txt" ,		"1.gif"  },
		{ "chm" ,	"2.gif"  },
		{ "hlp" ,	"2.gif"  },
		{ "doc" ,	"3.gif"  },
		{ "pdf" ,	"4.gif"  },
		{ "gif" ,		"6.gif"  },
		{ "ico" ,		"6.gif"  },
		{ "jpg" ,	"6.gif"  },
		{ "png" ,	"6.gif"  },
		{ "bmp" ,	"6.gif"  },
		{ "asp" ,	"7.gif"  },
		{ "jsp" ,		"7.gif"  },
		{ "js" ,		"7.gif"  },
		{ "php" ,	"7.gif"  },
		{ "php3" ,	"7.gif"  },
		{ "aspx" ,	"7.gif"  },
		{ "htm" ,	"8.gif"  },
		{ "html" ,	"8.gif"  },
		{ "shtml" ,	"8.gif"  },
		{ "zip" ,		"9.gif"  },
		{ "rar" ,		"9.gif"  },
		{ "exe" ,	"10.gif"  },
		{ "avi" ,		"11.gif"  },
		{ "mpg" ,	"11.gif"  },
		{ "rm" ,		"11.gif"  },
		{ "rmvb" ,	"11.gif"  },
		{ "asf" ,		"11.gif"  },
		{ "ra" ,		"12.gif"  },
		{ "ram" ,	"12.gif"  },
		{ "mid" ,	"13.gif"  },
		{ "wav" ,	"13.gif"  },
		{ "mp3" ,	"13.gif"  },
		{ "c" ,		"14.gif"  },
		{ "h" ,		"15.gif"  },
		{ "cpp" ,	"16.gif"  },
		{ "s" ,		"17.gif"  },
		{ "asm" ,	"17.gif"  },
	};
	static const int type_total = sizeof(file_types) / sizeof(struct ftype_t);
	// content type
	char ext[10];
	int i;
	const char* p = strrchr( fname, '.' );
	if( p == NULL )
		strcpy(ext, "*");
	else{
		strncpy(ext, p+1, 6 );
		strlwr( ext );
	}
	for (i = 1; i < type_total; i++)
		if ( strcmp(ext, file_types[i].extension)==0 )
			return file_types[i].file;
	return file_types[0].file;
}

static int dir_builder( const void* p, const void* q )
{
	struct _retval* ret = (struct _retval*)q;
	struct finfo_t* fi = (struct finfo_t*)p;
	if( fi->attr & ATTR_DIR ){
		if( ret->siz - ret->pos < KB(2) )
			return -1;
		char timeBuf[64];
		if( fi->mtime )
			strftime( timeBuf, 64, "%a, %d %b %Y %X", localtime(&fi->mtime) );
		else
			strcpy( timeBuf, "null" );
		ret->pos += sprintf( ret->buf+ret->pos, "<tr><td width=\"20\"><img src=\"/system/icons/folder.gif\" alt=\"[DIR]\"></td><td width=\"360\"><a href=\""
			"%s/\">%s/</a></td><td width=\"240\">%s</td><td width=\"120\" align=\"right\">-</td></tr>\r\n", 
			fi->name, fi->name, timeBuf );
	}
	return 0;
}

static int file_builder( const void* p, const void* q )
{
	struct _retval* ret = (struct _retval*)q;
	finfo_t* fi = (finfo_t*)p;
	if( !(fi->attr & ATTR_DIR) ){
		if( ret->siz - ret->pos < KB(2) )
			return -1;
		char timeBuf[64];
		if( fi->mtime )
			strftime( timeBuf, 64, "%a, %d %b %Y %X", localtime(&fi->mtime) );
		else
			strcpy( timeBuf, "null" );
		ret->pos += sprintf( ret->buf+ret->pos, "<tr><td width=\"20\"><img src=\"/system/icons/%s\" alt=\"[FILE]\"></td><td width=\"360\"><a href=\""
			"%s\">%s</a></td><td width=\"240\">%s</td><td width=\"120\" align=\"right\">%u k</td></tr>\r\n", 
			icon_file(fi->name), fi->name, fi->name, timeBuf, fi->size );
	}
	return 0;
}

#ifdef __WIN32__
#define CHARSET "gb2312"
#else
#define CHARSET "utf-8"
#endif

int listdir( char* buffer, int size, const char* path, const char* uri )
{
	loop loop_files;
	struct _retval retval;
	retval.buf = buffer;
	retval.siz = size;
	retval.pos = 0;
	char* tmp;
	NEW( tmp, PATH_LEN+16 );
	if( !tmp )
		return 0;
	sprintf(tmp, "/%s/..", uri);
	parse_path(tmp);
	if( loop_create( &loop_files, 1024, loop_delfunc ) < 0 ){
		DEL(tmp);
		return 0;
	}
	search_dir( path, &loop_files );
	if( retval.siz - retval.pos >= KB(2) )
		retval.pos += sprintf( retval.buf+retval.pos, "<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset="CHARSET"\">"
			"<title>Index of %s</title><style>td{ font-size:14px; }</style></head><body><h1>"
			"<img alt=\"\" src=\"/system/icons/sega.ico\">Index of %s</h1><img src=\"/system/icons/folder.gif\" alt=\"     \"><hr>"
			"<img src=\"/system/icons/back.gif\" alt=\"[DIR]\"> <a href=\"%s%s/\">Parent directory</a>\r\n<table border=\"0\">",
			uri, uri, tmp[0]=='/'||tmp[0]=='\0'?"":"/", tmp );
	DEL(tmp);
	loop_search( &loop_files, &retval, dir_builder );
	loop_search( &loop_files, &retval, file_builder );
	loop_cleanup( &loop_files );
	if( retval.siz - retval.pos >= KB(2) )
		retval.pos += sprintf( retval.buf+retval.pos, "</table><hr><address>This page is created by Xiaoxia's WebServer (http://home.xxsyzx.com)"
		" </address></body></html>" );
	return retval.pos;
}

