
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

#define PROGRAM_NAME "install-package"

#include <defs.h>


char *program = PROGRAM_NAME;
char *root = NULL, *pkgs_path = NULL, *rempkgs_path = NULL,
     *pkg_fname = NULL, *asc_fname = NULL, *pkglog_fname = NULL, *pkglist_fname = NULL,
     *tmpdir = NULL, *curdir = NULL, *log_fname = NULL;

int   ask = 0, rqck = 0, gpgck = 0, ignore_chrefs_errors = 0;
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

enum _install_mode {
  CONSOLE = 0,
  INFODIALOG,
  MENUDIALOG
} install_mode = CONSOLE;

enum _priority {
  REQUIRED = 0, /* synonims: REQUIRED    | required    | REQ | req */
  RECOMMENDED,  /* synonims: RECOMMENDED | recommended | REC | rec */
  OPTIONAL,     /* synonims: OPTIONAL    | optional    | OPT | opt */
  SKIP          /* synonims: SKIP        | skip        | SKP | skp */
} priority = REQUIRED;

enum _procedure
{
  INSTALL = 0, /* 'install' */
  UPDATE       /* 'update'  */
} procedure = INSTALL;

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
  if( total_files )       { free( total_files );       } total_files = NULL

void free_resources()
{
  if( root )          { free( root );          root          = NULL; }
  if( pkgs_path )     { free( pkgs_path );     pkgs_path     = NULL; }
  if( rempkgs_path )  { free( rempkgs_path );  rempkgs_path  = NULL; }
  if( pkg_fname )     { free( pkg_fname );     pkg_fname     = NULL; }
  if( asc_fname )     { free( asc_fname );     asc_fname     = NULL; }
  if( pkglog_fname )  { free( pkglog_fname );  pkglog_fname  = NULL; }

  if( pkglist_fname ) { free( pkglist_fname ); pkglist_fname = NULL; }

  if( dirs )          { free_list( dirs );     dirs          = NULL; }
  if( files )         { free_list( files );    files         = NULL; }
  if( links )         { free_list( links );    links         = NULL; }

  if( curdir )        { free( curdir );        curdir        = NULL; }
  if( log_fname )     { free( log_fname );     log_fname     = NULL; }

  if( selfdir )       { free( selfdir );       selfdir       = NULL; }

  FREE_PKGINFO_VARIABLES();
}

void usage()
{
  free_resources();

  fprintf( stdout, "\n" );
  fprintf( stdout, "Usage: %s [options] <package>\n", program );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Install package.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Options:\n" );
  fprintf( stdout, "  -h,--help                     Display this information.\n" );
  fprintf( stdout, "  -v,--version                  Display the version of %s utility.\n", program );
  fprintf( stdout, "  -a,--always-ask               Used with menudialog mode: always ask\n" );
  fprintf( stdout, "                                if a package should be installed regardless\n" );
  fprintf( stdout, "                                of what the package priority is. Without\n" );
  fprintf( stdout, "                                this option, if the priority is equal to\n" );
  fprintf( stdout, "                                REQUIRED, the package is installed without\n" );
  fprintf( stdout, "                                asking for confirmation the installation.\n" );
  fprintf( stdout, "  -c,--check-requires           Check package requires before install.\n" );
#if defined( HAVE_GPG2 )
  fprintf( stdout, "  -g,--gpg-verify               Verify GPG2 signature. The signature must be\n" );
  fprintf( stdout, "                                saved in a file whose name is the same as the\n" );
  fprintf( stdout, "                                package file name, but with the extension '.asc'\n" );
  fprintf( stdout, "                                and located in the same directory as the package.\n" );
#endif
  fprintf( stdout, "  --ignore-chrefs-errors        Ignore change references errors (code: 48).\n" );
#if defined( HAVE_DIALOG )
  fprintf( stdout, "  -i,--info-dialog              Show package description during install\n" );
  fprintf( stdout, "                                process using ncurses dialog.\n" );
  fprintf( stdout, "  -m,--menu-dialog              Ask for confirmation the inatallation,\n" );
  fprintf( stdout, "                                unless the priority is REQUIRED.\n" );
#endif
  fprintf( stdout, "  -l,--pkglist=<FILENAME>       Specify a different package list file\n" );
  fprintf( stdout, "                                to use for read package priority and type\n" );
  fprintf( stdout, "                                of install procedure. By default used the\n" );
  fprintf( stdout, "                                '.pkglist' file found in the directory\n" );
  fprintf( stdout, "                                where source package is placed.\n" );
  fprintf( stdout, "  -p,--priority=<required|recommended|optional|skip>\n" );
  fprintf( stdout, "                                Provides a priority of package instead of\n" );
  fprintf( stdout, "                                the priority defined in the .pkglist file.\n" );
  fprintf( stdout, "  -r,--root=<DIR>               Target rootfs path.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <package>                     The PACKAGE tarball.\n" );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Return codes:\n" );
  fprintf( stdout, "  ------+-------------------------------------------------------\n" );
  fprintf( stdout, "   code | status\n"  );
  fprintf( stdout, "  ------+-------------------------------------------------------\n" );
  fprintf( stdout, "     31 | package is already installed\n" );
  fprintf( stdout, "     32 | package is already installed but not correct\n" );
  fprintf( stdout, "     33 | previous version is already installed\n" );
  fprintf( stdout, "     34 | previous version is already installed but not correct\n" );
  fprintf( stdout, "     35 | a newer version is already installed\n" );
  fprintf( stdout, "     36 | a newer version is already installed but not correct\n" );
  fprintf( stdout, "    ----+----\n" );
  fprintf( stdout, "     41 | installation is aborted due to priority=SKIP\n" );
  fprintf( stdout, "     42 | .pkglist appointed the 'update' procedure instead\n" );
  fprintf( stdout, "     43 | pre-install script returned error status\n" );
  fprintf( stdout, "     44 | uncompress process returned error status\n" );
  fprintf( stdout, "     45 | restore-links script returned error status\n" );
  fprintf( stdout, "     46 | post-install script returned error status\n" );
  fprintf( stdout, "     47 | PKGLOG cannot be stored in the Setup Database\n" );
  fprintf( stdout, "     48 | references cannot be updated in Setup Database\n" );
#if defined( HAVE_GPG2 )
  fprintf( stdout, "    ----+----\n" );
  fprintf( stdout, "     51 | signature verification returned error status\n" );
#endif
  fprintf( stdout, "  ------+-------------------------------------------------------\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "Upon successful completion zero is returned. Other non-zero return\n" );
  fprintf( stdout, "codes imply incorrect completion of the installation.\n" );
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


static void bind_asc_extention( char *name )
{
  char *p = NULL, *q = NULL;

  if( (p = rindex( name, '.' )) && (strlen(p) < 5) )
  {
    if( !strncmp( p, ".gz",  3 ) ||
        !strncmp( p, ".bz2", 4 ) ||
        !strncmp( p, ".xz",  3 )   )
    {
      *p = '\0';
      q = rindex( name, '.' );
      if( q && (strlen(q) < 5) && !strncmp( q, ".tar", 4 ) )
      {
        *q = '\0';
      }
    }
    else if( !strncmp( p, ".tar", 4 ) ||
             !strncmp( p, ".tbz", 4 ) ||
             !strncmp( p, ".tgz", 4 ) ||
             !strncmp( p, ".txz", 4 )   )
    {
      *p = '\0';
    }
  }

  (void)strcat( name, ".asc" );
}

////////////////////////////////////////////////////
//static char *strmode( enum _install_mode mode )
//{
//  char *p = NULL;
//
//  switch( mode )
//  {
//    case CONSOLE:
//      p = "CONSOLE";
//      break;
//    case INFODIALOG:
//      p = "INFODIALOG";
//      break;
//    case MENUDIALOG:
//      p = "MENUDIALOG";
//      break;
//  }
//  return p;
//}
////////////////////////////////////////////////////

static char *strprio( enum _priority priority, int short_name )
{
  char *p = NULL;

  switch( priority )
  {
    case REQUIRED:
      p = ( short_name ) ? "REQ" : "required";
      break;
    case RECOMMENDED:
      p = ( short_name ) ? "REC" : "recommended";
      break;
    case OPTIONAL:
      p = ( short_name ) ? "OPT" : "optional";
      break;
    case SKIP:
      p = ( short_name ) ? "SKP" : "skip";
      break;
  }
  return p;
}

static char *strproc( enum _procedure procedure )
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
#if defined( HAVE_GPG2 )
#if defined( HAVE_DIALOG )
  const char* short_options = "hvacgiml:p:r:";
#else
  const char* short_options = "hvacgl:p:r:";
#endif
#else
#if defined( HAVE_DIALOG )
  const char* short_options = "hvaciml:p:r:";
#else
  const char* short_options = "hvacl:p:r:";
#endif
#endif

#define IGNORE_CHREFS_ERRORS 872

  const struct option long_options[] =
  {
    { "help",                 no_argument,       NULL, 'h' },
    { "version",              no_argument,       NULL, 'v' },
    { "always-ask",           no_argument,       NULL, 'a' },
    { "check-requires",       no_argument,       NULL, 'c' },
#if defined( HAVE_GPG2 )
    { "gpg-verify",           no_argument,       NULL, 'g' },
#endif
    { "ignore-chrefs-errors", no_argument,       NULL, IGNORE_CHREFS_ERRORS },
#if defined( HAVE_DIALOG )
    { "info-dialog",          no_argument,       NULL, 'i' },
    { "menu-dialog",          no_argument,       NULL, 'm' },
#endif
    { "pkglist",              required_argument, NULL, 'l' },
    { "priority",             required_argument, NULL, 'p' },
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
      case 'a':
      {
        ask = 1;
        break;
      }
      case 'c':
      {
        rqck = 1;
        break;
      }
#if defined( HAVE_GPG2 )
      case 'g':
      {
        gpgck = 1;
        break;
      }
#endif

#if defined( HAVE_DIALOG )
      case 'i':
      {
        install_mode = INFODIALOG;
        break;
      }
      case 'm':
      {
        install_mode = MENUDIALOG;
        break;
      }
#endif

      case IGNORE_CHREFS_ERRORS:
      {
        ignore_chrefs_errors = 1;
        break;
      }

      case 'p':
      {
        if( optarg != NULL )
        {
          char *match = NULL;

          if( strlen( (const char *)optarg ) > 2 )
          {
            to_lowercase( optarg );
            if( (match = strstr( optarg, "req" )) && match == optarg ) {
              priority = REQUIRED;
            }
            else if( (match = strstr( optarg, "rec" )) && match == optarg ) {
              priority = RECOMMENDED;
            }
            else if( (match = strstr( optarg, "opt" )) && match == optarg ) {
              priority = OPTIONAL;
            }
            else if( (match = strstr( optarg, "sk" )) && match == optarg ) {
              priority = SKIP;
            }
            else {
              FATAL_ERROR( "Unknown --priority '%s' value", optarg );
            }
          }
          else
          {
            FATAL_ERROR( "Unknown --priority '%s' value", optarg );
          }
        }
        else
          /* option is present but without value */
          usage();
        break;
      }

      case 'l':
      {
        if( optarg != NULL )
        {
          pkglist_fname = strdup( optarg );
        }
        else
          /* option is present but without value */
          usage();
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

    if( stat( (const char *)&buf[0], &st ) == -1 )
    {
      FATAL_ERROR( "Cannot access '%s' file: %s", buf, strerror( errno ) );
    }

    if( S_ISREG(st.st_mode) )
    {
      pkg_fname = strdup( (const char *)&buf[0] );
      bind_asc_extention( buf );
      asc_fname = strdup( (const char *)&buf[0] );
      free( buf );
    }
    else
    {
      FATAL_ERROR( "Input package '%s' is not a regular file", (const char *)argv[optind] );
    }
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


static char *trim( char *s )
{
  char *p = (char *)0;

  if( !s || *s == '\0' ) return p;

  p = s + strlen( s ) - 1;
  while( isspace( *p ) ) { *p-- = '\0'; }
  p = s; while( isspace( *p ) ) { ++p; }

  return( p );
}


static char *size_to_string( size_t pkgsize )
{
  int    nd;
  double sz = (double)pkgsize / (double)1024;

  char  *ret = NULL;
  char  *tmp = NULL;

  tmp = (char *)malloc( PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

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

  ret = strdup( (const char *)&tmp[0] );
  free( tmp );

  return ret;
}

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

  if( !pkgname || !pkgver || !arch || !distroname || !distrover )
  {
    FATAL_ERROR( "Invalid input .PKGINFO file" );
  }

  fclose( pkginfo );
}


static void read_service_files( void )
{
  struct stat st;
  char *fname = pkg_fname;

  enum _input_type  type = IFMT_UNKNOWN;

  bzero( (void *)&st, sizeof( struct stat ) );

  if( stat( (const char *)fname, &st ) == -1 )
  {
    FATAL_ERROR( "Cannot access input '%s' file: %s", fname, strerror( errno ) );
  }

  type = check_input_file( &uncompress, fname );
  if( type != IFMT_PKG )
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
                    "%s/pkginfo -d %s"
                    " -o pkginfo,description,requires,restore-links,install-script,filelist"
                    " %s > /dev/null 2>&1",
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

    compressed_size = size_to_string( st.st_size );

    /******************
      Get PKGLOG file:
     */
    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/pkglog -m -d %s %s  > /dev/null 2>&1",
                    selfdir, tmp, tmp );
    if( len == 0 || len == PATH_MAX - 1 )
    {
      FATAL_ERROR( "Cannot get PKGLOG from %s file", basename( (char *)fname ) );
    }
    p = sys_exec_command( cmd );
    rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
    if( rc != 0 )
    {
      FATAL_ERROR( "Cannot get PKGLOG from '%s' file", basename( (char *)fname ) );
    }

    if( group )
      (void)sprintf( cmd, "%s/%s/%s-%s-%s-%s-%s", tmp, group, pkgname, pkgver, arch, distroname, distrover );
    else
      (void)sprintf( cmd, "%s/%s-%s-%s-%s-%s", tmp, pkgname, pkgver, arch, distroname, distrover );

    bzero( (void *)&st, sizeof( struct stat ) );
    if( stat( (const char *)cmd, &st ) == -1 )
    {
      FATAL_ERROR( "Cannot get PKGLOG from '%s' file: %s", basename( (char *)fname ), strerror( errno ) );
    }

    pkglog_fname = strdup( (const char *)cmd );

    /*************************************
      Attempt to read packages list file:
     */
    {
      if( !pkglist_fname )
      {
        /*****************************************
          Get source packages path if applicable:
         */
        (void)strcpy( cmd, (const char *)fname );
        (void)strcpy( tmp, dirname( cmd ) );

        if( group && !strcmp( group, basename( tmp ) ) )
          (void)strcpy( cmd, (const char *)dirname( tmp ) );
        else
          (void)strcpy( cmd, (const char *)tmp );

        /*****************************************
          Save default packages list file name:
         */
        (void)strcat( cmd, "/.pkglist" );
        pkglist_fname = strdup( (const char *)cmd );
      }

      /**************************
        read .pkglist if exists:
       */
      bzero( (void *)&st, sizeof( struct stat ) );
      if( (stat( (const char *)pkglist_fname, &st ) == 0) && S_ISREG(st.st_mode) )
      {
        char *ln      = NULL;
        char *line    = NULL;

        FILE *pkglist = NULL;

        pkglist = fopen( (const char *)pkglist_fname, "r" );
        if( !pkglist )
        {
          FATAL_ERROR( "Cannot open %s file", pkglist_fname );
        }

        line = (char *)malloc( (size_t)PATH_MAX );
        if( !line )
        {
          FATAL_ERROR( "Cannot allocate memory" );
        }

        while( (ln = fgets( line, PATH_MAX, pkglist )) )
        {
          char *match = NULL;

          ln[strlen(ln) - 1] = '\0'; /* replace new-line symbol      */
          skip_eol_spaces( ln );     /* remove spaces at end-of-line */

          if( (match = strstr( ln, pkgname )) && match == ln )
          {
            char *p = NULL;
            char *name = NULL, *vers = NULL, *desc = NULL, *ball = NULL, *proc = NULL, *prio = NULL;

            name = ln;
            if( (p = index( (const char *)name, ':' )) ) { *p = '\0'; vers = ++p; name = trim( name ); } else continue;
            if( (p = index( (const char *)vers, ':' )) ) { *p = '\0'; desc = ++p; vers = trim( vers ); } else continue;
            if( (p = index( (const char *)desc, ':' )) ) { *p = '\0'; ball = ++p; desc = trim( desc ); } else continue;
            if( (p = index( (const char *)ball, ':' )) ) { *p = '\0'; proc = ++p; ball = trim( ball ); } else continue;
            if( (p = index( (const char *)proc, ':' )) ) { *p = '\0'; prio = ++p; proc = trim( proc ); } else continue;
            prio = trim( prio );

            if( name && vers && desc && ball && proc && prio )
            {
              char *grp = index( (const char *)ball, '/' );
              if( grp )
              {
                *grp = '\0'; grp = ball; grp = trim( grp );
                if( strcmp( group, grp ) ) continue;
              }

              /* read priority: */
              if( strlen( (const char*)prio ) > 2 )
              {
                char *m = NULL;

                to_lowercase( prio );
                if( (m = strstr( prio, "req" )) && m == prio ) {
                  priority = REQUIRED;
                }
                else if( (m = strstr( prio, "rec" )) && m == prio ) {
                  priority = RECOMMENDED;
                }
                else if( (m = strstr( prio, "opt" )) && m == prio ) {
                  priority = OPTIONAL;
                }
                else if( (m = strstr( prio, "sk" )) && m == prio ) {
                  priority = SKIP;
                }
                else {
                  priority = REQUIRED;
                }
              }
              else
              {
                priority = REQUIRED;
              }

              /* read procedure: */
              if( strlen( (const char*)proc ) > 5 )
              {
                char *m = NULL;

                to_lowercase( proc );
                if( (m = strstr( proc, "install" )) && m == proc ) {
                  procedure = INSTALL;
                }
                else if( (m = strstr( proc, "update" )) && m == proc ) {
                  procedure = UPDATE;
                }
                else {
                  procedure = INSTALL;
                }
              }
              else
              {
                procedure = INSTALL;
              }
            }

          } /* End if( match ) */

        } /* End of while( ln = fgets() ) */

        free( line );
        fclose( pkglist );

      } /* End of reading .pkglist */

    } /* End of attemption of reading .pkflist file */

    free( cmd );
    free( tmp );

    if( priority == SKIP )
    {
      exit_status = 41;

      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        char *tmp = NULL;

        tmp = (char *)malloc( (size_t)PATH_MAX );
        if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
        bzero( (void *)tmp, PATH_MAX );

        (void)sprintf( &tmp[0],
                       "\nInstall procedure is skipped due to specified\nthe '%s' priority.\n",
                       strprio( priority, 0 ) );

        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      (const char *)&tmp[0], 6, 0, 0 );

        free( tmp );
#else
        fprintf( stdout,
                 "\nInstall procedure of package '%s-%s' is skipped due to specified '%s' priority.\n\n",
                 pkgname, pkgver, strprio( priority, 0 ) );
#endif
      }
      else
      {
        fprintf( stdout,
                 "\nInstall procedure of package '%s-%s' is skipped due to specified '%s' priority.\n\n",
                 pkgname, pkgver, strprio( priority, 0 ) );
      }

      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();

      exit( exit_status );
    }

    if( procedure != INSTALL )
    {
      exit_status = 42;

      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        char *tmp = NULL;

        tmp = (char *)malloc( (size_t)PATH_MAX );
        if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
        bzero( (void *)tmp, PATH_MAX );

        (void)sprintf( &tmp[0],
                       "\nInstall procedure is skipped because the '%s' procedure\nis specified.\n",
                       strproc( procedure ) );

        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      (const char *)&tmp[0], 6, 0, 0 );

        free( tmp );
#else
        fprintf( stdout,
                 "\nInstall procedure of package '%s-%s' is skipped because the '%s' procedure is specified.\n\n",
                 pkgname, pkgver, strproc( procedure ) );
#endif
      }
      else
      {
        fprintf( stdout,
                 "\nInstall procedure of package '%s-%s' is skipped because the '%s' procedure is specified.\n\n",
                 pkgname, pkgver, strproc( procedure ) );
      }

      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();

      exit( exit_status );
    }

  }
  else
  {
    FATAL_ERROR( "Input %s file is not a regular file", basename( (char *)fname ) );
  }
}

static void check_package( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "%s/check-package --quiet --root=%s %s > /dev/null 2>&1",
                  selfdir, root, pkglog_fname );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot check whether the package '%s-%s' is already installed", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  switch( rc )
  {
    case 30:
      /* Package is not installed. Continue the installation. */
      break;
    case 31:
      fprintf( stdout, "\nPackage '%s-%s' is already installed.\n\n", pkgname, pkgver );
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nPackage is already installed.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nPackage '%s-%s' is already installed.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nPackage '%s-%s' is already installed.\n\n", pkgname, pkgver );
      }
      break;
    case 32:
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nPackage is already installed but not correct.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nPackage '%s-%s' is already installed but not correct.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nPackage '%s-%s' is already installed but not correct.\n\n", pkgname, pkgver );
      }
      break;
    case 33:
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nPrevious version of package is installed.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nPrevious version of package '%s-%s' is installed.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nPrevious version of package '%s-%s' is installed.\n\n", pkgname, pkgver );
      }
      break;
    case 34:
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nPrevious version of package is installed but not correct.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nPrevious version of package '%s-%s' is installed but not correct.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nPrevious version of package '%s-%s' is installed but not correct.\n\n", pkgname, pkgver );
      }
      break;
    case 35:
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nA newer version of package is already installed.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nA newer version of package '%s-%s' is already installed.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nA newer version of package '%s-%s' is already installed.\n\n", pkgname, pkgver );
      }
      break;
    case 36:
      if( install_mode != CONSOLE )
      {
#if defined( HAVE_DIALOG )
        info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                      "\nA newer version of package is installed but not correct.\n", 5, 0, 0 );
#else
        fprintf( stdout, "\nA newer version of package '%s-%s' is installed but not correct.\n\n", pkgname, pkgver );
#endif
      }
      else
      {
        fprintf( stdout, "\nA newer version of package '%s-%s' is installed but not correct.\n\n", pkgname, pkgver );
      }
      break;
    default:
      FATAL_ERROR( "Cannot check whether the package '%s-%s' is already installed", pkgname, pkgver );
      break;
  }

  free( cmd );

  if( rc != 30 )
  {
    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( rc );
  }
}


static void check_requires( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "%s/check-requires --root=%s %s > /dev/null 2>&1",
                  selfdir, root, pkglog_fname );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot check required packages for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\nPackage requires other packages to be installed.\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nPackage '%s-%s' requires other packages to be installed.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nPackage '%s-%s' requires other packages to be installed.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( rc );
  }
}

/********************************************************
  Read .FILELIST and .RESTORELINKS functions used for
  roolback in case postinstall errors:
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

  (void)sprintf( &tmp[0], "%s/.FILELIST", tmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&tmp[0], &st ) == -1) )
  {
    FATAL_ERROR( "Cannot get .FILELIST from '%s' file", basename( (char *)pkg_fname ) );
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

  (void)sprintf( &tmp[0], "%s/.RESTORELINKS", tmpdir );
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

/********************************************************
  Rollback functions:
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
  if( S_ISDIR(path_sb.st_mode) == 0 )     return ret; /* dirpath is not a directory */
  if( (dir = opendir(dirpath) ) == NULL ) return ret; /* Cannot open direcroty; errno is set */

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

static void rollback( void )
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

  /* Try to remove PKGLOG file */
  {
    char *tmp = NULL;

    tmp = (char *)malloc( PATH_MAX );
    if( tmp )
    {
      const char *fname = basename( (char *)pkglog_fname );

      bzero( (void *)tmp, PATH_MAX );

      if( group )
        (void)sprintf( &tmp[0], "%s/%s/%s", pkgs_path, group, fname );
      else
        (void)sprintf( &tmp[0], "%s/%s", pkgs_path, fname );

      (void)unlink( (const char *)&tmp[0] );

      if( group )
      {
        const char *dir = (const char *)dirname( (char *)&tmp[0] );
        if( is_dir_empty( dir ) )
        {
          (void)rmdir( dir );
        }
      }
      free( tmp );
    }
  }

  /* Try to change CWD to the CURRENT directory: */
  (void)chdir( (const char *)curdir );
}
/*
  End of rollback functions.
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

  (void)sprintf( &tmp[0], "%s/.DESCRIPTION", tmpdir );
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
  (void)sprintf( lp, "   Compressed Size: %s\n", compressed_size );
  lp += strlen( compressed_size ) + 21;

  description = strdup( (const char *)&tmp[0] );

  free( buf );
  free( line );
  free( tmp );
}

static int ask_for_install( void )
{
  int ret = 0; /* continue installation */
#if defined( HAVE_DIALOG )
  /******************************************************
    Ask for install dialog shown only in MENUDIALOG mode
    when priority != REQUIRED or --always-ask=yes:
   */
  if( (install_mode == MENUDIALOG) && (((priority == REQUIRED) && ask) || (priority != REQUIRED)) )
  {
    ret =  ask_install_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                            description, 18, 0, 0 );
  }

  if( ret )
  {
    info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                  "\nInstallation terminated by user.\n", 5, 0, 0 );
  }
#endif
  return ret;
}


static void show_install_progress( void )
{
  fprintf( stdout, "\033[2J" ); /* clear screen */

  if( install_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                  description, 16, 0, 0 );
#else
    fprintf( stdout, "\n Install: %s-%s [%s]...\n", pkgname, pkgver, strprio( priority, 0 ));
    /*************************************************
      Ruler: 68 characters + 2 spaces left and right:

                      | ----handy-ruler----------------------------------------------------- | */
    fprintf( stdout, "|======================================================================|\n" );
    fprintf( stdout, "%s\n", description );
    fprintf( stdout, "|======================================================================|\n\n" );
    fprintf( stdout, "\n\n\n" ); /* 3 lines up for final message */
#endif
  }
  else
  {
    fprintf( stdout, "\n Install: %s-%s [%s]...\n", pkgname, pkgver, strprio( priority, 0 ));
    /*************************************************
      Ruler: 68 characters + 2 spaces left and right:

                      | ----handy-ruler----------------------------------------------------- | */
    fprintf( stdout, "|======================================================================|\n" );
    fprintf( stdout, "%s\n", description );
    fprintf( stdout, "|======================================================================|\n\n" );
    fprintf( stdout, "\n\n\n" ); /* 3 lines up for final message */
  }
}


static void pre_install_routine( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cd %s && %s/.INSTALL pre_install %s > /dev/null 2>&1",
                  root, tmpdir, pkgver );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot run pre-install script for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    exit_status = 43;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Pre-install script returned error status.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nPre-install script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nPre-install script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

static const char *fill_decompressor( char *buffer, char compressor )
{
  switch( compressor )
  {
    case 'J':
      (void)sprintf( buffer, "xz --threads=%d -dc", get_nprocs() );
      break;
    case 'j':
      (void)sprintf( buffer, "bzip2 -dc" );
      break;
    case 'z':
      (void)sprintf( buffer, "gzip -dc" );
      break;
    default:
      (void)sprintf( buffer, "cat -" );
      break;
  }
  return (const char *)buffer;
}

static void uncompress_package( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  char decompressor[64];

  (void)fill_decompressor( (char *)&decompressor[0], uncompress );

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cat %s | %s  | tar -C %s "
                  "--exclude='.DESCRIPTION' "
                  "--exclude='.FILELIST' "
                  "--exclude='.INSTALL' "
                  "--exclude='.PKGINFO' "
                  "--exclude='.REQUIRES' "
                  "--exclude='.RESTORELINKS' "
                  "-xf - > /dev/null 2>&1",
                  pkg_fname, decompressor, root );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot uncompress '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    exit_status = 44;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Cannot uncompress package.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nCannot uncompress '%s-%s' package.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nCannot uncompress '%s-%s' package.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

static void restore_links( void )
{
  struct stat st;
  pid_t  p = (pid_t) -1;
  int    rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  (void)sprintf( &cmd[0], "%s/.RESTORELINKS", tmpdir );
  bzero( (void *)&st, sizeof( struct stat ) );
  if( (stat( (const char *)&cmd[0], &st ) == -1) || (st.st_size < 8) )
  {
    free( cmd );
    return;
  }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cd %s && sh %s/.RESTORELINKS > /dev/null 2>&1",
                  root, tmpdir );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot restore links for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    rollback();

    exit_status = 45;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Restore-links script returned error status.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nRestore-links script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nRestore-links script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

static void post_install_routine( void )
{
  pid_t p = (pid_t) -1;
  int   rc;

  int   len = 0;
  char *cmd = NULL;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "cd %s && %s/.INSTALL post_install %s > /dev/null 2>&1",
                  root, tmpdir, pkgver );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot run post-install script for '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    rollback();

    exit_status = 46;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Post-install script returned error status.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nPost-install script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nPost-install script of '%s-%s' returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

static void finalize_installation( void )
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

  if( group )
    (void)sprintf( &tmp[0], "%s/%s/", pkgs_path, group );
  else
    (void)sprintf( &tmp[0], "%s/", pkgs_path );

  if( _mkdir_p( tmp, S_IRWXU | S_IRWXG | S_IRWXO ) != 0 )
  {
    FATAL_ERROR( "Cannot access '/%s' directory", PACKAGES_PATH );
  }

  /****************************************
    Store PKGLOG file into Setup Database:
   */
  len = snprintf( &cmd[0], PATH_MAX,
                  "cp %s %s > /dev/null 2>&1",
                  pkglog_fname, (const char *)&tmp[0] );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot store '%s' pkglog file", basename( (char *)pkglog_fname ) );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
  if( rc != 0 )
  {
    rollback();

    free( cmd );
    free( tmp );

    exit_status = 47;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Cannot store PKGLOG file into Setup Database.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nCannot store '%s' pkglog file into Setup Database.\n\n", basename( (char *)pkglog_fname ) );
#endif
    }
    else
    {
      fprintf( stdout, "\nCannot store '%s' pkglog file into Setup Database.\n\n", basename( (char *)pkglog_fname ) );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }

  /*********************************************
    Increment references in the Setup Database:
   */
  if( group )
    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/chrefs --operation=inc --destination=%s %s/%s > /dev/null 2>&1",
                    selfdir, pkgs_path, group, basename( (char *)pkglog_fname ) );
  else
    len = snprintf( &cmd[0], PATH_MAX,
                    "%s/chrefs --operation=inc --destination=%s %s > /dev/null 2>&1",
                    selfdir, pkgs_path, basename( (char *)pkglog_fname ) );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot increment '%s-%s' package references", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( (rc != 0) && !ignore_chrefs_errors )
  {
    free( tmp );

    rollback();

    exit_status = 48;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Cannot increment package references in Setup Database.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nCannot increment '%s-%s' package references in Setup Database.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nCannot increment '%s-%s' package references in Setup Database.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }

  /*************************************************
    Remove backup PKGLOG file from removed-packages
    directory if exists:
   */
  bzero( (void *)tmp, PATH_MAX );
  {
    const char *fname = basename( (char *)pkglog_fname );

    if( group )
      (void)sprintf( &tmp[0], "%s/%s/%s", rempkgs_path, group, fname );
    else
      (void)sprintf( &tmp[0], "%s/%s", rempkgs_path, fname );

    (void)unlink( (const char *)&tmp[0] );

    if( group )
    {
      const char *dir = (const char *)dirname( (char *)&tmp[0] );
      if( is_dir_empty( dir ) )
      {
        (void)rmdir( dir );
      }
    }
  }

  free( tmp );
}

#if defined( HAVE_GPG2 )
static void verify_gpg_signature( void )
{
  struct stat st;
  pid_t  p = (pid_t) -1;
  int    rc;

  int   len = 0;
  char *cmd = NULL;

  bzero( (void *)&st, sizeof( struct stat ) );

  /******************************************************************
    Do not try to verify signature if '.asc' file is not accessible:
   */
  if( stat( (const char *)asc_fname, &st ) == -1 ) return;

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "gpg2 --verify %s %s > /dev/null 2>&1",
                  asc_fname, pkg_fname );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot verify GPG2 signature of '%s-%s' package", pkgname, pkgver );
  }
  p = sys_exec_command( cmd );
  rc = sys_wait_command( p, (char *)NULL, PATH_MAX );

  free( cmd );

  if( rc != 0 )
  {
    exit_status = 51;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                    "\n\\Z1Cannot verify GPG2 signature of the package.\\Zn\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nGPG2 signature verification of '%s-%s' package returned error status.\n\n", pkgname, pkgver );
#endif
    }
    else
    {
      fprintf( stdout, "\nGPG2 signature verification of '%s-%s' package returned error status.\n\n", pkgname, pkgver );
    }

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}
#endif


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


  /************************************************************
    Getting Service Files, reading pkginfo, preserving pkglog:
   */
  read_service_files();

  /****************************************************
    Checking whether the package is already installed:
   */
  check_package();

  if( rqck ) check_requires();
#if defined( HAVE_GPG2 )
  if( gpgck ) verify_gpg_signature();
#endif

  read_filelist();
  read_restorelinks();

  read_description();

  if( ask_for_install() )
  {
    /* Terminate installation: */
    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }

  show_install_progress();

  /*************
    DO INSTALL:
   */
  pre_install_routine();
  uncompress_package();
  restore_links();
  post_install_routine();
  finalize_installation();

  fprintf( stdout, "\033[3A" ); /* move cursor up 3 lines */

  if( (install_mode != CONSOLE) && (install_mode == MENUDIALOG) )
  {
#if defined( HAVE_DIALOG )
    info_pkg_box( "Install:", pkgname, pkgver, strprio( priority, 0 ),
                  "\nPackage has been installed.\n", 5, 0, 0 );
#else
    fprintf( stdout, "\nPackage '%s-%s' has been installed.\n\n", pkgname, pkgver );
#endif
  }
  else
  {
    if( (install_mode != INFODIALOG) )
    {
      fprintf( stdout, "\nPackage '%s-%s' has been installed.\n\n", pkgname, pkgver );
    }
  }

  setup_log( "Package '%s-%s' has been installed", pkgname, pkgver );

  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
