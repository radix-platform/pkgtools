
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

#define PROGRAM_NAME "pkginfo"

#include <defs.h>


char *program     = PROGRAM_NAME;
char *destination = NULL, *operation = NULL, *pkglog_fname = NULL;
int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

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

FILE *pkglog = NULL;
FILE *output = NULL;


#define FREE_PKGINFO_VARIABLES() \
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
  if( selfdir )      { free( selfdir );      selfdir     = NULL;  }
  if( destination )  { free( destination );  destination = NULL;  }
  if( operation )    { free( operation );    operation = NULL;    }
  if( pkglog_fname ) { free( pkglog_fname ); pkglog_fname = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <pkglog|package>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Read information from <pkglog> or <package> file and create\n" );
  fprintf( stdout, "requested package's service files in the destination directory.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  -d,--destination=<DIR>        Target directory to save output files.\n" );
  fprintf( stdout, "  -o,--operations=<OP1,..,OPn>  Comma separated list of:\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Operations:\n" );
  fprintf( stdout, "  ----------------+----------------\n" );
  fprintf( stdout, "   operation name | output file\n"     );
  fprintf( stdout, "  ----------------+----------------\n" );
  fprintf( stdout, "   pkginfo        | .PKGINFO\n"        );
  fprintf( stdout, "   references     | .REFERENCES\n"     );
  fprintf( stdout, "   requires       | .REQUIRES\n"       );
  fprintf( stdout, "   description    | .DESCRIPTION\n"    );
  fprintf( stdout, "   restore-links  | .RESTORELINKS\n"   );
  fprintf( stdout, "   install-script | .INSTALL\n"        );
  fprintf( stdout, "   filelist       | .FILELIST\n"       );
  fprintf( stdout, "  ----------------+----------------\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <pkglog|package>              PKGLOG file or package tarball.\n"  );
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

void fatal_error_actions( void )
{
  logmsg( errlog, MSG_NOTICE, "Free resources on FATAL error..." );
  free_resources();
}

void sigint( int signum )
{
  (void)signum;
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
    { "operations",  required_argument, NULL, 'o' },
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

  if( operation == NULL ) usage();

  /* last command line argument is the LOGFILE */
  if( optind < argc )
  {
    pkglog_fname = strdup( argv[optind++] );
    if( pkglog_fname == NULL )
    {
      usage();
    }
    pkglog_type = check_pkglog_file( (const char *)pkglog_fname );
    if( pkglog_type == PKGLOG_UNKNOWN )
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


int printf_variable( FILE *output, char *log_fname, char *name, char *value, char *pattern, int error )
{
  int   exit_status = 0; /* local errors counter */
  char  buf[24];
  char *p;

  bzero( (void *)buf, 24 );

  if( pattern )
    (void)sprintf( (char *)&buf[0], "%s", pattern );

  p = (char *)&buf[0];
  p[strlen(buf) - 1] = '\0'; /* skip colon at end of pattern */

  if( value )
  {
    fprintf( output, "%s=%s\n", name, value );
  }
  else
  {
    if( error )
    {
      ERROR( "There is no %s declaration in the %s file", p, log_fname );
    }
    else
    {
      if( ! DO_NOT_WARN_ABOUT_OPT_PKGINFO_ITEMS )
        WARNING( "There is no %s declaration in the %s file", p, log_fname );
    }
  }

  return( exit_status );
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
    *s = '"'; ++s; /* start " */
    ++p;
    while( *p && p < q )
    {
      *s = *p;
      ++p; ++s;
    }
    *s++ = '"';    /* stop "  */
    *s   = '\0';
  }
}

int write_pkginfo()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 10 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.PKGINFO" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;
    char *desc = NULL;

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

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    while( (ln = fgets( line, PATH_MAX, pkglog )) )
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
      if( (match = strstr( ln, url_pattern )) && match == ln )
      {
        url = skip_spaces( ln + strlen( url_pattern ) );
      }
      if( (match = strstr( ln, license_pattern )) && match == ln )
      {
        license = skip_spaces( ln + strlen( license_pattern ) );
      }
      if( (match = strstr( ln, uncompressed_size_pattern )) && match == ln )
      {
        uncompressed_size = skip_spaces( ln + strlen( uncompressed_size_pattern ) );
      }
      if( (match = strstr( ln, total_files_pattern )) && match == ln )
      {
        total_files = skip_spaces( ln + strlen( total_files_pattern ) );
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
        ln = fgets( line, PATH_MAX, pkglog );
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol */

        bzero( (void *)buf, PATH_MAX );
        get_short_description( buf, (const char *)line );
        if( buf[0] != '\0' )
        {
          desc = strdup( buf );
        }
        free( buf );
      }
    }

    free( line );

    ret += printf_variable( output, pkglog_fname, "pkgname",           pkgname,           pkgname_pattern,           1 );
    ret += printf_variable( output, pkglog_fname, "pkgver",            pkgver,            pkgver_pattern,            1 );
    ret += printf_variable( output, pkglog_fname, "arch",              arch,              arch_pattern,              1 );
    ret += printf_variable( output, pkglog_fname, "distroname",        distroname,        distroname_pattern,        1 );
    ret += printf_variable( output, pkglog_fname, "distrover",         distrover,         distrover_pattern,         1 );
    ret += printf_variable( output, pkglog_fname, "group",             group,             group_pattern,             0 );
    if( desc != NULL )
    {
      ret += printf_variable( output, pkglog_fname, "short_description", desc, "SHORT DESCRIPTION:", 0 );
      free( desc ); desc = NULL;
    }
    ret += printf_variable( output, pkglog_fname, "url",               url,               url_pattern,               0 );
    ret += printf_variable( output, pkglog_fname, "license",           license,           license_pattern,           0 );
    ret += printf_variable( output, pkglog_fname, "uncompressed_size", uncompressed_size, uncompressed_size_pattern, 0 );
    ret += printf_variable( output, pkglog_fname, "total_files",       total_files,       total_files_pattern,       0 );

    FREE_PKGINFO_VARIABLES();

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
}


/*
  NOTE:
    1. first line has number 1.
    2. sections are ordered according to following list:
 */
int reference_counter   = 0;
int requires            = 0;
int package_description = 0;
int restore_links       = 0;
int install_script      = 0;
int file_list           = 0;

int refcount = 0;


int get_pkglog_sections()
{
  int ret = -1, found = 0;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( pkglog != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    while( (ln = fgets( line, PATH_MAX, pkglog )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, "REFERENCE COUNTER:" )) && match == ln ) /* at start of line only */
      {
        reference_counter = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "REQUIRES:" )) && match == ln )
      {
        requires = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "PACKAGE DESCRIPTION:" )) && match == ln )
      {
        package_description = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "RESTORE LINKS:" )) && match == ln )
      {
        restore_links = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "INSTALL SCRIPT:" )) && match == ln )
      {
        install_script = ret + 1;
        ++found;
      }
      if( (match = strstr( ln, "FILE LIST:" )) && match == ln )
      {
        file_list = ret + 1;
        ++found;
      }

      ++ret;
    }

    free( line );

    ret = found;

    fclose( pkglog ); pkglog = NULL;
  }

  return( ret );
}


int get_pkglog_line( char *pattern )
{
  int ret = -1, found = 0;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( pkglog != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;
    while( (ln = fgets( line, PATH_MAX, pkglog )) )
    {
      char *match = NULL;

      if( (match = strstr( ln, pattern )) && match == ln ) /* at start of line only */
      {
        ++ret;
        ++found;
        break;
      }
      ++ret;
    }
    if( !found ) ret = 0;

    free( line );

    fclose( pkglog ); pkglog = NULL;
  }
  return( ret );
}


int get_ref_cnt()
{
  int ret = -1, found = 0;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( pkglog != NULL )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    while( (ln = fgets( line, PATH_MAX, pkglog )) )
    {
      char *match = NULL;

      ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
      skip_eol_spaces( ln );     /* remove spaces at end-of-line */

      if( (match = strstr( ln, "REFERENCE COUNTER:" )) && match == ln ) /* at start of line only */
      {
        char *cnt = skip_spaces( ln + strlen( "REFERENCE COUNTER:" ) );
        ret = atoi( cnt );
        free( cnt );
        ++found;
        break;
      }
    }
    if( !found ) ret = -1;

    free( line );

    fclose( pkglog ); pkglog = NULL;
  }
  return( ret );
}


int write_references()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 13 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.REFERENCES" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( reference_counter && reference_counter < requires )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > reference_counter) && (n < requires) )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
}


int write_requires()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 11 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.REQUIRES" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( requires && requires < package_description )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > requires) && (n < package_description) )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
}


int write_package_description()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 14 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.DESCRIPTION" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( package_description && package_description < restore_links )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > package_description) && (n < restore_links) )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
}


int write_restore_links()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 15 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.RESTORELINKS" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( restore_links && restore_links < install_script )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > restore_links) && (n < install_script) )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
}


int write_install_script()
{
  int ret = -1;
  char *output_fname = NULL;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    output_fname = (char *)alloca( strlen( destination ) + 10 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.INSTALL" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( install_script && install_script < file_list )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( (n > install_script) && (n < file_list) )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  chmod( (const char *)output_fname, (mode_t)0755 );

  return( ret );
}


int write_filelist()
{
  int ret = -1;

  if( pkglog_fname != NULL )
  {
    pkglog = fopen( (const char *)pkglog_fname, "r" );
    if( !pkglog )
    {
      FATAL_ERROR( "Cannot open %s file", pkglog_fname );
    }
  }

  if( destination != NULL )
  {
    char *output_fname = NULL;

    output_fname = (char *)alloca( strlen( destination ) + 11 );
    strcpy( output_fname, destination );
    strcat( output_fname, "/.FILELIST" );
    output = fopen( (const char *)output_fname, "w" );
    if( !output )
    {
      FATAL_ERROR( "Cannot create %s file", output_fname );
    }
  }

  if( (pkglog != NULL) && (output != NULL) )
  {
    char *ln   = NULL;
    char *line = NULL;

    line = (char *)malloc( (size_t)PATH_MAX );
    if( !line )
    {
      FATAL_ERROR( "Cannot allocate memory" );
    }

    ++ret;

    if( file_list )
    {
      int n = 1, lines = 0;

      ++ret;

      while( (ln = fgets( line, PATH_MAX, pkglog )) )
      {
        ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
        skip_eol_spaces( ln );     /* remove spaces at end-of-line */

        if( n > file_list )
        {
          fprintf( output, "%s\n", ln );
          ++lines;
        }
        ++n;
      }

      ret = lines; /* number of lines in the LIST */
    }

    free( line );

    fclose( pkglog ); pkglog = NULL;
    fclose( output ); output = NULL;
  }

  return( ret );
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
  int    sections = 0;
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

  if( pkglog_type == PKGLOG_TEXT )
  {
    sections = get_pkglog_sections();
    if( sections < 3 )
    {
      FATAL_ERROR( "%s: Wrong PKGLOG file format", basename( pkglog_fname) );
    }

    refcount = get_ref_cnt();

    if( strstr( operation, "pkginfo" ) ) exit_status += write_pkginfo();

    if( strstr( operation, "references" ) )
    {
      if( !reference_counter )
      {
        WARNING( "The REFERENCE COUNTER is not present in %s file", basename( pkglog_fname ) );
      }
      else
      {
        if( write_references() != refcount )
        {
          WARNING( "The REFERENCE COUNTER invalid in %s file", basename( pkglog_fname ) );
        }
      }
    }

    if( strstr( operation, "requires" ) )
    {
      if( write_requires() <= 0 )
      {
        if( ! DO_NOT_WARN_ABOUT_EMPTY_REQUIRES )
          WARNING( "The REQUIRES is not present in %s file", basename( pkglog_fname ) );
      }
    }

    if( strstr( operation, "description" ) )
    {
      if( write_package_description() <= 0 )
      {
        WARNING( "The PACKAGE DESCRIPTION is not present in %s file", basename( pkglog_fname ) );
      }
    }

    if( strstr( operation, "restore-links" ) )
    {
      if( write_restore_links() <= 0 )
      {
        if( ! DO_NOT_WARN_ABOUT_EMPTY_RESTORE_LINKS )
          WARNING( "The RESTORE LINKS is not present in %s file", basename( pkglog_fname ) );
      }
    }

    if( strstr( operation, "install-script" ) )
    {
      if( write_install_script() <= 0 )
      {
        ERROR( "The INSTALL SCRIPT is not present in %s file", basename( pkglog_fname ) );
      }
    }

    if( strstr( operation, "filelist" ) )
    {
      if( write_filelist() <= 0 )
      {
        ERROR( "The FILE LIST is not present in %s file", basename( pkglog_fname ) );
      }
    }

  }
  else /* TARBALL: */
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


    if( strstr( operation, "pkginfo" ) ) /* strongly required */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get .PKGINFO from %s file", basename( pkglog_fname ) );

      len = snprintf( &cmd[0], PATH_MAX, "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".PKGINFO" );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( errmsg );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 )
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

    /* .REFERENCES is not present in package tarball */

    if( strstr( operation, "requires" ) ) /* optional; may be warning */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get .REQUIRES from %s file", basename( pkglog_fname ) );

      (void)sprintf( &cmd[0], "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".REQUIRES" );
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 && DO_NOT_WARN_ABOUT_EMPTY_REQUIRES == 0 )
      {
        WARNING( errmsg );
      }
    }

    if( strstr( operation, "description" ) ) /* optional; always warning */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &cmd[0], "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".DESCRIPTION" );
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 )
      {
        WARNING( "Cannot get package .DESCRIPTION from %s file", basename( pkglog_fname ) );
      }
    }

    if( strstr( operation, "restore-links" ) ) /* optional; may be warning */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get .RESTORELINKS script from %s file", basename( pkglog_fname ) );

      (void)sprintf( &cmd[0], "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".RESTORELINKS" );
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 && DO_NOT_WARN_ABOUT_EMPTY_RESTORE_LINKS == 0 )
      {
        WARNING( errmsg );
      }
    }

    if( strstr( operation, "install-script" ) ) /* strongly required */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get .INSTALL script from %s file", basename( pkglog_fname ) );

      len = snprintf( &cmd[0], PATH_MAX, "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".INSTALL" );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( errmsg );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 )
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

    if( strstr( operation, "filelist" ) ) /* strongly required */
    {
      bzero( (void *)cmd, PATH_MAX );
      bzero( (void *)errmsg, PATH_MAX );
      bzero( (void *)wmsg, PATH_MAX );

      (void)sprintf( &errmsg[0], "Cannot get .FILELIST from %s file", basename( pkglog_fname ) );

      len = snprintf( &cmd[0], PATH_MAX, "tar -C %s -x%sf %s %s > /dev/null 2>&1", destination, uncompress, pkglog_fname, ".FILELIST" );
      if( len == 0 || len == PATH_MAX - 1 )
      {
        FATAL_ERROR( errmsg );
      }
      p = sys_exec_command( cmd );
      rc = sys_wait_command( p, (char *)&wmsg[0], PATH_MAX );
      if( rc != 0 )
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


  free_resources();

  exit( exit_status );
}
