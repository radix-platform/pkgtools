
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

#ifndef _MSG_LOG_H_
#define _MSG_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

extern FILE *errlog;

extern void (*fatal_error_hook)( void );

extern char *program;
extern int   exit_status;

enum _msg_type
{
  MSG_FATAL = 0,
  MSG_ERROR,
  MSG_WARNING,
  MSG_NOTICE,
  MSG_INFO,
  MSG_DEBUG,

  MSG_LOG
};

#define FATAL_ERROR( ... )                    \
  do                                          \
  {                                           \
    logmsg( errlog, MSG_FATAL, __VA_ARGS__ ); \
    if( fatal_error_hook) fatal_error_hook(); \
    exit( EXIT_FAILURE );                     \
  } while (0)

#define ERROR( ... )                          \
  do                                          \
  {                                           \
    logmsg( errlog, MSG_ERROR, __VA_ARGS__ ); \
    ++exit_status;                            \
  } while (0)

#define WARNING( ... )                          \
  do                                            \
  {                                             \
    logmsg( errlog, MSG_WARNING, __VA_ARGS__ ); \
  } while (0)

#define NOTICE( ... )                          \
  do                                           \
  {                                            \
    logmsg( errlog, MSG_NOTICE, __VA_ARGS__ ); \
  } while (0)

#define INFO( ... )                          \
  do                                         \
  {                                          \
    logmsg( errlog, MSG_INFO, __VA_ARGS__ ); \
  } while (0)

#define DEBUG( ... )                          \
  do                                          \
  {                                           \
    logmsg( errlog, MSG_DEBUG, __VA_ARGS__ ); \
  } while (0)

#define LOG( ... )                          \
  do                                        \
  {                                         \
    logmsg( errlog, MSG_LOG, __VA_ARGS__ ); \
  } while (0)


extern void logmsg( FILE *logfile, enum _msg_type type, char *format, ... );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _MSG_LOG_H_ */
