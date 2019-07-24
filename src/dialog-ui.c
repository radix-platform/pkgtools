
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

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <strings.h>  /* index(3)    */

#include <dialog.h>
#include <dlg_colors.h>
#include <dlg_keys.h>

#include <msglog.h>

  /*************************************************
    Ruler: 68 characters + 2 spaces left and right:

                           | ----handy-ruler----------------------------------------------------- | */

int info_box( const char *title, const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dlg_put_backtitle();

  status = dialog_msgbox( title, message, height, 74, 0 );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int info_pkg_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                  const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  char *tmp = NULL;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dlg_put_backtitle();

  if( pkgver )
    (void)sprintf( &tmp[0], " %s \\Z1%s-%s\\Zn ", title, pkgname, pkgver );
  else
    (void)sprintf( &tmp[0], " %s \\Z1%s\\Zn ",title, pkgname );

  if( priority )
  {
    (void)strcat( &tmp[0], "[" );
    (void)strcat( &tmp[0], priority );
    (void)strcat( &tmp[0], "] " );
  }
  status = dialog_msgbox( (const char *)&tmp[0], message, height, 74, 0 );

  free( tmp );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int ask_install_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                     const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  char *tmp = NULL;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dialog_vars.yes_label = "Install";
  dialog_vars.no_label  = "Cancel";

  dlg_put_backtitle();

  (void)sprintf( &tmp[0],
                 " %s \\Z1%s-%s\\Zn [%s] ",
                 title, pkgname, pkgver, priority );
  status = dialog_yesno( (const char *)&tmp[0], message, height, 74 );

  free( tmp );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int ask_remove_box( const char *title, const char *pkgname, const char *pkgver,
                    const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  char *tmp = NULL;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dialog_vars.yes_label = "Remove";
  dialog_vars.no_label  = "Cancel";

  dlg_put_backtitle();

  (void)sprintf( &tmp[0],
                 " %s \\Z1%s-%s\\Zn ",
                 title, pkgname, pkgver );
  status = dialog_yesno( (const char *)&tmp[0], message, height, 74 );

  free( tmp );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int ask_reinstall_box( const char *title, const char *pkgname, const char *pkgver,
                       const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  char *tmp = NULL;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dialog_vars.yes_label = "Re-install";
  dialog_vars.no_label  = "Cancel";

  dlg_put_backtitle();

  (void)sprintf( &tmp[0],
                 " %s \\Z1%s-%s\\Zn ",
                 title, pkgname, pkgver );
  status = dialog_yesno( (const char *)&tmp[0], message, height, 74 );

  free( tmp );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int ask_update_box( const char *title, const char *pkgname, const char *pkgver, const char *priority,
                    const char *message, int height, int sleep, int clear_screen )
{
  int status = 0;

  char *tmp = NULL;

  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  tmp = (char *)malloc( (size_t)PATH_MAX );
  if( !tmp ) { FATAL_ERROR( "Cannot allocate memory" ); }
  bzero( (void *)tmp, PATH_MAX );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dialog_vars.yes_label = "Update";
  dialog_vars.no_label  = "Cancel";

  dlg_put_backtitle();

  (void)sprintf( &tmp[0],
                 " %s \\Z1%s-%s\\Zn [%s] ",
                 title, pkgname, pkgver, priority );
  status = dialog_yesno( (const char *)&tmp[0], message, height, 74 );

  free( tmp );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}

int select_packages_box( DIALOG_LISTITEM *items, int items_num, int sleep, int clear_screen )
{
  int status  = 0;

  int current = 0;
  const char *states = " *";
  FILE *in = stdin, *out = stdout;

  bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

  init_dialog( in, out );

  dialog_vars.colors = 1;
  dialog_vars.column_separator = " ";
  dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
  if( clear_screen ) dialog_vars.dlg_clear_screen = 1;
  dialog_vars.sleep_secs = sleep;

  dialog_vars.yes_label = "Install";
  dialog_vars.no_label  = "Cancel";

  dlg_put_backtitle();

  status = dlg_checklist( " \\Z0SELECT PACKAGES TO INSTALL\\Zn ",
                          "\n"
                          " Please confirm the packages  you wish to install.  Use the UP/DOWN\n"
                          " keys to scroll through the list, and the SPACE key to deselect any\n"
                          " items you don't want to install.\n"
                          "\n"
                          " Press ENTER when you are done.\n",
                          19, 74 /* min 73 */, 7, items_num, items, states, 1, &current );

  if( dialog_vars.sleep_secs )
    (void)napms(dialog_vars.sleep_secs * 1000);

  if( dialog_vars.dlg_clear_screen )
  {
    dlg_clear();
    (void)refresh();
  }
  end_dialog();

  return status;
}



void show_install_dlg_progress( int percent )
{
  static void *gauge = NULL;


  if( percent < 1 )
  {
    if( gauge ) return; /* only one instance of progress box */

    bzero( (void *)&dialog_vars, sizeof(DIALOG_VARS) );

    init_dialog( stdin, stdout );

    dialog_vars.colors = 1;
    dialog_vars.backtitle = "\\Z7Radix\\Zn \\Z1cross\\Zn \\Z7Linux\\Zn";
    dialog_vars.dlg_clear_screen = 0;
    dialog_vars.sleep_secs = 0;

    dlg_put_backtitle();
    gauge = dlg_allocate_gauge( " \\Z0INSTALL PACKAGES\\Zn ",
                                "\n"
                                "  Please wait for install all specified packages:\n"
                                "\n\n", 8, 74, 0 );
  }

  if( gauge )
    dlg_update_gauge( gauge, percent );

  if( percent > 99 )
  {
    if( gauge )
    {
      dlg_free_gauge( gauge );
      gauge = NULL;

      if( dialog_vars.sleep_secs )
        (void)napms(dialog_vars.sleep_secs * 1000);

      if( dialog_vars.dlg_clear_screen )
      {
        dlg_clear();
        (void)refresh();
      }
      end_dialog();
    }
  }
}
