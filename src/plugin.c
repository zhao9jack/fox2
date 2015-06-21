/*
 *  plugin.c
 *
 *  Plugin Management
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-8-29 13:51:15 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  Plugin Management
 *
 */

#include "plugin.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __WIN32__
#include <io.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

#include "debug.h"
#include "memory.h"
#include "server.h"
#include "connection.h"
#include "loop.h"

static void plugin_delete_fun( const void* p )
{
	DBG("Unloading plugin [%s]", ((plugin*)p)->name );
	if( ((plugin*)p)->cleanup ){
		((plugin*)p)->cleanup( ((plugin*)p)->server );
	}
#ifdef __WIN32__
	FreeLibrary( (HMODULE)((plugin*)p)->handle );
#else
	dlclose( (void*)((plugin*)p)->handle );
#endif
	DEL( p );
}

static int plugin_searcher( const void* p, const void* v )
{
	return ( stricmp( ((plugin*)p)->name, v ) == 0 );
}


int plugin_init ( webserver* srv )
{
	loop_create( &srv->loop_plugin, MAX_PLUGIN, plugin_delete_fun );
	//search direcotry ./plugins
#ifdef __WIN32__
	int fhandle;
	struct _finddata_t filesx;
	fhandle = _findfirst( "./plugins/*.dll",&filesx);
	if(fhandle == -1)
	{
		DBG("fhandle error\n");
		return -1;
	}
	do{
		if(strcmp(filesx.name,"..")==0 || strcmp(filesx.name,".")==0)
			continue;
		if(filesx.attrib & _A_SUBDIR)//Ŀ¼
		{
			DBG("ignored direcotry %s", filesx.name );
		}else{
			plugin_create( srv, filesx.name );
		}
	}while(!_findnext(fhandle,&filesx));
	_findclose(fhandle);
#else
	DIR* dir_info;
	struct dirent* dir_entry;
	dir_info = opendir("./plugins");
	if( dir_info ){
		while ( (dir_entry = readdir(dir_info)) != NULL)
		{
			if(strcmp(dir_entry->d_name, "..")==0 || strcmp(dir_entry->d_name, ".")==0)
				continue;
			plugin_create( srv, dir_entry->d_name );
		} // while
		closedir(dir_info);
	}else{
		DBG("failed opening dir ./plugins");
	}
#endif
	return 0;
}

int plugin_create( webserver* srv, char* name )
{
	plugin * p;
	char path[PATH_LEN];
	if( loop_is_full( &srv->loop_plugin ) ){
		DBG("plugin add failed: loop is full");
		return -1;
	}
	NEW( p, sizeof(plugin) );
	p->server = srv;
	strncpy( p->name, name, PATH_LEN-1 );
	strcpy( path, "plugins/" );
	strncat( path, name, PATH_LEN-1 );
	DBG("Loading plugin [%s]", name );
#ifdef __WIN32__
	p->handle = LoadLibrary( path );
	if( p->handle ){
		p->entry = (plugin_entry) GetProcAddress( p->handle,"plugin_entry");//
		p->cleanup = (plugin_entry) GetProcAddress( p->handle,"plugin_cleanup");//
	}
	if( !p->handle || p->entry==NULL ){
		DBG("# Loading plugin: %s failed: %d", path, GetLastError() );
#else
//linux
	p->handle = (int)dlopen( path, RTLD_LAZY );
	if( p->handle ){
		p->entry = (plugin_entry) dlsym( (void*)p->handle,"plugin_entry");//
		p->cleanup = (plugin_entry) dlsym( (void*)p->handle,"plugin_cleanup");//
	}
	if( !p->handle || p->entry==NULL ){
		DBG("# Loading plugin: %s failed: %s", path, dlerror() );
#endif
		DEL( p );
		return -1;
	}else{
		loop_push_to_tail( &srv->loop_plugin, p );
		int ret;
		if( (ret=p->entry( srv )) < 0 ){
			DBG("# Init plugin: %s failed. ret=%d", path, ret );
			return -1;
		}
	}
	return 0;
}

int plugin_delete( webserver* srv, char* name )
{
	plugin * v;
	v = loop_search( &srv->loop_plugin, name, plugin_searcher );
	if( v ){
		loop_remove( &srv->loop_plugin, v );
	}
	return 0;
}


void plugin_cleanup( webserver* srv )
{
	loop_cleanup( &srv->loop_plugin );
}
