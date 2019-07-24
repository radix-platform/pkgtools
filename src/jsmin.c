
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
#include <libgen.h>   /* basename(3) */

#include <msglog.h>

#include <jsmin.h>

static FILE *ifile;
static FILE *ofile;

static const char *input_fname = NULL;

static void error( const char *fname, char *s )
{
  if( fname )
    ERROR( "JSMIN: %s: %s", basename( (char *)fname ), s );
  else
    ERROR( "JSMIN: %s", s );
}

static int is_alpha_or_num( int c )
{
  return( (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
          (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c == '\\' || c > 126 );
}

static int  a;
static int  b;
static int  lookahead = EOF;
static int  x = EOF;
static int  y = EOF;

/*
  get - return the next character from stdin. Watch out for lookahead. If
        the character is a control character, translate it to a space or
        linefeed.
 */
static int get()
{
  int c = lookahead;
  lookahead = EOF;
  if( c == EOF )
  {
    c = getc( ifile );
  }
  if( c >= ' ' || c == '\n' || c == EOF )
  {
    return c;
  }
  if( c == '\r' )
  {
    return '\n';
  }
  return ' ';
}


/*
  peek - get the next character without getting it.
 */
static int peek()
{
  lookahead = get();
  return lookahead;
}


/*
  next - get the next character, excluding comments. peek() is used to see
         if a '/' is followed by a '/' or '*'.
 */
static int next()
{
  int c = get();
  if ( c == '/' )
  {
    switch( peek() )
    {
      case '/':
        for( ;; )
        {
          c = get();
          if( c <= '\n' )
          {
            break;
          }
        }
        break;
      case '*':
        get();
        while( c != ' ' )
        {
          switch( get() )
          {
            case '*':
              if( peek() == '/' )
              {
                get();
                c = ' ';
              }
              break;
            case EOF:
              error( input_fname, "Unterminated comment" );
              return EOF;
          }
        }
        break;
    }
  }
  y = x;
  x = c;
  return c;
}


/*
  action - do something! What you do is determined by the argument:
           1   Output A. Copy B to A. Get the next B.
           2   Copy B to A. Get the next B. (Delete A).
           3   Get the next B. (Delete B).
  action treats a string as a single character. Wow!
  action recognizes a regular expression if it is preceded by ( or , or =.
 */
static void action( int d )
{
  switch( d )
  {
    case 1:
      /* skip first carriage return */
      if( a != '\n' ) putc( a, ofile );
      if( (y == '\n' || y == ' ') &&
          (a == '+' || a == '-' || a == '*' || a == '/') &&
          (b == '+' || b == '-' || b == '*' || b == '/')    )
      {
        putc( y, ofile );
      }
    case 2:
      a = b;
      if( a == '\'' || a == '"' || a == '`' )
      {
        for( ;; )
        {
          putc( a, ofile );
          a = get();
          if( a == b )
          {
            break;
          }
          if( a == '\\' )
          {
            putc( a, ofile );
            a = get();
          }
          if( a == EOF )
          {
            error( input_fname, "Unterminated string literal" );
            return;
          }
        }
      }
    case 3:
      b = next();
      if( b == '/' &&
          ( a == '(' || a == ',' || a == '=' || a == ':' ||
            a == '[' || a == '!' || a == '&' || a == '|' ||
            a == '?' || a == '+' || a == '-' || a == '~' ||
            a == '*' || a == '/' || a == '{' || a == '\n' ) )
      {
        putc( a, ofile );
        if( a == '/' || a == '*' )
        {
          putc( ' ', ofile );
        }
        putc( b, ofile );
        for( ;; )
        {
          a = get();
          if( a == '[' )
          {
            for( ;; )
            {
              putc( a, ofile );
              a = get();
              if( a == ']' )
              {
                break;
              }
              if( a == '\\' )
              {
                putc( a, ofile );
                a = get();
              }
              if( a == EOF )
              {
                error( input_fname, "Unterminated set in Regular Expression literal" );
                return;
              }
            }
          }
          else if( a == '/' )
          {
            switch( peek() )
            {
              case '/':
              case '*':
                error( input_fname, "Unterminated set in Regular Expression literal" );
                return;
            }
            break;
          }
          else if( a =='\\' )
          {
            putc( a, ofile );
            a = get();
          }
          if( a == EOF )
          {
            error( input_fname, "Unterminated Regular Expression literal" );
            return;
          }
          putc( a, ofile );
        }
      b = next();
    }
  }
}


/*
  jsmin - Copy the input to the output, deleting the characters which are
          insignificant to JavaScript. Comments will be removed. Tabs will be
          replaced with spaces. Carriage returns will be replaced with linefeeds.
          Most spaces and linefeeds will be removed.
*/
static void jsmin()
{
  if( peek() == 0xEF ) { get(); get(); get(); }
  a = '\n';
  action( 3 );

  while( a != EOF )
  {
    switch( a )
    {
      case ' ':
        action(is_alpha_or_num(b) ? 1 : 2);
        break;
      case '\n':
        switch( b )
        {
          case '{': case '[': case '(':
          case '+': case '-': case '!':
          case '~':
            action( 1 );
            break;
          case ' ':
            action( 3 );
            break;
          default:
            action( is_alpha_or_num(b) ? 1 : 2 );
        }
        break;
      default:
        switch( b )
        {
          case ' ':
            action( is_alpha_or_num(a) ? 1 : 3 );
            break;
          case '\n':
            switch( a )
            {
              case '}':  case ']': case ')':
              case '+':  case '-': case '"':
              case '\'': case '`':
                action( 1 );
                break;
              default:
                action( is_alpha_or_num(a) ? 1 : 3 );
            }
            break;
          default:
            action( 1 );
            break;
        }
    }
  }
  /* lats carriage return */
  putc( '\n', ofile );
}


int minimize_json( const char *ifname, const char *ofname )
{
  int status, ret = -1;

  if( !ifname || !ofname ) return ret;

  status = exit_status; exit_status = 0;

  input_fname = ifname;

  ret = 0;

  ifile = fopen( ifname, "r" );
  if( ifile == NULL )
  {
    ERROR( "JSMIN: Can't open '%s' file", ifname );
    exit_status = status + exit_status;
    return ret;
  }

  ofile = fopen( ofname, "w+" );
  if( ofile == NULL )
  {
    ERROR( "JSMIN: Can't open '%s' file", ofname );
    exit_status = status + exit_status;
    return ret;
  }

  jsmin();

  fclose( ifile ); ifile = NULL;
  fflush( ofile ); fclose( ofile ); ofile = NULL;

  if( exit_status == 0 )
  {
    ret = 1;
    exit_status = status;
  }
  else
  {
    exit_status = status + exit_status;
  }

  return ret;
}
