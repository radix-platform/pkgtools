
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

#ifndef _CMP_VERSION_H_
#define _CMP_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif


extern int cmp_version( const char *s1, const char *s2 );
extern const char *max_version( const char *s1, const char *s2 );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _CMP_VERSION_H_ */
