
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


#include <msglog.h>
#include <system.h>

#include <dlist.h>
#include <pkglist.h>

#define PROGRAM_NAME "make-pkglist"

#include <defs.h>


char *program = PROGRAM_NAME;
char *srcdir = NULL, *pkglist_fname = NULL,
     *tmpdir = NULL;

struct pkg *srcpkg = NULL;

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

int __done = 0, __child = 0;

enum _output_format {
  OFMT_LIST = 0,
  OFMT_JSON,

  OFMT_UNKNOWN
} output_format = OFMT_LIST;

enum _input_type {
  IFMT_PKG = 0,
  IFMT_LOG,

  IFMT_UNKNOWN
} input_format = IFMT_UNKNOWN;

enum _priority priority = REQUIRED;


void free_resources()
{
  if( selfdir )        { free( selfdir );        selfdir        = NULL; }
  if( srcdir )         { free( srcdir );         srcdir         = NULL; }
  if( srcpkg )         { pkg_free( srcpkg );     srcpkg         = NULL; }
  if( pkglist_fname )  { free( pkglist_fname );  pkglist_fname  = NULL; }

  free_tarballs();
  free_packages();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <pkglist>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Create Packages List in the installation order from set of PKGLOGs\n" );
  fprintf( stdout, "or  set of PACKAGEs placed in the source directory.  If the source\n" );
  fprintf( stdout, "directory is not defined then directory of <pkglist>  will be used\n" );
  fprintf( stdout, "as source PACKAGE directory.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  -s,--source=<DIR|pkg|log>     Setup Database packages directory or directory\n" );
  fprintf( stdout, "                                where set of PACKAGEs is placed.\n" );
  fprintf( stdout, "                                If the source argument is a package or PKGLOG\n" );
  fprintf( stdout, "                                file, then the source directory will be represented\n" );
  fprintf( stdout, "                                as the base directory of the source file excluding\n" );
  fprintf( stdout, "                                the group directory if defined.\n" );
  fprintf( stdout, "  -i,--iformat=<pkg|log>        Input format: PACKAGEs or PKGLOGs.\n" );
  fprintf( stdout, "  -o,--oformat=<list|json>      Output format: LIST or JSON.\n" );

  fprintf( stdout, "  -m,--minimize                 Create .min.json files. Applicable\n" );
  fprintf( stdout, "                                for JSON output format.\n" );

  fprintf( stdout, "  -p,--prioriy=<PRIORITY>       Default install priority: REQ|REC|OPT|SKP.\n" );
  fprintf( stdout, "  -w,--hardware=<HARDWARE>      Optional Hardware Name used\n" );
  fprintf( stdout, "                                for JSON output format.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <pkglist>                     Output PKGLIST file name or a target\n"  );
  fprintf( stdout, "                                directory to save output PKGLIST.\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "If hardware is not defined by option --hardware then hardware will\n" );
  fprintf( stdout, "be set as basename  of target <pkglist> file in lower case without\n" );
  fprintf( stdout, "extension.\n" );
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

void sigchld( int signum )
{
  pid_t  pid = 0;
  int    status;

  (void)signum;

  while( (pid = waitpid( -1, &status, WNOHANG )) > 0 )
  {
    ; /* One of children with 'pid' is terminated */

    if( WIFEXITED( status ) )
    {
      if( (int) WEXITSTATUS (status) > 0 )
      {
        ++exit_status; /* printf( "Child %d returned non zero status: %d\n", pid, (int)WEXITSTATUS (status) ); */
      }
      else
      {
        ; /* printf( "Child %d terminated with status: %d\n", pid, (int)WEXITSTATUS (status) ); */
      }
    }
    else if( WIFSIGNALED( status ) )
    {
      ++exit_status; /* printf( "Child %d terminated on signal: %d\n", pid,  WTERMSIG( status ) ); */
    }
    else
    {
      ++exit_status; /* printf( "Child %d terminated on unknown reason\n", pid ); */
    }

  }

  if( pid == -1 && errno == ECHILD )
  {
    /* No child processes: */
    __done = 1;
  }
  return;
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

  /* System V fork+wait does not work if SIGCHLD is ignored */
  memset( &sa, 0, sizeof( sa ) );
  sa.sa_handler = sigchld;         /* CHLD */
  sa.sa_flags = SA_RESTART;
  sigemptyset( &set );
  sigaddset( &set, SIGCHLD );
  sa.sa_mask = set;
  sigaction( SIGCHLD, &sa, NULL );

  memset( &sa, 0, sizeof( sa ) );  /* ignore SIGPIPE */
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  sigaction( SIGPIPE, &sa, NULL );
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
  const char* short_options = "hvms:o:i:p:w:";

  const struct option long_options[] =
  {
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { "minimize",    no_argument,       NULL, 'm' },
    { "source",      required_argument, NULL, 's' },
    { "oformat",     required_argument, NULL, 'o' },
    { "iformat",     required_argument, NULL, 'i' },
    { "priority",    required_argument, NULL, 'p' },
    { "hardware",    required_argument, NULL, 'w' },
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
      case 'm':
      {
        minimize = 1;
        break;
      }

      case 's':
      {
        if( optarg != NULL )
        {
          srcdir = strdup( optarg );
          remove_trailing_slash( srcdir );
        }
        else
          /* option is present but without value */
          usage();
        break;
      }
      case 'o':
      {
        char *fmt = (char *)alloca( strlen( optarg ) + 1 );

        strcpy( (char *)fmt, (const char *)optarg );
        to_lowercase( fmt );

        if(      !strcmp( fmt, "list" ) ) { output_format = OFMT_LIST; }
        else if( !strcmp( fmt, "json" ) ) { output_format = OFMT_JSON; }
        else
        {
          ERROR( "Invalid output format: %s", fmt );
          usage();
        }
        break;
      }
      case 'i':
      {
        char *fmt = (char *)alloca( strlen( optarg ) + 1 );

        strcpy( (char *)fmt, (const char *)optarg );
        to_lowercase( fmt );

        if(      !strcmp( fmt, "pkg" ) ) { input_format = IFMT_PKG; }
        else if( !strcmp( fmt, "log" ) ) { input_format = IFMT_LOG; }
        else
        {
          ERROR( "Invalid input format: %s", fmt );
          usage();
        }
        break;
      }
      case 'p':
      {
        char *p = (char *)alloca( strlen( optarg ) + 1 );

        strcpy( (char *)p, (const char *)optarg );
        to_lowercase( p );

        if(      !strcmp( p, "required" )    || !strcmp( p, "req" ) ) { priority = REQUIRED;    }
        else if( !strcmp( p, "recommended" ) || !strcmp( p, "rec" ) ) { priority = RECOMMENDED; }
        else if( !strcmp( p, "optional" )    || !strcmp( p, "opt" ) ) { priority = OPTIONAL;    }
        else if( !strcmp( p, "skip" )        || !strcmp( p, "skp" ) ) { priority = SKIP;        }
        else
        {
          ERROR( "Invalid default install priority: %s", p );
          usage();
        }
        break;
      }
      case 'w':
      {
        char *hw = (char *)alloca( strlen( optarg ) + 1 );

        strcpy( (char *)hw, (const char *)optarg );
        to_lowercase( hw );

        hardware = strdup( hw );
        break;
      }

      case '?': default:
      {
        usage();
        break;
      }
    }
  }


  /* last command line argument is the output PKGLIST file */
  if( optind < argc )
  {
    struct stat st;
    char  *buf = NULL;
    int    hwl = 0;

    bzero( (void *)&st, sizeof( struct stat ) );

    if( hardware ) { hwl = strlen( hardware ); }

    buf = (char *)malloc( strlen( (const char *)argv[optind] ) + hwl + 10 );
    if( !buf )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    (void)strcpy( buf, (const char *)argv[optind++] );
    remove_trailing_slash( (char *)&buf[0] );

    stat( (const char *)&buf[0], &st ); /* Do not check return status */

    if( S_ISDIR(st.st_mode) )
    {
      if( ! srcdir )
      {
        srcdir = strdup( (const char *)&buf[0] );
      }
      /* Add .pkglist or .json to the output dir name: */
      if( hardware )
      {
        (void)strcat( buf, "/" );
        (void)strcat( buf, hardware );

        if( output_format == OFMT_LIST )
          (void)strcat( buf, ".pkglist" );
        else
          (void)strcat( buf, ".json" );
      }
      else
      {
        if( output_format == OFMT_LIST )
          (void)strcat( buf, "/.pkglist" );
        else
          (void)strcat( buf, "/.json" );
      }
    }


    /* Check output directory; set srcdir and hardware if needed */
    {
      char   *d, *f, *fname;
      size_t  len;

      if( !rindex( (const char *)&buf[0], '/' ) )
      {
        d = ".";
        f = (char *)&buf[0];
        len = strlen( f ) + 3;
      }
      else
      {
        f = basename( (char *)&buf[0] );
        d =  dirname( (char *)&buf[0] ); /* dirname() cuts the filename from buf[] */
        len = strlen( d ) + strlen( f ) + 2;
      }

      fname = (char *)alloca( len );
      (void)sprintf( fname, "%s/%s", d, f );

      if( stat( (const char *)d, &st ) == -1 )
      {
        FATAL_ERROR( "Cannot access output '%s' directory: %s", d, strerror( errno ) );
      }

      pkglist_fname = strdup( (const char *)fname );
      if( pkglist_fname == NULL ) { usage(); }

      if( ! srcdir ) { srcdir = strdup( (const char *)d ); }

      if( ! hardware )
      {
        char *p;
        if( (p = rindex( (const char *)f, '.' )) && p != f )
        {
          *p = '\0';
          to_lowercase( f );
          hardware = strdup( f );
        }
      }
    }

    free( buf );
  }
  else
  {
    usage();
  }
}


/***************************************************************
  Extract functions:
 */
static void _extract_pkglog( const char *group, const char *fname )
{
  enum _input_type  type = IFMT_UNKNOWN;
  char              uncompress = '\0';

  type = check_input_file( &uncompress, fname );

  if( type == IFMT_PKG )
  {
    int   len = 0;
    char *tmp= NULL, *cmd = NULL, *tgz = NULL;

    tmp = (char *)malloc( (size_t)PATH_MAX );
    if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)tmp, PATH_MAX );

    if( group ) { (void)sprintf( &tmp[0], "%s/%s", tmpdir, group ); }
    else        { (void)sprintf( &tmp[0], "%s", tmpdir ); }

    if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      ERROR( "Cannot save PKGLOG from '%s' file", basename( (char *)fname ) );
      free( tmp );
      return;
    }

    cmd = (char *)malloc( (size_t)PATH_MAX );
    if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)cmd, PATH_MAX );

    len = snprintf( &cmd[0], PATH_MAX, "%s/pkglog -d %s %s > /dev/null 2>&1", selfdir, tmp, fname );
    if( len == 0 || len == PATH_MAX - 1 )
    {
      FATAL_ERROR( "Cannot get PKGLOG from %s file", basename( (char *)fname ) );
    }
    (void)sys_exec_command( cmd );
    ++__child;

    tgz = (char *)malloc( (size_t)PATH_MAX );
    if( !tgz ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)tgz, PATH_MAX );
    (void)sprintf( &tgz[0], "%s/%s", group, basename( (char *)fname ) );
    add_tarball( (char *)&tgz[0] );

    free( tmp );
    free( cmd );
    free( tgz );
  }
}

static void _search_packages( const char *dirpath, const char *grp )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

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
        _extract_pkglog( grp, (const char *)path );
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        _search_packages( (const char *)path, (const char *)entry->d_name );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

/**************************************************************
  extract_pkglogs() - returns number of extracted PKGLOGS
                      or 0 if no packages found in the srcdir.
                      The exit_status has been set.
 */
int extract_pkglogs( void )
{
  int ret = 0;

  __done = 0; __child = 0;

  _search_packages( (const char *)srcdir, NULL );

  if( __child > 0 )
  {
    while( !__done ) usleep( 1 );
    ret = __child;
  }

  __done = 0; __child = 0;

  return ret;
}
/*
  End of Extract functions.
 ***************************************************************/


/***************************************************************
  Copy functions:
 */
static void _copy_pkglog( const char *group, const char *fname )
{
  enum _input_type  type = IFMT_UNKNOWN;
  char              uncompress = '\0';

  type = check_input_file( &uncompress, fname );

  if( type == IFMT_LOG )
  {
    int   len = 0;
    char *tmp= NULL, *cmd = NULL;

    tmp = (char *)malloc( (size_t)PATH_MAX );
    if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)tmp, PATH_MAX );

    if( group ) { (void)sprintf( &tmp[0], "%s/%s", tmpdir, group ); }
    else        { (void)sprintf( &tmp[0], "%s", tmpdir ); }

    if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
    {
      ERROR( "Cannot copy '%s' PKGLOG file", basename( (char *)fname ) );
      free( tmp );
      return;
    }

    cmd = (char *)malloc( (size_t)PATH_MAX );
    if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)cmd, PATH_MAX );

    len = snprintf( &cmd[0], PATH_MAX, "cp %s %s/ > /dev/null 2>&1", fname, tmp );
    if( len == 0 || len == PATH_MAX - 1 )
    {
      FATAL_ERROR( "Cannot copy %s PKGLOG file", basename( (char *)fname ) );
    }
    (void)sys_exec_command( cmd );
    ++__child;

    free( tmp );
    free( cmd );
  }
}

static void _search_pkglogs( const char *dirpath, const char *grp )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

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
        _copy_pkglog( grp, (const char *)path );
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        _search_pkglogs( (const char *)path, (const char *)entry->d_name );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

/***********************************************************
  copy_pkglogs() - returns number of copied PKGLOGS
                   or 0 if no packages found in the srcdir.
                   The exit_status has been set.
 */
int copy_pkglogs( void )
{
  int ret = 0;

  __done = 0; __child = 0;

  _search_pkglogs( (const char *)srcdir, NULL );

  if( __child > 0 )
  {
    while( !__done ) usleep( 1 );
    ret = __child;
  }

  __done = 0; __child = 0;

  return ret;
}
/*
  Enf of Copy functions.
 ***************************************************************/


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

static size_t read_usize( char *s )
{
  size_t  size = 0;
  size_t  mult = 1;
  double  sz = 0.0;

  char    suffix;
  char   *q, *p = (char *)0;

  if( !s || *s == '\0' ) return size;

  p = s;

  while( (*p == ' ' || *p == '\t') && *p != '\0' ) { ++p; } q = p;
  while(  *q != ' ' && *q != '\t'  && *q != '\0' ) { ++q; } *q = '\0';

  if( *p == '\0' ) return size;

  --q;
  suffix = *q;
  switch( suffix )
  {
    /* by default size calculates in KiB - 1024 Bytes (du -s -h .) */
    case 'G':
    case 'g':
      mult = 1024 * 1024;
      *q = '\0';
      break;
    case 'M':
    case 'm':
      mult = 1024;
      *q = '\0';
      break;
    case 'K':
    case 'k':
      *q = '\0';
      break;
    default:
      break;
  }

  if( sscanf( p, "%lg", &sz ) != 1 ) return size;

  return (size_t)round( sz * (double)mult );
}

static int read_total_files( char *s )
{
  int   n = 0;
  char *q, *p = (char *)0;

  if( !s || *s == '\0' ) return n;

  p = s;

  while( (*p == ' ' || *p == '\t') && *p != '\0' ) { ++p; } q = p;
  while(  *q != ' ' && *q != '\t'  && *q != '\0' ) { ++q; } *q = '\0';

  if( *p == '\0' ) return n;

  if( sscanf( p, "%u", &n ) != 1 ) return 0;

  return n;
}


static struct pkg *input_package( const char *pkginfo_fname )
{
  char *ln      = NULL;
  char *line    = NULL;

  FILE *pkginfo = NULL;

  struct pkg *pkg = NULL;
  char *pkgname = NULL, *pkgver = NULL, *group = NULL;

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
    if( (match = strstr( ln, "group" )) && match == ln ) {
      char *p = index( match, '=' ) + 1;
      if( p != NULL ) group = skip_spaces( p );
    }
  }

  free( line );

  if( pkgname && pkgver )
  {
    pkg = pkg_alloc();
    if( pkg )
    {
      if( group )
      {
        pkg->group = group;
      }
      pkg->name    = pkgname;
      pkg->version = pkgver;
    }

  }
  else
  {
    if( group )      free( group );
    if( pkgname )    free( pkgname );
    if( pkgver )     free( pkgver );

    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }

  fclose( pkginfo );

  return( pkg );
}



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


static int get_references_section( int *start, int *stop, unsigned int *cnt, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop || !cnt ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "REFERENCE COUNTER:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;

        /* Get reference counter */
        {
          unsigned int count;
          int          rc;

          ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
          skip_eol_spaces( ln );     /* remove spaces at end-of-line */

          rc = sscanf( ln, "REFERENCE COUNTER: %u", &count );
          if( rc == 1 && cnt != NULL )
          {
            *cnt = count;
          }
        }
      }
      if( (match = strstr( ln, "REQUIRES:" )) && match == ln )
      {
        *stop = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 2 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}

static int get_requires_section( int *start, int *stop, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "REQUIRES:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "PACKAGE DESCRIPTION:" )) && match == ln )
      {
        *stop = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 2 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}

static int get_description_section( int *start, int *stop, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "PACKAGE DESCRIPTION:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "RESTORE LINKS:" )) && match == ln )
      {
        *stop = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 2 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}

static int get_restore_links_section( int *start, int *stop, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "RESTORE LINKS:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "INSTALL SCRIPT:" )) && match == ln )
      {
        *stop = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 2 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}


static int get_install_script_section( int *start, int *stop, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "INSTALL SCRIPT:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "FILE LIST:" )) && match == ln )
      {
        *stop = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 2 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}


static int get_file_list_section( int *start, int *stop, FILE *log )
{
  int ret = -1, found = 0;

  if( !start || !stop ) return ret;

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    *start = 0; *stop = 0;

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "FILE LIST:" )) && match == ln ) /* at start of line only */
      {
        *start = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = ( found == 1 ) ? 0 : 1; /* 0 - success; 1 - not found. */

    fseek( log, 0, SEEK_SET );
  }

  return( ret );
}


int read_pkginfo( FILE *log, struct package *package )
{
  int ret = -1;

  char *ln   = NULL;
  char *line = NULL;

  char           *pkgname_pattern = "PACKAGE NAME:",
                  *pkgver_pattern = "PACKAGE VERSION:",
                    *arch_pattern = "ARCH:",
              *distroname_pattern = "DISTRO:",
               *distrover_pattern = "DISTRO VERSION:",
                   *group_pattern = "GROUP:",
                     *url_pattern = "URL:",
                 *license_pattern = "LICENSE:",
       *uncompressed_size_pattern = "UNCOMPRESSED SIZE:",
             *total_files_pattern = "TOTAL FILES:";


  if( !log || !package ) return ret;

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  ++ret;

  while( (ln = fgets( line, PATH_MAX, log )) )
  {
    char *match = NULL;

    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( (match = strstr( ln, pkgname_pattern )) && match == ln ) /* at start of line only */
    {
      package->pkginfo->name = skip_spaces( ln + strlen( pkgname_pattern ) );
    }
    if( (match = strstr( ln, pkgver_pattern )) && match == ln )
    {
      package->pkginfo->version = skip_spaces( ln + strlen( pkgver_pattern ) );
    }
    if( (match = strstr( ln, arch_pattern )) && match == ln )
    {
      package->pkginfo->arch = skip_spaces( ln + strlen( arch_pattern ) );
    }
    if( (match = strstr( ln, distroname_pattern )) && match == ln )
    {
      package->pkginfo->distro_name = skip_spaces( ln + strlen( distroname_pattern ) );
    }
    if( (match = strstr( ln, distrover_pattern )) && match == ln )
    {
      package->pkginfo->distro_version = skip_spaces( ln + strlen( distrover_pattern ) );
    }
    if( (match = strstr( ln, group_pattern )) && match == ln )
    {
      package->pkginfo->group = skip_spaces( ln + strlen( group_pattern ) );
    }
    if( (match = strstr( ln, url_pattern )) && match == ln )
    {
      package->pkginfo->url = skip_spaces( ln + strlen( url_pattern ) );
    }
    if( (match = strstr( ln, license_pattern )) && match == ln )
    {
      package->pkginfo->license = skip_spaces( ln + strlen( license_pattern ) );
    }
    if( (match = strstr( ln, uncompressed_size_pattern )) && match == ln )
    {
      package->pkginfo->uncompressed_size = read_usize( ln + strlen( uncompressed_size_pattern ) );
    }
    if( (match = strstr( ln, total_files_pattern )) && match == ln )
    {
      package->pkginfo->total_files = read_total_files( ln + strlen( total_files_pattern ) );
    }

    if( (match = strstr( ln, "PACKAGE DESCRIPTION:" )) && match == ln )
    {
      char *buf = NULL;

      buf = (char *)malloc( (size_t)PATH_MAX );
      if( !buf )
      {
        FATAL_ERROR( "Cannot allocate memory" );
      }

      /* Get short_description from PACKAGE DESCRIPTION */
      ln = fgets( line, PATH_MAX, log );
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol */

      bzero( (void *)buf, PATH_MAX );
      get_short_description( buf, (const char *)line );
      if( buf[0] != '\0' )
      {
        package->pkginfo->short_description = strdup( buf );
      }
      free( buf );
    }

  } /* End of while() */

  free( line );

  if( package->pkginfo->name           == NULL ) ++ret;
  if( package->pkginfo->version        == NULL ) ++ret;
  if( package->pkginfo->arch           == NULL ) ++ret;
  if( package->pkginfo->distro_name    == NULL ) ++ret;
  if( package->pkginfo->distro_version == NULL ) ++ret;
  /* group can be equal to NULL */

  fseek( log, 0, SEEK_SET );

  return( ret );
}


static unsigned int read_references( FILE *log, int start, unsigned int *cnt, struct package *package )
{
  char *ln   = NULL;
  char *line = NULL;
  char *p = NULL, *group = NULL, *name = NULL, *version = NULL;
  int   n = 1;

  unsigned int counter, pkgs = 0;

  struct pkg *pkg = NULL;

  if( !log || !cnt || *cnt == 0 || !package ) return pkgs;

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  counter = *cnt;

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  n = 0;
  while( (ln = fgets( line, PATH_MAX, log )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( strstr( ln, "REQUIRES:" ) ) break; /* if cnt greater than real number of references */

    if( n < counter )
    {
      if( (p = index( (const char *)ln, '=' )) )
      {
        *p = '\0'; version = ++p;
        if( (p = index( (const char *)ln, '/' )) )
        {
          *p = '\0'; name = ++p; group = (char *)&ln[0];
        }
        else
        {
          name  = (char *)&ln[0]; group = NULL;
        }

        pkg = pkg_alloc();

        if( group ) pkg->group = strdup( group );
        pkg->name    = strdup( name );
        pkg->version = strdup( version );

        add_reference( package, pkg );
        ++pkgs;
      }
      ++n;
    }
    else
      break;
  }

  free( line );

  fseek( log, 0, SEEK_SET );

  *cnt = pkgs;

  return pkgs;
}


static unsigned int read_requires( FILE *log, int start, int stop, struct package *package )
{
  char *ln   = NULL;
  char *line = NULL;
  char *p = NULL, *group = NULL, *name = NULL, *version = NULL;
  int   n = 1;

  unsigned int pkgs = 0;

  struct pkg *pkg = NULL;

  if( !log || !package ) return pkgs;

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  if( start && start < stop )
  {
    ++n; /* skip section header */

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      if( strstr( ln, "PACKAGE DESCRIPTION:" ) ) break; /* if (stop - start - 1) greater than real number of requiress */

      if( (n > start) && (n < stop) )
      {
        if( (p = index( (const char *)ln, '=' )) )
        {
          *p = '\0'; version = ++p;
          if( (p = index( (const char *)ln, '/' )) )
          {
            *p = '\0'; name = ++p; group = (char *)&ln[0];
          }
          else
          {
            name  = (char *)&ln[0]; group = NULL;
          }

          pkg = pkg_alloc();

          if( group ) pkg->group = strdup( group );
          pkg->name    = strdup( name );
          pkg->version = strdup( version );

          add_required( package, pkg );
          ++pkgs;
        }

      }
      ++n;
    }

  } /* End if( start && start < stop ) */

  free( line );

  fseek( log, 0, SEEK_SET );

  return pkgs;
}


static unsigned int read_description( FILE *log, int start, int stop, struct package *package )
{
  char *ln      = NULL;
  char *line    = NULL;
  char *pattern = NULL;
  int   n = 1;

  char  *tmp_fname = NULL;
  FILE  *tmp = NULL;

  unsigned int lines = 0;

  if( !log || !package ) return lines;

  tmp_fname = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp_fname ) { FATAL_ERROR( "Cannot allocate memory" ); }

  bzero( (void *)tmp_fname, PATH_MAX );
  (void)sprintf( (char *)&tmp_fname[0], "%s/.DESCRIPTION", tmpdir );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )    { FATAL_ERROR( "Cannot allocate memory" ); }

  pattern = (char *)malloc( (size_t)strlen( package->pkginfo->name ) + 2 );
  if( !pattern ) { FATAL_ERROR( "Cannot allocate memory" ); }

  (void)sprintf( pattern, "%s:", package->pkginfo->name );


  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  if( start && start < stop )
  {
    ++n; /* skip section header */

    tmp = fopen( (const char *)&tmp_fname[0], "w" );
    if( !tmp )
    {
      FATAL_ERROR( "Cannot create temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      char *match = NULL;

      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      if( strstr( ln, "RESTORE LINKS:" ) ) break; /* if (stop - start - 1) greater than real number of lines */

      if( (n > start) && (n < stop) )
      {
        /*
          skip non-significant spaces at beginning of line
          and print lines started with 'pkgname:'
         */
        if( (match = strstr( ln, pattern )) && lines < DESCRIPTION_NUMBER_OF_LINES )
        {
          int mlen   = strlen( match ), plen = strlen( pattern );
          int length = ( mlen > plen )  ? (mlen - plen - 1) : 0 ;

          if( length > DESCRIPTION_LENGTH_OF_LINE )
          {
            /* WARNING( "Package DESCRIPTION contains lines with length greater than %d characters", DESCRIPTION_LENGTH_OF_LINE ); */
            match[plen + 1 + DESCRIPTION_LENGTH_OF_LINE] = '\0'; /* truncating description line  */
            skip_eol_spaces( match );                            /* remove spaces at end-of-line */
          }
          fprintf( tmp, "%s\n", match );
          ++lines;
        }

      }
      ++n;
    }

    if( lines < (unsigned int)DESCRIPTION_NUMBER_OF_LINES )
    {
      /* WARNING( "Package DESCRIPTION contains less than %d lines", DESCRIPTION_NUMBER_OF_LINES ); */
      while( lines < (unsigned int)DESCRIPTION_NUMBER_OF_LINES )
      {
        fprintf( tmp, "%s\n", pattern );
        ++lines;
      }
    }

    fflush( tmp );
    fclose( tmp );

  } /* End if( start && start < stop ) */

  free( pattern );
  free( line );

  fseek( log, 0, SEEK_SET );

  /* read temporary saved description */
  {
    struct stat sb;
    size_t size = 0;
    int    fd;

    char  *desc = NULL;

    if( stat( tmp_fname, &sb ) == -1 )
    {
      FATAL_ERROR( "Cannot stat temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }
    size = (size_t)sb.st_size;

    if( size )
    {
      ssize_t rc = 0;

      desc = (char *)malloc( size + 1 );
      if( !desc ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)desc, size + 1 );

      if( (fd = open( (const char *)&tmp_fname[0], O_RDONLY )) == -1 )
      {
        FATAL_ERROR( "Canot access temporary %s file: %s", basename( (char *)&tmp_fname[0] ), strerror( errno ) );
      }

      rc = read( fd, (void *)desc, size );
      if( rc != (ssize_t)size ) { ERROR( "The %s file is not fully read", basename( (char *)&tmp_fname[0] ) ); }

      package->description = desc;

      close( fd );
    }

  }

  (void)unlink( tmp_fname );
  free( tmp_fname );

  return lines;
}


static unsigned int read_restore_links( FILE *log, int start, int stop, struct package *package )
{
  char *ln      = NULL;
  char *line    = NULL;
  int   n = 1;

  char  *tmp_fname = NULL;
  FILE  *tmp = NULL;

  unsigned int lines = 0;

  if( !log || !package ) return lines;

  tmp_fname = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp_fname ) { FATAL_ERROR( "Cannot allocate memory" ); }

  bzero( (void *)tmp_fname, PATH_MAX );
  (void)sprintf( (char *)&tmp_fname[0], "%s/.RESTORELINKS", tmpdir );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )    { FATAL_ERROR( "Cannot allocate memory" ); }

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  if( start && start < stop )
  {
    ++n; /* skip section header */

    tmp = fopen( (const char *)&tmp_fname[0], "w" );
    if( !tmp )
    {
      FATAL_ERROR( "Cannot create temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      if( strstr( ln, "INSTALL SCRIPT:" ) ) break; /* if (stop - start - 1) greater than real number of lines */

      if( (n > start) && (n < stop) )
      {
        fprintf( tmp, "%s\n", ln );
        ++lines;
      }
      ++n;
    }

    fflush( tmp );
    fclose( tmp );

  } /* End if( start && start < stop ) */

  free( line );

  fseek( log, 0, SEEK_SET );

  /* read temporary saved description */
  {
    struct stat sb;
    size_t size = 0;
    int    fd;

    char  *links = NULL;

    if( stat( tmp_fname, &sb ) == -1 )
    {
      FATAL_ERROR( "Cannot stat temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }
    size = (size_t)sb.st_size;

    if( size )
    {
      ssize_t rc = 0;

      links = (char *)malloc( size + 1 );
      if( !links ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)links, size + 1 );

      if( (fd = open( (const char *)&tmp_fname[0], O_RDONLY )) == -1 )
      {
        FATAL_ERROR( "Canot access temporary %s file: %s", basename( (char *)&tmp_fname[0] ), strerror( errno ) );
      }

      rc = read( fd, (void *)links, size );
      if( rc != (ssize_t)size ) { ERROR( "The %s file is not fully read", basename( (char *)&tmp_fname[0] ) ); }

      package->restore_links = links;

      close( fd );
    }

  }

  (void)unlink( tmp_fname );
  free( tmp_fname );

  return lines;
}


static unsigned int read_install_script( FILE *log, int start, int stop, struct package *package )
{
  char *ln      = NULL;
  char *line    = NULL;
  int   n = 1;

  char  *tmp_fname = NULL;
  FILE  *tmp = NULL;

  unsigned int lines = 0;

  if( !log || !package ) return lines;

  tmp_fname = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp_fname ) { FATAL_ERROR( "Cannot allocate memory" ); }

  bzero( (void *)tmp_fname, PATH_MAX );
  (void)sprintf( (char *)&tmp_fname[0], "%s/.INSTALL", tmpdir );

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )    { FATAL_ERROR( "Cannot allocate memory" ); }

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  if( start && start < stop )
  {
    ++n; /* skip section header */

    tmp = fopen( (const char *)&tmp_fname[0], "w" );
    if( !tmp )
    {
      FATAL_ERROR( "Cannot create temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }

    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      if( strstr( ln, "FILE LIST:" ) ) break; /* if (stop - start - 1) greater than real number of lines */

      if( (n > start) && (n < stop) )
      {
        fprintf( tmp, "%s\n", ln );
        ++lines;
      }
      ++n;
    }

    fflush( tmp );
    fclose( tmp );

  } /* End if( start && start < stop ) */

  free( line );

  fseek( log, 0, SEEK_SET );

  /* read temporary saved description */
  {
    struct stat sb;
    size_t size = 0;
    int    fd;

    char  *install = NULL;

    if( stat( tmp_fname, &sb ) == -1 )
    {
      FATAL_ERROR( "Cannot stat temporary %s file", basename( (char *)&tmp_fname[0] ) );
    }
    size = (size_t)sb.st_size;

    if( size )
    {
      ssize_t rc = 0;

      install = (char *)malloc( size + 1 );
      if( !install ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)install, size + 1 );

      if( (fd = open( (const char *)&tmp_fname[0], O_RDONLY )) == -1 )
      {
        FATAL_ERROR( "Canot access temporary %s file: %s", basename( (char *)&tmp_fname[0] ), strerror( errno ) );
      }

      rc = read( fd, (void *)install, size );
      if( rc != (ssize_t)size ) { ERROR( "The %s file is not fully read", basename( (char *)&tmp_fname[0] ) ); }

      package->install_script = install;

      close( fd );
    }

  }

  (void)unlink( tmp_fname );
  free( tmp_fname );

  return lines;
}


static unsigned int read_file_list( FILE *log, int start, struct package *package )
{
  char *ln   = NULL;
  char *line = NULL;
  int   n = 1;

  unsigned int files = 0;

  if( !log || !package ) return files;

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  if( start )
  {
    while( (ln = fgets( line, PATH_MAX, log )) )
    {
      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      add_file( package, (const char *)ln );
      ++files;
    }

  } /* End if( start && start < stop ) */

  free( line );

  fseek( log, 0, SEEK_SET );

  return files;
}


static void _read_pkglog( const char *group, const char *fname )
{
  FILE *log   = NULL;
  char *bname = NULL;

  if( fname != NULL )
  {
    log = fopen( (const char *)fname, "r" );
    if( !log )
    {
      FATAL_ERROR( "Cannot open %s file", fname );
    }
    bname = (char *)fname + strlen( tmpdir ) + 1;
  }

  if( log != NULL )
  {
    struct package *package = NULL;
    int             rc, start, stop;
    unsigned int    counter;

    package = package_alloc();

    if( read_pkginfo( log, package ) != 0 )
    {
      ERROR( "%s: Invalid PKGLOG file", bname );
      package_free( package );
      fclose( log );
      return;
    }

    if( hardware ) package->hardware = strdup( hardware );
    if( tarballs ) /* find tarball and allocate package->tarball */
    {
      struct pkginfo *info = package->pkginfo;
      const char     *tgz  = NULL;
      char           *buf  = NULL;
      struct stat     sb;

      buf = (char *)malloc( (size_t)PATH_MAX );
      if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }

      if( info->group )
      {
        (void)sprintf( buf, "%s/%s-%s-%s-%s-%s",
                             info->group, info->name, info->version, info->arch,
                             info->distro_name, info->distro_version );
      }
      else
      {
        (void)sprintf( buf, "%s-%s-%s-%s-%s",
                             info->name, info->version, info->arch,
                             info->distro_name, info->distro_version );
      }
      tgz = find_tarball( (const char *)&buf[0] );
      if( tgz )
      {
        package->tarball = strdup( tgz );

        bzero( (void *)&buf[0], PATH_MAX );
        (void)sprintf( buf, "%s/%s", srcdir, tgz );
        if( stat( buf, &sb ) != -1 )
        {
          info->compressed_size = (size_t)sb.st_size;
        }
      }
      free( buf );
    }
    package->procedure = INSTALL;
    package->priority  = priority;

    if( package->pkginfo->group && group  && strcmp( package->pkginfo->group, group ) != 0 )
    {
      char *tgz;

      if( package->tarball ) { tgz = package->tarball; }
      else                   { tgz = basename( (char *)fname ); }

      WARNING( "%s: Should be moved into '%s' subdir", tgz, package->pkginfo->group );
    }

    /******************
      read references:
     */
    rc = get_references_section( &start, &stop, &counter, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains REFERENCE COUNTER section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( counter > 0 )
    {
      unsigned int pkgs = counter;

      if( read_references( log, start, &counter, package ) != pkgs )
      {
        ERROR( "%s: Invalid REFERENCE COUNTER section", bname );
        package_free( package );
        fclose( log );
        return;
      }
    }

    /******************
      read requires:
     */
    rc = get_requires_section( &start, &stop, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains REQUIRES section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( (stop - start) > 1 )
    {
      unsigned int pkgs = (unsigned int)(stop - start - 1); /* -1 skips section header */

      if( read_requires( log, start, stop, package ) != pkgs )
      {
        ERROR( "%s: Invalid REQUIRES section", bname );
        package_free( package );
        fclose( log );
        return;
      }
    }

    /*******************
      read description:
     */
    rc = get_description_section( &start, &stop, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains PACKAGE DESCRIPTION section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( (stop - start) > 1 )
    {
      if( read_description( log, start, stop, package ) != (unsigned int)DESCRIPTION_NUMBER_OF_LINES )
      {
        ERROR( "%s: Invalid DESCRIPTION section", bname );
        package_free( package );
        fclose( log );
        return;
      }
    }

    /*********************
      read restore links:
     */
    rc = get_restore_links_section( &start, &stop, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains RESTORE LINKS section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( (stop - start) > 1 )
    {
      (void)read_restore_links( log, start, stop, package );
    }

    /*********************
      read install script:
     */
    rc = get_install_script_section( &start, &stop, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains INSTALL SCRIPT section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( (stop - start) > 1 )
    {
      (void)read_install_script( log, start, stop, package );
    }

    /*****************
      read file_list:
     */
    rc = get_file_list_section( &start, &stop, log );
    if( rc != 0 )
    {
      ERROR( "%s: PKGLOG doesn't contains FILE LIST section", bname );
      package_free( package );
      fclose( log );
      return;
    }
    if( start )
    {
      unsigned int files = read_file_list( log, start, package );
      if( files == (unsigned int)0 )
      {
        /*
          Packages that do not contain regular files are ignored.
          For example, service package base/init-devices-1.2.3-s9xx-glibc-radix-1.1.txz
         */
        if( ! DO_NOT_PRINTOUT_INFO )
        {
          INFO( "%s: PKGLOG contains empty FILE LIST section", bname );
        }
        package_free( package );
        fclose( log );
        return;
      }
      package->pkginfo->total_files = (int)files;
    }

    add_package( package );

    ++__child;
    fclose( log );
  }
}

static void _read_pkglogs( const char *dirpath, const char *grp )
{
  DIR    *dir;
  char   *path;
  size_t  len;

  struct stat    path_sb, entry_sb;
  struct dirent *entry;

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
        if( check_input_file( NULL, (const char *)path ) == IFMT_LOG )
        {
          _read_pkglog( grp, (const char *)path );
        }
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        _read_pkglogs( (const char *)path, (const char *)entry->d_name );
      }
    }
    /* else { stat() returns error code; errno is set; and we have to continue the loop } */
  }

  closedir( dir );
}

int read_pkglogs( void )
{
  int ret = 0;

  __child = 0;

  _read_pkglogs( (const char *)tmpdir, NULL );

  ret = __child;

  __child = 0;

  return ret;
}


static void check_srcdir( void )
{
  struct stat st;
  char *fname = srcdir;

  bzero( (void *)&st, sizeof( struct stat ) );

  if( stat( (const char *)srcdir, &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access input '%s' file or directory: %s", srcdir, strerror( errno ) );
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

    len = snprintf( &cmd[0], PATH_MAX, "%s/pkginfo -d %s -o pkginfo %s > /dev/null 2>&1", selfdir, tmp, fname );
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

    bzero( (void *)&st, sizeof( struct stat ) );
    if( stat( (const char *)tmp, &st ) == -1 )
    {
      FATAL_ERROR( "Cannot get PKGINFO from %s file: %s", basename( (char *)fname ), strerror( errno ) );
    }

    /* overwrite or set input file format: */
    input_format = check_input_file( NULL, fname );

    srcpkg = input_package( (const char *)&tmp[0] );
    bzero( (void *)tmp, PATH_MAX );
    if( srcpkg )
    {
      char *e = NULL;

      if( srcpkg->group )
      {
        len = snprintf( &tmp[0], PATH_MAX, "%s/%s", srcpkg->group, basename( (char *)fname ) );
        if( len == 0 || len == PATH_MAX - 1 )
        {
          FATAL_ERROR( "Cannot get PKGINFO from %s file", basename( (char *)fname ) );
        }
      }
      else
      {
        len = snprintf( &tmp[0], PATH_MAX, "%s", basename( (char *)fname ) );
        if( len == 0 || len == PATH_MAX - 1 )
        {
          FATAL_ERROR( "Cannot get PKGINFO from %s file", basename( (char *)fname ) );
        }
      }

      /* remove source file name (with group directory if defined) from srcdir path */
      e = strstr( (const char *)srcdir, (const char *)&tmp[0] );
      if( !e ) { e = strstr( (const char *)srcdir, (const char *)basename( (char *)fname ) ); }
      *e = '\0';

      remove_trailing_slash( srcdir );
    }

    free( tmp );
    free( cmd );
  }
}


static void _print_extern_requires( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;

  if( pkg )
  {
    if( pkg->group )
      fprintf( stderr, "%s/%s:%s:%s\n", pkg->group, pkg->name, pkg->version, strproc( pkg->procedure ) );
    else
      fprintf( stderr, "%s:%s:%s\n", pkg->name, pkg->version, strproc( pkg->procedure ) );
  }
}

static void print_extern_requires( void )
{
  dlist_foreach( extern_requires, _print_extern_requires, NULL );
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

  /********************************************************
    Check if the --source argument is a file or directory:
   */
  check_srcdir();

  /* Extract or Copy PKGLOGs into TMPDIR: */
  if( input_format == IFMT_PKG )
  {
    int pkgs = extract_pkglogs();
    if( pkgs == 0 )       { FATAL_ERROR( "There are no packages in the '%s' directory", srcdir ); }
    if( exit_status > 0 ) { FATAL_ERROR( "Cannot extract PKGLOG from some package" ); }
    if( ! DO_NOT_PRINTOUT_INFO )
    {
      INFO( "Found %d packages in the '%s' directory", pkgs, srcdir );
    }
  }
  else
  {
    int pkgs = copy_pkglogs();
    if( pkgs == 0 )       { FATAL_ERROR( "There are no PKGLOG files in the '%s' directory", srcdir ); }
    if( exit_status > 0 ) { FATAL_ERROR( "Cannot copy some PKGLOG file" ); }
    if( ! DO_NOT_PRINTOUT_INFO )
    {
      INFO( "Found %d PKGLOG files in the '%s' directory", pkgs, srcdir );
    }
  }

  /* Read PKGLOGs from TMPDIR and create Double Linked List of PACKAGES: */
  {
    int pkgs = read_pkglogs();
    if( pkgs == 0 )       { FATAL_ERROR( "There are no PKGLOG files in the '%s' directory", tmpdir ); }
    if( exit_status > 0 ) { FATAL_ERROR( "Cannot read some PKGLOG file" ); }
    if( ! DO_NOT_PRINTOUT_INFO )
    {
      /* INFO( "Found %d PKGLOG files in the '%s' directory", pkgs, tmpdir ); */
    }
  }

  {
    int extern_pkgs = create_provides_list( srcpkg );
    if( output_format == OFMT_LIST )
    {
      print_provides_list( (const char *)pkglist_fname );
      if( extern_pkgs )
      {
        /* Output machine readable list of requires to stderr: */
        print_extern_requires();
        exit_status += 1;
      }
    }
    if( output_format == OFMT_JSON ) print_provides_tree( (const char *)pkglist_fname );
    free_provides_list();
  }

  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
