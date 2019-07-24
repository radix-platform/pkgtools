
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
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h> /* chmod(2)    */
#include <fcntl.h>
#include <limits.h>
#include <string.h>   /* strdup(3)   */
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

#include <sys/resource.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <msglog.h>
#include <system.h>
#include <dlist.h>

#define PROGRAM_NAME "make-package"

#include <defs.h>


char *program = PROGRAM_NAME;
char *destination = NULL, *srcdir = NULL, *flavour = NULL,
     *tmpdir = NULL;

FILE   *rlinks  = NULL;
size_t  pkgsize = 0;
int     nfiles  = 0;

const char *txz_suffix = ".txz";
char  compress = 'J';

#if defined( HAVE_GPG2 )
char *passphrase = NULL, *key_id = NULL;
#endif

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

int   mkgroupdir  = 0;
int   linkadd     = 1;

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
                  *total_files = NULL;

struct dlist *filelist = NULL;

static void create_file_list( void );
static void free_file_list( void );


#define FREE_PKGINFO_VARIABLES() \
  if( pkgname )           { free( pkgname );           } pkgname = NULL;            \
  if( pkgver )            { free( pkgver );            } pkgver = NULL;             \
  if( arch )              { free( arch );              } arch = NULL;               \
  if( distroname )        { free( distroname );        } distroname = NULL;         \
  if( distrover )         { free( distrover );         } distrover = NULL;          \
  if( group )             { free( group );             } group = NULL;              \
  if( short_description ) { free( short_description ); } short_description = NULL;  \
  if( url )               { free( url );               } url = NULL;                \
  if( license )           { free( license );           } license = NULL;            \
  if( uncompressed_size ) { free( uncompressed_size ); } uncompressed_size = NULL;  \
  if( total_files )       { free( total_files );       } total_files = NULL

void free_resources()
{
  if( srcdir )        { free( srcdir );        srcdir        = NULL; }
  if( destination )   { free( destination );   destination   = NULL; }
  if( flavour )       { free( flavour );       flavour       = NULL; }
  if( filelist )      { free_file_list();      filelist      = NULL; }

#if defined( HAVE_GPG2 )
  if( passphrase )    { free( passphrase );    passphrase    = NULL; }
  if( key_id )        { free( key_id );        key_id        = NULL; }
#endif

  if( selfdir )       { free( selfdir );       selfdir       = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <srcpkgdir>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Create PACKAGE from SRCPKGDIR where package is installed. The source\n" );
  fprintf( stdout, "directory should content the package service files:\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "  .PKGINFO, .INSTALL, .DESCRIPTION, and .REQUIRES (if applicable)\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "The '.PKGINFO' file is obligatory and should content declarations of\n" );
  fprintf( stdout, "following variables:  pkgname, pkgver, arch,  distroname, distrover.\n" );
  fprintf( stdout, "Also in the .PKGINFO file  can be defined additional and recommended\n" );
  fprintf( stdout, "variables: group, short_description, url, license.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                  Display this information.\n" );
  fprintf( stdout, "  -v,--version               Display the version of %s utility.\n", program );
  fprintf( stdout, "  -d,--destination=<DIR>     Target directory to save output PACKAGE.\n" );
  fprintf( stdout, "  -m,--mkgroupdir            Create GROUP subdirectory in the PACKAGE\n" );
  fprintf( stdout, "                             target directory.\n" );
  fprintf( stdout, "  -l,--linkadd={y|n}         Create .RESTORELINKS scrypt (default yes).\n" );
  fprintf( stdout, "  -f,--flavour=<subdir>      The name of additional subdirectory in the\n" );
  fprintf( stdout, "                             GROUP directory to save target PACKAGE.\n" );
  fprintf( stdout, "\n" );
#if defined( HAVE_GPG2 )
  fprintf( stdout, "OpenPGP options:\n" );
  fprintf( stdout, "  -p,--passphrase=<FILE>     File with passphrase of private certificate\n" );
  fprintf( stdout, "                             for signing package. For example:\n" );
  fprintf( stdout, "                                ~/.gnupg/.passphrase\n" );
  fprintf( stdout, "                             Passphrase should be placed in the first\n" );
  fprintf( stdout, "                             line of the file (the new-line symbol at\n" );
  fprintf( stdout, "                             end of passphrase is allowed). File must\n" );
  fprintf( stdout, "                             have access mode 600.\n" );
  fprintf( stdout, "  -k,--key-id=<USER-ID>      Use USER-ID to sign package, for example,\n" );
  fprintf( stdout, "                             --key-id=0xA5ED710298807270\n" );
  fprintf( stdout, "\n" );
#endif
  fprintf( stdout, "Compression options:\n" );
  fprintf( stdout, "  -J,--xz                    Filter the package archive through xz(1).\n" );
  fprintf( stdout, "  -j,--bzip2                 Filter the package archive through bzip2(1).\n" );
  fprintf( stdout, "  -z,--gzip                  Filter the package archive through gzip(1).\n" );
  fprintf( stdout, "\n" );
#if defined( HAVE_GPG2 )
  fprintf( stdout, "  If one of arguments:  passphrase or key-id  is not specified, then\n" );
  fprintf( stdout, "  signature will not be created and utility doesn't return any error\n" );
  fprintf( stdout, "  code.  If error occurs during the creation of a package signature,\n" );
  fprintf( stdout, "  the utility returns error code, but continue to create the package\n" );
  fprintf( stdout, "  (in this case the certificate will not be created).\n" );
  fprintf( stdout, "\n" );
#endif
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <srcpkgdir>                Directory wich contains source package\n"  );
  fprintf( stdout, "                             and package service files.\n"  );
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



void get_args( int argc, char *argv[] )
{
#if defined( HAVE_GPG2 )
  const char* short_options = "hvmd:l:f:p:k:Jjz";
#else
  const char* short_options = "hvmd:l:f:Jjz";
#endif

  const struct option long_options[] =
  {
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { "destination", required_argument, NULL, 'd' },
    { "mkgroupdir",  no_argument,       NULL, 'm' },
    { "linkadd",     required_argument, NULL, 'l' },
    { "flavour",     required_argument, NULL, 'f' },
#if defined( HAVE_GPG2 )
    { "passphrase",  required_argument, NULL, 'p' },
    { "key-id",      required_argument, NULL, 'k' },
#endif
    { "xz",          no_argument,       NULL, 'J' },
    { "bzip2",       no_argument,       NULL, 'j' },
    { "gzip",        no_argument,       NULL, 'z' },
    { NULL,          0,                 NULL,  0  }
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

      case 'd':
      {
        if( optarg != NULL )
        {
          destination = strdup( optarg );
          remove_trailing_slash( destination );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }
      case 'm':
      {
        mkgroupdir = 1;
        break;
      }

      case 'J':
      {
        compress = 'J';
        txz_suffix = ".txz";
        break;
      }
      case 'j':
      {
        compress = 'j';
        txz_suffix = ".tbz";
        break;
      }
      case 'z':
      {
        compress = 'z';
        txz_suffix = ".tgz";
        break;
      }

      case 'l':
      {
        if( optarg != NULL )
        {
          char  *buf = NULL;
          size_t len = strlen( optarg ) + 1;

          buf = (char *)malloc( len );
          if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
          bzero( (void *)buf, len );

          (void)strcpy( buf, (const char *)optarg );
          to_lowercase( buf );
          if( !strncmp( (const char *)&buf[0], "n", 1 ) )
          {
            linkadd = 0;
          }
          free( buf );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }
      case 'f':
      {
        if( optarg != NULL )
        {
          char  *buf = NULL;
          size_t len = strlen( optarg ) + 1;

          buf = (char *)malloc( len );
          if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
          bzero( (void *)buf, len );

          (void)strcpy( buf, (const char *)optarg );
          to_lowercase( buf );

          flavour = strdup( (const char *)&buf[0] );
          free( buf );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }

#if defined( HAVE_GPG2 )
      case 'p':
      {
        if( optarg != NULL )
        {
          struct stat st;
          char  *buf  = NULL;
          char  *home = NULL;

          bzero( (void *)&st, sizeof( struct stat ) );

          buf = (char *)malloc( (size_t)PATH_MAX );
          if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
          bzero( (void *)buf, PATH_MAX );

          if( *optarg == '~' )
          {
            home = getenv( "HOME" );
            if( home )
            {
              (void)sprintf( buf, "%s/%s", home, (const char *)((char *)optarg + 2) );
            }
            else
            {
              FATAL_ERROR( "Cannot get HOME directory" );
            }
          }
          else
          {
            (void)strcpy( buf, (const char *)optarg );
          }

          if( stat( (const char *)&buf[0], &st ) == -1 )
          {
            FATAL_ERROR( "Cannot access '%s' passphrase source file: %s", basename( buf ), strerror( errno ) );
          }
          if( !S_ISREG(st.st_mode) )
          {
            FATAL_ERROR( "The passphrase '%s' is not a regular file", basename( buf ) );
          }

          passphrase = strdup( (const char *)&buf[0] );
          free( buf );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }
      case 'k':
      {
        if( optarg != NULL )
        {
          key_id = strdup( optarg );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }
#endif

      case '?': default:
      {
        usage();
        break;
      }
    }
  }

  /* last command line argument is the PACKAGE source directory */
  if( optind < argc )
  {
    struct stat st;
    char  *buf = NULL;

    bzero( (void *)&st, sizeof( struct stat ) );

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    (void)strcpy( buf, (const char *)argv[optind] );
    remove_trailing_slash( (char *)&buf[0] );

    if( stat( (const char *)&buf[0], &st ) == -1 )
    {
      FATAL_ERROR( "Cannot access '%s' PACKAGE source directory: %s", basename( buf ), strerror( errno ) );
    }

    if( ! S_ISDIR(st.st_mode) )
    {
      FATAL_ERROR( "The PACKAGE source '%s' is not a directory", basename( buf ) );
    }

    /* Add .PKGINFO to the input dir name: */
    (void)strcat( buf, "/.PKGINFO" );
    if( stat( (const char *)&buf[0], &st ) == -1 ) {
      FATAL_ERROR( "The defined SRCPKGDIR doesn't contain a valid package" );
    }
    *(strstr( buf, "/.PKGINFO" )) = '\0'; /* restore tmpdir in tmp[] buffer */

    /* Add .DESCRIPTION to the input dir name: */
    (void)strcat( buf, "/.DESCRIPTION" );
    if( stat( (const char *)&buf[0], &st ) == -1 ) {
      FATAL_ERROR( "The defined SRCPKGDIR doesn't contain package '.DESCRIPTION' file" );
    }
    *(strstr( buf, "/.DESCRIPTION" )) = '\0'; /* restore tmpdir in tmp[] buffer */

    /* Add .INSTALL to the input dir name: */
    (void)strcat( buf, "/.INSTALL" );
    if( stat( (const char *)&buf[0], &st ) == -1 ) {
      FATAL_ERROR( "The defined SRCPKGDIR doesn't contain package '.INSTALL' script" );
    }
    *(strstr( buf, "/.INSTALL" )) = '\0'; /* restore tmpdir in tmp[] buffer */


    srcdir = strdup( (const char *)&buf[0] );
    if( srcdir == NULL )
    {
      usage();
    }

    free( buf );
  }
  else
  {
    usage();
  }

  if( destination == NULL )
  {
    char *buf = NULL;

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    (void)strcpy( buf, (const char *)srcdir );
    remove_trailing_slash( (char *)&buf[0] );

    destination = strdup( (const char *)dirname( (char *)&buf[0] ) );

    free( buf );
  }
}


/*
  Especialy for pkginfo lines.
  Remove leading spaces and take non-space characters only:
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

/*
  remove spaces at end of line:
 */
static void skip_eol_spaces( char *s )
{
  char *p = (char *)0;

  if( !s || *s == '\0' ) return;

  p = s + strlen( s ) - 1;
  while( isspace( *p ) ) { *p-- = '\0'; }
}


static void read_pkginfo( void )
{
  struct stat st;
  char  *buf = NULL;
  FILE  *pkginfo;

  bzero( (void *)&st, sizeof( struct stat ) );

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  (void)strcpy( buf, (const char *)srcdir );
  (void)strcat( buf, "/.PKGINFO" );

  if( stat( (const char *)&buf[0], &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access .PKGINFO file: %s", strerror( errno ) );
  }
  if( !S_ISREG(st.st_mode) )
  {
    FATAL_ERROR( "The '%s' is not a regular file", basename( buf ) );
  }

  pkginfo = fopen( (const char *)&buf[0], "r" );
  if( !pkginfo )
  {
    FATAL_ERROR( "Cannot open '%s' file", basename( buf ) );
  }

  {
    char *ln   = NULL;
    char *line = NULL;

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

      if( (match = strstr( ln, "group" )) && match == ln ) {
        char *p = index( match, '=' ) + 1;
        if( p != NULL ) group = skip_spaces( p );
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

      if( (match = strstr( ln, "short_description" )) && match == ln ) {
        char *p = index( match, '=' ) + 1;
        if( p != NULL )
        {
          char *b =  index( p, '"'),
               *e = rindex( p, '"');
          if( b && e && ( b != e ) )
          {
            p = ++b; *e = '\0';
            short_description = strdup( (const char *)p );
          }
        }
      }
      if( (match = strstr( ln, "url" )) && match == ln ) {
        char *p = index( match, '=' ) + 1;
        if( p != NULL ) url = skip_spaces( p );
      }
      if( (match = strstr( ln, "license" )) && match == ln ) {
        char *p = index( match, '=' ) + 1;
        if( p != NULL ) license = skip_spaces( p );
      }
    }

    free( line );

    if( !pkgname || !pkgver || !arch || !distroname || !distrover )
    {
      FATAL_ERROR( "Invalid input .PKGINFO file" );
    }

    if( !url )     { url     = strdup( DISTRO_URL );     }
    if( !license ) { license = strdup( DISTRO_LICENSE ); }
  }

  fclose( pkginfo );
  free( buf );
}

static void tune_destinations( void )
{
  if( mkgroupdir && group )
  {
    char  *buf = NULL;

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    (void)strcpy( buf, (const char *)destination );
    (void)strcat( buf, "/" );
    (void)strcat( buf, (const char *)group );
    if( flavour )
    {
      (void)strcat( buf, "/" );
      (void)strcat( buf, (const char *)flavour );
    }

    if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      FATAL_ERROR( "Cannot create target directory" );
    }

    free( destination );
    destination = strdup( (const char *)&buf[0] );
    free( buf );
  }

  /* here we can allocate memory for output filenames */
}

/*********************************************
  .RESTORELINKS functions:
 */
static void start_restorelinks_file( void )
{
  char *tmp = NULL;

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( (char *)&tmp[0], "%s/%s", tmpdir, ".RESTORELINKS" );

  rlinks = fopen( (const char *)&tmp[0], "w" );
  if( !rlinks )
  {
    FATAL_ERROR( "Cannot create '.RESTORELINKS' file"  );
  }
  free( tmp );
}

static void stop_restorelinks_file( void )
{
  struct stat sb;
  char  *tmp = NULL, *cmd = NULL;
  int    len = 0;

  fflush( rlinks ); fclose( rlinks );

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( (char *)&tmp[0], "%s/%s", tmpdir, ".RESTORELINKS" );

  if( stat( tmp, &sb ) == 0 )
  {
    if( S_ISREG(sb.st_mode) && sb.st_size != 0 )
    {
      pid_t p = (pid_t) -1;
      int   rc;

      cmd = (char *)malloc( (size_t)PATH_MAX );
      if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)cmd, PATH_MAX );

      len = snprintf( &cmd[0], PATH_MAX, "cp %s %s/ > /dev/null 2>&1", tmp, srcdir );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( "Cannot create '.RESTORELINKS' file" );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
      if( rc != 0 )
      {
        FATAL_ERROR( "Cannot create '.RESTORELINKS' file" );
      }

      free( cmd );
    }
  }

  free( tmp );
}


static void save_link( const char *name_in, const char *name_to, const char *name_is )
{
  if( rlinks )
  {
    fprintf( rlinks, "( cd %s ; rm -rf %s )\n", name_in, name_is );
    fprintf( rlinks, "( cd %s ; ln -sf %s %s )\n", name_in, name_to, name_is );
  }
}

/*
  End of .RESTORELINKS functions.
 *********************************************/

/*********************************************
  File list functions:
 */
static void _print_filelist_entry( void *data, void *user_data )
{
  const char *path = (const char *)data;
  FILE *output = (FILE *)user_data;

  if( !output ) output = stdout;

  fprintf( output, "%s\n", path );
}

static void _free_filelist_entry( void *data, void *user_data )
{
  if( data ) { free( data ); }
}

static int _compare_fnames( const void *a, const void *b )
{
  const char *s1 = (const char *)a;
  const char *s2 = (const char *)b;

  return strcmp( s1, s2 );
}

static void _push_file( const char *name )
{
  char *fname = (char *)name + strlen( srcdir ) + 1;
  filelist = dlist_append( filelist, (void *)strdup( fname ) );
}

static void _push_dir( const char *name )
{
  char *buf   = NULL;
  char *dname = (char *)name + strlen( srcdir ) + 1;

  buf = (char *)malloc( strlen( dname ) + 2 );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  (void)sprintf( &buf[0], "%s/", dname );

  filelist = dlist_append( filelist, (void *)strdup( (const char *)&buf[0] ) );
  free( buf );
}

static void _list_files( const char *dirpath )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

  if( stat( dirpath, &path_sb ) == -1 )
  {
    FATAL_ERROR( "%s: Cannot stat source directory", dirpath );
  }

  if( S_ISDIR(path_sb.st_mode) == 0 )
  {
    FATAL_ERROR( "%s: Source path is not a directory", dirpath );
  }

  if( (dir = opendir(dirpath) ) == NULL )
  {
    FATAL_ERROR( "Canot access '%s' directory: %s", dirpath, strerror( errno ) );
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

    if( lstat( path, &entry_sb ) == 0 )
    {
      if( S_ISDIR(entry_sb.st_mode) )
      {
        _push_dir( (const char *)path );
        pkgsize += (size_t)entry_sb.st_size;
        _list_files( (const char *)path );
      }
      else
      {
        if( S_ISREG(entry_sb.st_mode) )
        {
          char *service = basename( path );

          /* skip service files: */
          if( strcmp( service, ".DESCRIPTION"  ) &&
              strcmp( service, ".FILELIST"     ) &&
              strcmp( service, ".INSTALL"      ) &&
              strcmp( service, ".PKGINFO"      ) &&
              strcmp( service, ".REQUIRES"     ) &&
              strcmp( service, ".RESTORELINKS" )   )
          {
            _push_file( (const char *)path );
            pkgsize += (size_t)entry_sb.st_size;
            ++nfiles;
          }
        }
        if( S_ISBLK(entry_sb.st_mode)  ||
            S_ISCHR(entry_sb.st_mode)  ||
            S_ISFIFO(entry_sb.st_mode) ||
            S_ISSOCK(entry_sb.st_mode)   )
        {
          _push_file( (const char *)path );
          ++nfiles;
        }
        if( S_ISLNK(entry_sb.st_mode) )
        {
          if( linkadd )
          {
            char   *buf = NULL;
            ssize_t len = 0;

            const char *in = (char *)dirpath + strlen( srcdir ) + 1;
            const char *is = (char *)path + strlen( dirpath ) + 1;

            buf = (char *)malloc( PATH_MAX );
            if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
            bzero( (void *)buf, PATH_MAX );

            if( (len = readlink( (const char *)path, buf, PATH_MAX - 1 )) == -1 )
            {
              FATAL_ERROR( "%s: Cannot read link: %s", is, strerror( errno ) );
            }
            buf[len] = '\0';
            save_link( in, (const char *)&buf[0], is );
            free( buf );

            /* remove the link: */
            (void)unlink( (const char *)path );
          }
          else
          {
            _push_file( (const char *)path );
            ++nfiles;
          }
        }

      } /* End if( S_ISDIR(entry_sb.st_mode) ) */

    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

static void create_file_list( void )
{
  start_restorelinks_file();
  _list_files( (const char *)srcdir );
  stop_restorelinks_file();

  if( filelist )
  {
    FILE *flist = NULL;
    char *tmp   = NULL;

    tmp = (char *)malloc( PATH_MAX );
    if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)tmp, PATH_MAX );

    (void)sprintf( (char *)&tmp[0], "%s/%s", srcdir, ".FILELIST" );

    flist = fopen( (const char *)&tmp[0], "w" );
    if( !flist )
    {
      FATAL_ERROR( "Cannot create '.FILELIST' file"  );
    }

    /*********************************
      save total number of files:
     */
    (void)sprintf( (char *)&tmp[0], "%d", nfiles );
    total_files = strdup( (const char *)&tmp[0] );

    /*********************************
      save uncompressed package size:
     */
    {
      int    nd;
      double sz = (double)pkgsize / (double)1024;

      if( sz > (double)1048576 )
      {
        sz = sz / (double)1048576;
        /*
          NOTE:
          ----
          Операция округления до одного знака после десятичной точки: sz = round(sz*10.0)/10.0;
          здесь не нужна; можно обойтись вычислением количества цифр, выводимых на печать с помощью
          формата '%.*g':

          Количество десятичных цифр, необходимое для предстваления целой части числа + 1(одна)
          десятичная цифра после десятичной точки. Формат %.*g не будет выводить дробную часть
          числа, если после округления, до одного знака после десятичной точки, дробная часть
          равна нулю:
         */
        nd = (int)ceil(log10(floor(sz) + 1.0)) + 1;
        (void)sprintf( (char *)&tmp[0], "%.*gG", nd, sz );
      }
      else if( sz > (double)1024 )
      {
        sz = sz / (double)1024;
        nd = (int)ceil(log10(floor(sz) + 1.0)) + 1;
        (void)sprintf( (char *)&tmp[0], "%.*gM", nd, sz );
      }
      else
      {
        nd = (int)ceil(log10(floor(sz) + 1.0)) + 1;
        (void)sprintf( (char *)&tmp[0], "%.*gK", nd, sz );
      }
    }
    uncompressed_size = strdup( (const char *)&tmp[0] );

    free( tmp );

    filelist = dlist_sort( filelist, _compare_fnames );
    dlist_foreach( filelist, _print_filelist_entry, flist );

    fflush( flist );
    fclose( flist );
  }
  else
  {
    FATAL_ERROR( "There are no files in the source package"  );
  }
}

static void free_file_list( void )
{
  if( filelist ) { dlist_free( filelist, _free_filelist_entry ); filelist = NULL; }
}
/*
  End of file list functions.
 *********************************************/

/*********************************************
  Description functions.
 */
static void get_short_description( char *buf, const char *line )
{
  char *s, *p, *q;

  if( buf ) { buf[0] = '\0'; s = buf; }
  if( !line || line[0] == '\0' ) return;

  p = index( line, '(' );
  q = index( line, ')' );
  if( p && q && q > p )
  {
    ++p;
    while( *p && p < q )
    {
      *s = *p;
      ++p; ++s;
    }
    *s = '\0';
  }
  else
  {
    /*
      If short description declaration is incorrect at first line
      of description; then we take whole first line of description:
     */
    p = index( line, ':' ); ++p;
    while( (*p == ' ' || *p == '\t') && *p != '\0' ) { ++p; }
    strcpy( buf, p );
  }
}

static char *read_short_description( FILE *fh )
{
  char *ret  = NULL;
  char *buf  = NULL;

  char *ln   = NULL;
  char *line = NULL;

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  /* Get short_description from PACKAGE DESCRIPTION */
  ln = fgets( line, PATH_MAX, fh );
  ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol */

  get_short_description( buf, (const char *)line );
  if( buf[0] != '\0' )
  {
    ret = strdup( buf );
  }
  free( buf );

  return ret;
}

static void create_description_file( void )
{
  struct stat sb;
  char  *buf  = NULL, *tmp = NULL;
  int    n = 0;

  char *ln   = NULL;
  char *line = NULL;

  FILE *srcdesc = NULL;
  FILE *tmpdesc = NULL;
  FILE *outdesc = NULL;

  buf = (char *)malloc( PATH_MAX );
  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)buf, PATH_MAX );

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  (void)sprintf( (char *)&tmp[0], "%s/%s", srcdir, ".DESCRIPTION" );

  if( stat( tmp, &sb ) == 0 )
  {
    if( S_ISREG(sb.st_mode) && sb.st_size != 0 )
    {
      srcdesc = fopen( (const char *)&tmp[0], "r" );
      if( !srcdesc )
      {
        FATAL_ERROR( "Cannot read source '.DESCRIPTION' file"  );
      }
      bzero( (void *)tmp, PATH_MAX );

      (void)sprintf( (char *)&tmp[0], "%s/%s", tmpdir, ".DESCRIPTION" );
      tmpdesc = fopen( (const char *)&tmp[0], "w+" );
      if( !tmpdesc )
      {
        FATAL_ERROR( "Cannot create output '.DESCRIPTION' file"  );
      }
      bzero( (void *)tmp, PATH_MAX );

      (void)sprintf( (char *)&tmp[0], "%s/%s-%s-%s-%s-%s.txt",
                                       destination, pkgname, pkgver, arch, distroname, distrover );
      outdesc = fopen( (const char *)&tmp[0], "w" );
      if( !outdesc )
      {
        FATAL_ERROR( "Cannot create output '.DESCRIPTION' file"  );
      }
      bzero( (void *)tmp, PATH_MAX );

      (void)sprintf( (char *)&buf[0], "%s:", pkgname );

      fprintf( outdesc, "\n/* begin *\n\n" );

      while( (ln = fgets( line, PATH_MAX, srcdesc )) && n < DESCRIPTION_NUMBER_OF_LINES )
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

          fprintf( tmpdesc, "%s\n", match );
          match += plen + 1;
          if( match[0] != '\0' ) { fprintf( outdesc, "   %s\n", match ); }
          else                   { fprintf( outdesc, "\n" );             }
          ++n;
        }
      }

      fprintf( outdesc, " * end */\n" );

      if( !short_description )
      {
        /* try to get short description from .DESCRIPTION file */
        fseek( tmpdesc, 0, SEEK_SET );
        short_description = read_short_description( tmpdesc );
      }

      fflush( tmpdesc ); fclose( tmpdesc );
      fflush( outdesc ); fclose( outdesc );
      fclose( srcdesc );

      /* Copy tmpdesc file to the source package directory: */
      {
        char *cmd = NULL;

        bzero( (void *)tmp, PATH_MAX );
        (void)sprintf( (char *)&tmp[0], "%s/%s", tmpdir, ".DESCRIPTION" );

        if( stat( tmp, &sb ) == 0 )
        {
          if( S_ISREG(sb.st_mode) && sb.st_size != 0 )
          {
            pid_t p = (pid_t) -1;
            int   rc, len = 0;

            cmd = (char *)malloc( (size_t)PATH_MAX );
            if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
            bzero( (void *)cmd, PATH_MAX );

            len = snprintf( &cmd[0], PATH_MAX, "cp %s %s/ > /dev/null 2>&1", tmp, srcdir );
            if( len == 0 || len == PATH_MAX - 1 )
            {
              FATAL_ERROR( "Cannot create output '.DESCRIPTION' file" );
            }
            p = sys_exec_command( cmd );
            rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
            if( rc != 0 )
            {
              FATAL_ERROR( "Cannot create output '.DESCRIPTION' file" );
            }

            free( cmd );
          }
        }
      } /* End of copy tmpdesc file. */
    }
  }

  free( line );
  free( tmp );
  free( buf );
}
/*
  End of description functions.
 *********************************************/

static void rewrite_pkginfo_file( void )
{
  FILE *info = NULL;
  char *tmp  = NULL;

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  (void)sprintf( (char *)&tmp[0], "%s/%s", srcdir, ".PKGINFO" );

  info = fopen( (const char *)&tmp[0], "w" );
  if( !info )
  {
    FATAL_ERROR( "Cannot create '.PKGINFO' file"  );
  }

  if( pkgname )
  {
    (void)sprintf( (char *)&tmp[0], "pkgname=%s\n", pkgname );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( pkgver )
  {
    (void)sprintf( (char *)&tmp[0], "pkgver=%s\n", pkgver );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( arch )
  {
    (void)sprintf( (char *)&tmp[0], "arch=%s\n", arch );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( distroname )
  {
    (void)sprintf( (char *)&tmp[0], "distroname=%s\n", distroname );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( distrover )
  {
    (void)sprintf( (char *)&tmp[0], "distrover=%s\n", distrover );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( group )
  {
    (void)sprintf( (char *)&tmp[0], "group=%s\n", group );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( short_description )
  {
    (void)sprintf( (char *)&tmp[0], "short_description=\"%s\"\n", short_description );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( url )
  {
    (void)sprintf( (char *)&tmp[0], "url=%s\n", url );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( license )
  {
    (void)sprintf( (char *)&tmp[0], "license=%s\n", license );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( uncompressed_size )
  {
    (void)sprintf( (char *)&tmp[0], "uncompressed_size=%s\n", uncompressed_size );
    fprintf( info, (const char *)&tmp[0] );
  }
  if( total_files )
  {
    (void)sprintf( (char *)&tmp[0], "total_files=%s\n", total_files );
    fprintf( info, (const char *)&tmp[0] );
  }

  free( tmp );

  fflush( info );
  fclose( info );
}

static const char *fill_compressor( char *buffer, char compressor )
{
  switch( compressor )
  {
    default:
    case 'J':
      (void)sprintf( buffer, "xz -9 --threads=%d -c", get_nprocs() );
      break;
    case 'j':
      (void)sprintf( buffer, "bzip2 -9 -c" );
      break;
    case 'z':
      (void)sprintf( buffer, "gzip -9 -c" );
      break;
  }
  return (const char *)buffer;
}

static void create_package( void )
{
  pid_t p = (pid_t) -1;
  int   rc, len = 0;

  char *tmp = NULL, *cwd = NULL, *dst = NULL, *cmd = NULL;

#define tar_suffix ".tar"
#define sha_suffix ".sha"
#define asc_suffix ".asc"
#define ACLS       "--acls"
#define XATTRS     "--xattrs"
#define HASHER     "sha256sum -b"

  char compressor[64];
  (void)fill_compressor( (char *)&compressor[0], compress );

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  cwd = (char *)malloc( PATH_MAX );
  if( !cwd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cwd, PATH_MAX );

  dst = (char *)malloc( PATH_MAX );
  if( !dst ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)dst, PATH_MAX );

  cmd = (char *)malloc( PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  /* absolute current directory path: */
  if( getcwd( cwd, (size_t)PATH_MAX ) == NULL )
  {
    FATAL_ERROR( "%s-%s-%s-%s-%s%s: Cannot create PACKAGE: %s",
                  pkgname, pkgver, arch, distroname, distrover, txz_suffix, strerror( errno ) );
  }
  remove_trailing_slash( cwd );

  /****************************
    absolute destination path:
   */
  (void)sprintf( (char *)&tmp[0], "%s", destination );
  if( tmp[0] != '/' )
  {
    (void)sprintf( (char *)&dst[0], "%s/%s/%s", cwd, dirname( (char *)&tmp[0] ), basename( destination ) );
  }
  else
  {
    (void)sprintf( (char *)&dst[0], "%s", destination );
  }

  /*****************************************
    change CWD to source package directory:
   */
  if( chdir( (const char *)srcdir ) == -1 )
  {
    FATAL_ERROR( "Cannot change CWD to the package source directory" );
  }

  /************************************
    Set mode 0755 for .INSTALL script:
   */
  (void)sprintf( (char *)&cmd[0], "chmod 0755 ./.INSTALL" );
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot make .INSTAL script executable" );
  }

  /**********************************
    push package files into tarball:
   */
  len = snprintf( (char *)&cmd[0], PATH_MAX,
    "find ./ | sed 's,^\\./,,' | "
              "sed 's,\\.DESCRIPTION,,'  | "
              "sed 's,\\.FILELIST,,'     | "
              "sed 's,\\.INSTALL,,'      | "
              "sed 's,\\.PKGINFO,,'      | "
              "sed 's,\\.REQUIRES,,'     | "
              "sed 's,\\.RESTORELINKS,,' | "
              "tar --no-recursion %s %s -T - -cvf %s/%s-%s-%s-%s-%s%s",
    ACLS, XATTRS, dst, pkgname, pkgver, arch, distroname, distrover, tar_suffix );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot push package files into tarball" );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot push package files into tarball" );
  }

  /**********************************
    push service files into tarball:
   */
  len = snprintf( (char *)&cmd[0], PATH_MAX,
    "find ./ -type f \\( -name '.DESCRIPTION' -o "
                        "-name '.FILELIST'    -o "
                        "-name '.INSTALL'     -o "
                        "-name '.PKGINFO'     -o "
                        "-name '.REQUIRES'    -o "
                        "-name '.RESTORELINKS' \\) | "
              "sed 's,^\\./,,' | "
              "tar --no-recursion %s %s -T - --append -f %s/%s-%s-%s-%s-%s%s",
    ACLS, XATTRS, dst, pkgname, pkgver, arch, distroname, distrover, tar_suffix );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot push service files into tarball" );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot push service files into tarball" );
  }

  /**********************************
    push service files into tarball:
   */
  len = snprintf( (char *)&cmd[0], PATH_MAX,
    "cat %s/%s-%s-%s-%s-%s%s | %s > %s/%s-%s-%s-%s-%s%s",
         dst, pkgname, pkgver, arch, distroname, distrover, tar_suffix,
         compressor, dst, pkgname, pkgver, arch, distroname, distrover, txz_suffix );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot compress tarball" );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot compress tarball" );
  }

  /******************************
    remove uncompressed tarball:
   */
  len = snprintf( (char *)&cmd[0], PATH_MAX,
    "rm -f %s/%s-%s-%s-%s-%s%s",
         dst, pkgname, pkgver, arch, distroname, distrover, tar_suffix );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot remove umcompressed tarball" );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot remove umcompressed tarball" );
  }

  /***********************************
    NOTE:
      To check SHA sum we can make use 'shasum' utility:
      $ ( cd destination ; shasum --check filename.sha )
      without explicitly indicated algorithm [-a 256].

    generate SHA-256 sum of tarball:
   */
  len = snprintf( (char *)&cmd[0], PATH_MAX,
    "%s %s/%s-%s-%s-%s-%s%s | sed 's,%s/,,' > %s/%s-%s-%s-%s-%s%s", HASHER,
     dst, pkgname, pkgver, arch, distroname, distrover, txz_suffix,
      dst, dst, pkgname, pkgver, arch, distroname, distrover, sha_suffix );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot generate SHA-256 sum of package tarball" );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    FATAL_ERROR( "Cannot generate SHA-256 sum of package tarball" );
  }

#if defined( HAVE_GPG2 )
  /*******************************
    generate GPG ascii-signature:
   */
  if( passphrase && key_id )
  {

    len = snprintf( (char *)&cmd[0], PATH_MAX,
      "cat %s | gpg2 -u %s --batch --passphrase-fd=0 --pinentry-mode=loopback"
               " --armor --yes --emit-version"
               " --comment %s-%s"
               " -o %s/%s-%s-%s-%s-%s%s"
               " --detach-sign %s/%s-%s-%s-%s-%s%s",
      passphrase, key_id,
      pkgname, pkgver,
      dst, pkgname, pkgver, arch, distroname, distrover, asc_suffix,
      dst, pkgname, pkgver, arch, distroname, distrover, txz_suffix );
    if( len == 0 || len == PATH_MAX - 1 )
    {
      FATAL_ERROR( "Cannot generate GPG signature of package tarball" );
    }
    p = sys_exec_command( cmd );
    rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
    if( rc != 0 )
    {
      ERROR( "Cannot generate GPG signature of package tarball" );
    }
  }
#endif

  /******************
    change CWD back:
   */
  if( chdir( (const char *)&cwd[0] ) == -1 )
  {
    FATAL_ERROR( "Cannot change CWD back" );
  }

  fprintf( stdout, "\n%s package %s/%s-%s-%s-%s-%s%s has been created.\n\n",
                      DISTRO_CAPTION,
                        destination, pkgname, pkgver, arch, distroname, distrover, txz_suffix );

  free( cmd );
  free( dst );
  free( cwd );
  free( tmp );
}



/*********************************************
  Get directory where this program is placed:
 */
char *get_selfdir( void )
{
  char    *buf = NULL;
  ssize_t  len;

  buf = (char *)malloc( (size_t)PATH_MAX );
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

  read_pkginfo();
  tune_destinations();
  create_file_list();
  create_description_file();
  /*
    NOTE:
      rewrite_pkginfo_file() should be called after create_description_file()
      because if there is no short description in the source .PKGINFO file
      then we can try to get the short description from the first line of
      .DESCRIPTION file (between opening and closing round brackets).
   */
  rewrite_pkginfo_file();
  create_package();


  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
