
/**********************************************************************

  Copyright 2019 Andrey V.Kosteltsev

  Licensed under the Radix.pro License, Version 1.0 (the "License");
  you may not use this file  except  in compliance with the License.
  You may obtain a copy of the License at

     https://radix.pro/licenses/LICENSE-1.0-en_US.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
  implied.

 **********************************************************************/

#ifndef _PKG_LIST_H_
#define _PKG_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <dlist.h>


enum _procedure
{
  INSTALL = 0, /* 'install' */
  UPDATE       /* 'update'  */
};

enum _priority
{
  REQUIRED = 0, /* synonims: REQUIRED    | required    | REQ | req */
  RECOMMENDED,  /* synonims: RECOMMENDED | recommended | REC | rec */
  OPTIONAL,     /* synonims: OPTIONAL    | optional    | OPT | opt */
  SKIP          /* synonims: SKIP        | skip        | SKP | skp */
};


struct pkginfo
{
  char   *name;
  char   *version;
  char   *arch;
  char   *distro_name;
  char   *distro_version;
  char   *group;
  char   *short_description;
  char   *url;
  char   *license;
  size_t  uncompressed_size; /* size in 1024-byte blocks */
  size_t  compressed_size;   /* size in bytes            */
  int     total_files;
};

struct pkg
{
  char *group;
  char *name;
  char *version;

  enum  _procedure procedure; /* install procedure */
};

struct references
{
  int    size;
  struct dlist *list; /* list of pkg structs */
};

struct requires
{
  int    size;
  struct dlist *list; /* list of pkg structs */
};

struct files
{
  int    size;
  struct dlist *list;     /* list of strings */
};


struct package
{
  struct pkginfo *pkginfo;

  char  *hardware;      /* optional parameter for JSON */

  char  *tarball;
  enum  _procedure procedure; /* install procedure     */
  enum  _priority  priority;  /* install user priority */

  struct references *references;
  struct requires   *requires;

  char  *description;

  char  *restore_links;
  char  *install_script;

  struct files *files;
};


extern char *hardware;
extern int   minimize;

extern char *strprio( enum _priority priority, int short_name );
extern char *strproc( enum _procedure procedure );

extern struct dlist *tarballs;

extern void add_tarball( char *tarball ); /* append the tarballs list */
extern void free_tarballs( void );
extern const char *find_tarball( const char *name );
extern void print_tarballs( void );


extern struct dlist *packages;

extern struct pkg *pkg_alloc( void );
extern void pkg_free( struct pkg *pkg );

extern struct package *package_alloc( void );
extern void package_free( struct package *package );

extern void add_reference( struct package *package, struct pkg *pkg );
extern void add_required( struct package *package, struct pkg *pkg );
extern void add_file( struct package *package, const char *fname );
extern void package_print_references( struct package *package );
extern void package_print_requires( struct package *package );
extern void package_print_files( struct package *package );

extern void add_package( struct package *package ); /* append the packages list */
extern void free_packages( void );


struct dlist *provides;
struct dlist *extern_requires;

extern  int create_provides_list( struct pkg *single_package );
extern void print_provides_list( const char *plist_fname );
extern void print_provides_tree( const char *json_fname );
extern void free_provides_list( void );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _PKG_LIST_H_ */
