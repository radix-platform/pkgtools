
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

#include <sys/resource.h>

#include <signal.h>
#if !defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

#define _GNU_SOURCE
#include <getopt.h>

#include <msglog.h>
#include <system.h>

#define PROGRAM_NAME "chrefs"

#include <defs.h>


char *program     = PROGRAM_NAME;
char *destination = NULL, *operation = NULL, *pkglog_fname = NULL,
     *tmpdir = NULL, *requires_fname = NULL;

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

char           *pkgname = NULL,
                *pkgver = NULL,
                  *arch = NULL,
            *distroname = NULL,
             *distrover = NULL,
                 *group = NULL;


/********************************************
  *requires[] declarations:
 */
struct package {
  char *name;
  char *version;
  char *arch;
  char *distro_name;
  char *distro_version;
};

static struct package *create_package( char *name, char *version,
                                       char *arch, char *distro_name,
                                       char *distro_version );
static void free_package( struct package *pkg );
static struct package **create_requires( size_t size );
static void free_requires( struct package **requires );
/*
  End of *requires[] declarations.
 ********************************************/

struct package **requires = NULL;


/********************************************
  LOCK FILE declarations:
 */
static int __lock_file( FILE *fp );
static void __unlock_file( int fd );
/*
  End of LOCK FILE declarations.
 ********************************************/


#define FREE_PKGINFO_VARIABLES() \
  if( pkgname )           { free( pkgname );           } pkgname    = NULL; \
  if( pkgver )            { free( pkgver );            } pkgver     = NULL; \
  if( arch )              { free( arch );              } arch       = NULL; \
  if( distroname )        { free( distroname );        } distroname = NULL; \
  if( distrover )         { free( distrover );         } distrover  = NULL; \
  if( group )             { free( group );             } group      = NULL; \
  if( requires )          { free_requires( requires ); } requires   = NULL

void free_resources()
{
  if( selfdir )        { free( selfdir );        selfdir        = NULL; }
  if( destination )    { free( destination );    destination    = NULL; }
  if( operation )      { free( operation );      operation      = NULL; }
  if( pkglog_fname )   { free( pkglog_fname );   pkglog_fname   = NULL; }
  if( requires_fname ) { free( requires_fname ); requires_fname = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <pkglog>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Increment or Decrement reference counters in required PKGLOG files\n" );
  fprintf( stdout, "in the  Setup Database packages  directory, which is determined by\n" );
  fprintf( stdout, "option --destination  OR by the directory where the input <pkglog>\n" );
  fprintf( stdout, "file is located. If destination is defined then <pkglog> file name\n" );
  fprintf( stdout, "should be defined relative to destination.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  -d,--destination=<DIR>        Setup Database packages directory.\n" );
  fprintf( stdout, "  -o,--operation=<inc|dec>      Operation: Increment or Decrement.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <pkglog>                      Input PKGLOG file (TEXT format).\n"  );
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


/********************************************
  *requires[] functions:
 */
static struct package *create_package( char *name, char *version,
                                       char *arch, char *distro_name,
                                       char *distro_version )
{
  struct package *pkg = (struct package *)0;

  if( !name           || *name           == '\0' ) return pkg;
  if( !version        || *version        == '\0' ) return pkg;
  if( !arch           || *arch           == '\0' ) return pkg;
  if( !distro_name    || *distro_name    == '\0' ) return pkg;
  if( !distro_version || *distro_version == '\0' ) return pkg;

  pkg = (struct package *)malloc( sizeof(struct package) );
  if( pkg )
  {
    pkg->name           = strdup( name           );
    pkg->version        = strdup( version        );
    pkg->arch           = strdup( arch           );
    pkg->distro_name    = strdup( distro_name    );
    pkg->distro_version = strdup( distro_version );
  }

  return pkg;
}

static void free_package( struct package *pkg )
{
  if( pkg )
  {
    if( pkg->name           ) free( pkg->name           );
    if( pkg->version        ) free( pkg->version        );
    if( pkg->arch           ) free( pkg->arch           );
    if( pkg->distro_name    ) free( pkg->distro_name    );
    if( pkg->distro_version ) free( pkg->distro_version );

    free( pkg );
  }
}

static struct package **create_requires( size_t size )
{
  struct package **requires = (struct package **)0;

  if( size > 0 )
  {
    requires = (struct package **)malloc( size * sizeof(struct package *) );
    bzero( (void *)requires, size * sizeof(struct package *) );
  }

  return( requires );
}

static void free_requires( struct package **requires )
{
  if( requires )
  {
    struct package **ptr = requires;

    while( *ptr )
    {
      if( *ptr ) free_package( *ptr );
      ptr++;
    }
    free( requires );
  }
}
/*
  End of *requires[] functions.
 ********************************************/


/********************************************
  LOCK FILE functions:
 */
static int __lock_file( FILE *fp )
{
  int fd = fileno( fp );

  if( flock( fd, LOCK_EX ) == -1 )
  {
    return -1;
    /*
      Мы не проверяем errno == EWOULDBLOCK, так какданная ошибка
      говорит о том что файл заблокирован другим процессом с флагом
      LOCK_NB, а мы не собираемся циклически проверять блокировку.
      У нас все просто: процесс просто ждет освобождения дескриптора
      и не пытается во время ожидания выполнять другие задачи.
     */
  }
  return fd;
}

static void __unlock_file( int fd )
{
  if( fd != -1 ) flock( fd, LOCK_UN );
  /*
    Здесь, в случае ошибки, мы не будем выводить
    никаких сообщений. Наш процесс выполняет простую
    атомарную задачу и, наверное, завершится в скором
    времени, освободив все дескрипторы.
   */
}
/*
  End of LOCK FILE functions.
 ********************************************/


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

enum _pkglog_type
{
  PKGLOG_TEXT = 0,
  PKGLOG_GZ,
  PKGLOG_BZ2,
  PKGLOG_XZ,
  PKGLOG_TAR,

  PKGLOG_UNKNOWN
};

static enum _pkglog_type pkglog_type = PKGLOG_UNKNOWN;
static char uncompress[2] = { 0, 0 };


static enum _pkglog_type check_pkglog_file( const char *fname )
{
  struct stat st;
  size_t pkglog_size = 0;
  unsigned char buf[8];
  int rc, fd;

  /* SIGNATURES: https://www.garykessler.net/library/file_sigs.html */

  uncompress[0] = '\0';

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
    FATAL_ERROR( "Unknown type of input file %s", basename( (char *)fname ) );
  }
  buf[7] = '\0';

  /* TEXT */
  if( !strncmp( (const char *)&buf[0], "PACKAGE", 7 ) )
  {
    close( fd ); return PKGLOG_TEXT;
  }

  /* GZ */
  if( buf[0] == 0x1F && buf[1] == 0x8B && buf[2] == 0x08 )
  {
    uncompress[0] = 'x';
    close( fd ); return PKGLOG_GZ;
  }

  /* BZ2 */
  if( buf[0] == 0x42 && buf[1] == 0x5A && buf[2] == 0x68 )
  {
    uncompress[0] = 'j';
    close( fd ); return PKGLOG_BZ2;
  }

  /* XZ */
  if( buf[0] == 0xFD && buf[1] == 0x37 && buf[2] == 0x7A &&
      buf[3] == 0x58 && buf[4] == 0x5A && buf[5] == 0x00   )
  {
    uncompress[0] = 'J';
    close( fd ); return PKGLOG_XZ;
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
      close( fd ); return PKGLOG_TAR;
    }
  }

  close( fd ); return PKGLOG_UNKNOWN;
}


void get_args( int argc, char *argv[] )
{
  const char* short_options = "hvd:o:";

  const struct option long_options[] =
  {
    { "help",        no_argument,       NULL, 'h' },
    { "version",     no_argument,       NULL, 'v' },
    { "destination", required_argument, NULL, 'd' },
    { "operation",   required_argument, NULL, 'o' },
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
      case 'o':
      {
        operation = strdup( optarg );
        to_lowercase( operation );
        if( strcmp( operation, "inc" ) != 0 && strcmp( operation, "dec" ) != 0 )
        {
          ERROR( "Invalid '%s' operation requested", operation );
          usage();
        }
        break;
      }

      case '?': default:
      {
        usage();
        break;
      }
    }
  }


  if( operation == NULL )
  {
    usage();
  }

  /* last command line argument is the LOGFILE */
  if( optind < argc )
  {
    char *buf = NULL;

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }

    if( destination == NULL )
    {
      pkglog_fname = strdup( argv[optind++] );
      if( pkglog_fname == NULL )
      {
        FATAL_ERROR( "Unable to set input PKGLOG file name" );
      }

      bzero( (void *)buf, PATH_MAX );
      (void)sprintf( buf, "%s", pkglog_fname );
      destination  = strdup( dirname( buf ) );
      if( destination == NULL )
      {
        FATAL_ERROR( "Unable to set destination directory" );
      }

    }
    else
    {
      bzero( (void *)buf, PATH_MAX );
      (void)sprintf( buf, "%s/%s", destination, argv[optind++] );
      pkglog_fname = strdup( buf );
      if( pkglog_fname == NULL )
      {
        FATAL_ERROR( "Unable to set inpit PKGLOG file name" );
      }
    }

    free( buf );

    pkglog_type = check_pkglog_file( (const char *)pkglog_fname );
    if( pkglog_type != PKGLOG_TEXT )
    {
      ERROR( "%s: Unknown input file format", basename( pkglog_fname ) );
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



int get_pkginfo()
{
  int   ret = -1;
  FILE *log = NULL;

  if( pkglog_fname != NULL )
  {
    log = fopen( (const char *)pkglog_fname, "r" );
    if( !log )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( log != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    char    *pkgname_pattern = "PACKAGE NAME:",
             *pkgver_pattern = "PACKAGE VERSION:",
              *group_pattern = "GROUP:",
               *arch_pattern = "ARCH:",
         *distroname_pattern = "DISTRO:",
          *distrover_pattern = "DISTRO VERSION:";

    int last = 10; /* read first 10 lines only */

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( last )
    {
      int n = 1;

      while( (ln = fgets( line, PATH_MAX, log )) )
      {
        char *match = NULL;

        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (match = strstr( ln, pkgname_pattern )) && match == ln ) /* at start of line only */
        {
          pkgname = skip_spaces( ln + strlen( pkgname_pattern ) );
        }
        if( (match = strstr( ln, pkgver_pattern )) && match == ln )
        {
          pkgver = skip_spaces( ln + strlen( pkgver_pattern ) );
        }
        if( (match = strstr( ln, arch_pattern )) && match == ln )
        {
          arch = skip_spaces( ln + strlen( arch_pattern ) );
        }
        if( (match = strstr( ln, distroname_pattern )) && match == ln )
        {
          distroname = skip_spaces( ln + strlen( distroname_pattern ) );
        }
        if( (match = strstr( ln, distrover_pattern )) && match == ln )
        {
          distrover = skip_spaces( ln + strlen( distrover_pattern ) );
        }
        if( (match = strstr( ln, group_pattern )) && match == ln )
        {
          group = skip_spaces( ln + strlen( group_pattern ) );
        }

        if( n < last ) ++n;
        else break;

      } /* End of while() */

    }

    free( line );

    if(    pkgname == NULL ) ++ret;
    if(     pkgver == NULL ) ++ret;
    if(       arch == NULL ) ++ret;
    if( distroname == NULL ) ++ret;
    if(  distrover == NULL ) ++ret;
    /* group can be equal to NULL */

    fclose( log );
  }

  return( ret );
}



int get_references_section( int *start, int *stop, unsigned int *cnt, FILE *log )
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



int get_requires_section( int *start, int *stop, const char *log_fname )
{
  int ret = -1, found = 0;

  FILE *log = NULL;

  if( !start || !stop ) return ret;

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( !log )
    {
      return ret;
    }
  }

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

    fclose( log );
  }

  return( ret );
}


int get_description_section( int *start, int *stop, const char *log_fname )
{
  int ret = -1, found = 0;

  FILE *log = NULL;

  if( !start || !stop ) return ret;

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( !log )
    {
      return ret;
    }
  }

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

    fclose( log );
  }

  return( ret );
}


int get_restore_links_section( int *start, int *stop, const char *log_fname )
{
  int ret = -1, found = 0;

  FILE *log = NULL;

  if( !start || !stop ) return ret;

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( !log )
    {
      return ret;
    }
  }

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

    fclose( log );
  }

  return( ret );
}


int get_install_script_section( int *start, int *stop, const char *log_fname )
{
  int ret = -1, found = 0;

  FILE *log = NULL;

  if( !start || !stop ) return ret;

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( !log )
    {
      return ret;
    }
  }

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

    fclose( log );
  }

  return( ret );
}


int get_file_list_section( int *start, int *stop, const char *log_fname )
{
  int ret = -1, found = 0;

  FILE *log = NULL;

  if( !start || !stop ) return ret;

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( !log )
    {
      return ret;
    }
  }

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
    *stop = 0;

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

    fclose( log );
  }

  return( ret );
}




/***********************************************************
  get_requires():
  --------------
    if( success ) - returns number of requires
    if(   error ) - returns -1
 */
int get_requires( const char *requires, const char *log_fname )
{
  int   ret = -1;
  FILE *req = NULL, *log = NULL;

  int   start = 0, stop  = 0;

  if( get_requires_section( &start, &stop, log_fname ) != 0 )
  {
    return ret;
  }

  if( log_fname != NULL )
  {
    log = fopen( (const char *)log_fname, "r" );
    if( ! log )
    {
      return ret;
    }
  }

  if( requires != NULL )
  {
    req = fopen( (const char *)requires, "w" );
    if( ! req )
    {
      return ret;
    }
  }

  /* get PKGLOG sections */
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( start && start < stop )
    {
      int n = 1, lines = 0;

      while( (ln = fgets( line, PATH_MAX, log )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > start) && (n < stop) )
        {
          fprintf( req, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( log );
    fflush( req ); fclose( req );
  }

  return( ret );
}


struct package **read_requires( const char *fname, int n )
{
  struct package **requires = (struct package **)0;
  struct package **ptr      = (struct package **)0;

  FILE  *file;

  if( ! fname ) return NULL;

  requires = create_requires( n + 1 );
  if( requires )
  {
    int   i;
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    struct package *package;

    file = fopen( fname, "r" );
    if( !file )
    {
      free( line );
      free_requires( requires );
      return NULL;
    }

    ptr = requires;
    for( i = 0; i < n; ++i )
    {
      if( (ln = fgets( line, PATH_MAX, file )) )
      {
        char *d, *name, *version;

        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        /* сut 'name=version' by delimiter '=': */
        d = index( ln, '=' ); *d = '\0';
        version = ++d; name = ln;

        package = create_package( name, version, arch, DISTRO_NAME, DISTRO_VERSION );
        if( package )
        {
          *ptr = package;
          ptr++;
        }
      }
    }
    *ptr = (struct package *)0;

    free( line );
  }

  return requires;
}

int find_requires( char *pname, const char *grp )
{
  char *name = NULL;
  char *buf  = NULL;

  struct package **ptr = requires;

  buf = (char *)malloc( (size_t)PATH_MAX );

  if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }

  while( *ptr )
  {
    if( grp && *grp != '\0' )
    {
      char   *p = NULL;
      size_t  len = strlen( (*ptr)->name );

      if( (p = strstr( (*ptr)->name, grp )) && (p == (*ptr)->name) )
      {
        /* if group is equal to required package's group then remove group from the name */
        name = alloca( len + 1 );
        strcpy( name, (const char *)(*ptr)->name );

        name = index( name, '/' ) + 1;
      }
      else
      {
        /* if group is not equal to required package's group then add group to the name */
        name = alloca( len + strlen( grp ) + 2 );
        (void)sprintf( name, "%s/%s", grp, (const char *)(*ptr)->name );
      }
    }
    else
    {
      name = (*ptr)->name;
    }

    (void)sprintf( buf, "%s-%s-%s-%s-%s",
                         name, (*ptr)->version, (*ptr)->arch,
                         (*ptr)->distro_name, (*ptr)->distro_version );

    if( ! strcmp( buf, pname ) )
    {
      free( buf ); return 1;
    }
    ptr++;
  }

  free( buf );

  return 0;
}


int save_tmp_head( FILE *log, int stop, const char *fname )
{
  FILE *fp;
  int   ret = -1;

  char *ln   = NULL;
  char *line = NULL;
  int   n = 1, lines = 0;

  if( !stop || !log || !fname || *fname == '\0' ) return ret;

  fp = fopen( fname, "w" );
  if( !fp )
  {
    return ret;
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  ++ret;

  while( (ln = fgets( line, PATH_MAX, log )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( n < stop )
    {
      fprintf( fp, "%s\n", ln );
      ++n; ++lines;
    }
    else
      break;
  }

  ret = lines; /* number of lines in the HEAD */

  free( line );

  fseek( log, 0, SEEK_SET );
  fclose( fp );

  return ret;
}

int save_tmp_tail( FILE *log, int start, const char *fname )
{
  FILE *fp;
  int   ret = -1;

  char *ln   = NULL;
  char *line = NULL;
  int   n = 1, lines = 0;

  if( !start || !log || !fname || *fname == '\0' ) return ret;

  fp = fopen( fname, "w" );
  if( !fp )
  {
    return ret;
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  ++ret;

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  while( (ln = fgets( line, PATH_MAX, log )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    fprintf( fp, "%s\n", ln );
    ++lines;
  }

  ret = lines; /* number of lines in the TAIL */

  free( line );

  fseek( log, 0, SEEK_SET );
  fclose( fp );

  return ret;
}

int write_tmp_part( FILE *log, const char *fname )
{
  FILE *fp;
  int   ret = -1;

  char *ln   = NULL;
  char *line = NULL;
  int   lines = 0;

  if( !log || !fname || *fname == '\0' ) return ret;

  fp = fopen( fname, "r" );
  if( !fp )
  {
    return ret;
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  ++ret;

  while( (ln = fgets( line, PATH_MAX, fp )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    fprintf( log, "%s\n", ln );
    ++lines;
  }

  ret = lines; /* number of written lines */

  free( line );

  fclose( fp );

  return ret;
}



static char **create_references( size_t size )
{
  char **references = (char **)0;

  if( size > 0 )
  {
    references = (char **)malloc( size * sizeof(char *) );
    bzero( (void *)references, size * sizeof(char *) );
  }

  return( references );
}

static void free_references( char **references )
{
  if( references )
  {
    char **ptr = references;

    while( *ptr )
    {
      if( *ptr ) free( *ptr );
      ptr++;
    }
    free( references );
  }
}


static char **get_references( FILE *log, int start, unsigned int *cnt, char *grp, char *name, char *version )
{
  char **refs = (char **)0;
  char **ptr;

  char *ln   = NULL;
  char *line = NULL;
  int   n = 1;

  size_t len = 0;

  unsigned int counter, pkgs;

  char *pkg = NULL;

  if( !log || !cnt || *cnt == 0 || !name || !version ) return refs;

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  pkg = (char *)malloc( (size_t)PATH_MAX );
  if( !pkg )
  {
    FATAL_ERROR( "Cannot allocate memory" );
  }

  counter = *cnt;

  if( grp && *grp != '\0' ) { (void)sprintf( pkg, "%s/%s=", grp, name ); }
  else                      { (void)sprintf( pkg, "%s=", name );         }

  len = strlen( pkg );

  refs = ptr = create_references( counter + 1 ); /* null terminated char *references[] */

  while( (ln = fgets( line, PATH_MAX, log )) && (n < start) ) ++n;

  n = 0; pkgs = 0;
  while( (ln = fgets( line, PATH_MAX, log )) )
  {
    ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
    skip_eol_spaces( ln );     /* remove spaces at end-of-line */

    if( strstr( ln, "REQUIRES:" ) ) break; /* if cnt greater than real number of references */

    if( n < counter )
    {
      if( strncmp( ln, pkg, len ) ) /* always remove 'name=version' from list */
      {
        if( refs )
        {
          *ptr = strdup( ln ); ++ptr;
          *ptr = (char *)0;
          ++pkgs;
        }
      }
      ++n;
    }
    else
      break;
  }

  free( line ); free( pkg );

  fseek( log, 0, SEEK_SET );

  if( pkgs == 0 )
  {
    free_references( refs );
    refs = (char **)0;
  }

  *cnt = pkgs;

  return refs;
}


static void _change_references( char *grp, char *name, char *version, const char *log_fname )
{
  int    fd;
  FILE  *log;

  char  *head_fname = NULL, *tail_fname = NULL;
  int    head_lines, tail_lines;

  int          rc, start, stop;
  unsigned int counter;

  int    inc = ( strcmp( operation, "inc" ) == 0 ) ? 1 : 0;

  char **references = NULL;

  if( !name || !version || log_fname == NULL ) return;
  if( check_pkglog_file( log_fname ) != PKGLOG_TEXT ) return;

  log = fopen( (const char *)log_fname, "r+" );
  if( !log )
  {
    ERROR( "Cannot access %s file: %s", log_fname, strerror( errno ) );
    return;
  }

  fd = __lock_file( log );

  rc = get_references_section( &start, &stop, &counter, log );
  if( rc != 0 )
  {
    ERROR( "%s: PKGLOG doesn't contains REFERENCE COUNTER section", log_fname );
    __unlock_file( fd ); fclose( log );
    return;
  }

  head_fname = (char *)alloca( strlen( tmpdir ) + 7 );
  (void)sprintf( head_fname, "%s/.HEAD", tmpdir );

  tail_fname = (char *)alloca( strlen( tmpdir ) + 7 );
  (void)sprintf( tail_fname, "%s/.TAIL", tmpdir );

  head_lines = save_tmp_head( log, start, (const char *)head_fname );
  tail_lines = save_tmp_tail( log, stop - 1, (const char *)tail_fname );

  if( head_lines < 10 && tail_lines < 12 )
  {
    ERROR( "%s: Invalid PKGLOG file", log_fname );
    __unlock_file( fd ); fclose( log );
    return;
  }

  references = get_references( log, start, &counter, grp, name, version );

  if( ftruncate( fd, 0 ) != 0 )
  {
    ERROR( "Cannot change REFERENCE COUNTER in the %s file: %s", log_fname, strerror( errno ) );
    free_references( references );
    __unlock_file( fd ); fclose( log );
    return;
  }

  head_lines = write_tmp_part( log, (const char *)head_fname );

  if( inc ) ++counter;
  fprintf( log, "REFERENCE COUNTER: %u\n", counter );
  if( inc )
  {
    if( grp && *grp != '\0' )
    {
      fprintf( log, "%s/%s=%s\n", grp, name, version );
    }
    else
    {
      fprintf( log, "%s=%s\n", name, version );
    }
  }

  if( references )
  {
    char **ptr = references;

    while( *ptr )
    {
      if( *ptr ) fprintf( log, "%s\n", *ptr );
      ptr++;
    }

    free_references( references );
  }

  tail_lines = write_tmp_part( log, (const char *)tail_fname );

  __unlock_file( fd );
  fclose( log );
}


static void _search_required_packages( const char *dirpath, const char *grp )
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
        if( find_requires( entry->d_name, grp ) )
        {
          _change_references( group, pkgname, pkgver, (const char *)path );
        }
      }
      if( S_ISDIR(entry_sb.st_mode) && grp == NULL )
      {
        _search_required_packages( (const char *)path, (const char *)entry->d_name );
      }
    }
      /* else { stat() returns error code; errno is set; and we have to continue the loop } */

  }

  closedir( dir );
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

  int    ret;

  set_signal_handlers();

  gid = getgid();
  setgroups( 1, &gid );

  fatal_error_hook = fatal_error_actions;

  selfdir = get_selfdir();

  errlog = stderr;

  program = basename( argv[0] );
  get_args( argc, argv );

  /* set_stack_size(); */

  exit_status = get_pkginfo();
  if( exit_status != 0 )
  {
    FATAL_ERROR( "%s: Invalid input PKGLOG file", basename( pkglog_fname ) );
  }

  tmpdir = _mk_tmpdir();
  if( !tmpdir )
  {
    FATAL_ERROR( "Cannot create temporary directory" );
  }
  else
  {
    char *buf = NULL;

    buf = (char *)malloc( (size_t)strlen( tmpdir ) + 11 );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }

    (void)sprintf( (char *)&buf[0], "%s/.REQUIRES", tmpdir );
    requires_fname = strdup( (char *)&buf[0] );
    free( buf );
  }

  /*******************
    Getting REQUIRES:
   */
  if( (ret = get_requires( (const char *)requires_fname, (const char *)pkglog_fname )) > 0 )
  {
    /* We have non-empty list of REQUIRES in the 'requires_fname' file */
    requires = read_requires( (const char *)requires_fname, ret );
    _search_required_packages( (const char *)destination, NULL );
  }

  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
