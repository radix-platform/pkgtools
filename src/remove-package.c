
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
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h> /* chmod(2)    */
#include <sys/file.h> /* flock(2)    */
#include <fcntl.h>
#include <limits.h>
#include <string.h>   /* strdup(3)   */
#include <strings.h>  /* index(3)    */
#include <libgen.h>   /* basename(3) */
#include <ctype.h>    /* tolower(3)  */
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <unistd.h>

#include <math.h>

#include <sys/wait.h>

#include <sys/resource.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <config.h>

#include <msglog.h>
#include <system.h>

#include <cmpvers.h>
#include <dlist.h>

#if defined( HAVE_DIALOG )
#include <dialog-ui.h>
#endif

#define PROGRAM_NAME "remove-package"

#include <defs.h>


char *program = PROGRAM_NAME;
char *root = NULL, *pkgs_path = NULL, *rempkgs_path = NULL,
     *pkg_fname = NULL, *pkglog_fname = NULL,
     *tmpdir = NULL, *rtmpdir = NULL, *curdir = NULL, *log_fname = NULL;

int   quiet = 0, ignore_chrefs_errors = 0;
char *description = NULL;

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

static char           *pkgname = NULL,
                       *pkgver = NULL,
                         *arch = NULL,
                   *distroname = NULL,
                    *distrover = NULL,
                        *group = NULL,
            *short_description = NULL,
                          *url = NULL,
                      *license = NULL,
            *uncompressed_size = NULL,
              *compressed_size = NULL,
                  *total_files = NULL;

static char *requested_version = NULL;

enum _remove_mode {
  CONSOLE = 0,
  INFODIALOG,
  MENUDIALOG
} remove_mode = CONSOLE;

enum _input_type {
  IFMT_PKG = 0,
  IFMT_LOG,

  IFMT_UNKNOWN
} input_format = IFMT_PKG;

char  uncompress = '\0';

static struct dlist *dirs  = NULL;
static struct dlist *files = NULL;
static struct dlist *links = NULL;

static void free_list( struct dlist *list );


#define FREE_PKGINFO_VARIABLES() \
  if( pkgname )           { free( pkgname );           } pkgname = NULL;            \
  if( pkgver )            { free( pkgver );            } pkgver = NULL;             \
  if( arch )              { free( arch );              } arch = NULL;               \
  if( distroname )        { free( distroname );        } distroname = NULL;         \
  if( distrover )         { free( distrover );         } distrover = NULL;          \
  if( group )             { free( group );             } group = NULL;              \
  if( short_description ) { free( short_description ); } short_description = NULL;  \
  if( description )       { free( description );       } description = NULL;        \
  if( url )               { free( url );               } url = NULL;                \
  if( license )           { free( license );           } license = NULL;            \
  if( uncompressed_size ) { free( uncompressed_size ); } uncompressed_size = NULL;  \
  if( compressed_size )   { free( compressed_size );   } compressed_size = NULL;    \
  if( total_files )       { free( total_files );       } total_files = NULL;        \
  if( requested_version ) { free( requested_version ); } requested_version = NULL

void free_resources()
{
  if( root )          { free( root );          root          = NULL; }
  if( pkgs_path )     { free( pkgs_path );     pkgs_path     = NULL; }
  if( rempkgs_path )  { free( rempkgs_path );  rempkgs_path  = NULL; }
  if( pkg_fname )     { free( pkg_fname );     pkg_fname     = NULL; }

  if( dirs )          { free_list( dirs );     dirs          = NULL; }
  if( files )         { free_list( files );    files         = NULL; }
  if( links )         { free_list( links );    links         = NULL; }

  if( rtmpdir )       { free( rtmpdir );       rtmpdir       = NULL; }
  if( curdir )        { free( curdir );        curdir        = NULL; }
  if( log_fname )     { free( log_fname );     log_fname     = NULL; }

  if( selfdir )       { free( selfdir );       selfdir       = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <package|pkglog|pkgname>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Remove installed package.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  --ignore-chrefs-errors        Ignore change references errors (code: 48).\n" );
#if defined( HAVE_DIALOG )
  fprintf( stdout, "  -i,--info-dialog              Show package description during remove\n" );
  fprintf( stdout, "                                process using ncurses dialog.\n" );
  fprintf( stdout, "  -m,--menu-dialog              Ask for confirmation the removal.\n" );
#endif
  fprintf( stdout, "  -q,--quiet                    Do not display results. This option\n" );
  fprintf( stdout, "                                works unless options -i, -m\n" );
  fprintf( stdout, "                                are enabled.\n" );
  fprintf( stdout, "  -r,--root=<DIR>               Target rootfs path.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <package|pkglog|pkgname>      The PKGNAME, PACKAGE tarball or PKGLOG.\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Return codes:\n" );
  fprintf( stdout, "  ------+-------------------------------------------------------\n" );
  fprintf( stdout, "   code | status\n"  );
  fprintf( stdout, "  ------+-------------------------------------------------------\n" );
  fprintf( stdout, "     30 | package is not installed\n" );
  fprintf( stdout, "    ----+----\n" );
  fprintf( stdout, "     47 | cannot backup PKGLOG file in the Setup Database\n" );
  fprintf( stdout, "     43 | pre-remove script returned error status\n" );
  fprintf( stdout, "     46 | post-remove script returned error status\n" );
  fprintf( stdout, "     48 | references cannot be updated in Setup Database\n" );
  fprintf( stdout, "  ------+-------------------------------------------------------\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Upon successful completion zero is returned. Other non-zero return\n" );
  fprintf( stdout, "codes imply incorrect completion of the deinstallation.\n" );
  fprintf( stdout, "\n" );

  exit( EXIT_FAILURE );
}

void to_lowercase( char *s )
{
  char *p = s;
  while( p && *p ) { int c = *p; *p = tolower( c ); ++p; }
}

void to_uppercase( char *s )
{
  char *p = s;
  while( p && *p ) { int c = *p; *p = toupper( c ); ++p; }
}

void version()
{
  char *upper = NULL;

  upper = (char *)alloca( strlen( program ) + 1 );

  strcpy( (char *)upper, (const char *)program );
  to_uppercase( upper );

  fprintf( stdout, "%s (%s) %s\n", program, upper, PROGRAM_VERSION );

  fprintf( stdout, "Copyright (C) 2019 Andrey V.Kosteltsev.\n" );
  fprintf( stdout, "This is free software.   There is NO warranty; not even\n" );
  fprintf( stdout, "for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n" );
  fprintf( stdout, "\n" );

  free_resources();
  exit( EXIT_SUCCESS );
}


static void remove_trailing_slash( char *dir )
{
  char *s;

  if( !dir || dir[0] == '\0' ) return;

  s = dir + strlen( dir ) - 1;
  while( *s == '/' )
  {
    *s = '\0'; --s;
  }
}


static int _mkdir_p( const char *dir, const mode_t mode )
{
  char  *buf;
  char  *p = NULL;
  struct stat sb;

  if( !dir ) return -1;

  buf = (char *)alloca( strlen( dir ) + 1 );
  strcpy( buf, dir );

  remove_trailing_slash( buf );

  /* check if path exists and is a directory */
  if( stat( buf, &sb ) == 0 )
  {
    if( S_ISDIR(sb.st_mode) )
    {
      return 0;
    }
  }

  /* mkdir -p */
  for( p = buf + 1; *p; ++p )
  {
    if( *p == '/' )
    {
      *p = 0;
      /* test path */
      if( stat( buf, &sb ) != 0 )
      {
        /* path does not exist - create directory */
        if( mkdir( buf, mode ) < 0 )
        {
          return -1;
        }
      } else if( !S_ISDIR(sb.st_mode) )
      {
        /* not a directory */
        return -1;
      }
      *p = '/';
    }
  }

  /* test path */
  if( stat( buf, &sb ) != 0 )
  {
    /* path does not exist - create directory */
    if( mkdir( buf, mode ) < 0 )
    {
      return -1;
    }
  } else if( !S_ISDIR(sb.st_mode) )
  {
    /* not a directory */
    return -1;
  }

  return 0;
}

static void _rm_tmpdir( const char *dirpath )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

  if( stat( dirpath, &path_sb ) == -1 )
  {
    return; /* stat returns error code; errno is set */
  }

  if( S_ISDIR(path_sb.st_mode) == 0 )
  {
    return; /* dirpath is not a directory */
  }

  if( (dir = opendir(dirpath) ) == NULL )
  {
    return; /* Cannot open direcroty; errno is set */
  }

  len = strlen( dirpath );

  while( (entry = readdir( dir )) != NULL)
  {

    /* skip entries '.' and '..' */
    if( ! strcmp( entry->d_name, "." ) || ! strcmp( entry->d_name, ".." ) ) continue;

    /* determinate a full name of an entry */
    path = alloca( len + strlen( entry->d_name ) + 2 );
    strcpy( path, dirpath );
    strcat( path, "/" );
    strcat( path, entry->d_name );

    if( stat( path, &entry_sb ) == 0 )
    {
      if( S_ISDIR(entry_sb.st_mode) )
      {
        /* recursively remove a nested directory */
        _rm_tmpdir( path );
      }
      else
      {
        /* remove a file object */
        (void)unlink( path );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */

  }

  /* remove the devastated directory and close the object of this directory */
  (void)rmdir( dirpath );

  closedir( dir );
}

static char *_mk_tmpdir( void )
{
  char   *buf = NULL, *p, *tmp = "/tmp";
  size_t  len = 0, size = 0;

  (void)umask( S_IWGRP | S_IWOTH ); /* octal 022 */

  /* Get preferred directory for tmp files */
  if( (p = getenv( "TMP" )) != NULL ) {
    tmp = p;
  }
  else if( (p = getenv( "TEMP" )) != NULL ) {
    tmp = p;
  }

  size = strlen( tmp ) + strlen( DISTRO_NAME ) + strlen( program ) + 12;

  buf = (char *)malloc( size );
  if( !buf ) return NULL;

  len = snprintf( buf, size, (const char *)"%s/%s/%s-%.7u", tmp, DISTRO_NAME, program, getpid() );
  if( len == 0 || len == size - 1 )
  {
    free( buf ); return NULL;
  }

  _rm_tmpdir( (const char *)&buf[0] );

  if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) == 0 )
  {
    return buf;
  }

  free( buf ); return NULL;
}


void fatal_error_actions( void )
{
  logmsg( errlog, MSG_NOTICE, "Free resources on FATAL error..." );
  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();
}

void sigint( int signum )
{
  (void)signum;

  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();
}

static void set_signal_handlers()
{
  struct sigaction  sa;
  sigset_t          set;

  memset( &sa, 0, sizeof( sa ) );
  sa.sa_handler = sigint;          /* TERM, INT */
  sa.sa_flags = SA_RESTART;
  sigemptyset( &set );
  sigaddset( &set, SIGTERM );
  sigaddset( &set, SIGINT );
  sa.sa_mask = set;
  sigaction( SIGTERM, &sa, NULL );
  sigaction( SIGINT, &sa,  NULL );

  memset( &sa, 0, sizeof( sa ) );  /* ignore SIGPIPE */
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction( SIGPIPE, &sa, NULL );

  /* System V fork+wait does not work if SIGCHLD is ignored */
  signal( SIGCHLD, SIG_DFL );
}


static enum _input_type check_input_file( char *uncompress, const char *fname )
{
  struct stat st;
  size_t pkglog_size = 0;
  unsigned char buf[8];
  int rc, fd;

  /* SIGNATURES: https://www.garykessler.net/library/file_sigs.html */

  if( uncompress )
  {
    *uncompress = '\0';
  }

  if( stat( fname, &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access %s file: %s", basename( (char *)fname ), strerror( errno ) );
  }

  pkglog_size = st.st_size;

  if( (fd = open( fname, O_RDONLY )) == -1 )
  {
    FATAL_ERROR( "Cannot open %s file: %s", basename( (char *)fname ), strerror( errno ) );
  }

  rc = (int)read( fd, (void *)&buf[0], 7 );
  if( rc != 7 )
  {
    close( fd ); return IFMT_UNKNOWN;
  }
  buf[7] = '\0';

  /* TEXT */
  if( !strncmp( (const char *)&buf[0], "PACKAGE", 7 ) )
  {
    close( fd ); return IFMT_LOG;
  }

  /* GZ */
  if( buf[0] == 0x1F && buf[1] == 0x8B && buf[2] == 0x08 )
  {
    if( uncompress ) { *uncompress = 'x'; }
    close( fd ); return IFMT_PKG;
  }

  /* BZ2 */
  if( buf[0] == 0x42 && buf[1] == 0x5A && buf[2] == 0x68 )
  {
    if( uncompress ) { *uncompress = 'j'; }
    close( fd ); return IFMT_PKG;
  }

  /* XZ */
  if( buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A &&
      buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00   )
  {
    if( uncompress ) { *uncompress = 'J'; }
    close( fd ); return IFMT_PKG;
  }

  if( pkglog_size > 262 )
  {
    if( lseek( fd, 257, SEEK_SET ) == -1 )
    {
      FATAL_ERROR( "Cannot check signature of %s file: %s", basename( (char *)fname ), strerror( errno ) );
    }
    rc = (int)read( fd, &buf[0], 5 );
    if( rc != 5 )
    {
      FATAL_ERROR( "Cannot read signature of %s file", basename( (char *)fname ) );
    }
    /* TAR */
    if( buf[0] == 0x75 && buf[1] == 0x73 && buf[2] == 0x74 && buf[3] == 0x61 && buf[4] == 0x72 )
    {
      close( fd ); return IFMT_PKG;
    }
  }

  close( fd ); return IFMT_UNKNOWN;
}


void get_args( int argc, char *argv[] )
{
#if defined( HAVE_DIALOG )
  const char* short_options = "hvimqr:";
#else
  const char* short_options = "hvqr:";
#endif

#define IGNORE_CHREFS_ERRORS 872

  const struct option long_options[] =
  {
    { "help",                 no_argument,       NULL, 'h' },
    { "version",              no_argument,       NULL, 'v' },
    { "ignore-chrefs-errors", no_argument,       NULL, IGNORE_CHREFS_ERRORS },
#if defined( HAVE_DIALOG )
    { "info-dialog",          no_argument,       NULL, 'i' },
    { "menu-dialog",          no_argument,       NULL, 'm' },
#endif
    { "quiet",                no_argument,       NULL, 'q' },
    { "root",                 required_argument, NULL, 'r' },
    { NULL,                   0,                 NULL,  0  }
  };

  int ret;
  int option_index = 0;

  while( (ret = getopt_long( argc, argv, short_options, long_options, &option_index )) != -1 )
  {
    switch( ret )
    {
      case 'h':
      {
        usage();
        break;
      }
      case 'v':
      {
        version();
        break;
      }

#if defined( HAVE_DIALOG )
      case 'i':
      {
        remove_mode = INFODIALOG;
        break;
      }
      case 'm':
      {
        remove_mode = MENUDIALOG;
        break;
      }
#endif
      case 'q':
      {
        quiet = 1;
        break;
      }

      case IGNORE_CHREFS_ERRORS:
      {
        ignore_chrefs_errors = 1;
        break;
      }

      case 'r':
      {
        if( optarg != NULL )
        {
          char cwd[PATH_MAX];

          bzero( (void *)cwd, PATH_MAX );
          if( optarg[0] != '/' && curdir )
          {
            /* skip current directory definition './' at start of argument: */
            if( !strncmp( optarg, "./", 2 ) && strncmp( optarg, "..", 2 ) )
              (void)sprintf( cwd, "%s/%s", curdir, optarg + 2 );
            else if( (strlen( optarg ) == 1) && !strncmp( optarg, ".", 1 ) )
              (void)sprintf( cwd, "%s", curdir );
            else
              (void)sprintf( cwd, "%s/%s", curdir, optarg );
            root = strdup( (const char *)cwd );
          }
          else
          {
            root = strdup( optarg );
          }
          remove_trailing_slash( root );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }

      case '?': default:
      {
        usage();
        break;
      }
    }
  }


  if( optind < argc )
  {
    struct stat st;
    char  *buf = NULL;

    bzero( (void *)&st, sizeof( struct stat ) );

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    /* absolute path to input package: */
    if( argv[optind][0] != '/' && curdir )
      (void)sprintf( buf, "%s/%s", curdir, (const char *)argv[optind] );
    else
      (void)strcpy( buf, (const char *)argv[optind] );

    pkg_fname = strdup( (const char *)&buf[0] );
    free( buf );
  }
  else
  {
    usage();
  }


  if( !pkgs_path )
  {
    struct stat st;
    char  *buf = NULL;
    int    len;

    bzero( (void *)&st, sizeof( struct stat ) );

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    if( !root )
    {
      buf[0] = '/'; buf[1] = '\0';
      root = strdup( (const char *)buf );
    }
    else
    {
      len = strlen( root );

      (void)strcpy( buf, (const char *)root );
      if( buf[ len - 1 ] != '/' )
      {
        buf[len] = '/'; buf[len+1] = '\0';
        free( root ); root = strdup( (const char *)buf );
      }
    }

    if( stat( (const char *)&buf[0], &st ) == -1 )
    {
      FATAL_ERROR( "Cannot access '%s' file or directory: %s", buf, strerror( errno ) );
    }
    if( !S_ISDIR(st.st_mode) )
    {
      FATAL_ERROR( "Defined --root '%s' is not a directory", buf );
    }

    len = strlen( (const char *)buf );

    (void)strcat( buf, PACKAGES_PATH );
    if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot access '/%s' directory", PACKAGES_PATH );
    }
    pkgs_path = strdup( (const char *)&buf[0] );

    /*********************************************
      Create other directories of Setup Database:
     */
    buf[len] = '\0';
    (void)strcat( buf, REMOVED_PKGS_PATH );
    if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot access '/%s' directory", REMOVED_PKGS_PATH );
    }
    rempkgs_path = strdup( (const char *)&buf[0] );

    buf[len] = '\0';
    (void)strcat( buf, SETUP_PATH );
    if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot access '/%s' directory", SETUP_PATH );
    }

    /*********************************************
      Allocate memory for Setup LOG File name:
     */
    buf[len] = '\0';
    (void)strcat( buf, LOG_PATH );
    (void)strcat( buf, SETUP_LOG_FILE );
    log_fname = strdup( (const char *)&buf[0] );

    free( buf );

  } /* End if( !pkgs_path ) */
}

static void setup_log( char *format, ... )
{
  FILE *fp = NULL;

  time_t     t = time( NULL );
  struct tm tm = *localtime(&t);

  va_list argp;

  if( ! format ) return;

  fp = fopen( (const char *)log_fname, "a" );
  if( !fp )
  {
    FATAL_ERROR( "Cannot open /%s%s file", LOG_PATH, SETUP_LOG_FILE );
  }

  fprintf( fp, "[%04d-%02d-%02d %02d:%02d:%02d]: ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                 tm.tm_hour, tm.tm_min, tm.tm_sec );

  va_start( argp, format );
  vfprintf( fp, format, argp );
  fprintf( fp, "\n" );

  fflush( fp );
  fclose( fp );
}

/***********************************************************
  Remove leading spaces and take non-space characters only:
  (Especialy for pkginfo lines)
 */
static char *skip_spaces( char *s )
{
  char *q, *p = (char *)0;

  if( !s || *s == '\0' ) return p;

  p = s;

  while( (*p == ' ' || *p == '\t') && *p != '\0' ) { ++p; } q = p;
  while(  *q != ' ' && *q != '\t'  && *q != '\0' ) { ++q; } *q = '\0';

  if( *p == '\0' ) return (char *)0;

  return( strdup( p ) );
}


/*******************************
  remove spaces at end of line:
 */
static void skip_eol_spaces( char *s )
{
  char *p = (char *)0;

  if( !s || *s == '\0' ) return;

  p = s + strlen( s ) - 1;
  while( isspace( *p ) ) { *p-- = '\0'; }
}



/********************************************************
  Read .FILELIST and .RESTORELINKS functions:
 */
static int __cmp_list_items( const void *a, const void *b )
{
  if( a && b )
    return strcmp( (const char *)a, (const char *)b );
  else if( a )
    return 1;
  else
    return -1;
}

static void __free_list( void *data, void *user_data )
{
  if( data ) { free( data ); }
}

static void free_list( struct dlist *list )
{
  if( list ) { dlist_free( list, __free_list ); }
}

////////////////////////////////////////////////////
//static void __print_list( void *data, void *user_data )
//{
//  int *counter = (int *)user_data;
//
//  if( counter ) { fprintf( stdout, "item[%.5d]: %s\n", *counter, (char *)data ); ++(*counter); }
//  else          { fprintf( stdout, "item: %s\n", (char *)data ); }
//}
//
//static void print_list( struct dlist *list )
//{
//  int cnt = 0;
//  if( list ) { dlist_foreach( list, __print_list, (void *)&cnt ); }
//}
////////////////////////////////////////////////////

static void read_filelist( void )
{
  struct stat st;
  FILE  *fp = NULL;

  char *ln   = NULL;
  char *line = NULL, *tmp = NULL;

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( &tmp[0], "%s/.FILELIST", rtmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&tmp[0], &st ) == -1) )
  {
    FATAL_ERROR( "Cannot get .FILELIST from '%s' file", basename( (char *)pkglog_fname ) );
  }

  fp = fopen( (const char *)&tmp[0], "r" );
  if( !fp )
  {
    FATAL_ERROR( "Cannot open .FILELIST file" );
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  while( (ln = fgets( line, PATH_MAX, fp )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( *(ln + strlen(ln) - 1) == '/' )
    {
      *(ln + strlen(ln) - 1) = '\0';
      (void)sprintf( &tmp[0], "%s%s", (const char *)root, (const char *)ln );
      dirs = dlist_append( dirs, strdup( (const char *)&tmp[0] ) );
    }
    else
    {
      (void)sprintf( &tmp[0], "%s%s", (const char *)root, (const char *)ln );
      files = dlist_append( files, strdup( (const char *)&tmp[0] ) );
    }

  } /* End of while( file list entry ) */

  fclose( fp );

  free( line );
  free( tmp );
}

static void read_restorelinks( void )
{
  struct stat st;
  FILE  *fp = NULL;

  char *ln   = NULL;
  char *line = NULL, *tmp = NULL;

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( &tmp[0], "%s/.RESTORELINKS", rtmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&tmp[0], &st ) == -1) || (st.st_size < 8) )
  {
    free( tmp );
    return;
  }

  fp = fopen( (const char *)&tmp[0], "r" );
  if( !fp )
  {
    FATAL_ERROR( "Cannot open .RESTORELINKS file" );
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  while( (ln = fgets( line, PATH_MAX, fp )) )
  {
    char *match = NULL;

    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( (match = strstr( ln, "; rm -rf " )) )
    {
      char *q = NULL;
      char *p = strstr( ln, "cd" ) + 2;
      char *f = strstr( ln, "; rm -rf" ) + 8;

      if( !p || !f ) continue;

      while( (*p == ' ' || *p == '\t') && *p != '\0' ) ++p;
      while( (*f == ' ' || *f == '\t') && *f != '\0' ) ++f;

      q = p; while( *q != ' ' && *q != '\t' && *q != ';' && *q != '\0' ) ++q; *q = '\0';
      q = f; while( *q != ' ' && *q != '\t' && *q != ';' && *q != '\0' ) ++q; *q = '\0';

      if( p && f )
      {
        (void)sprintf( &tmp[0], "%s%s/%s", (const char *)root, p, f );
        links = dlist_append( links, strdup( (const char *)&tmp[0] ) );
      }
    }
  } /* End of while( restore links entry ) */

  fclose( fp );

  free( line );
  free( tmp );
}
/*
  End of read .FILELIST and .RESTORELINKS functions.
 ********************************************************/

static void read_description( void )
{
  struct stat st;
  FILE  *fp = NULL;

  char  *buf = NULL, *tmp = NULL;
  char  *lp  = NULL;
  int    n   = 0;

  char *ln   = NULL;
  char *line = NULL;

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  (void)sprintf( &tmp[0], "%s/.DESCRIPTION", rtmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&tmp[0], &st ) == -1) || (st.st_size < 8) )
  {
    free( tmp );
    return;
  }

  fp = fopen( (const char *)&tmp[0], "r" );
  if( !fp )
  {
    FATAL_ERROR( "Cannot open .DESCRIPTION file" );
  }

  (void)sprintf( (char *)&buf[0], "%s:", pkgname );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  lp = (char *)&tmp[0];
  bzero( (void *)tmp, PATH_MAX );
  (void)sprintf( (char *)&tmp[0], "\n" );
  ++lp;

  while( (ln = fgets( line, PATH_MAX, fp )) && n < DESCRIPTION_NUMBER_OF_LINES )
  {
    char *match = NULL;

    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol */

    if( (match = strstr( (const char *)ln, (const char *)buf )) && match == ln ) /* at start of line only */
    {
      int mlen   = strlen( match ), plen = strlen( buf );
      int length = ( mlen > plen )  ? (mlen - plen - 1) : 0 ;

      if( length > DESCRIPTION_LENGTH_OF_LINE )
      {
        /* WARNING( "Package DESCRIPTION contains lines with length greater than %d characters", DESCRIPTION_LENGTH_OF_LINE ); */
        match[plen + 1 + DESCRIPTION_LENGTH_OF_LINE] = '\0'; /* truncating description line  */
        skip_eol_spaces( match );                            /* remove spaces at end-of-line */
      }

      match += plen + 1;
      if( match[0] != '\0' ) { (void)sprintf( lp, " %s\n", match ); lp += strlen( match ) + 2; }
      else                   { (void)sprintf( lp, "\n" ); ++lp; }
      ++n;
    }
  } /* End of while( ln = fgets() ) */

  fclose( fp );

  (void)sprintf( lp, " Uncompressed Size: %s\n", uncompressed_size );
  lp += strlen( uncompressed_size ) + 21;

  description = strdup( (const char *)&tmp[0] );

  free( buf );
  free( line );
  free( tmp );
}

static void pre_remove_routine( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cd %s && %s/.INSTALL pre_remove %s > /dev/null 2>&1",
                  root, tmpdir, pkgver );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot run pre-remove script for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    exit_status = 43;

    if( remove_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                    "\n\\Z1Pre-remove script returned error status.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nPre-remove script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nPre-remove script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

/********************************************************
  Removal functions:
 */
static void __remove_link( void *data, void *user_data )
{
  const char *fname = (const char *)data;

  if( fname )
  {
    (void)unlink( fname );
  }
}

static void __remove_file( void *data, void *user_data )
{
  const char *fname = (const char *)data;

  if( fname )
  {
    char *p = rindex( fname, '.' );
    /*
      Если .new файл остался с тем же именем, это значит что до инсталляции
      в системе существовал такой же файл но без расширения .new и при этом
      он отличался от нового. В данном случае надо удалять только файл .new.

      Если же файл .new не существует, то надо удалять такой же файл но без
      расширения .new .
     */
    if( p && !strncmp( (const char *)p, ".new", 4 ) )
    {
      struct stat st;

      bzero( (void *)&st, sizeof( struct stat ) );
      if( (stat( fname, &st ) == -1) ) *p = '\0';
    }

    (void)unlink( fname );
  }
}

static int is_dir_empty( const char *dirpath )
{
  int ret = 0;

  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

  if( stat( dirpath, &path_sb ) == -1 )   return ret; /* stat returns error code; errno is set */
  if( S_ISDIR(path_sb.st_mode) == 0 )     return ret; /* dirpath is not a directory            */
  if( (dir = opendir(dirpath) ) == NULL ) return ret; /* Cannot open direcroty; errno is set   */

  ret = 1;

  len = strlen( dirpath );
  while( (entry = readdir( dir )) != NULL)
  {
    /* skip entries '.' and '..' */
    if( ! strcmp( entry->d_name, "." ) || ! strcmp( entry->d_name, ".." ) ) continue;

    /* determinate a full name of an entry */
    path = alloca( len + strlen( entry->d_name ) + 2 );
    strcpy( path, dirpath );
    strcat( path, "/" );
    strcat( path, entry->d_name );

    if( stat( path, &entry_sb ) == 0 )
    {
      ret = 0;
      break;
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }
  closedir( dir );

  return ret;
}

static void __remove_dir( void *data, void *user_data )
{
  const char *dname = (const char *)data;

  if( dname && is_dir_empty( (const char *)dname ) )
  {
    (void)rmdir( dname );
  }
}

static void remove_package( void )
{
  /* Try to change CWD to the ROOT directory: */
  (void)chdir( (const char *)root );

  if( links ) { dlist_foreach( links, __remove_link, NULL ); }

  if( files ) { dlist_foreach( files, __remove_file, NULL ); }

  if( dirs )
  {
    dirs = dlist_sort( dirs, __cmp_list_items );
    dirs = dlist_reverse( dirs );
    dlist_foreach( dirs, __remove_dir, NULL );
  }

  /* Try to change CWD to the CURRENT directory: */
  (void)chdir( (const char *)curdir );
}
/*
  End of removal functions.
 ********************************************************/

static void post_remove_routine( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cd %s && %s/.INSTALL post_remove %s > /dev/null 2>&1",
                  root, tmpdir, pkgver );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot run post-remove script for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    exit_status = 46;

    if( remove_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                    "\n\\Z1Post-remove script returned error status.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nPost-remove script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nPost-remove script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

static void finalize_removal( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL, *tmp = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  /*********************************************
    Decrement references in the Setup Database:
   */
  if( group )
    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/chrefs --operation=dec --destination=%s %s/%s > /dev/null 2>&1",
                    selfdir, pkgs_path, group, basename( (char *)pkglog_fname ) );
  else
    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/chrefs --operation=dec --destination=%s %s > /dev/null 2>&1",
                    selfdir, pkgs_path, basename( (char *)pkglog_fname ) );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot decrement '%s-%s' package references", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( (rc != 0) && !ignore_chrefs_errors )
  {
    free( cmd );
    free( tmp );

    exit_status = 48;

    if( remove_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                    "\n\\Z1Cannot decrement package references in Setup Database.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nCannot decrement '%s-%s' package references in Setup Database.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      if( !quiet )
      {
        fprintf( stdout, "\nCannot decrement '%s-%s' package references in Setup Database.\n\n", pkgname, pkgver );
      }
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }

  /*****************************************************
    Backup PKGLOG file into removed-packages directory:
   */
  bzero( (void *)tmp, PATH_MAX );

  if( group )
    (void)sprintf( &tmp[0], "%s/%s/", rempkgs_path, group );
  else
    (void)sprintf( &tmp[0], "%s/", rempkgs_path );

  if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
  {
    FATAL_ERROR( "Cannot access '/%s' directory", REMOVED_PKGS_PATH );
  }

  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "mv %s %s > /dev/null 2>&1",
                  pkglog_fname, (const char *)&tmp[0] );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot backup '%s' pkglog file", basename( (char *)pkglog_fname ) );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    free( tmp );

    exit_status = 47;

    if( remove_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                    "\n\\Z1Cannot backup PKGLOG file.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nCannot backup '%s' pkglog file.\n\n", basename( (char *)pkglog_fname ) );
#endif
    }
    else
    {
      if( !quiet )
      {
        fprintf( stdout, "\nCannot backup '%s' pkglog file.\n\n", basename( (char *)pkglog_fname ) );
      }
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }

  /****************************************
    Remove group directory if it is empty:
   */
  bzero( (void *)tmp, PATH_MAX );

  if( group )
  {
    (void)sprintf( &tmp[0], "%s/%s/", pkgs_path, group );

    const char *dir = (const char *)&tmp[0];
    if( is_dir_empty( dir ) )
    {
      (void)rmdir( dir );
    }
  }

  free( tmp );
}


static int ask_for_remove( int prev )
{
  int ret = 0; /* continue removal */
#if defined( HAVE_DIALOG )
  /******************************************************
    Ask for remove dialog shown only in MENUDIALOG mode:
   */
  if( (remove_mode == MENUDIALOG) )
  {
    if( prev < 0 )
    {
      char *msg = NULL;

      msg = (char *)malloc( (size_t)PATH_MAX );
      if( !msg ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)msg, PATH_MAX );

      (void)sprintf( &msg[0], "\nPrevious version '\\Z4%s\\Zn' of requested package installed.\n"
                              "\n\\Z1Remove a previous vesion?\\Zn\n", pkgver );

      ret =  ask_remove_box( "Remove:", pkgname, requested_version, (const char *)&msg[0], 9, 0, 0 );

      free( msg );
    }
    else if( prev > 0 )
    {
      char *msg = NULL;

      msg = (char *)malloc( (size_t)PATH_MAX );
      if( !msg ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)msg, PATH_MAX );

      (void)sprintf( &msg[0], "\nA newer version '\\Z4%s\\Zn' of requested package installed.\n"
                              "\n\\Z1Remove a newer vesion?\\Zn\n", pkgver );

      ret =  ask_remove_box( "Remove:", pkgname, requested_version, (const char *)&msg[0], 9, 0, 0 );

      free( msg );
    }
    else
    {
      ret =  ask_remove_box( "Remove:", pkgname, pkgver, description, 18, 0, 0 );
    }
  }

  if( ret )
  {
    info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                  "\nPackage removal terminated by user.\n", 5, 0, 0 );
  }
#endif
  return ret;
}


static void show_removal_progress( void )
{
  if( remove_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                  description, 16, 1, 0 );
#else
    fprintf( stdout, "\n Remobe: %s-%s ...\n", pkgname, pkgver );
    /*************************************************
      Ruler: 68 characters + 2 spaces left and right:

                      | ----handy-ruler----------------------------------------------------- | */
    fprintf( stdout, "|======================================================================|\n" );
    fprintf( stdout, "%s\n", description );
    fprintf( stdout, "|======================================================================|\n\n" );
#endif
  }
  else
  {
    if( !quiet )
    {
      fprintf( stdout, "\n Remove: %s-%s ...\n", pkgname, pkgver );
      /*************************************************
        Ruler: 68 characters + 2 spaces left and right:

                        | ----handy-ruler----------------------------------------------------- | */
      fprintf( stdout, "|======================================================================|\n" );
      fprintf( stdout, "%s\n", description );
      fprintf( stdout, "|======================================================================|\n\n" );
    }
  }
}


static void read_pkginfo( const char *pkginfo_fname )
{
  char *ln      = NULL;
  char *line    = NULL;

  FILE *pkginfo = NULL;

  if( pkginfo_fname != NULL )
  {
    pkginfo = fopen( (const char *)pkginfo_fname, "r" );
    if( !pkginfo )
    {
      FATAL_ERROR( "Cannot open %s file", pkginfo_fname );
    }
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  while( (ln = fgets( line, PATH_MAX, pkginfo )) )
  {
    char *match = NULL;

    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( (match = strstr( ln, "pkgname" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( pkgname ) { free( pkgname ); }
        pkgname = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "pkgver" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( pkgver ) { free( pkgver ); }
        pkgver = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "arch" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( arch ) { free( arch ); }
        arch = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "distroname" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( distroname ) { free( distroname ); }
        distroname = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "distrover" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( distrover ) { free( distrover ); }
        distrover = skip_spaces( p );
      }
    }

    if( (match = strstr( ln, "group" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( group ) { free( group ); }
        group = skip_spaces( p );
      }
    }

    if( (match = strstr( ln, "short_description" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        char *b =  index( p, '"'),
             *e = rindex( p, '"');
        if( b && e && ( b != e ) )
        {
          p = ++b; *e = '\0';
          if( short_description ) { free( short_description ); }
          short_description = strdup( (const char *)p );
        }
      }
    }
    if( (match = strstr( ln, "url" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( url ) { free( url ); }
        url = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "license" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( license ) { free( license ); }
        license = skip_spaces( p );
      }
    }

    if( (match = strstr( ln, "uncompressed_size" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( uncompressed_size ) { free( uncompressed_size ); }
        uncompressed_size = skip_spaces( p );
      }
    }
    if( (match = strstr( ln, "total_files" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL )
      {
        if( total_files ) { free( total_files ); }
        total_files = skip_spaces( p );
      }
    }
  }

  free( line );

  if( !pkgname || !pkgver || !arch || !distroname || !distrover )
  {
    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }

  fclose( pkginfo );
}


/***************************************************************
  Probe functions:
 */
static void _probe_pkglog( const char *dirpath, const char *grp )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

  if( pkglog_fname ) return;

  if( stat( dirpath, &path_sb ) == -1 )
  {
    FATAL_ERROR( "%s: Cannot stat Setup Database or destination directory", dirpath );
  }

  if( S_ISDIR(path_sb.st_mode) == 0 )
  {
    FATAL_ERROR( "%s: Setup Database or destination is not a directory", dirpath );
  }

  if( (dir = opendir(dirpath) ) == NULL )
  {
    FATAL_ERROR( "Canot access %s directory: %s", dirpath, strerror( errno ) );
  }

  len = strlen( dirpath );

  while( (entry = readdir( dir )) != NULL)
  {
    /* skip entries '.' and '..' */
    if( ! strcmp( entry->d_name, "." ) || ! strcmp( entry->d_name, ".." ) ) continue;

    /* determinate a full name of an entry */
    path = alloca( len + strlen( entry->d_name ) + 2 );

    strcpy( path, dirpath );
    strcat( path, "/" );
    strcat( path, entry->d_name );

    if( stat( path, &entry_sb ) == 0 )
    {
      if( S_ISREG(entry_sb.st_mode) )
      {
        char *match  = NULL;
        char *pkglog = basename( path );

        if( (match = strstr( pkglog, (const char *)basename( pkg_fname ) )) && match == pkglog )
        {
          char *buf = NULL, *p = NULL, *q = NULL;

          p = q = buf = strdup( (const char *)pkglog );
          ++p;
          while( *p != '\0' && !isblank(*p) && !(*q == '-' && isdigit(*p)) )
          {
            /* package version starts with a number and separated by '-' */
            ++p; ++q;
          }
          *(--p) = '\0';

          /*******************************************************
            We have to make sure that the name we are looking for
            is not shorter than the name of the found package.
           */
          if( strlen(pkg_fname) >= strlen(buf) )
          {

            pkglog_fname = strdup( (const char *)path );
            free( buf );
            closedir( dir );
            return;
          }
          free( buf );
        }
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        _probe_pkglog( (const char *)path, (const char *)entry->d_name );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

/***********************************************************
  probe_package():
  ---------------
 */
static char *probe_package( void )
{
  char *ret = NULL;

  _probe_pkglog( (const char *)pkgs_path, NULL );
  if( pkglog_fname )
  {
    free( pkg_fname );
    ret = pkg_fname = pkglog_fname;
  }

  return ret;
}
/*
  Enf of Probe functions.
 ***********************************************************/

/***********************************************************
  Find functions:
 */
static void _search_pkglog( const char *dirpath, const char *grp )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

  char   *pname = (char *)dirpath + strlen( root ); /* do not remove leading '/' */

  if( stat( dirpath, &path_sb ) == -1 )
  {
    FATAL_ERROR( "%s: Cannot stat Setup Database or group directory", pname );
  }

  if( S_ISDIR(path_sb.st_mode) == 0 )
  {
    FATAL_ERROR( "%s: Setup Database or group is not a directory", pname );
  }

  if( (dir = opendir(dirpath) ) == NULL )
  {
    FATAL_ERROR( "Canot access %s directory: %s", pname, strerror( errno ) );
  }

  len = strlen( dirpath );

  while( (entry = readdir( dir )) != NULL)
  {
    /* skip entries '.' and '..' */
    if( ! strcmp( entry->d_name, "." ) || ! strcmp( entry->d_name, ".." ) ) continue;

    /* determinate a full name of an entry */
    path = alloca( len + strlen( entry->d_name ) + 2 );

    strcpy( path, dirpath );
    strcat( path, "/" );
    strcat( path, entry->d_name );

    if( stat( path, &entry_sb ) == 0 )
    {
      if( S_ISREG(entry_sb.st_mode) )
      {
        char *match = NULL, *name  = basename( path );

        if( (match = strstr( name, pkgname )) && match == name )
        {
          /****************************************************************
            Здесь мы еще должны проверить, что найденный пакет не имеет
            более длинное имя, которое начинается с имени искомого пакета.
            Полагаясь на факт, что версия может начинаться только с цифры,
            мы пропускаем символ '-', разделяющий имя и версию пакета,
            а затем проверяем начальный символ версии:
           */
          if( isdigit( *(name + strlen( pkgname ) + 1) ) )
          {
            pkglog_fname = strdup( (const char *)path );
            closedir( dir );
            return;
          }
        }
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        /**************************************************************************
          NOTE:
            In the Setup Database can be only one package with the same pkgname
            but in different groups. For example, the package named 'cairo'
            has two instance: libs/cairo-1.14.6 and xlibs/cairo-1.14.6. During
            system installation the package libs/cairo-1.14.6 installed first
            and then updated by xlibs/cairo-1.14.6 and PKGLOG of libs/cairo-1.14.6
            moved from /var/log/radix/packages to /var/log/radix/removed-packages.

            So here we have to look for the PKGLOG in all group directories:
         */
        _search_pkglog( (const char *)path, (const char *)entry->d_name );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

static char *find_package( void )
{
  char *ret = NULL;

  _search_pkglog( (const char *)pkgs_path, NULL );
  if( pkglog_fname )
  {
    free( pkg_fname );
    ret = pkg_fname = pkglog_fname;
  }

  return ret;
}
/*
  Enf of Find functions.
 ***********************************************************/


/***********************************************************
  check_input_package():
  ---------------------

    Возвращает:
     -1 если пакет установлен, но его версия меньше
        запрашиваемого,
      0 если версия установленного и запрашиваемого равны,
      1 если пакет установлен, но его версия больше
        запрашиваемого.

    В случае возврата -1 или 1, устанавливается переменная
    requested_version, равная версии пакета который запросили
    на удаление.

    Если пакет не установлен, осуществляется выход со статусом 30.
 */
static int check_input_package( void )
{
  struct stat st;
  char *fname = pkg_fname;

  enum _input_type  type = IFMT_UNKNOWN;
  char              uncompress = '\0';

  int ret = 0;

  bzero( (void *)&st, sizeof( struct stat ) );

  if( stat( (const char *)fname, &st ) == -1 )
  {
    /*************************************************
      Specified pkg_fname is not a file or directory.
      Try to find installed package  with name equal
      to pkg_fname:
     */
    fname = NULL;
    fname = probe_package();
    if( !fname )
    {
      if( remove_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Remove:", basename( pkg_fname ), NULL, NULL,
                      "\nPackage is not installed.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nPackage '%s' is not installed.\n\n", basename( pkg_fname ) );
#endif
      }
      else
      {
        if( !quiet ) fprintf( stdout, "Specified package '%s' is not installed.\n\n", basename( pkg_fname ) );
      }

      exit_status = 30; /* Package is not installed: install */

      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();

      exit( exit_status );
    }
  }
  else
  {
    if( S_ISREG(st.st_mode) )
    {
      pid_t p = (pid_t) -1;
      int   rc;

      int   len = 0;
      char *tmp= NULL, *cmd = NULL;

      tmp = (char *)malloc( (size_t)PATH_MAX );
      if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)tmp, PATH_MAX );

      (void)sprintf( &tmp[0], "%s", tmpdir );
      if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
      {
        FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)fname ) );
      }

      cmd = (char *)malloc( (size_t)PATH_MAX );
      if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)cmd, PATH_MAX );

      len = snprintf( &cmd[0], PATH_MAX,
                      "%s/pkginfo -d %s -o pkginfo %s > /dev/null 2>&1",
                      selfdir, tmp, fname );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( "Cannot get PKGINFO from %s file", basename( (char *)fname ) );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
      if( rc != 0 )
      {
        FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)fname ) );
      }

      (void)strcat( tmp, "/.PKGINFO" );
      read_pkginfo( (const char *)&tmp[0] );
      (void)unlink( (const char *)&tmp[0] ); /* :remove unnecessary .PKGINFO file */
      *(strstr( tmp, "/.PKGINFO" )) = '\0';  /* :restore 'tmpdir' in tmp[] buffer */

      requested_version = strdup( (const char *)pkgver );

      fname = NULL;
      fname = find_package();
      if( !fname )
      {
        if( remove_mode != CONSOLE )
        {
#if defined( HAVE_DIALOG )
          info_pkg_box( "Remove:", basename( pkg_fname ), NULL, NULL,
                        "\nPackage is not installed.\n", 5, 0, 0 );
#else
          fprintf( stdout, "\nPackage '%s' is not installed.\n\n", basename( pkg_fname ) );
#endif
        }
        else
        {
          if( !quiet ) fprintf( stdout, "Specified package '%s' is not installed.\n\n", basename( pkg_fname ) );
        }

        exit_status = 30; /* Package is not installed: install */

        if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
        free_resources();

        exit( exit_status );
      }

      free( cmd );
      free( tmp );
    }
    else
    {
      FATAL_ERROR( "Input %s file is not a regular file", basename( (char *)fname ) );
    }
  }

  /* check pkg_fname again: */
  if( stat( (const char *)fname, &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access input '%s' file: %s", fname, strerror( errno ) );
  }

  type = check_input_file( &uncompress, fname );
  if( type == IFMT_UNKNOWN )
  {
    FATAL_ERROR( "Unknown format of input '%s' file", fname );
  }

  if( S_ISREG(st.st_mode) )
  {
    pid_t p = (pid_t) -1;
    int   rc;

    int   len = 0;
    char *tmp= NULL, *cmd = NULL;

    tmp = (char *)malloc( (size_t)PATH_MAX );
    if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)tmp, PATH_MAX );

    (void)sprintf( &tmp[0], "%s/to-remove", tmpdir );
    if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)fname ) );
    }
    rtmpdir = strdup( (const char *)&tmp[0] );

    cmd = (char *)malloc( (size_t)PATH_MAX );
    if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)cmd, PATH_MAX );

    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/pkginfo -d %s -o pkginfo,description,restore-links,filelist %s > /dev/null 2>&1",
                    selfdir, tmp, fname );
    if( len == 0 || len == PATH_MAX - 1 )
    {
      FATAL_ERROR( "Cannot get PKGINFO from %s file", basename( (char *)fname ) );
    }
    p = sys_exec_command( cmd );
    rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
    if( rc != 0 )
    {
      FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)fname ) );
    }

    (void)strcat( tmp, "/.PKGINFO" );
    read_pkginfo( (const char *)&tmp[0] );
    *(strstr( tmp, "/.PKGINFO" )) = '\0'; /* :restore tmpdir in tmp[] buffer */

    free( cmd );
    free( tmp );

    if( requested_version )
    {
      ret = cmp_version( (const char *)pkgver, (const char *)requested_version );
    }
  }
  else
  {
    FATAL_ERROR( "Input %s file is not a regular file", basename( (char *)fname ) );
  }

  return ret;
}



static void dialogrc( void )
{
  struct stat st;
  char  *tmp = NULL;

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  /* imagine that the utility is in /sbin directory: */
  (void)sprintf( &tmp[0], "%s/../usr/share/%s/.dialogrc", selfdir, PACKAGE_NAME );
  if( stat( (const char *)&tmp[0], &st ) == -1 )
  {
    /* finaly assume that /usr/sbin is a sbindir: */
    (void)sprintf( &tmp[0], "%s/../../usr/share/%s/.dialogrc", selfdir, PACKAGE_NAME );
  }

  setenv( "DIALOGRC", (const char *)&tmp[0], 1 );

  free( tmp );
}

static char *get_curdir( void )
{
  char *cwd = NULL;

  cwd = (char *)malloc( PATH_MAX );
  if( !cwd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cwd, PATH_MAX );

  if( getcwd( cwd, (size_t)PATH_MAX ) != NULL )
  {
    char *p = NULL;
    remove_trailing_slash( cwd );
    p = strdup( cwd );
    free( cwd );
    return p;
  }
  else
  {
    FATAL_ERROR( "Cannot get absolute path to current directory" );
  }

  return (char *)NULL;
}


/*********************************************
  Get directory where this program is placed:
 */
char *get_selfdir( void )
{
  char    *buf = NULL;
  ssize_t  len;

  buf = (char *)malloc( PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  bzero( (void *)buf, PATH_MAX );
  len = readlink( "/proc/self/exe", buf, (size_t)PATH_MAX );
  if( len > 0 && len < PATH_MAX )
  {
    char *p = strdup( dirname( buf ) );
    free( buf );
    return p;
  }
  return (char *)NULL;
}

void set_stack_size( void )
{
  const rlim_t   stack_size = 16 * 1024 * 1024; /* min stack size = 16 MB */
  struct rlimit  rl;
  int ret;

  ret = getrlimit( RLIMIT_STACK, &rl );
  if( ret == 0 )
  {
    if( rl.rlim_cur < stack_size )
    {
      rl.rlim_cur = stack_size;
      ret = setrlimit( RLIMIT_STACK, &rl );
      if( ret != 0 )
      {
        fprintf(stderr, "setrlimit returned result = %d\n", ret);
        FATAL_ERROR( "Cannot set stack size" );
      }
    }
  }
}


int main( int argc, char *argv[] )
{
  gid_t  gid;

  set_signal_handlers();

  gid = getgid();
  setgroups( 1, &gid );

  fatal_error_hook = fatal_error_actions;

  selfdir = get_selfdir();
  curdir  = get_curdir();
  dialogrc();

  errlog = stderr;

  program = basename( argv[0] );
  get_args( argc, argv );

  /* set_stack_size(); */

  tmpdir = _mk_tmpdir();
  if( !tmpdir )
  {
    FATAL_ERROR( "Cannot create temporary directory" );
  }

  {
    int status = 0;

    /**********************************************************
      Fill pkginfo data and put or replace pkglog into tmpdir:
     */
    status = check_input_package();

    read_filelist();
    read_restorelinks();
    read_description();

    if( ask_for_remove( status ) )
    {
      /* Terminate removal: */
      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();
      exit( exit_status );
    }
  }

  show_removal_progress();

  /************
    DO REMOVE:
   */
  pre_remove_routine();
  remove_package();
  post_remove_routine();
  finalize_removal();

  if( remove_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    info_pkg_box( "Remove:", pkgname, pkgver, NULL,
                  "\nPackage has been removed.\n", 5, 0, 0 );
#else
    fprintf( stdout, "\nPackage '%s-%s' has been removed.\n\n", pkgname, pkgver );
#endif
  }
  else
  {
    if( !quiet )
    {
      fprintf( stdout, "\nPackage '%s-%s' has been removed.\n\n", pkgname, pkgver );
    }
  }

  setup_log( "Package '%s-%s' has been removed", pkgname, pkgver );

  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
