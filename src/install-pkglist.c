
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

#include <pthread.h>

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

#if defined( HAVE_DIALOG )
#include <dialog-ui.h>
#endif

#define PROGRAM_NAME "install-pkglist"

#include <defs.h>

#define WAIT_USEC_FOR_CHILD 10000

char *program = PROGRAM_NAME;
char *root = NULL, *srcdir = NULL, *pkglist_fname = NULL,
     *tmpdir = NULL, *curdir = NULL;

int   rqck = 0, gpgck = 0, progress = 0, parallel = 0, ncpus = 0;

int   exit_status = EXIT_SUCCESS; /* errors counter */
char *selfdir     = NULL;

int __done = 0, __child = 0, __terminated = 0, __successful = 0, __all = 0;

enum _install_mode {
  CONSOLE = 0,
  INFODIALOG,
  MENUDIALOG
} install_mode = CONSOLE;

enum _input_type {
  IFMT_PKG = 0,
  IFMT_LOG,

  IFMT_UNKNOWN
};

/*********************************************
  Package structures and declarations:
 */
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

struct pkg
{
  char *group;
  char *name;
  char *version;
};

struct package
{
  char  *name;
  char  *version;
  char  *group;
  char  *description;

  char  *tarball;

  enum  _procedure procedure; /* install procedure     */
  enum  _priority  priority;  /* install user priority */
};

enum _priority install_priority = OPTIONAL; /* by default allow all packages exept 'SKIP' */

struct dlist *requires = NULL; /* list of pkg structs     */
struct dlist *packages = NULL; /* list of package structs */

static void free_requires( void );
static void free_packages( void );

/*
  End of package structures and declarations.
 *********************************************/

/*********************************************
  Return status declarations:
 */
struct pkgrc
{
  pid_t  pid;
  int    status;
  char  *name;
  char  *version;
  char  *group;
};

struct dlist *pkgrcl = NULL; /* list of pkgrc structs */

static void          free_pkgrcl( void );
static struct pkgrc *find_pkgrc( struct dlist *list, pid_t pid );
/*
  End of return status declarations.
 *********************************************/


void free_resources()
{
  if( root )           { free( root );           root           = NULL; }
  if( srcdir )         { free( srcdir );         srcdir         = NULL; }
  if( pkglist_fname )  { free( pkglist_fname );  pkglist_fname  = NULL; }

  if( requires ) free_requires();
  if( packages ) free_packages();
  if( pkgrcl )   free_pkgrcl();

  if( curdir )         { free( curdir );         curdir         = NULL; }
  if( selfdir )        { free( selfdir );        selfdir        = NULL; }
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

  fprintf( stdout, "  -c,--check-requires           Check the list of requires before install.\n" );
  fprintf( stdout, "  -g,--gpg-verify               Verify GPG2 signature. The signature must be\n" );
  fprintf( stdout, "                                saved in a file whose name is the same as the\n" );
  fprintf( stdout, "                                package file name, but with the extension '.asc'\n" );
  fprintf( stdout, "                                and located in the same directory as the package.\n" );
#if defined( HAVE_DIALOG )
  fprintf( stdout, "  -i,--info-dialog              Show package description during install\n" );
  fprintf( stdout, "                                process using ncurses dialog.\n" );
  fprintf( stdout, "  -m,--menu-dialog              Ask for confirmation the inatallation.\n" );
#endif
  fprintf( stdout, "  --parallel                    Parallel installation (dangerous; required the\n" );
  fprintf( stdout, "                                checking of DB integrity after installation).\n" );
  fprintf( stdout, "  --progress                    Show progress bar instead of packages information.\n" );

  fprintf( stdout, "  -p,--priority=<required|recommended|optional|all>\n" );
  fprintf( stdout, "                                Рackage priorities allowed for installation:\n" );
  fprintf( stdout, "                                - optional | all ) install all packages;\n" );
  fprintf( stdout, "                                - recommended )    install required and recommended;\n" );
  fprintf( stdout, "                                - required )       install only required packages.\n" );

  fprintf( stdout, "  -r,--root=<DIR>               Target rootfs path.\n" );

  fprintf( stdout, "  -s,--source=<DIR>             Packages source directory.\n" );

  fprintf( stdout, "\n" );
  fprintf( stdout, "Parameter:\n" );
  fprintf( stdout, "  <DIR|pkglist>                 Input PKGLIST file name or a source\n"  );
  fprintf( stdout, "                                directory to find default PKGLIST.\n"  );
  fprintf( stdout, "\n" );
  fprintf( stdout, "If sourse packages directory is defined by option -s,--source then\n" );
  fprintf( stdout, "specified <DIR|pkglist> argumet will be considered relative to the\n" );
  fprintf( stdout, "source directory.\n" );
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
    struct pkgrc *pkgrc = find_pkgrc( pkgrcl, pid );

    ++__terminated; /* One of children with 'pid' is terminated */

    if( WIFEXITED( status ) )
    {
      if( (int)WEXITSTATUS( status ) > 0 )
      {
        ++exit_status; /* printf( "Child %d returned non zero status: %d\n", pid, (int)WEXITSTATUS (status) ); */
        if( pkgrc ) pkgrc->status = (int)WEXITSTATUS (status);
      }
      else
      {
        ++__successful; /* printf( "Child %d terminated with status: %d\n", pid, (int)WEXITSTATUS (status) ); */
        if( pkgrc ) pkgrc->status = (int)WEXITSTATUS (status);
      }
    }
    else if( WIFSIGNALED( status ) )
    {
      ++exit_status; /* printf( "Child %d terminated on signal: %d\n", pid, WTERMSIG( status ) ); */
      if( pkgrc ) pkgrc->status = 253;
    }
    else
    {
      ++exit_status; /* printf( "Child %d terminated on unknown reason\n", pid ); */
      if( pkgrc ) pkgrc->status = 254;
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


static enum _input_type check_package_file( char *uncompress, const char *fname )
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
  const char* short_options = "hvcgimp:r:s:";
#else
  const char* short_options = "hvcgp:r:s:";
#endif

#define PROGRESS 812
#define PARALLEL 872

  const struct option long_options[] =
  {
    { "help",           no_argument,       NULL, 'h' },
    { "version",        no_argument,       NULL, 'v' },
    { "check-requires", no_argument,       NULL, 'c' },
    { "gpg-verify",     no_argument,       NULL, 'g' },
#if defined( HAVE_DIALOG )
    { "info-dialog",    no_argument,       NULL, 'i' },
    { "menu-dialog",    no_argument,       NULL, 'm' },
#endif
    { "parallel",       no_argument,       NULL, PARALLEL },
    { "progress",       no_argument,       NULL, PROGRESS },
    { "priority",       required_argument, NULL, 'p' },
    { "root",           required_argument, NULL, 'r' },
    { "source",         required_argument, NULL, 's' },
    { NULL,             0,                 NULL,  0  }
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
      case 'c':
      {
        rqck = 1;
        break;
      }
      case 'g':
      {
        gpgck = 1;
        break;
      }

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

      case PARALLEL:
      {
        parallel = 1;
        break;
      }
      case PROGRESS:
      {
        progress = 1;
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
              install_priority = REQUIRED;
            }
            else if( (match = strstr( optarg, "rec" )) && match == optarg ) {
              install_priority = RECOMMENDED;
            }
            else if( (match = strstr( optarg, "opt" )) && match == optarg ) {
              install_priority = OPTIONAL;
            }
            else if( (match = strstr( optarg, "all" )) && match == optarg ) {
              install_priority = OPTIONAL;
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

      case 's':
      {
        if( optarg != NULL )
        {
          char cwd[PATH_MAX];

          bzero( (void *)cwd, PATH_MAX );
          if( optarg[0] != '/' && curdir )
          {
            (void)sprintf( cwd, "%s/%s", curdir, optarg );
            srcdir = strdup( (const char *)cwd );
          }
          else
          {
            srcdir = strdup( optarg );
          }
          remove_trailing_slash( srcdir );
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


  /* last command line argument is the intput PKGLIST file */
  if( optind < argc )
  {
    struct stat st;
    char  *buf = NULL;

    bzero( (void *)&st, sizeof( struct stat ) );

    buf = (char *)malloc( (size_t)PATH_MAX );
    if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
    bzero( (void *)buf, PATH_MAX );

    (void)strcpy( buf, (const char *)argv[optind++] );
    remove_trailing_slash( (char *)&buf[0] );

    if( srcdir)
    {
      char *tmp = strdup( (const char *)&buf[0] );

      /* Ignore already defined srcdir if absolute path is specified: */
      if( buf[0] != '/' )
      {
        if( !strncmp( tmp, "./", 2 ) && strncmp( tmp, "..", 2 ) )
          (void)sprintf( buf, "%s/%s", srcdir, tmp + 2 );
        else if( (strlen( tmp ) == 1) && !strncmp( tmp, ".", 1 ) )
          (void)sprintf( buf, "%s", srcdir );
        else
          (void)sprintf( buf, "%s/%s", srcdir, tmp );
      }
      free( tmp );

      free( srcdir ); srcdir = strdup( (const char *)buf );
    }
    else
    {
      char *tmp = strdup( (const char *)&buf[0] );

      if( buf[0] != '/' && curdir )
      {
        if( !strncmp( tmp, "./", 2 ) && strncmp( tmp, "..", 2 ) )
          (void)sprintf( buf, "%s/%s", curdir, tmp + 2 );
        else if( (strlen( tmp ) == 1) && !strncmp( tmp, ".", 1 ) )
          (void)sprintf( buf, "%s", curdir );
        else
          (void)sprintf( buf, "%s/%s", curdir, tmp );
      }
      free( tmp );

      srcdir = strdup( (const char *)buf );
    }

    stat( (const char *)&buf[0], &st ); /* Do not check return status */

    if( S_ISDIR(st.st_mode) )
    {
      /**********************************************************
        Add default '.pkglist' file name to the source dir name:
       */
      (void)sprintf( buf, "%s/.pkglist", srcdir );
      pkglist_fname = strdup( (const char *)buf );
    }
    else
    {
      if( S_ISREG(st.st_mode) )
      {
        pkglist_fname = strdup( (const char *)buf );
        free( srcdir ); srcdir = strdup( (const char *)dirname( (char *)&buf[0] ) );
      }
      else
      {
        FATAL_ERROR( "Specified '%s' PKGLIST is not a regular file", basename( (char *)&buf[0] ) );
      }
    }

    free( buf );
  }
  else
  {
    usage();
  }

  /*********************************************
    root is always have the trailing slash '/':
   */
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

    free( buf );
  }
  /*
    End of set root path routine.
   *********************************************/
}




/*********************************************
  Package functions:
 */
static char *strprio( enum _priority priority, int short_name )
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


static struct pkg *pkg_alloc( void )
{
  struct pkg *pkg = NULL;

  pkg = (struct pkg *)malloc( sizeof( struct pkg ) );
  if( !pkg ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)pkg, sizeof( struct pkg ) );

  return pkg;
}

static void pkg_free( struct pkg *pkg )
{
  if( pkg )
  {
    if( pkg->name )    { free( pkg->name );    pkg->name    = NULL; }
    if( pkg->version ) { free( pkg->version ); pkg->version = NULL; }
    if( pkg->group )   { free( pkg->group );   pkg->group   = NULL; }

    free( pkg );
  }
}

static void __pkg_free_func( void *data, void *user_data )
{
  struct pkg *pkg = (struct pkg *)data;
  if( pkg ) { pkg_free( pkg ); }
}

static void free_requires( void )
{
  if( requires ) { dlist_free( requires, __pkg_free_func ); requires = NULL; }
}

static void add_required( struct pkg *pkg )
{
  requires = dlist_append( requires, (void *)pkg );
}

///////////////////// only if we deside to print requires list
//static void _print_requires( void *data, void *user_data )
//{
//  struct pkg *pkg = (struct pkg *)data;
//
//  if( pkg )
//  {
//    if( pkg->group )
//      fprintf( stderr, "%s/%s:%s\n", pkg->group, pkg->name, pkg->version );
//    else
//      fprintf( stderr, "%s:%s\n", pkg->name, pkg->version );
//  }
//}
//
//static void print_requires( void )
//{
//  dlist_foreach( requires, _print_requires, NULL );
//}
/////////////////////

static struct package *package_alloc( void )
{
  struct package *package = NULL;

  package = (struct package *)malloc( sizeof( struct package ) );
  if( !package ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)package, sizeof( struct package ) );

  return package;
}

static void package_free( struct package *package )
{
  if( package )
  {
    if( package->name )    { free( package->name );    package->name    = NULL; }
    if( package->version ) { free( package->version ); package->version = NULL; }
    if( package->group )   { free( package->group );   package->group   = NULL; }

    if( package->description ) { free( package->description ); package->description   = NULL; }
    if( package->tarball )     { free( package->tarball );     package->tarball   = NULL; }

    free( package );
  }
}

static void __package_free_func( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  if( package ) { package_free( package ); }
}

static void free_packages( void )
{
  if( packages ) { dlist_free( packages, __package_free_func ); packages = NULL; }
}

static void add_package( struct package *package )
{
  packages = dlist_append( packages, (void *)package );
}

////////////////////////////// just for testing
//static void _print_packages( void *data, void *user_data )
//{
//  struct package *package = (struct package *)data;
//
//  if( package )
//  {
//    if( package->group )
//      fprintf( stderr, "%s/%s:%s:%s:%s:%s:%s\n", package->group, package->name, package->version, strproc( package->procedure ), strprio( package->priority, 0 ),
//                                                 package->description, package->tarball );
//    else
//      fprintf( stderr, "%s:%s:%s:%s:%s:%s\n", package->name, package->version, strproc( package->procedure ), strprio( package->priority, 0 ),
//                                              package->description, package->tarball );
//  }
//}
//
//static void print_packages( void )
//{
//  dlist_foreach( packages, _print_packages, NULL );
//}
//////////////////////////////

/*
  End of package functions.
 *********************************************/


/*********************************************
  Return status functions:
 */
static struct pkgrc *pkgrc_alloc( void )
{
  struct pkgrc *pkgrc = NULL;

  pkgrc = (struct pkgrc *)malloc( sizeof( struct pkgrc ) );
  if( !pkgrc ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)pkgrc, sizeof( struct pkgrc ) );

  return pkgrc;
}

static void pkgrc_free( struct pkgrc *pkgrc )
{
  if( pkgrc )
  {
    if( pkgrc->name )    { free( pkgrc->name );    pkgrc->name    = NULL; }
    if( pkgrc->version ) { free( pkgrc->version ); pkgrc->version = NULL; }
    if( pkgrc->group )   { free( pkgrc->group );   pkgrc->group   = NULL; }

    free( pkgrc );
  }
}

static void __pkgrc_free_func( void *data, void *user_data )
{
  struct pkgrc *pkgrc = (struct pkgrc *)data;
  if( pkgrc ) { pkgrc_free( pkgrc ); }
}

static void free_pkgrcl( void )
{
  if( pkgrcl ) { dlist_free( pkgrcl, __pkgrc_free_func ); pkgrcl = NULL; }
}

static void add_pkgrc( struct pkgrc *pkgrc )
{
  pkgrcl = dlist_append( pkgrcl, (void *)pkgrc );
}

static struct pkgrc *find_pkgrc( struct dlist *list, pid_t pid )
{
  if( !list ) return NULL;

  while( list && list->data )
  {
    if( ((struct pkgrc *)list->data)->pid == pid ) { return (struct pkgrc *)list->data; }
    list = dlist_next( list );
  }

  return NULL;
}

static void __remove_success_pkgrc( void *data, void *user_data )
{
  struct pkgrc *pkgrc = (struct pkgrc *)data;

  if( pkgrc && pkgrc->status == 0 )
  {
    pkgrcl = dlist_remove( pkgrcl, (const void *)data );
    pkgrc_free( pkgrc );
  }
}

static void cleanup_pkgrcl( void )
{
  dlist_foreach( pkgrcl, __remove_success_pkgrc, NULL );
}

static void _print_pkgrcl( void *data, void *user_data )
{
  struct pkgrc *pkgrc = (struct pkgrc *)data;

  if( pkgrc )
  {
    if( pkgrc->group )
      fprintf( stdout, "    %5d | %s/%s-%s\n", pkgrc->status, pkgrc->group, pkgrc->name, pkgrc->version );
    else
      fprintf( stdout, "    %5d | %s-%s\n", pkgrc->status, pkgrc->name, pkgrc->version );
  }
}

static void print_pkgrcl( void )
{
  if( pkgrcl )
  {
    /*************************************************
      Ruler: 68 characters + 2 spaces left and right:

                    | ----handy-ruler----------------------------------------------------- | */
    fprintf( stdout, "The install procedure of following packages has returned bad status:\n\n" );

    fprintf( stdout, "  --------+-------------------------------------------------------\n" );
    fprintf( stdout, "   status | package\n" );
    fprintf( stdout, "  --------+-------------------------------------------------------\n" );

    dlist_foreach( pkgrcl, _print_pkgrcl, NULL );

    fprintf( stdout, "  --------+-------------------------------------------------------\n\n" );

    fprintf( stdout, "   status 253 - install process terminated on signal;\n"
                     "   status 254 - terminated on unknown reason.\n\n" );
  }
}
/*
  End of return status functions.
 *********************************************/


/*******************************
  remove spaces at end of line:
 */
//static void skip_eol_spaces( char *s )
//{
//  char *p = (char *)0;
//
//  if( !s || *s == '\0' ) return;
//
//  p = s + strlen( s ) - 1;
//  while( isspace( *p ) ) { *p-- = '\0'; }
//}
//
//static char *skip_lead_spaces( char *s )
//{
//  char *p = (char *)0;
//
//  if( !s || *s == '\0' ) return p;
//
//  p = s; while( isspace( *p ) ) { ++p; }
//
//  return( p );
//}

static char *trim( char *s )
{
  char *p = (char *)0;

  if( !s || *s == '\0' ) return p;

  p = s + strlen( s ) - 1;
  while( isspace( *p ) ) { *p-- = '\0'; }
  p = s; while( isspace( *p ) ) { ++p; }

  return( p );
}


static void read_pkglist_file( const char *fname )
{
  char *ln   = NULL;
  char *line = NULL;
  int   lnum = 0;

  FILE *fp   = NULL;

  if( !fname || (*fname == '\0') ) return;

  fp = fopen( fname, "r" );
  if( !fp )
  {
    FATAL_ERROR( "Cannot open '%s' PKGLIST file", basename( (char *)fname ) );
  }

  line = (char *)malloc( (size_t)PATH_MAX );
  if( !line ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)line, PATH_MAX );

  while( (ln = fgets( line, PATH_MAX, fp )) )
  {
    char *p = NULL;
    char *name = NULL, *vers = NULL, *desc = NULL, *ball = NULL, *proc = NULL, *prio = NULL;

    ++lnum;

    ln[strlen(ln) - 1] = '\0';  /* replace new-line symbol */
    ln = trim( ln ); /* remove leading and trailing spaces */

    if( *ln == '#' )
    {
      if( !strncmp( ln, "# required:", 11 ) )
      {
        char *n = NULL, *v = NULL, *g = NULL, *q = NULL;
        char *rq = ln + 11;
        rq = trim( rq );

        n = rq;
        if( (q = index( (const char *)n, '/' )) ) { *q = '\0'; g = n; n = ++q; g = trim( g ); }
        if( (q = index( (const char *)n, '=' )) ) { *q = '\0'; v = ++q; n = trim( n ); }
        v = trim( v );

        if( n && v  )
        {
          struct pkg *pkg = pkg_alloc();

          pkg->name    = strdup( (const char *)n );
          pkg->version = strdup( (const char *)v );
          if( g )
            pkg->group = strdup( (const char *)g );

          add_required( pkg );
        }
      }
      continue; /* skip commented lines */
    }

    name = ln;
    if( (p = index( (const char *)name, ':' )) ) { *p = '\0'; vers = ++p; name = trim( name ); } else continue;
    if( (p = index( (const char *)vers, ':' )) ) { *p = '\0'; desc = ++p; vers = trim( vers ); } else continue;
    if( (p = index( (const char *)desc, ':' )) ) { *p = '\0'; ball = ++p; desc = trim( desc ); } else continue;
    if( (p = index( (const char *)ball, ':' )) ) { *p = '\0'; proc = ++p; ball = trim( ball ); } else continue;
    if( (p = index( (const char *)proc, ':' )) ) { *p = '\0'; prio = ++p; proc = trim( proc ); } else continue;
    prio = trim( prio );

    if( name && vers && desc && ball && proc && prio )
    {
      char  *buf = NULL;
      struct package *package = NULL;
      char  *group = index( (const char *)ball, '/' );
      enum _priority priority = OPTIONAL;

      /*******************
        Package priority:
       */
      if( strlen( (const char*)prio ) > 2 )
      {
        char *match = NULL;

        to_lowercase( prio );
        if( (match = strstr( prio, "req" )) && match == prio ) {
          priority = REQUIRED;
        }
        else if( (match = strstr( prio, "rec" )) && match == prio ) {
          priority = RECOMMENDED;
        }
        else if( (match = strstr( prio, "opt" )) && match == prio ) {
          priority = OPTIONAL;
        }
        else if( (match = strstr( prio, "sk" )) && match == prio ) {
          priority = SKIP;
        }
        else {
          FATAL_ERROR( "%s: %d: Unknown '%s' priority value", basename( pkglist_fname ), lnum, prio );
        }
      }
      else
      {
        FATAL_ERROR( "%s: %d: Unknown '%s' priority value", basename( pkglist_fname ), lnum, prio );
      }

      if( priority > install_priority ) continue;

      buf = (char *)malloc( (size_t)PATH_MAX );
      if( !buf ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)buf, PATH_MAX );

      package = package_alloc();

      package->name        = strdup( (const char *)name );
      package->version     = strdup( (const char *)vers );
      package->description = strdup( (const char *)desc );

      (void)sprintf( buf, "%s/%s", (const char *)srcdir, (const char *)ball );
      {
        enum _input_type type = IFMT_UNKNOWN;
        char             uncompress = '\0';

        type = check_package_file( &uncompress, (const char *)&buf[0] );
        if( type != IFMT_PKG )
        {
          FATAL_ERROR( "Unknown format of '%s' package file", (const char *)&buf[0] );
        }
      }
      package->tarball     = strdup( (const char *)&buf[0] );

      free( buf );

      package->priority = priority;

      /********************
        Install procedure:
       */
      if( strlen( (const char*)proc ) > 5 )
      {
        char *match = NULL;

        to_lowercase( proc );
        if( (match = strstr( proc, "install" )) && match == proc ) {
          package->procedure = INSTALL;
        }
        else if( (match = strstr( proc, "update" )) && match == proc ) {
          package->procedure = UPDATE;
        }
        else {
          FATAL_ERROR( "%s: %d: Unknown '%s' procedure value", basename( pkglist_fname ), lnum, proc );
        }
      }
      else
      {
        FATAL_ERROR( "%s: %d: Unknown '%s' procedure value", basename( pkglist_fname ), lnum, proc );
      }

      if( group )
      {
        *group = '\0';
        group = ball;
        package->group = strdup( (const char *)group );
      }

      add_package( package );
    }

  } /* End of while( ln = fgets( line ) ) */

  free( line );
  fclose( fp );
}


static void check_requires( void )
{
  if( requires )
  {
    exit_status = 1;

    fprintf( stdout, "\nThe input '%s' has the list of %d required packages.\n\n", basename( pkglist_fname ), dlist_length( requires ) );

    if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
    free_resources();
    exit( exit_status );
  }
}

#if defined( HAVE_DIALOG )
static DIALOG_LISTITEM *alloc_select_items( void )
{
  DIALOG_LISTITEM *items   = NULL;
  int i = 0, num = dlist_length( packages );
  struct dlist *list = packages, *next = NULL;

  items = (DIALOG_LISTITEM *)malloc( (num + 1) * sizeof(DIALOG_LISTITEM) );
  if( !items )  { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)items, (num + 1) * sizeof(DIALOG_LISTITEM) );

  while( list )
  {
    struct package *package = NULL;

    next = dlist_next( list );
    package = (struct package *)list->data;
    if( package )
    {
#define COLUMN_LENGHT  23         /* 22 symbols for name + " [UP] " */
#define NAME_LENGHT    (COLUMN_LENGHT - 7) /* strlen(" [UP] ") + 1; */
#define UPDATE_SUFFIX  " [UP] "
#define INSTALL_SUFFIX " [in] "

      char *name = (char *)malloc( (size_t)COLUMN_LENGHT );
      if( !name ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)name, (size_t)COLUMN_LENGHT );

      if( strlen( package->name ) > (size_t)NAME_LENGHT )
      {
        (void)strncpy( name, (const char *)package->name, NAME_LENGHT - 2 );
        (void)strcat( name, ".." );
      }
      else
      {
        (void)strcpy( name, (const char *)package->name );
      }

      if( package->procedure == UPDATE )
      {
        int p = strlen( name );
        while( p < NAME_LENGHT ) { name[p] = ' '; ++p; }
        name[p] = '\0';
        (void)strcat( name, UPDATE_SUFFIX );
      }
      else
      {
        int p = strlen( name );
        /* while( p < (COLUMN_LENGHT - 1) ) { name[p] = ' '; ++p; } */
        while( p < NAME_LENGHT ) { name[p] = ' '; ++p; }
        name[p] = '\0';
        (void)strcat( name, INSTALL_SUFFIX );
      }

      items[i].name = name;
      items[i].text = strdup( (const char *)package->description );
      if( package->group )
        items[i].help = strdup( (const char *)package->group );
      items[i].state = 1;
    }
    ++i;
    list = next;
  }
  return items;
}

static void free_select_items( DIALOG_LISTITEM *items )
{
  DIALOG_LISTITEM *p = items;

  if( !p ) return;

  while( p->name ) { free( p->name ); free( p->text ); if( p->help ) free( p->help ); ++p; }

  free( items );
}

static void remove_unselected_packages( DIALOG_LISTITEM *items )
{
  DIALOG_LISTITEM *p   = items;
  struct dlist    *rem = NULL, *list = NULL, *next = NULL;
  int              n   = 0;

  if( !p ) return;

  while( p->name )
  {
    /* copy packages list item to the local list: */
    if( !p->state ) { list = dlist_append( list, dlist_nth_data( packages, n ) ); }
    ++p; ++n;
  }

  /****************************************************
    remove items of the local list from packages list:
   */
  rem = list;
  while( rem )
  {
    next = dlist_next( rem );
    packages = dlist_remove( packages, rem->data );
    rem = next;
  }

  /***************************
    free unselected packages:
   */
  dlist_free( list, __package_free_func );
}
#endif


/*********************************************
  Progress functions:
 */
static void show_install_con_progress( const char *title, int percent )
{
#define GAUGE_LENGTH  68
  size_t  prefix = strlen( title ) + 2; /* title + ' [' */
  size_t  suffix = 6;                   /* '] 100%' */
  size_t  length = prefix  + GAUGE_LENGTH + suffix;
  int     i, ctx = GAUGE_LENGTH * percent / 100;

  if( percent <   0 ) percent = 0;
  if( percent > 100 ) percent = 100;

  printf( "\033[1A" );               /* move the cursor up 1 line   */
  printf( "\033[%dD", (int)length ); /* move cursor to start of line */
  printf( "%s [", title );

  for( i = 0; i < ctx; ++i )     { fprintf( stdout, "\u2588" ); }
  for( ; i < GAUGE_LENGTH; ++i ) { fprintf( stdout, " " );      }

  printf( "] %3d%%\n", percent );
  fflush( stdout );
}

static void show_progress( void )
{
  const char *title   = "Install:";
  const char *message = "\n"
                        "Please wait for install all specified packages:\n"
                        "\n\n";

  if( install_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    show_install_dlg_progress( 0 );
#else
    fprintf( stdout, "%s", message );
    show_install_con_progress( title, 0 );
#endif
  }
  else
  {
    fprintf( stdout, "%s", message );
    show_install_con_progress( title, 0 );
  }

}

static void update_progress( int percent )
{
  const char *title   = "Install:";

  if( install_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    show_install_dlg_progress( percent );
#else
    show_install_con_progress( title, percent );
#endif
  }
  else
  {
    show_install_con_progress( title, percent );
  }
}

static void stop_progress( void )
{
  const char *title   = "Install:";

  if( install_mode != CONSOLE )
  {
#if defined( HAVE_DIALOG )
    show_install_dlg_progress( 100 );
#else
    show_install_con_progress( title, 100 );
    fprintf( stdout, "\n" );
#endif
  }
  else
  {
    show_install_con_progress( title, 100 );
    fprintf( stdout, "\n" );
  }
}
/*
  End of progress functions.
 *********************************************/



/*********************************************
  Install functions.
 */
static void install_package( struct package *package )
{
  int   len = 0;
  char *cmd = NULL, *opt = NULL, *out = "> /dev/null 2>&1";

  if( !package ) return;

  opt = (char *)malloc( (size_t)PATH_MAX );
  if( !opt ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)opt, PATH_MAX );
  opt[0] = ' ';

  if( gpgck ) (void)sprintf( opt, "--gpg-verify " );
  if( (install_mode != CONSOLE) && !parallel && !progress ) (void)strcat( opt, "--info-dialog " );
  if( parallel ) (void)strcat( opt, "--ignore-chrefs-errors " );
  if( (install_mode == CONSOLE) && !parallel && !progress ) out = " ";

  cmd = (char *)malloc( (size_t)PATH_MAX );
  if( !cmd ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)cmd, PATH_MAX );

  len = snprintf( &cmd[0], PATH_MAX,
                  "%s/%s-package %s --priority=%s --root=%s %s %s",
                  selfdir, strproc( package->procedure ), opt,
                  strprio( package->priority, 1 ),
                  root, package->tarball, out );
  if( len == 0 || len == PATH_MAX - 1 )
  {
    FATAL_ERROR( "Cannot install %s-%s package", package->name, package->version );
  }
  if( parallel )
  {
    struct pkgrc *pkgrc = pkgrc_alloc();

    pkgrc->name    = strdup( (const char *)package->name );
    pkgrc->version = strdup( (const char *)package->version );
    if( package->group )
      pkgrc->group = strdup( (const char *)package->group );
    pkgrc->pid     = sys_exec_command( cmd );

    add_pkgrc( pkgrc );
    ++__child;
  }
  else
  {
    pid_t p = (pid_t) -1;
    int  rc = 0;

    p = sys_exec_command( cmd );
    rc = sys_wait_command( p, (char *)NULL, PATH_MAX );
    if( rc != 0 && rc != 31 )
    {
      FATAL_ERROR( "Cannot install '%s-%s' package", package->name, package->version );
    }
    ++__successful;
  }

  free( cmd );
  free( opt );
}

static void _serial_install_package( void *data, void *user_data )
{
  struct package *package = (struct package *)data;
  int    percent;

  if( package ) { install_package( package ); }

  if( progress )
  {
    percent = ( __successful < __all ) ? __successful * 100 / __all : 100;
    update_progress( percent );
  }
}

static void serial_install_packages( void )
{
  if( progress )
  {
    show_progress();
  }

  dlist_foreach( packages, _serial_install_package, NULL );

  if( progress )
  {
    stop_progress();
  }
}


static void *install_process( void *args )
{
  struct dlist *list = packages, *next = NULL;

  int nstreams = ncpus * 2; /* two concurents for CPU */

  while( list )
  {
    struct package *package = NULL;

    next = dlist_next( list );
    package = (struct package *)list->data;
    if( package )
    {
      install_package( package );
    }
    list = next;

    /* wait for available CPU: */
    while( (__child - __terminated) > nstreams ) usleep( WAIT_USEC_FOR_CHILD );
  }

  return NULL;
}

static void parallel_install_packages( void )
{
  pthread_t install_process_id;
  int       status;

  /* Start the parallel installation process: */
  status = pthread_create( &install_process_id, NULL, install_process, NULL );
  if( status != 0 )
  {
    FATAL_ERROR( "Cannot start parallel installation process" );
  }
  (void)pthread_detach( install_process_id );
}

/*
  End of install functions.
 *********************************************/




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

  ncpus = get_nprocs();

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

  /***********************************************************
    Fill requires and packages lists, check package tarballs,
    skip unnesessary packages according to --priority option:
   */
  read_pkglist_file( (const char *)pkglist_fname );

  /* check only the list of requires in the input PKGLIST: */
  if( rqck ) check_requires();

#if defined( HAVE_DIALOG )
  if( install_mode == MENUDIALOG )
  {
    int  status = 0, num   = dlist_length( packages );
    DIALOG_LISTITEM *items = alloc_select_items();

    status = select_packages_box( items, num, 0, 0 );
    if( !status )
    {
      remove_unselected_packages( items );
      free_select_items( items );
    }
    else
    {
      /* Abort installation: */
      if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
      free_resources();

      exit( exit_status );
    }
  }
#endif


  if( parallel )
  {
    /************************
      parallel installation:
     */
    int percent = 0;

    __all  = dlist_length( packages );
    __done = 0; __child = 0;

    __terminated = 0; __successful = 0;

    show_progress();

    parallel_install_packages();

    if( __terminated < __all )
    {
      while( !__done )
      {
        percent = ( __terminated < __all ) ? __terminated * 100 / __all : 100;

        update_progress( percent );
        usleep( WAIT_USEC_FOR_CHILD );
      }
    }

    __done = 0; __child = 0;

    stop_progress();

    if( __successful < __terminated ) { percent = __successful * 100 / __terminated; }
    else                              { percent = 100; }

    __terminated = 0; __successful = 0;

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      char *msg = NULL;

      msg = (char *)malloc( (size_t)80 );
      if( !msg ) { FATAL_ERROR( "Cannot allocate memory" ); }
      bzero( (void *)msg, 80 );

      (void)sprintf( msg, "\nSuccessfully installed %d%% of %d specified packages.\n", percent, __all );

      info_box( " \\Z0INSTALL PACKAGES\\Zn ", msg, 5, 0, 0 );

      free( msg );
#else
      fprintf( stdout, "\nSuccessfully installed %d%% of %d specified packages.\n\n", percent, __all );
#endif
    }
    else
    {
      fprintf( stdout, "\nSuccessfully installed %d%% of %d specified packages.\n\n", percent, __all );
    }

    cleanup_pkgrcl(); /* remove successfully installed packages from return status list */

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      if( pkgrcl )
      {
        ; /* TODO: show the list of not installed packages */
      }
#else
      print_pkgrcl();
#endif
    }
    else
    {
      print_pkgrcl();
    }

  }
  else
  {
    /**********************
      serial installation:
     */

    __successful = 0;
    __all        = dlist_length( packages );

    serial_install_packages();

    if( install_mode != CONSOLE )
    {
#if defined( HAVE_DIALOG )
      info_box( " \\Z4INSTALL PACKAGES\\Zn ",
                "\nAll of specified packages have been installed.\n", 5, 0, 0 );
#else
      fprintf( stdout, "\nAll of specified packages have been installed.\n\n" );
#endif
    }
    else
    {
      fprintf( stdout, "\nAll of specified packages have been installed.\n\n" );
    }

  }


  if( tmpdir ) { _rm_tmpdir( (const char *)tmpdir ); free( tmpdir ); }
  free_resources();

  exit( exit_status );
}
