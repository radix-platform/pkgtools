
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

#ifndef _DEFS_H_
#define _DEFS_H_

#ifdef __cplusplus
extern "C" {
#endif


#define SETUP_DB_PATH     "var/log/" DISTRO_NAME
#define PACKAGES_PATH     SETUP_DB_PATH "/packages"
#define REMOVED_PKGS_PATH SETUP_DB_PATH "/removed-packages"
#define SETUP_PATH        SETUP_DB_PATH "/setup"
#define LOG_PATH          SETUP_DB_PATH
#define SETUP_LOG_FILE    "/setup.log"            /* : used by: update-package, remove-package, install-package.   */
#define LOG_FILE          "/" PROGRAM_NAME ".log" /* : used by: check-requires, check-package, check-db-integrity. */


#define DO_NOT_WARN_ABOUT_EMPTY_REQUIRES       (1)
#define DO_NOT_WARN_ABOUT_EMPTY_RESTORE_LINKS  (1)
#define DO_NOT_WARN_ABOUT_SERVICE_FILES        (1)
#define DO_NOT_WARN_ABOUT_OPT_PKGINFO_ITEMS    (1)

#define DO_NOT_PRINTOUT_INFO    (1)

#define DESCRIPTION_NUMBER_OF_LINES  11
#define DESCRIPTION_LENGTH_OF_LINE   68


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _DEFS_H_ */
