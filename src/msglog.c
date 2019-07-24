
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
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <msglog.h>

FILE *errlog;

void (*fatal_error_hook)( void );

void logmsg( FILE *logfile, enum _msg_type type, char *format, ... )
{
  va_list argp;

  if( ! format ) return;

  switch( type )
  {
    case MSG_FATAL:   fprintf( logfile, "%s: FATAL: ",   program ); break;
    case MSG_ERROR:   fprintf( logfile, "%s: ERROR: ",   program ); break;
    case MSG_WARNING: fprintf( logfile, "%s: WARNING: ", program ); break;
    case MSG_NOTICE:  fprintf( logfile, "%s: NOTE: ",    program ); break;
    case MSG_INFO:    fprintf( logfile, "%s: INFO: ",    program ); break;
    case MSG_DEBUG:   fprintf( logfile, "%s: DEBUG: ",   program ); break;
    case MSG_LOG:
    {
      time_t     t = time( NULL );
      struct tm tm = *localtime(&t);

      fprintf( logfile, "[%04d-%02d-%02d %02d:%02d:%02d]: %s: ",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                         tm.tm_hour, tm.tm_min, tm.tm_sec,
                                                          program );
      break;
    }
    default:
      fprintf( logfile, "%s: ", program );
      break;
  }
  va_start( argp, format );
  vfprintf( errlog, format, argp );
  fprintf( errlog, "\n" );
}
