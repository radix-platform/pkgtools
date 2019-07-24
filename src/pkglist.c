
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

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>   /* basename(3) */
#include <unistd.h>
#include <time.h>

#include <msglog.h>

#include <make-pkglist.h>

#include <cmpvers.h>
#include <dlist.h>
#include <jsmin.h>
#include <pkglist.h>


char *hardware = NULL;
int   minimize = 0;

struct dlist *packages = NULL;
struct dlist *tarballs = NULL;

struct dlist *provides = NULL;
struct dlist *extern_requires = NULL;

static struct dlist *tree = NULL;

static char *pkgs_fname = NULL,
            *tree_fname = NULL,
            *html_fname = NULL;

static char *pkgs_min_fname = NULL,
            *tree_min_fname = NULL;

static const char *tarball_suffix = "txz";

/***************************************************************
  tarballs List functions:
  =======================

  NOTE:
  ----
    TARBALLS  is an optional list  created in case when we have
    a set of PACKAGES as input of make-pkglist utility. When we
    are working with a set of input PKGLOGs the  TARBALLS  list
    is not chreated and pointer to the tarballs == NULL.
 */
void add_tarball( char *tarball )
{
  tarballs = dlist_append( tarballs, (void *)strdup( tarball ) );
}

static void __free_tarball( void *data, void *user_data )
{
  if( data ) { free( data ); }
}

void free_tarballs( void )
{
  if( tarballs ) { dlist_free( tarballs, __free_tarball ); tarballs = NULL; }
}

static int __compare_tarballs( const void *a, const void *b )
{
  return strncmp( (const char *)a, (const char *)b, (size_t)strlen((const char *)b) );
}

const char *find_tarball( const char *name )
{
  struct dlist *node = NULL;

  if( !tarballs || !name ) return NULL;

  node = dlist_find_data( tarballs, __compare_tarballs, (const void *)name );
  if( node )
  {
    return (const char *)node->data;
  }

  return NULL;
}

/*********************
  Just for debugging:
 */
static void __print_tarball( void *data, void *user_data )
{
  int *counter = (int *)user_data;

  if( counter ) { fprintf( stdout, "tarball[%.5d]: %s\n", *counter, (char *)data ); ++(*counter); }
  else          { fprintf( stdout, "tarball: %s\n", (char *)data ); }
}

void print_tarballs( void )
{
  int cnt = 0;
  if( tarballs ) { dlist_foreach( tarballs, __print_tarball, (void *)&cnt ); }
}
/*
  End of tarballs List functions.
 ***************************************************************/



char *strprio( enum _priority priority, int short_name )
{
  char *p = NULL;

  switch( priority )
  {
    case REQUIRED:
      p = ( short_name ) ? "REQ" : "REQUIRED";
      break;
    case RECOMMENDED:
      p = ( short_name ) ? "REC" : "RECOMMENDED";
      break;
    case OPTIONAL:
      p = ( short_name ) ? "OPT" : "OPTIONAL";
      break;
    case SKIP:
      p = ( short_name ) ? "SKP" : "SKIP";
      break;
  }
  return p;
}

char *strproc( enum _procedure procedure )
{
  char *p = NULL;

  switch( procedure )
  {
    case INSTALL:
      p = "install";
      break;
    case UPDATE:
      p = "update";
      break;
  }
  return p;
}


/***************************************************************
  PACKAGE functions:
 */

struct pkg *pkg_alloc( void )
{
  struct pkg *pkg = NULL;

  pkg = (struct pkg *)malloc( sizeof( struct pkg ) );
  if( !pkg ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)pkg, sizeof( struct pkg ) );

  return pkg;
}

void pkg_free( struct pkg *pkg )
{
  if( pkg )
  {
    if( pkg->group )   { free( pkg->group );   pkg->group   = NULL; }
    if( pkg->name )    { free( pkg->name );    pkg->name    = NULL; }
    if( pkg->version ) { free( pkg->version ); pkg->version = NULL; }

    free( pkg );
  }
}

static void __pkg_free_func( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;
  if( pkg ) { pkg_free( pkg ); }
}


static struct pkginfo *__pkginfo_alloc( void )
{
  struct pkginfo *pkginfo = NULL;

  pkginfo = (struct pkginfo *)malloc( sizeof( struct pkginfo ) );
  if( !pkginfo ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)pkginfo, sizeof( struct pkginfo ) );

  return pkginfo;
}

static void __pkginfo_free( struct pkginfo *pkginfo )
{
  if( pkginfo )
  {
    if( pkginfo->name )              { free( pkginfo->name );              pkginfo->name              = NULL; }
    if( pkginfo->version )           { free( pkginfo->version );           pkginfo->version           = NULL; }
    if( pkginfo->arch )              { free( pkginfo->arch );              pkginfo->arch              = NULL; }
    if( pkginfo->distro_name )       { free( pkginfo->distro_name );       pkginfo->distro_name       = NULL; }
    if( pkginfo->distro_version )    { free( pkginfo->distro_version );    pkginfo->distro_version    = NULL; }
    if( pkginfo->group )             { free( pkginfo->group );             pkginfo->group             = NULL; }
    if( pkginfo->short_description ) { free( pkginfo->short_description ); pkginfo->short_description = NULL; }
    if( pkginfo->url )               { free( pkginfo->url );               pkginfo->url               = NULL; }
    if( pkginfo->license )           { free( pkginfo->license );           pkginfo->license           = NULL; }

    free( pkginfo );
  }
}


static struct references *__references_alloc( void )
{
  struct references *references = NULL;

  references = (struct references *)malloc( sizeof( struct references ) );
  if( !references ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)references, sizeof( struct references ) );

  return references;
}

static void __references_free( struct references *references )
{
  if( references )
  {
    if( references->list ) { dlist_free( references->list, __pkg_free_func ); references->list = NULL; }
    free( references );
  }
}


static struct requires *__requires_alloc( void )
{
  struct requires *requires = NULL;

  requires = (struct requires *)malloc( sizeof( struct requires ) );
  if( !requires ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)requires, sizeof( struct requires ) );

  return requires;
}

static void __requires_free( struct requires *requires )
{
  if( requires )
  {
    if( requires->list ) { dlist_free( requires->list, __pkg_free_func ); requires->list = NULL; }
    free( requires );
  }
}


static struct files *__files_alloc( void )
{
  struct files *files = NULL;

  files = (struct files *)malloc( sizeof( struct files ) );
  if( !files ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)files, sizeof( struct files ) );

  return files;
}

static void __files_free_func( void *data, void *user_data )
{
  if( data ) { free( data ); }
}

static void __files_free( struct files *files )
{
  if( files )
  {
    if( files->list ) { dlist_free( files->list, __files_free_func ); files->list = NULL; }
    free( files );
  }
}


struct package *package_alloc( void )
{
  struct package    *package    = NULL;
  struct pkginfo    *pkginfo    = __pkginfo_alloc();
  struct references *references = __references_alloc();
  struct requires   *requires   = __requires_alloc();
  struct files      *files      = __files_alloc();

  package = (struct package *)malloc( sizeof( struct package ) );
  if( !package ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)package, sizeof( struct package ) );

  package->pkginfo    = pkginfo;
  package->references = references;
  package->requires   = requires;
  package->files      = files;

  return package;
}

void package_free( struct package *package )
{
  if( package )
  {
    if( package->pkginfo )    {    __pkginfo_free( package->pkginfo );    package->pkginfo    = NULL; }
    if( package->references ) { __references_free( package->references ); package->references = NULL; }
    if( package->requires )   {   __requires_free( package->requires );   package->requires   = NULL; }
    if( package->files )      {      __files_free( package->files );      package->files      = NULL; }

    if( package->description )    { free( package->description );     package->description    = NULL; }
    if( package->restore_links )  { free( package->restore_links );   package->restore_links  = NULL; }
    if( package->install_script ) { free( package->install_script );  package->install_script = NULL; }
    if( package->hardware )       { free( package->hardware );        package->hardware       = NULL; }
    if( package->tarball )        { free( package->tarball );         package->tarball        = NULL; }

    free( package );
  }
}

static void __package_free_func( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  if( package ) { package_free( package ); }
}

void free_packages( void )
{
  if( packages ) { dlist_free( packages, __package_free_func ); packages = NULL; }
}


void add_package( struct package *package )
{
  packages = dlist_append( packages, (void *)package );
}

void add_reference( struct package *package, struct pkg *pkg )
{
  if( package && package->references && pkg )
  {
    package->references->list = dlist_append( package->references->list, (void *)pkg );
    package->references->size = dlist_length( package->references->list );
  }
}

void add_required( struct package *package, struct pkg *pkg )
{
  if( package && package->requires && pkg )
  {
    package->requires->list = dlist_append( package->requires->list, (void *)pkg );
    package->requires->size = dlist_length( package->requires->list );
  }
}

void add_file( struct package *package, const char *fname )
{
  if( package && package->files && fname )
  {
    package->files->list = dlist_append( package->files->list, (void *)strdup( fname ) );
    package->files->size = dlist_length( package->files->list );
  }
}

/*********************
  Just for debugging:
 */
static void __print_reference( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;

  if( pkg )
  {
    if( pkg->group ) { fprintf( stdout, "reference: %s/%s=%s\n", pkg->group, pkg->name, pkg->version ); }
    else             { fprintf( stdout, "reference: %s=%s\n",                pkg->name, pkg->version ); }
  }
}

void package_print_references( struct package *package )
{
  if( !package ) return;

  if( package->references->list )
  {
    dlist_foreach( package->references->list, __print_reference, NULL );
  }
}

static void __print_required( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;

  if( pkg )
  {
    if( pkg->group ) { fprintf( stdout, "required: %s/%s=%s\n", pkg->group, pkg->name, pkg->version ); }
    else             { fprintf( stdout, "required: %s=%s\n",                pkg->name, pkg->version ); }
  }
}

void package_print_requires( struct package *package )
{
  if( !package ) return;

  if( package->requires->list )
  {
    dlist_foreach( package->requires->list, __print_required, NULL );
  }
}

static void __print_file( void *data, void *user_data )
{
  int *counter = (int *)user_data;

  if( counter ) { fprintf( stdout, "file[%.5d]: %s\n", *counter, (char *)data ); ++(*counter); }
  else          { fprintf( stdout, "file: %s\n", (char *)data ); }
}

void package_print_files( struct package *package )
{
  int cnt = 0;

  if( !package ) return;

  if( package->files->list )
  {
    dlist_foreach( package->files->list, __print_file, (void *)&cnt );
  }
}

/*
  End of PACKAGES functions.
 ***************************************************************/

/***************************************************************
  Extern REQUIRES list functions:
 */

static int __compare_required( const void *a, const void *b )
{
  int  ret = -1;

  struct pkg *pkg1 = (struct pkg *)a;
  struct pkg *pkg2 = (struct pkg *)b;

  if( pkg1->group && pkg2->group )
  {
    ret = strcmp( pkg1->group, pkg2->group );
  }
  else if( !pkg1->group && !pkg2->group )
  {
    ret = 0;
  }
  else if( pkg1->group )
  {
    ret = 1;
  }

  if( ! ret )
  {
    return strcmp( pkg1->name, pkg2->name );
  }
  return ret;
}

static int __compare_required_with_version( const void *a, const void *b )
{
  int  ret = -1;

  struct pkg *pkg1 = (struct pkg *)a;
  struct pkg *pkg2 = (struct pkg *)b;

  if( pkg1->group && pkg2->group )
  {
    ret = strcmp( pkg1->group, pkg2->group );
  }
  else if( !pkg1->group && !pkg2->group )
  {
    ret = 0;
  }
  else if( pkg1->group )
  {
    ret = 1;
  }

  if( ! ret )
  {
    ret = strcmp( pkg1->name, pkg2->name );
    if( ! ret )
    {
      return cmp_version( (const char *)pkg1->version, (const char *)pkg2->version );
    }
  }
  return ret;
}

static void __add_unique_required( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;

  if( pkg )
  {
    struct dlist *found = dlist_find_data( extern_requires, __compare_required, (const void *)data );

    if( found )
    {
      if( cmp_version( (const char *)((struct pkg *)found->data)->version, (const char *)pkg->version ) )
      {
        char *s = ((struct pkg *)found->data)->version;
        ((struct pkg *)found->data)->version =
           strdup( max_version( (const char *)((struct pkg *)found->data)->version, (const char *)pkg->version ) );
        free( s );
      }
    }
    else
    {
      struct pkg *req = pkg_alloc();
      if( req )
      {
        if( pkg->group )
        {
          req->group = strdup( pkg->group   );
        }
        req->name    = strdup( pkg->name    );
        req->version = strdup( pkg->version );

        extern_requires = dlist_append( extern_requires, (void *)req );
      }
    }
  }
}

static void __fill_extern_requires( void *data, void *user_data )
{
  struct package *package = (struct package *)data;

  if( package )
  {
    struct pkg *provide = pkg_alloc();

    if( provide )
    {
      if( package->pkginfo->group )
      {
        provide->group = strdup( package->pkginfo->group   );
      }
      provide->name    = strdup( package->pkginfo->name    );
      provide->version = strdup( package->pkginfo->version );

      provides = dlist_append( provides, (void *)provide );
    }

    if( package->requires->list )
    {
      dlist_foreach( package->requires->list, __add_unique_required, NULL );
    }
  }
}

static void __clean_extern_requires( void *data, void *user_data )
{
  if( data )
  {
    extern_requires = dlist_remove_data( extern_requires, __compare_required_with_version, __pkg_free_func, (const void *)data );
  }
}

static int __compare_provided_old_package( const void *a, const void *b )
{
  int  ret = -1;

  struct package *pkg1 = (struct package *)a;
  struct     pkg *pkg2 = (struct     pkg *)b;

  if( pkg1->pkginfo->group && pkg2->group )
  {
    ret = strcmp( pkg1->pkginfo->group, pkg2->group );
  }
  else if( !pkg1->pkginfo->group && !pkg2->group )
  {
    ret = 0;
  }
  else if( pkg1->pkginfo->group )
  {
    ret = 1;
  }

  if( ! ret )
  {
    ret = strcmp( pkg1->pkginfo->name, pkg2->name );
    if( ! ret )
    {
      pkg2->procedure = UPDATE; /* mark as too old */
      return ret;
    }
  }
  return ret;
}

static void __remove_old_package( void *data, void *user_data )
{
  packages = dlist_remove_data( packages, __compare_provided_old_package, __package_free_func, (const void *)data );
}

static void remove_old_packages( void )
{
  dlist_foreach( extern_requires, __remove_old_package, NULL );
}
/*
  End of Extern REQUIRES list functions.
 ***************************************************************/


/***************************************************************
  Check REQUIRES functions:
 */
static int __compare_provided( const void *a, const void *b )
{
  int  ret = -1;

  struct package *pkg1 = (struct package *)a;
  struct     pkg *pkg2 = (struct     pkg *)b;

  if( pkg1->pkginfo->group && pkg2->group )
  {
    ret = strcmp( pkg1->pkginfo->group, pkg2->group );
  }
  else if( !pkg1->pkginfo->group && !pkg2->group )
  {
    ret = 0;
  }
  else if( pkg1->pkginfo->group )
  {
    ret = 1;
  }

  if( ! ret )
  {
    return strcmp( pkg1->pkginfo->name, pkg2->name );
  }
  return ret;
}

static int __compare_packages_by_name( const void *a, const void *b )
{
  int  ret = -1;

  struct package *pkg1 = (struct package *)a;
  struct package *pkg2 = (struct package *)b;

  if( !strcmp( pkg1->pkginfo->name, pkg2->pkginfo->name ) )
  {
    if( pkg1->pkginfo->group && pkg2->pkginfo->group )
    {
      ret = strcmp( pkg1->pkginfo->group, pkg2->pkginfo->group );
    }
    else if( !pkg1->pkginfo->group && !pkg2->pkginfo->group )
    {
      ret = 0;
    }
    else if( pkg1->pkginfo->group )
    {
      ret = 1;
    }

    /* returns equal only if groups are not equal */
    if( ret ) return 0;
  }

  return ret;
}

static int check_dependencies( struct package *package )
{
  struct dlist *list = NULL, *next = NULL, *update = NULL;
  int    depended    = -1;

  if( !package ) return depended;
  depended = 0;

  if( !(list = package->requires->list) ) return depended;

  while( list )
  {
    next = dlist_next( list );
    {
      int has_extern_dependencies = 0, already_provided = 0;

      struct pkg   *pkg   = (struct pkg *)list->data;
      struct dlist *found = dlist_find_data( extern_requires, __compare_required, (const void *)pkg );

      if( found )
      {
        if( cmp_version( (const char *)((struct pkg *)found->data)->version, (const char *)pkg->version ) >= 0 )
        {
          /* required package is found in the extern_requires list */
          has_extern_dependencies += 1;
        }
      }

      found = dlist_find_data( provides, __compare_provided, (const void *)pkg );
      if( found )
      {
        if( cmp_version( (const char *)((struct package *)found->data)->pkginfo->version, (const char *)pkg->version ) >= 0 )
        {
          /* required package is found in the extern_requires list */
          already_provided += 1;
        }
      }

      if( !already_provided && !has_extern_dependencies ) depended += 1;
    }
    list = next;
  }

  /* Check if the package with the same name already exists in the provides list */
  update = dlist_find_data( provides, __compare_packages_by_name, (const void *)package );
  if( update )
  {
    /* Set install procedure to UPDATE: */
    package->procedure = UPDATE;
  }

  return depended;
}
/*
  End of Check REQUIRES functions.
 ***************************************************************/

static void __fill_provides_list( void *data, void *user_data )
{
  struct package *package = (struct package *)data;

  if( package )
  {
    if( !check_dependencies( package ) )
    {
      /* move independed package to the provides list */
      packages = dlist_remove( packages, (const void *)data );
      provides = dlist_append( provides, (void *)package );
    }
  }
}


static void __print_extern_package( void *data, void *user_data )
{
  FILE *output = (FILE *)user_data;
  struct pkg *pkg = (struct pkg *)data;

  if( pkg )
  {
    if( pkg->group ) { fprintf( output, "# required: %s/%s=%s\n", pkg->group, pkg->name, pkg->version ); }
    else             { fprintf( output, "# required: %s=%s\n",                pkg->name, pkg->version ); }
  }
}

static void __print_provided_package( void *data, void *user_data )
{
  FILE *output = (FILE *)user_data;
  struct package *package = (struct package *)data;

  if( package )
  {
    fprintf( output, "%s:",  package->pkginfo->name );
    fprintf( output, "%s:",  package->pkginfo->version );
    fprintf( output, "%s:",  package->pkginfo->short_description );
    if( package->tarball )
    {
      fprintf( output, "%s:",  package->tarball );
    }
    else
    {
      if( package->pkginfo->group ) fprintf( output, "%s/", package->pkginfo->group );

      fprintf( output, "%s-", package->pkginfo->name );
      fprintf( output, "%s-", package->pkginfo->version );
      fprintf( output, "%s-", package->pkginfo->arch );
      fprintf( output, "%s-", package->pkginfo->distro_name );
      fprintf( output, "%s.", package->pkginfo->distro_version );
      fprintf( output, "%s:", tarball_suffix ); /* default is '.txz' */
    }
    fprintf( output, "%s:",  strproc( package->procedure ) );
    fprintf( output, "%s\n", strprio( package->priority, 0 ) );
  }
}


static void __reduce_packages_list( struct pkg *pkg )
{
  struct package *package = NULL;
  struct dlist   *found   = NULL;

  if( !pkg ) return;

  found = dlist_find_data( packages, __compare_provided, (const void *)pkg );
  if( found && found->data )
  {
    struct dlist *list = NULL, *next = NULL;

    package = (struct package *)found->data;

    packages = dlist_remove( packages, (const void *)package );
    provides = dlist_append( provides, (void *)package );

    if( !(list = package->requires->list) ) return;

    while( list )
    {
      next = dlist_next( list );
      {
        __reduce_packages_list( (struct pkg *)list->data );
      }
      list = next;
    }
  }
}

static void reduce_packages_list( struct pkg *pkg )
{
  __reduce_packages_list( pkg );

  dlist_free( packages, __package_free_func );
  if( dlist_length( provides ) != 0 )
  {
    packages = provides;
    provides = NULL;
  }
}

int create_provides_list( struct pkg *single_package )
{
  int ret = 0;

  if( !packages ) return ret;

  if( single_package )
  {
    /**********************************************************************
      Reduce packages list to the list of requires of single_package only:
     */
    reduce_packages_list( single_package );
  }

  /* Fill two lists: provides and extern_requires: */
  dlist_foreach( packages, __fill_extern_requires, NULL );

  /* Remove packages from extern_requires list which present in the provides list: */
  dlist_foreach( provides, __clean_extern_requires, NULL );

  /* Now we don't need previous contents of provides list: */
  dlist_free( provides, __pkg_free_func );
  provides = NULL;

  /* Remove old packages if required new version of them */
  remove_old_packages();

  /* move packages into provides list in order of installation: */
  while( dlist_length( packages ) != 0 )
  {
    dlist_foreach( packages, __fill_provides_list, NULL );
  }

  return dlist_length( extern_requires );
}

void free_provides_list( void )
{
  if( hardware ) { free( hardware ); hardware = NULL; }

  dlist_free( extern_requires, __pkg_free_func );
  dlist_free( provides, __package_free_func );
}

void print_provides_list( const char *plist_fname )
{
  FILE *plist = NULL;

  if( !plist_fname || !provides ) return;

  plist = fopen( plist_fname, "w" );
  if( !plist )
  {
    FATAL_ERROR( "Cannot create output %s file", basename( (char *)plist_fname ) );
  }

  fprintf( plist, "#\n" );
  fprintf( plist, "# file format:\n" );
  fprintf( plist, "# ===========\n" );
  fprintf( plist, "#\n" );
  fprintf( plist, "# Each line contains six fields separated by colon symbol ':' like following.\n" );
  fprintf( plist, "#\n" );
  fprintf( plist, "# pkgname:version:description:tarball:procedure:priority\n" );
  fprintf( plist, "#\n" );
  fprintf( plist, "# where:\n" );
  fprintf( plist, "#\n" );
  fprintf( plist, "#   pkgname     - should be the same as the value of pkgname  in the '.DESCRIPTION' file;\n" );
  fprintf( plist, "#   version     - package version for showing in check list  dialog box  if this file is\n" );
  fprintf( plist, "#                 used to complete common check dialog for installing group  of packages;\n" );
  fprintf( plist, "#   description - short description for showing in check list dialog box if this file is\n" );
  fprintf( plist, "#                 used to complete common check dialog for installing  group of packages;\n" );
  fprintf( plist, "#   tarball     - should end in '.txz';\n" );
  fprintf( plist, "#   procedure   - installation procedure {install | update}:\n" );
  fprintf( plist, "#                  * 'install' - if package requires normal installation,\n" );
  fprintf( plist, "#                  * 'update'  - if already installed package should be updated by this\n" );
  fprintf( plist, "#                                package archive;\n" );
  fprintf( plist, "#   priority    - { REQUIRED|RECOMMENDED|OPTIONAL|SKIP }\n" );
  fprintf( plist, "#                  synonims:\n" );
  fprintf( plist, "#                    { REQUIRED    | required    | REQ | req }\n" );
  fprintf( plist, "#                    { RECOMMENDED | recommended | REC | rec }\n" );
  fprintf( plist, "#                    { OPTIONAL    | optional    | OPT | opt }\n" );
  fprintf( plist, "#                    { SKIP        | skip        | SKP | skp }\n" );
  fprintf( plist, "#\n" );

  if( extern_requires )
  {
    dlist_foreach( extern_requires, __print_extern_package, plist );
    fprintf( plist, "#\n" );
  }
  dlist_foreach( provides, __print_provided_package, plist );

  fflush( plist );
  fclose( plist );
}


/***************************************************************
  Requires TREE functions:
 */

struct _ctx
{
  FILE *output;
  int   index, size, depth;
};

/**************************
  HTML Template Variables:
 */
static char *root       = NULL;
static char *bug_url    = NULL;

static int   svg_width  = 2;
static int   svg_height = 2;

static char *json_pkgs_file = NULL;
static char *json_tree_file = NULL;

static char *copying = "Radix cross Linux";

#define max(a,b) ({ typeof (a) _a = (a); typeof (b) _b = (b); _a > _b ? _a : _b; })

/*
  формирование имен файлов для вывода REQUIRES tree:

   json_fname              | last argument of make-pkglist | last argument type
  -------------------------+-------------------------------+--------------------
   './a.txt'               | a.txt                         | regular file
   './a.json'              | a.json                        | regular file
   './.json'               | .json                         | regular file
   './khadas-vim.json'     | .                             | directory
   './tmp/khadas-vim.json' | tmp                           | directory
  -------------------------+-------------------------------+--------------------

   - если есть основное базовое имя файла и расширение,  то расширение
     заменяем на: '.pkgs.json', '.tree.json', '.tree.html';

   - если есть основное базовое имя файла без расширения, то добавляем
     расширение: '.pkgs.json', '.tree.json', '.tree.html';

   - если основное базовое имя файла начинается с точки, то расширение
     заменяем на: 'pkgs.json', 'tree.json', 'tree.html'.
*/
static void allocate_fnames( const char *json_fname )
{
  char *p, *e, *f = NULL;
  char *buf = NULL;

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  (void)sprintf( &buf[0], "%s", json_fname );
  p = rindex( (const char *)&buf[0], '/' );
  if( p )
  {
    if( p != &buf[0] ) f = ++p;
    else               f = &buf[0];
  }
  e = rindex( (const char *)f, '.' );
  if( e )
  {
    if( e != f )
    {
      (void)sprintf( e, ".pkgs.json" ); pkgs_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, ".tree.json" ); tree_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, ".tree.html" ); html_fname = strdup( (const char *)&buf[0] );

      (void)sprintf( e, ".pkgs.min.json" ); pkgs_min_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, ".tree.min.json" ); tree_min_fname = strdup( (const char *)&buf[0] );
    }
    else
    {
      (void)sprintf( e, "pkgs.json" ); pkgs_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, "tree.json" ); tree_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, "tree.html" ); html_fname = strdup( (const char *)&buf[0] );

      (void)sprintf( e, "pkgs.min.json" ); pkgs_min_fname = strdup( (const char *)&buf[0] );
      (void)sprintf( e, "tree.min.json" ); tree_min_fname = strdup( (const char *)&buf[0] );
    }
  }
  else
  {
    e = f + strlen( f );

    (void)sprintf( e, ".pkgs.json" ); pkgs_fname = strdup( (const char *)&buf[0] );
    (void)sprintf( e, ".tree.json" ); tree_fname = strdup( (const char *)&buf[0] );
    (void)sprintf( e, ".tree.html" ); html_fname = strdup( (const char *)&buf[0] );

    (void)sprintf( e, ".pkgs.min.json" ); pkgs_min_fname = strdup( (const char *)&buf[0] );
    (void)sprintf( e, ".tree.min.json" ); tree_min_fname = strdup( (const char *)&buf[0] );
  }

  if( minimize )
  {
    json_pkgs_file = strdup( (const char *)basename( pkgs_min_fname ) );
    json_tree_file = strdup( (const char *)basename( tree_min_fname ) );
  }
  else
  {
    json_pkgs_file = strdup( (const char *)basename( pkgs_fname ) );
    json_tree_file = strdup( (const char *)basename( tree_fname ) );
  }

  free( buf );
}



static struct package * find_package( struct dlist *list, struct pkg *pkg )
{
  struct package *package = NULL;
  struct dlist   *found   = NULL;

  if( !pkg ) return package;

  found = dlist_find_data( list, __compare_provided, (const void *)pkg );
  if( found )
  {
    return (struct package *)found->data;
  }

  return package;
}

void print_package_data( FILE *output, struct package *package )
{
  if( !output || !package ) return;

  /* "id": "net:bind-9.10.1", */
  if( package->pkginfo->group ) {
    fprintf( output, "  \"id\": \"%s:%s-%s\",\n", package->pkginfo->group,
                                                  package->pkginfo->name,
                                                  package->pkginfo->version );
  } else {
    fprintf( output, "  \"id\": \"%s-%s\",\n", package->pkginfo->name,
                                               package->pkginfo->version );
  }
  /* "name": "bind", */
  fprintf( output, "  \"name\": \"%s\",\n", package->pkginfo->name );
  /* "version": "9.10.1", */
  fprintf( output, "  \"version\": \"%s\",\n", package->pkginfo->version );
  /* "group": "net", */
  if( package->pkginfo->group ) {
    fprintf( output, "  \"group\": \"%s\",\n", package->pkginfo->group );
  } else {
    fprintf( output, "  \"group\": \"\",\n" );
  }
  /* "arch": "omap543x-eglibc", */
  fprintf( output, "  \"arch\": \"%s\",\n", package->pkginfo->arch );
  /* "hardware": "omap5uevm", */
  fprintf( output, "  \"hardware\": \"%s\",\n", hardware );
  /* "license": "custom", */
  fprintf( output, "  \"license\": \"%s\",\n", package->pkginfo->license );
  /* "description": "bind 9.10.1 (DNS server and utilities)", */
  fprintf( output, "  \"description\": \"%s %s (%s)\",\n", package->pkginfo->name,
                                                           package->pkginfo->version,
                                                           package->pkginfo->short_description );
  /* "uncompressed_size": "17M", */
  fprintf( output, "  \"uncompressed_size\": \"" );
  if( package->pkginfo->uncompressed_size > 1048576 ) {
    fprintf( output, "%ldG\",\n", package->pkginfo->uncompressed_size / 1048576 );
  } else if( package->pkginfo->uncompressed_size > 1024 ) {
    fprintf( output, "%ldM\",\n", package->pkginfo->uncompressed_size / 1024 );
  } else {
    fprintf( output, "%ldK\",\n", package->pkginfo->uncompressed_size );
  }
  /* "total_files": "421" */
  fprintf( output, "  \"total_files\": \"%d\"\n", package->pkginfo->total_files );
}

static void __print_pkgs_node( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  struct _ctx    *ctx     = (struct _ctx *)user_data;

  if( !package || !ctx ) return;

  if( ctx->index != 0 )
  {
    fprintf( ctx->output, " },\n {\n" );
  }
  print_package_data( ctx->output, package );
  ++ctx->index;
}

static void print_pkgs_json( FILE *output, struct dlist *list )
{
  struct _ctx ctx;

  if( !output ) return;

  bzero( (void *)&ctx, sizeof(struct _ctx) );

  ctx.output = output;
  ctx.index  = 0;

  fprintf( output, "[{\n" );

  dlist_foreach( list, __print_pkgs_node, (void *)&ctx );

  fprintf( output, " }]\n" );
}

static void __remove_required_package( void *data, void *user_data )
{
  struct package *package = NULL;
  struct pkg     *pkg = (struct pkg *)data;

  if( pkg )
  {
    package = find_package( tree, pkg );
    if( package )
    {
      /*******************************************
        if package reqired for some other package
        we have to remove it from tree list:
       */
      tree = dlist_remove( tree, (const void *)package );
    }
  }
}

static void __remove_required_packages( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  struct dlist   *list = NULL;

  if( !package ) return;

  if( !(list = package->requires->list) ) return;

  dlist_foreach( list, __remove_required_package, NULL );
}

static void remove_required_packages( struct dlist *list )
{
  dlist_foreach( list, __remove_required_packages, NULL );
}


static void __check_pkg_requires( void *data, void *user_data )
{
  struct pkg *pkg     = (struct pkg *)data;
  int        *counter = (int *)user_data;

  if( pkg )
  {
    struct package *package = find_package( provides, pkg );
    if( package ) { ++(*counter); }
  }
}

static int check_pkg_requires( struct dlist *list )
{
  int cnt = 0;
  dlist_foreach( list, __check_pkg_requires, (void *)&cnt );
  return cnt;
}


static void print_pkg_tree( struct _ctx *ctx, struct dlist *list )
{
  struct dlist *next = NULL;

  if( !ctx || !list ) return;

  ctx->depth += 2;
  svg_width = max( svg_width, ctx->depth );

  while( list )
  {
    next = dlist_next( list );
    {
      struct pkg     *pkg     = (struct pkg *)list->data;
      struct package *package = find_package( provides, pkg );

      if( package )
      {
        char *p, *buf = NULL;
        int   depth = 0;

        struct dlist *reqs = NULL;

        buf = (char *)malloc( (size_t)PATH_MAX );
        if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
        bzero( (void *)buf, PATH_MAX );

        buf[0] = ' ';
        buf[1] = '\0';

        p = (char *)&buf[1];
        depth = ctx->depth;

        while( depth ) { (void)sprintf( p, " " ); --depth; ++p; *p = '\0'; }

        (void)sprintf( p - 1, "{\n" );
        fprintf( ctx->output, (char *)&buf[0] );
        *(p - 1) = ' '; *p = '\0';

        if( pkg->group )
          (void)sprintf( p, "\"name\": \"%s:%s-%s\"", pkg->group,
                                                      pkg->name,
                                                      pkg->version );
        else
          (void)sprintf( p, "\"name\": \"%s-%s\"", pkg->name,
                                                   pkg->version );

        fprintf( ctx->output, (char *)&buf[0] );

        if( (reqs = package->requires->list) && check_pkg_requires( reqs ) > 0 )
        {
          fprintf( ctx->output, ",\n" );

          (void)sprintf( p, "\"children\": [\n" );
          fprintf( ctx->output, (char *)&buf[0] );

          print_pkg_tree( ctx, reqs );

          (void)sprintf( p, "]\n" );
          fprintf( ctx->output, (char *)&buf[0] );
        }
        else
        {
          fprintf( ctx->output, "\n" );
        }

        (void)sprintf( p - 1, "}" );
        fprintf( ctx->output, (char *)&buf[0] );
        *(p - 1) = ' '; *p = '\0';

        if( next ) { fprintf( ctx->output, ",\n" ); }
        else       { fprintf( ctx->output, "\n" );  }

        free( buf );
      } /* End if( package )  */
    }
    list = next;
  } /* End of while( list ) */

  ctx->depth -= 2;
}

static void print_package_node( struct _ctx *ctx, struct package *package )
{
  char *p, *buf = NULL;
  int   depth = 0;

  struct dlist *list = NULL;

  if( !package || !ctx ) return;

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  buf[0] = ' ';
  buf[1] = '\0';

  p = (char *)&buf[1];
  depth = ctx->depth;

  while( depth ) { (void)sprintf( p, " " ); --depth; ++p; *p = '\0'; }

  (void)sprintf( p - 1, "{\n" );
  fprintf( ctx->output, (char *)&buf[0] );
  *(p - 1) = ' '; *p = '\0';

  if( package->pkginfo->group )
    (void)sprintf( p, "\"name\": \"%s:%s-%s\"", package->pkginfo->group,
                                                package->pkginfo->name,
                                                package->pkginfo->version );
  else
    (void)sprintf( p, "\"name\": \"%s-%s\"", package->pkginfo->name,
                                             package->pkginfo->version );

  fprintf( ctx->output, (char *)&buf[0] );

  if( (list = package->requires->list) && check_pkg_requires( list ) > 0 )
  {
    fprintf( ctx->output, ",\n" );

    (void)sprintf( p, "\"children\": [\n" );
    fprintf( ctx->output, (char *)&buf[0] );

    print_pkg_tree( ctx, list );

    (void)sprintf( p, "]\n" );
    fprintf( ctx->output, (char *)&buf[0] );
  }
  else
  {
    fprintf( ctx->output, "\n" );
  }

  (void)sprintf( p - 1, "}" );
  fprintf( ctx->output, (char *)&buf[0] );
  *(p - 1) = ' '; *p = '\0';

  free( buf );
}

static void __print_tree_node( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  struct _ctx    *ctx     = (struct _ctx *)user_data;

  if( !package || !ctx ) return;

  if( ctx->size > 1 )
  {
    if( ctx->index == 0 )
    {
      char *buf = NULL;

      buf = (char *)malloc( (size_t)PATH_MAX );
      if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)buf, PATH_MAX );

      (void)sprintf( &buf[0], "%s", hardware );
      root = strdup( (const char *)&buf[0] );
      (void)sprintf( &buf[0], "%s", package->pkginfo->url );
      bug_url = strdup( (const char *)&buf[0] );
      free( buf );

      fprintf( ctx->output, " \"distro\": [ \"%s\", \"%s\", \"%s\" ],\n",
                                              package->pkginfo->distro_name,
                                                      package->pkginfo->distro_version,
                                                               package->pkginfo->url );
      fprintf( ctx->output, " \"name\": \"%s\",\n", hardware );
      fprintf( ctx->output, " \"children\": [\n" );
    }


    print_package_node( ctx, package );
    svg_height += 2;


    if( ctx->index < ctx->size - 1 ) fprintf( ctx->output, "," );
    else                             fprintf( ctx->output, "\n ]" );

    fprintf( ctx->output, "\n" );
  }
  else
  {
    struct dlist *reqs = NULL;
    char *buf = NULL;

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    if( package->pkginfo->group )
      (void)sprintf( &buf[0], "%s/%s-%s", package->pkginfo->group,
                                          package->pkginfo->name,
                                          package->pkginfo->version );
    else
      (void)sprintf( &buf[0], "%s-%s", package->pkginfo->name,
                                       package->pkginfo->version );

    root = strdup( (const char *)&buf[0] );
    (void)sprintf( &buf[0], "%s", package->pkginfo->url );
    bug_url = strdup( (const char *)&buf[0] );
    free( buf );

    fprintf( ctx->output, " \"distro\": [ \"%s\", \"%s\", \"%s\" ],\n",
                                            package->pkginfo->distro_name,
                                                    package->pkginfo->distro_version,
                                                             package->pkginfo->url );
    if( package->pkginfo->group )
      fprintf( ctx->output, " \"name\": \"%s:%s-%s\"", package->pkginfo->group,
                                                       package->pkginfo->name,
                                                       package->pkginfo->version );
    else
      fprintf( ctx->output, " \"name\": \"%s-%s\"", package->pkginfo->name,
                                                    package->pkginfo->version );


    svg_height += 2;
    ctx->depth  -=2;

    if( (reqs = package->requires->list) && check_pkg_requires( reqs ) > 0 )
    {
      fprintf( ctx->output, ",\n" );

      fprintf( ctx->output, " \"children\": [\n" );

      print_pkg_tree( ctx, reqs );

      fprintf( ctx->output, " ]\n" );
    }

  }

  ++ctx->index;
}

static void print_tree_json( FILE *output, struct dlist *list )
{
  struct _ctx ctx;

  if( !output || !list ) return;

  bzero( (void *)&ctx, sizeof(struct _ctx) );

  ctx.output = output;
  ctx.index  = 0;
  ctx.size   = dlist_length( list );
  ctx.depth  = 2;

  fprintf( output, "{\n" );
  dlist_foreach( list, __print_tree_node, (void *)&ctx );
  fprintf( output, "}\n" );

  svg_height += svg_width / 2;

  svg_width  = (svg_width  + 4) * 160;
  svg_height = (svg_height + 4) * 24;
}

#include <pkglist.html.c>

void print_provides_tree( const char *json_fname )
{
  FILE *pkgs_fp = NULL, *tree_fp = NULL, *html_fp = NULL;

  allocate_fnames( json_fname );

  pkgs_fp = fopen( (const char *)pkgs_fname, "w" );
  if( !pkgs_fp ) { FATAL_ERROR( "Cannot create %s file", basename( pkgs_fname ) ); }
  tree_fp = fopen( (const char *)tree_fname, "w" );
  if( !tree_fp ) { FATAL_ERROR( "Cannot create %s file", basename( tree_fname ) ); }
  html_fp = fopen( (const char *)html_fname, "w" );
  if( !html_fp ) { FATAL_ERROR( "Cannot create %s file", basename( html_fname ) ); }

  tree = dlist_copy( provides );

  /*****************************************************
    print out the array of all packages in JSON format:
   */
  print_pkgs_json( pkgs_fp, provides );
  fflush( pkgs_fp ); fclose( pkgs_fp );

  provides = dlist_reverse( provides );

  /********************************************************
    remove unneded packages from tree list to to leave the
    last installation layer of packages presented in DAG:
   */
  remove_required_packages( provides );

  /***********************************************
    print out the REQIIRES TREE in JSON format
    starting from last installation layer of DAG:
   */
  print_tree_json( tree_fp, tree );
  fflush( tree_fp ); fclose( tree_fp );

  if( minimize )
  {
    if( minimize_json( (const char *)pkgs_fname, (const char *)pkgs_min_fname ) < 1 )
    {
      (void)unlink( (const char *)pkgs_min_fname );
    }
    if( minimize_json( (const char *)tree_fname, (const char *)tree_min_fname ) < 1 )
    {
      (void)unlink( (const char *)tree_min_fname );
    }
  }


  /***********************************************
    print out the HTML to view REQIIRES TREE:
   */
  print_tree_html( html_fp );
  fflush( html_fp ); fclose( html_fp );


  /*****************
    free resources:
   */
  if( root )    { free( root );       root = NULL; }
  if( bug_url ) { free( bug_url ); bug_url = NULL; }

  if( pkgs_fname ) { free( pkgs_fname ); pkgs_fname = NULL; }
  if( tree_fname ) { free( tree_fname ); tree_fname = NULL; }
  if( html_fname ) { free( html_fname ); html_fname = NULL; }

  if( pkgs_min_fname ) { free( pkgs_min_fname ); pkgs_min_fname = NULL; }
  if( tree_min_fname ) { free( tree_min_fname ); tree_min_fname = NULL; }

  if( json_pkgs_file ) { free( json_pkgs_file ); json_pkgs_file = NULL; }
  if( json_tree_file ) { free( json_tree_file ); json_tree_file = NULL; }

  __dlist_free( tree ); /* do not free node data */
}
/*
  End of Requires TREE functions.
 ***************************************************************/
