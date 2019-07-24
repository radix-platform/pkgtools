
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

#include <sys/resource.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <msglog.h>
#include <system.h>

#define PROGRAM_NAME "pkglog"

#include <defs.h>


char *program     = PROGRAM_NAME;
char *destination = NULL, *srcdir = NULL, *pkginfo_fname = NULL, *output_fname = NULL;
int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

int   mkgroupdir  = 0;

int   rm_srcdir_at_exit = 0;

char           *pkgname = NULL,
                *pkgver = NULL,
                  *arch = NULL,
            *distroname = NULL,
             *distrover = NULL,
                 *group = NULL,
                   *url = NULL,
               *license = NULL,
     *uncompressed_size = NULL,
           *total_files = NULL;

FILE *pkginfo = NULL;
FILE *output  = NULL;


#define FREE_PKGLOG_VARIABLES() \
  if( pkgname )           { free( pkgname );           } pkgname = NULL;            \
  if( pkgver )            { free( pkgver );            } pkgver = NULL;             \
  if( arch )              { free( arch );              } arch = NULL;               \
  if( distroname )        { free( distroname );        } distroname = NULL;         \
  if( distrover )         { free( distrover );         } distrover = NULL;          \
  if( group )             { free( group );             } group = NULL;              \
  if( url )               { free( url );               } url = NULL;                \
  if( license )           { free( license );           } license = NULL;            \
  if( uncompressed_size ) { free( uncompressed_size ); } uncompressed_size = NULL;  \
  if( total_files )       { free( total_files );       } total_files = NULL

void free_resources()
{
  if( selfdir )       { free( selfdir );       selfdir       = NULL; }
  if( srcdir )        { free( srcdir );        srcdir        = NULL; }
  if( destination )   { free( destination );   destination   = NULL; }
  if( pkginfo_fname ) { free( pkginfo_fname ); pkginfo_fname = NULL; }
  if( output_fname )
  {
    if( output ) { (void)fflush( output ); fclose( output ); output = NULL; }
    free( output_fname ); output_fname = NULL;
  }

  FREE_PKGLOG_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <dir|pkginfo|package>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Read information from package's service files and create PKGLOG file\n" );
  fprintf( stdout, "in the destination directory. <pkginfo> is a full name of '.PKGINFO'\n" );
  fprintf( stdout, "service file of package.  Rest of package's  serfice files should be\n" );
  fprintf( stdout, "present in  the  directory of '.PKGINFO'.  If last argument is <dir>\n" );
  fprintf( stdout, "then pkglog utility will try to read '.PKGINFO' and rest of service\n" );
  fprintf( stdout, "files from directory given by <dir> argument.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                  Display this information.\n" );
  fprintf( stdout, "  -v,--version               Display the version of %s utility.\n", program );
  fprintf( stdout, "  -d,--destination=<DIR>     Target directory to save output PKGLOG.\n" );
  fprintf( stdout, "  -m,--mkgroupdir            Create group subdirectory in the PKGLOG\n" );
  fprintf( stdout, "                             target directory.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <dir|pkginfo|package>      Directory wich contains the package's\n"  );
  fprintf( stdout, "                             service files or path to .PKGINFO file\n"  );
  fprintf( stdout, "                             or package tarball.\n"  );
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
  if( rm_srcdir_at_exit ) _rm_tmpdir( (const char *)srcdir );
  if( output_fname )
  {
    if( output ) { (void)fflush( output ); fclose( output ); output = NULL; }
    (void)unlink( output_fname );
    free( output_fname ); output_fname = NULL;
  }
  free_resources();
}

void sigint( int signum )
{
  (void)signum;

  if( rm_srcdir_at_exit ) _rm_tmpdir( (const char *)srcdir );
  if( output_fname )
  {
    if( output ) { (void)fflush( output ); fclose( output ); output = NULL; }
    (void)unlink( output_fname );
    free( output_fname ); output_fname = NULL;
  }
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

enum _pkginfo_type
{
  PKGINFO_TEXT = 0,
  PKGINFO_GZ,
  PKGINFO_BZ2,
  PKGINFO_XZ,
  PKGINFO_TAR,

  PKGINFO_UNKNOWN
};

static enum _pkginfo_type pkginfo_type = PKGINFO_UNKNOWN;
static char uncompress[2] = { 0, 0 };


static enum _pkginfo_type check_pkginfo_file( const char *fname )
{
  struct stat st;
  size_t pkginfo_size = 0;
  unsigned char buf[8];
  int rc, fd;

  /* SIGNATURES: https://www.garykessler.net/library/file_sigs.html */

  uncompress[0] = '\0';

  if( stat( fname, &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access %s file: %s", basename( (char *)fname ), strerror( errno ) );
  }

  pkginfo_size = st.st_size;

  if( (fd = open( fname, O_RDONLY )) == -1 )
  {
    FATAL_ERROR( "Cannot open %s file: %s", basename( (char *)fname ), strerror( errno ) );
  }

  rc = (int)read( fd, (void *)&buf[0], 7 );
  if( rc != 7 )
  {
    FATAL_ERROR( "Unknown type of input file %s", basename( (char *)fname ) );
  }
  buf[7] = '\0';

  /* TEXT */
  if( !strncmp( (const char *)&buf[0], "pkgname", 7 ) )
  {
    close( fd ); return PKGINFO_TEXT;
  }

  /* GZ */
  if( buf[0] == 0x1F && buf[1] == 0x8B && buf[2] == 0x08 )
  {
    uncompress[0] = 'x';
    close( fd ); return PKGINFO_GZ;
  }

  /* BZ2 */
  if( buf[0] == 0x42 && buf[1] == 0x5A && buf[2] == 0x68 )
  {
    uncompress[0] = 'j';
    close( fd ); return PKGINFO_BZ2;
  }

  /* XZ */
  if( buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A &&
      buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00   )
  {
    uncompress[0] = 'J';
    close( fd ); return PKGINFO_XZ;
  }

  if( pkginfo_size > 262 )
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
      close( fd ); return PKGINFO_TAR;
    }
  }

  close( fd ); return PKGINFO_UNKNOWN;
}


void get_args( int argc, char *argv[] )
{
  const char* short_options = "hvmd:";

  const struct option long_options[] =
  {
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { "destination", required_argument, NULL, 'd' },
    { "mkgroupdir",  no_argument,       NULL, 'm' },
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

      case '?': default:
      {
        usage();
        break;
      }
    }
  }

  if( destination == NULL )
  {
    char cwd[PATH_MAX];
    if( getcwd( cwd, sizeof(cwd) ) != NULL )
      destination = strdup( cwd );
    else
      destination = strdup( "." );
  }

  /* last command line argument is the PKGLOG file */
  if( optind < argc )
  {
    struct stat st;
    char  *buf = NULL;

    buf = (char *)malloc( strlen( (const char *)argv[optind] ) + 10 );
    if( !buf )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    (void)strcpy( buf, (const char *)argv[optind++] );
    remove_trailing_slash( (char *)&buf[0] );

    if( stat( (const char *)&buf[0], &st ) == -1 )
    {
      FATAL_ERROR( "Cannot access '%s' file: %s", basename( buf ), strerror( errno ) );
    }

    if( S_ISDIR(st.st_mode) )
    {
      /* Add .PKGINFO to the input dir name: */
      (void)strcat( buf, "/.PKGINFO" );
    }

    pkginfo_fname = strdup( (const char *)&buf[0] );
    if( pkginfo_fname == NULL )
    {
      usage();
    }

    free( buf );

    pkginfo_type = check_pkginfo_file( (const char *)pkginfo_fname );
    if( pkginfo_type == PKGINFO_UNKNOWN )
    {
      ERROR( "%s: Unknown input file format", basename( pkginfo_fname ) );
      usage();
    }
  }
  else
  {
    usage();
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


void write_pkginfo( void )
{
  char *ln   = NULL;
  char *line = NULL;

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
    /* variable short_description="..." is not stored in the PKGLOG file */
    if( (match = strstr( ln, "url" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) url = skip_spaces( p );
    }
    if( (match = strstr( ln, "license" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) license = skip_spaces( p );
    }
    if( (match = strstr( ln, "uncompressed_size" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) uncompressed_size = skip_spaces( p );
    }
    if( (match = strstr( ln, "total_files" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) total_files = skip_spaces( p );
    }
  }

  free( line );

  if( pkgname && pkgver && arch && distroname && distrover )
  {
    int   len;
    char *buf = NULL;

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    if( mkgroupdir && group )
    {
      len = snprintf( buf, PATH_MAX, "%s/%s",
                                      destination, group );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( "Cannot create output file" );
      }

      if( _mkdir_p( buf, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
      {
        FATAL_ERROR( "Cannot create output directory" );
      }

      len = snprintf( buf, PATH_MAX, "%s/%s/%s-%s-%s-%s-%s",
                                      destination, group, pkgname, pkgver, arch, distroname, distrover );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( "Cannot create output file" );
      }
    }
    else
    {
      len = snprintf( buf, PATH_MAX, "%s/%s-%s-%s-%s-%s",
                                      destination, pkgname, pkgver, arch, distroname, distrover );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( "Cannot create output file" );
      }
    }
    output_fname = strdup( (const char *)&buf[0] );
    if( output_fname )
    {
      output = fopen( (const char *)output_fname, "w" );
      if( !output )
      {
        FATAL_ERROR( "Cannot create %s file", output_fname );
      }
    }

    free( buf );

    fprintf( output, "PACKAGE NAME: %s\n",    pkgname    );
    fprintf( output, "PACKAGE VERSION: %s\n", pkgver     );
    fprintf( output, "ARCH: %s\n",            arch       );
    fprintf( output, "DISTRO: %s\n",          distroname );
    fprintf( output, "DISTRO VERSION: %s\n",  distrover  );
  }
  else
  {
    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }

  if( group )
    fprintf( output, "GROUP: %s\n", group );
  if( url )
    fprintf( output, "URL: %s\n", url );
  if( license )
    fprintf( output, "LICENSE: %s\n", license );
  if( uncompressed_size )
    fprintf( output, "UNCOMPRESSED SIZE: %s\n", uncompressed_size );
  if( total_files )
    fprintf( output, "TOTAL FILES: %s\n", total_files );

  /* reference counter of not installed package is always zero */
  fprintf( output, "REFERENCE COUNTER: 0\n" );
}


void write_requires( void )
{
  struct stat sb;
  char  *buf = NULL;

  if( output == NULL && output_fname == NULL )
  {
    FATAL_ERROR( "Unable to access output file" );
  }

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  fprintf( output, "REQUIRES:\n" );

  bzero( (void *)buf, PATH_MAX );
  (void)sprintf( (char *)&buf[0], "%s/.REQUIRES", srcdir );

  /* check if path exists and is a regular file */
  if( stat( (const char *)&buf[0], &sb ) == 0 && S_ISREG(sb.st_mode) )
  {
    char *ln   = NULL;
    char *line = NULL;

    FILE *input;

    input = fopen( (const char *)&buf[0], "r" );
    if( !input )
    {
      FATAL_ERROR( "Unable to access %s file", (char *)&buf[0] );
    }

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    /* cat .REQUIRES >> PKGLOG */
    while( (ln = fgets( line, PATH_MAX, input )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      /* print non-empty lines */
      if( *ln )
      {
        fprintf( output, "%s\n", ln );
      }
    }
    free( line );
  }
  free( buf );
}


void write_description( void )
{
  struct stat sb;
  char  *buf = NULL;

  if( output == NULL && output_fname == NULL )
  {
    FATAL_ERROR( "Unable to access output file" );
  }

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  fprintf( output, "PACKAGE DESCRIPTION:\n" );

  bzero( (void *)buf, PATH_MAX );
  (void)sprintf( (char *)&buf[0], "%s/.DESCRIPTION", srcdir );

  /* check if path exists and is a regular file */
  if( stat( (const char *)&buf[0], &sb ) == 0 && S_ISREG(sb.st_mode) )
  {
    char *ln      = NULL;
    char *line    = NULL;
    char *pattern = NULL;
    int   n = 0;

    FILE *input;

    input = fopen( (const char *)&buf[0], "r" );
    if( !input )
    {
      FATAL_ERROR( "Unable to access %s file", (char *)&buf[0] );
    }

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )    { FATAL_ERROR( "Cannot allocate memory" ); }

    pattern = (char *)malloc( (size_t)strlen( pkgname ) + 2 );
    if( !pattern ) { FATAL_ERROR( "Cannot allocate memory" ); }

    (void)sprintf( pattern, "%s:", pkgname );

    /* cat .DESCRIPTION >> PKGLOG */
    while( (ln = fgets( line, PATH_MAX, input )) )
    {
      char *match = NULL;

      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      /*
        skip non-significant spaces at beginning of line
        and print lines started with 'pkgname:'
       */
      if( (match = strstr( ln, pattern )) && n < DESCRIPTION_NUMBER_OF_LINES )
      {
        int mlen   = strlen( match ), plen = strlen( pattern );
        int length = ( mlen > plen )  ? (mlen - plen - 1) : 0 ;

        if( length > DESCRIPTION_LENGTH_OF_LINE )
        {
          /* WARNING( "Package DESCRIPTION contains lines with length greater than %d characters", DESCRIPTION_LENGTH_OF_LINE ); */
          match[plen + 1 + DESCRIPTION_LENGTH_OF_LINE] = '\0'; /* truncating description line  */
          skip_eol_spaces( match );                            /* remove spaces at end-of-line */
        }
        fprintf( output, "%s\n", match );
        ++n;
      }
    }

    if( n < DESCRIPTION_NUMBER_OF_LINES )
    {
      /* WARNING( "Package DESCRIPTION contains less than %d lines", DESCRIPTION_NUMBER_OF_LINES ); */
      while( n < DESCRIPTION_NUMBER_OF_LINES )
      {
        fprintf( output, "%s\n", pattern );
        ++n;
      }
    }

    free( pattern );
    free( line );
  }

  free( buf );
}


void write_restore_links( void )
{
  struct stat sb;
  char  *buf = NULL;

  if( output == NULL && output_fname == NULL )
  {
    FATAL_ERROR( "Unable to access output file" );
  }

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  fprintf( output, "RESTORE LINKS:\n" );

  bzero( (void *)buf, PATH_MAX );
  (void)sprintf( (char *)&buf[0], "%s/.RESTORELINKS", srcdir );

  /* check if path exists and is a regular file */
  if( stat( (const char *)&buf[0], &sb ) == 0 && S_ISREG(sb.st_mode) )
  {
    char *ln   = NULL;
    char *line = NULL;

    FILE *input;

    input = fopen( (const char *)&buf[0], "r" );
    if( !input )
    {
      FATAL_ERROR( "Unable to access %s file", (char *)&buf[0] );
    }

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    /* cat .REQUIRES >> PKGLOG */
    while( (ln = fgets( line, PATH_MAX, input )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      /* print non-empty lines */
      if( *ln )
      {
        fprintf( output, "%s\n", ln );
      }
    }

    free( line );
  }

  free( buf );
}


void write_install_script( void )
{
  struct stat sb;
  char  *buf = NULL;

  if( output == NULL && output_fname == NULL )
  {
    FATAL_ERROR( "Unable to access output file" );
  }

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  fprintf( output, "INSTALL SCRIPT:\n" );

  bzero( (void *)buf, PATH_MAX );
  (void)sprintf( (char *)&buf[0], "%s/.INSTALL", srcdir );

  /* check if path exists and is a regular file */
  if( stat( (const char *)&buf[0], &sb ) == 0 && S_ISREG(sb.st_mode) )
  {
    char *ln   = NULL;
    char *line = NULL;

    FILE *input;

    input = fopen( (const char *)&buf[0], "r" );
    if( !input )
    {
      FATAL_ERROR( "Unable to access %s file", (char *)&buf[0] );
    }

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    /* cat .REQUIRES >> PKGLOG */
    while( (ln = fgets( line, PATH_MAX, input )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      /* print all lines */
      fprintf( output, "%s\n", ln );
    }
    free( line );
  }
  else
  {
    FATAL_ERROR( "Package doesn't contains the INSTALL script" );
  }

  free( buf );
}


void write_file_list( void )
{
  struct stat sb;
  char  *buf = NULL;

  if( output == NULL && output_fname == NULL )
  {
    FATAL_ERROR( "Unable to access output file" );
  }

  buf = (char *)malloc( (size_t)PATH_MAX );
  if( !buf )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  fprintf( output, "FILE LIST:\n" );

  bzero( (void *)buf, PATH_MAX );
  (void)sprintf( (char *)&buf[0], "%s/.FILELIST", srcdir );

  /* check if path exists and is a regular file */
  if( stat( (const char *)&buf[0], &sb ) == 0 && S_ISREG(sb.st_mode) )
  {
    char *ln;
    char *line = NULL;

    FILE *input;

    input = fopen( (const char *)&buf[0], "r" );
    if( !input )
    {
      FATAL_ERROR( "Unable to access %s file", (char *)&buf[0] );
    }

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    /* cat .REQUIRES >> PKGLOG */
    while( (ln = fgets( line, PATH_MAX, input )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      /* print non-empty lines */
      if( *ln )
      {
        fprintf( output, "%s\n", ln );
      }
    }

    free( line );
  }
  else
  {
    FATAL_ERROR( "Package doesn't contains the FILE list" );
  }

  free( buf );
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

  if( pkginfo_type != PKGINFO_TEXT )
  {
    /* Create tmpdir */
    srcdir = _mk_tmpdir();
    if( !srcdir )
    {
      FATAL_ERROR( "Cannot create temporary dir" );
    }
    rm_srcdir_at_exit = 1;

    /* Unpack SERVICE files */
    {
      pid_t p = (pid_t) -1;
      int   rc;

      int   len = 0;
      char *cmd = NULL, *errmsg = NULL, *wmsg = NULL;

      cmd = (char *)malloc( (size_t)PATH_MAX );
      if( !cmd )    { FATAL_ERROR( "Cannot allocate memory" ); }

      errmsg = (char *)malloc( (size_t)PATH_MAX );
      if( !errmsg ) { FATAL_ERROR( "Cannot allocate memory" ); }

      wmsg = (char *)malloc( (size_t)PATH_MAX );
      if( !wmsg )   { FATAL_ERROR( "Cannot allocate memory" ); }

      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get SERVICE files from %s file", basename( pkginfo_fname ) );

      len = snprintf( &cmd[0], PATH_MAX, "tar -C %s -x%sf %s %s > /dev/null 2>&1",
                                          srcdir, uncompress, pkginfo_fname,
                                         ".PKGINFO .REQUIRES .DESCRIPTION .RESTORELINKS .INSTALL .FILELIST" );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( errmsg );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 )
      {
        if( ! DO_NOT_WARN_ABOUT_SERVICE_FILES )
        {
          /*****************************************
            if( rc > 0 ) { return TAR exit status }
            else         { return EXIT_FAILURE    }
           */
          if( rc > 0 ) exit_status = rc - 1; /* ERROR() will add one */
          ERROR( errmsg );
          if( fatal_error_hook) fatal_error_hook();
          exit( exit_status );
        }
      }

      if( cmd )    free( cmd );
      if( errmsg ) free( errmsg );
      if( wmsg )   free( wmsg );
    }

    /* Change input pkginfo file name and type */
    if( pkginfo_fname )
    {
      char *buf = NULL;

      buf = (char *)malloc( strlen( pkginfo_fname ) + 10 );
      if( !buf )
      {
        FATAL_ERROR( "Cannot allocate memory" );
      }

      (void)sprintf( &buf[0], "%s/.PKGINFO", srcdir );

      free( pkginfo_fname ); pkginfo_fname = NULL;

      pkginfo_fname = strdup( buf );
      free( buf );
      pkginfo_type  = PKGINFO_TEXT;
    }

  }
  else /* TEXT: */
  {
    char *buf = NULL;

    buf = (char *)malloc( (size_t)strlen( pkginfo_fname ) + 1 );
    if( !buf )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    /* function dirname() spoils the source contents: */
    (void)sprintf( buf, "%s", pkginfo_fname );

    srcdir = strdup( dirname( (char *)&buf[0] ) );
    free( buf );
    rm_srcdir_at_exit = 0;
  }


  write_pkginfo();
  write_requires();
  write_description();
  write_restore_links();
  write_install_script();
  write_file_list();


  if( rm_srcdir_at_exit ) _rm_tmpdir( (const char *)srcdir );
  free_resources();

  exit( exit_status );
}
