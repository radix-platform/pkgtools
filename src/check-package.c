
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

#include <sys/wait.h>

#include <sys/resource.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <msglog.h>
#include <system.h>
#include <cmpvers.h>

#define PROGRAM_NAME "check-package"

#include <defs.h>


char *program = PROGRAM_NAME;
char *root = NULL, *pkgs_path = NULL, *pkg_fname = NULL, *pkg_found = NULL,
     *tmpdir = NULL;

int   quiet = 0, print_broken_files = 0;

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

static char           *pkgname = NULL,
                       *pkgver = NULL,
                         *arch = NULL,
                   *distroname = NULL,
                    *distrover = NULL,
                        *group = NULL;

static char *installed_version = NULL;

enum _input_type {
  IFMT_PKG = 0,
  IFMT_LOG,

  IFMT_UNKNOWN
} input_format = IFMT_PKG;


#define FREE_PKGINFO_VARIABLES() \
  if( pkgname )           { free( pkgname );           } pkgname = NULL;            \
  if( pkgver )            { free( pkgver );            } pkgver = NULL;             \
  if( arch )              { free( arch );              } arch = NULL;               \
  if( distroname )        { free( distroname );        } distroname = NULL;         \
  if( distrover )         { free( distrover );         } distrover = NULL;          \
  if( group )             { free( group );             } group = NULL;              \
  if( installed_version ) { free( installed_version ); } installed_version = NULL

void free_resources()
{
  if( root )         { free( root );         root         = NULL; }
  if( pkgs_path )    { free( pkgs_path );    pkgs_path    = NULL; }
  if( pkg_fname )    { free( pkg_fname );    pkg_fname    = NULL; }

  if( selfdir )      { free( selfdir );      selfdir      = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <package|pkglog|pkgname>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "This utility checks if specified package is installed.  If package\n" );
  fprintf( stdout, "or some another version  of this package is already installed then\n" );
  fprintf( stdout, "this utility checks if installed package is correct.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  -p,--print-broken-files       Print the list of broken directories,\n" );
  fprintf( stdout, "                                files, or symbolic links to stdout.\n" );
  fprintf( stdout, "  -q,--quiet                    Do not display explanations for\n" );
  fprintf( stdout, "                                return codes.\n" );
  fprintf( stdout, "  -r,--root=<DIR>               Target rootfs path.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <package|pkglog|pkgname>      The PKGNAME, PACKAGE tarball or PKGLOG.\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Return codes:\n" );
  fprintf( stdout, "  ------+---------------------------+--------------------\n"  );
  fprintf( stdout, "   code |          status           | what can be done\n"  );
  fprintf( stdout, "  ------+---------------------------+--------------------\n"  );
  fprintf( stdout, "     30 | not installed             | install\n"  );
  fprintf( stdout, "     31 | installed correctly       | nothing to do\n"  );
  fprintf( stdout, "     32 | installed but not correct | repair, re-install\n"  );
  fprintf( stdout, "     33 | installed correctly       | upgrade\n"  );
  fprintf( stdout, "     34 | installed but not correct | repair, upgrade\n"  );
  fprintf( stdout, "     35 | installed correctly       | downgrade\n"  );
  fprintf( stdout, "     36 | installed but not correct | repair, downgrade\n"  );
  fprintf( stdout, "  ------+---------------------------+--------------------\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Other non-zero codes assumes that this utility faced to errors not\n" );
  fprintf( stdout, "related to quality of installed package.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "If package specified by short name (probably with version) instead\n" );
  fprintf( stdout, "of regular files such as package tarball or pkglog file  then this\n" );
  fprintf( stdout, "utility doesn't check other installed versions of this package.\n" );
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
  const char* short_options = "hvpqr:";

  const struct option long_options[] =
  {
    { "help",               no_argument,       NULL, 'h' },
    { "version",            no_argument,       NULL, 'v' },
    { "print-broken-files", no_argument,       NULL, 'p' },
    { "quiet",              no_argument,       NULL, 'q' },
    { "root",               required_argument, NULL, 'r' },
    { NULL,                 0,                 NULL,  0  }
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
      case 'p':
      {
        print_broken_files = 1;
        break;
      }
      case 'q':
      {
        quiet = 1;
        break;
      }

      case 'r':
      {
        if( optarg != NULL )
        {
          root = strdup( optarg );
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
    pkg_fname = strdup( (const char *)argv[optind] );
  }
  else
  {
    usage();
  }


  if( !pkgs_path )
  {
    struct stat st;
    char  *buf = NULL;

    bzero( (void *)&st, sizeof( struct stat ) );

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    if( !root )
    {
      buf[0] = '/'; buf[1] = '\0';
    }
    else
    {
      int len = strlen( root );

      (void)strcpy( buf, (const char *)root );
      if( buf[ len - 1 ] != '/' )
      {
        buf[len] = '/'; buf[len+1] = '\0';
      }
    }

    (void)strcat( buf, PACKAGES_PATH );
    if( stat( (const char *)&buf[0], &st ) == -1 )
    {
      FATAL_ERROR( "Cannot access '%s' file or directory: %s", buf, strerror( errno ) );
    }

    if( S_ISDIR(st.st_mode) )
    {
      pkgs_path = strdup( (const char *)&buf[0] );
      free( buf );
    }
    else
    {
      FATAL_ERROR( "Defined --root '%s' is not a directory", buf );
    }

  } /* End if( !pkgs_path ) */
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

  if( pkg_found ) return;

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

            pkg_found = strdup( (const char *)path );
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
char *probe_package( void )
{
  char *ret = NULL;

  _probe_pkglog( (const char *)pkgs_path, NULL );
  if( pkg_found )
  {
    free( pkg_fname );
    ret = pkg_fname = pkg_found;
  }

  return ret;
}
/*
  Enf of Probe functions.
 ***********************************************************/



static void read_input_pkginfo( const char *pkginfo_fname )
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
      if( p != NULL ) pkgname = skip_spaces( p );
    }
    if( (match = strstr( ln, "pkgver" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) pkgver = skip_spaces( p );
    }
    if( (match = strstr( ln, "arch" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) arch = skip_spaces( p );
    }
    if( (match = strstr( ln, "distroname" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) distroname = skip_spaces( p );
    }
    if( (match = strstr( ln, "distrover" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) distrover = skip_spaces( p );
    }

    if( (match = strstr( ln, "group" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) group = skip_spaces( p );
    }
  }

  free( line );

  if( !pkgname || !pkgver || !arch || !distroname || !distrover )
  {
    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }

  fclose( pkginfo );
}

static void read_found_pkginfo( const char *pkginfo_fname )
{
  char *ln      = NULL;
  char *line    = NULL;

  FILE *pkginfo = NULL;

  char *pn = NULL, *pv = NULL, *ar = NULL, *dn = NULL, *dv = NULL;

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
      if( p != NULL ) pn = skip_spaces( p );
    }
    if( (match = strstr( ln, "pkgver" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) pv = skip_spaces( p );
    }
    if( (match = strstr( ln, "arch" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) ar = skip_spaces( p );
    }
    if( (match = strstr( ln, "distroname" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) dn = skip_spaces( p );
    }
    if( (match = strstr( ln, "distrover" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) dv = skip_spaces( p );
    }
  }

  free( line );

  if( !pn || !pv || !ar || !dn || !dv )
  {
    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }
  else
  {
    if( pn ) free( pn );
    if( pv ) installed_version = pv;
    if( ar ) free( ar );
    if( dn ) free( dn );
    if( dv ) free( dv );
  }

  fclose( pkginfo );
}

static void _get_found_pkginfo( const char *pkglog )
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
    FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)pkglog ) );
  }

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "%s/pkginfo -d %s -o pkginfo,restore-links,filelist %s > /dev/null 2>&1",
                  selfdir, tmp, pkglog );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot get PKGINFO from %s file", basename( (char *)pkglog ) );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)pkglog ) );
  }

  (void)strcat( tmp, "/.PKGINFO" );
  read_found_pkginfo( (const char *)&tmp[0] );
  *(strstr( tmp, "/.PKGINFO" )) = '\0'; /* :restore tmpdir in tmp[] buffer */

  free( tmp );
  free( cmd );
}

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
            _get_found_pkginfo( (const char *)path );
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

static void find_installed_package( void )
{
  char *tmp = NULL;

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( &tmp[0], "%s", pkgs_path );

  _search_pkglog( (const char *)&tmp[0], NULL );

  free( tmp );
}

/***************************************************************
  check_input_package():
  ---------------------

    Возвращает:
     -1 если пакет установлен, но его версия меньше
        запрашиваемого,
      0 если пакет не установлен или версия установленного и
        запрашиваемого равны,
      1 если пакет установлен, но его версия больше
        запрашиваемого.

    В случае возврата -1 или 1, устанавливается переменная
    installed_version, равная версии уже установленного пакета.

    В случае возврата нуля есть два варанта:
      а) пакет установлен и его надо проверить на целостность
         (переменная installed_version != NULL );
      б) пакет не установлен.

    Если installed_version != NULL, проверка целостности
    пакета будет осуществлена, вне зависимости от его версии.
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
      if( !quiet ) fprintf( stdout, "Specified package '%s' is not installed.\n\n", pkg_fname );

      exit_status = 30; /* Package is not installed: install */

      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();

      exit( exit_status );
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

    (void)sprintf( &tmp[0], "%s", tmpdir );
    if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot get PKGINFO from '%s' file", basename( (char *)fname ) );
    }

    cmd = (char *)malloc( (size_t)PATH_MAX );
    if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)cmd, PATH_MAX );

    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/pkginfo -d %s -o pkginfo,restore-links,filelist %s > /dev/null 2>&1",
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
    read_input_pkginfo( (const char *)&tmp[0] );
    *(strstr( tmp, "/.PKGINFO" )) = '\0'; /* :restore tmpdir in tmp[] buffer */

    find_installed_package();

    free( cmd );
    free( tmp );

    if( installed_version )
    {
      ret = cmp_version( (const char *)installed_version, (const char *)pkgver );
    }
  }
  else
  {
    FATAL_ERROR( "Input %s file is not a regular file", basename( (char *)fname ) );
  }

  return ret;
}

static int check_package_integrity( void )
{
  struct stat st;
  FILE  *fp = NULL;

  char *ln   = NULL;
  char *line = NULL;

  char *buf = NULL, *tmp = NULL;

  int restore_links = 0;
  int ret = 1;

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  /* Check if .RESTORELINKS is present */
  (void)sprintf( &tmp[0], "%s/.RESTORELINKS", tmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&tmp[0], &st ) == 0) && (st.st_size > 8) )
  {
    restore_links = 1;
  }

  (void)sprintf( &tmp[0], "%s/.FILELIST", tmpdir );
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
    int dir = 0;

    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( *(ln + strlen(ln) - 1) == '/' ) { dir = 1; *(ln + strlen(ln) - 1) = '\0'; }
    else                                { dir = 0; }

    if( !dir )
    {
      char *p = rindex( ln, '.' );
      if( p && !strncmp( (const char *)p, ".new", 4 ) )
      {
        /**************************
          Do not check .new files:
         */
        *p = '\0';
      }
    }

    (void)sprintf( &buf[0], "%s/%s", root, ln );
    bzero( (void *)&st, sizeof( struct stat ) );

    if( lstat( (const char *)&buf[0], &st ) == -1 )
    {
      /* cannot access file list entry */
      if( dir )
      {
        if( print_broken_files )
          fprintf( stdout, "%s-%s: /%s: no such directory\n", pkgname, installed_version, ln );
      }
      else
      {
        if( print_broken_files )
          fprintf( stdout, "%s-%s: /%s: no such file\n", pkgname, installed_version, ln );
      }

      ret = 0; continue;
    }

    if( dir )
    {
      if( S_ISDIR(st.st_mode) == 0 )
      {
        /* not a directory */
        if( print_broken_files )
          fprintf( stdout, "%s-%s: /%s: not a directory\n", pkgname, installed_version, ln );
        ret = 0; continue;
      }
    }
    else
    {
      if( S_ISREG(st.st_mode) == 0 )
      {
        /* not a regular file */
        if( print_broken_files )
          fprintf( stdout, "%s-%s: /%s: not a regular file\n", pkgname, installed_version, ln );
        ret = 0; continue;
      }
      if( !restore_links )
      {
        if( S_ISLNK(st.st_mode) == 0 )
        {
          /* not a symbolic link */
          if( print_broken_files )
            fprintf( stdout, "%s-%s: /%s: not a symbolic link\n", pkgname, installed_version, ln );
          ret = 0; continue;
        }
      }
    }
  } /* End of while( file list entry ) */
  fclose( fp );


  (void)sprintf( &tmp[0], "%s/.RESTORELINKS", tmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );

  if( stat( (const char *)&tmp[0], &st ) == 0 )
  {
    fp = fopen( (const char *)&tmp[0], "r" );
    if( !fp )
    {
      FATAL_ERROR( "Cannot open .RESTORELINKS file" );
    }

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
          (void)sprintf( &buf[0], "%s/%s/%s", root, p, f );
          bzero( (void *)&st, sizeof( struct stat ) );

          if( lstat( (const char *)&buf[0], &st ) == -1 )
          {
            /* cannot access restore links entry */
            if( print_broken_files )
              fprintf( stdout, "%s-%s: /%s/%s: no such file or directory\n", pkgname, installed_version, p, f );
            ret = 0; continue;
          }

          if( S_ISLNK(st.st_mode) == 0 )
          {
            /* not a symbolic link */
            if( print_broken_files )
              fprintf( stdout, "%s-%s: /%s/%s: not a symbolic link\n", pkgname, installed_version, p, f );
            ret = 0; continue;
          }
        }
      }
    } /* End of while( restore links entry ) */
    fclose( fp );
  }

  free( line );
  free( tmp );
  free( buf );

  return ret;
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
    int status = 0, correctly = 0;

    /**********************************************************
      Fill pkginfo data and put or replace pkglog into tmpdir:
     */
    status = check_input_package();

    if( installed_version )
    {
      /* In this case we have to check package integrity */
      correctly = check_package_integrity(); /* returns 1 if correct */
    }

    if( exit_status == EXIT_SUCCESS )
    {
      if( status < 0 )
      {
        if( !correctly )
        {
          if( !quiet )
            fprintf( stdout,
                     "Previous version '%s' of specified package '%s-%s' is installed but not correct.\n\n",
                     installed_version, pkgname, pkgver );
          exit_status = 34;   /* Package is installed but not correct: repair, upgrade    */
        }
        else
        {
          if( !quiet )
            fprintf( stdout,
                     "Previous version '%s' of specified package '%s-%s' is installed.\n\n",
                     installed_version, pkgname, pkgver );
          exit_status = 33;   /* Package is installed correctly: upgrade                  */
        }
      }
      else if( status > 0 )
      {
        if( !correctly )
        {
          if( !quiet )
            fprintf( stdout,
                     "A newer version '%s' of specified package '%s-%s' is installed but not correct.\n\n",
                     installed_version, pkgname, pkgver );
          exit_status = 36;   /* Package is installed but not correct: repair, downgrade  */
        }
        else
        {
          if( !quiet )
            fprintf( stdout,
                     "A newer version '%s' of specified package '%s-%s' is installed.\n\n",
                     installed_version, pkgname, pkgver );
          exit_status = 35;   /* Package is installed correctly: downgrade                */
        }
      }
      else
      {
        if( installed_version )
        {
          if( !correctly )
          {
            if( !quiet )
              fprintf( stdout,
                       "Specified package '%s-%s' is already installed but not correct.\n\n",
                       pkgname, pkgver );
            exit_status = 32; /* Package is installed but not correct: repair, re-install */
          }
          else
          {
            if( !quiet )
              fprintf( stdout,
                       "Specified package '%s-%s' is already installed.\n\n",
                       pkgname, pkgver );
            exit_status = 31; /* Package is installed correctly: nothing to do            */
          }
        }
        else
        {
          if( !quiet )
            fprintf( stdout,
                     "Specified package '%s-%s' is not installed.\n\n",
                     pkgname, pkgver );
          exit_status = 30;   /* Package is not installed: install */
        }
      }
    }

  }


  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
