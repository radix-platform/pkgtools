
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

#ifndef _SYSTEM_H_
#define _SYSTEM_H_

#ifdef __cplusplus
extern "C" {
#endif


extern pid_t sys_exec_command( const char *cmd );

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
extern int sys_wait_command( pid_t pid, char *errmsg, size_t size );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _SYSTEM_H_ */
