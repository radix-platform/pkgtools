
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

#ifndef _DIALOG_UI_H_
#define _DIALOG_UI_H_

#include <dialog.h>

#ifdef __cplusplus
extern "C" {
#endif

  /*************************************************
    Ruler: 68 characters + 2 spaces left and right:

                           | ----handy-ruler----------------------------------------------------- | */

extern int info_box( const char *title, const char *message, int height, int sleep, int clear_screen );

extern int info_pkg_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                         const char *message, int height, int sleep, int clear_screen );

extern int ask_install_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                            const char *message, int height, int sleep, int clear_screen );

extern int ask_remove_box( const char *title, const char *pkgname, const char *pkgver,
                           const char *message, int height, int sleep, int clear_screen );

extern int ask_reinstall_box( const char *title, const char *pkgname, const char *pkgver,
                              const char *message, int height, int sleep, int clear_screen );

extern int ask_update_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                           const char *message, int height, int sleep, int clear_screen );

extern int select_packages_box( DIALOG_LISTITEM *items, int items_num, int sleep, int clear_screen );

extern void show_install_dlg_progress( int percent );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _DIALOG_UI_H_ */
