
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
#include <errno.h>
#include <error.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <unistd.h>

#include <msglog.h>


static void xexec( const char *cmd )
{
  char *argv[4];
  const char *shell = getenv ("SHELL");

  if( !shell ) shell = "/bin/sh";

  argv[0] = (char *) shell;
  argv[1] = (char *) "-c";
  argv[2] = (char *) cmd;
  argv[3] = NULL;

  execv( shell, argv );

  /******************************************
    xexec() is called by child process, and
    here child process faced to FATAL error:
   */
  logmsg( errlog, MSG_FATAL, "%s: Cannot exec", cmd );
  exit( EXIT_FAILURE );
}

static pid_t xfork( void )
{
  pid_t p = fork();

  if( p == (pid_t) -1 )
  {
    FATAL_ERROR( "Cannot %s", "fork" );
  }

  return p;
}

pid_t sys_exec_command( const char *cmd )
{
  pid_t pid = xfork();

  if( pid != 0 )
  {
    return pid;
  }

  xexec( cmd );
  return pid; /* only to avoid compilaton warning */
}


/*****************************************************************
  sys_wait_command() - Wait for pid.

  Return values:
  -------------
     0  - SUCCESS
   >=1  - status returned by child process
    -1  - Child terminated on signal
    -2  - Child terminated on unknown reason
    -3  - Cannot waitpid: waitpid() retusrs -1

     Error message with SIZE length saved into *ERRMSG buffer.
 *****************************************************************/
int sys_wait_command( pid_t pid, char *errmsg, size_t size )
{
  int status;

  if( pid < 0 ) return (pid_t) -1;

  while( waitpid( pid, &status, 0 ) == -1 )
    if( errno != EINTR )
    {
      if( errmsg && size ) {
        (void)snprintf( errmsg, size, "PID %lu: Cannot %s", (unsigned long)pid, "waitpid" );
      }
      return (int) -3;
    }

  if( WIFEXITED( status ) )
  {
    if( WEXITSTATUS (status) )
    {
      if( errmsg && size ) {
        (void)snprintf( errmsg, size, "PID %lu: Child returned status %d", (unsigned long)pid, WEXITSTATUS( status ) );
      }
      return (int) WEXITSTATUS( status );
    }
  }
  else if( WIFSIGNALED( status ) )
  {
    if( errmsg && size ) {
      (void)snprintf( errmsg, size, "PID %lu: Child terminated on signal %d", (unsigned long)pid, WTERMSIG( status ) );
    }
    return (int) -1;
  }
  else
  {
    if( errmsg && size ) {
      (void)snprintf( errmsg, size, "PID %lu: Child terminated on unknown reason", (unsigned long)pid );
    }
    return (int) -2;
  }

  return 0;
}
