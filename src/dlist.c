
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

#include <msglog.h>

#include <dlist.h>

struct dlist *__dlist_alloc( void )
{
  struct dlist *list = NULL;

  list = (struct dlist *)malloc( sizeof( struct dlist ) );
  if( !list ) { FATAL_ERROR( "Cannot allocate memory" ); }

  bzero( (void *)list, sizeof( struct dlist ) );

  return list;
}



struct dlist *dlist_first( struct dlist *list )
{
  while( list && dlist_prev( list ) ) list = dlist_prev( list );

  return list;
}

struct dlist *dlist_last( struct dlist *list )
{
  while( list && dlist_next( list ) ) list = dlist_next( list );

  return list;
}

int dlist_length( struct dlist *list )
{
  int n = 0;

  while( list )
  {
    list = dlist_next( list );
    ++n;
  }

  return n;
}

struct dlist *dlist_nth( struct dlist *list, int n )
{
  while( list && (n-- > 0) )
  {
    list = dlist_next( list );
  }

  return list;
}

void *dlist_nth_data( struct dlist *list, int n )
{
  while( list && (n-- > 0) )
  {
    list = dlist_next( list );
  }

  return list ? list->data : NULL;
}

int dlist_position( struct dlist *list, struct dlist *link )
{
  int n = 0;

  while( list )
  {
    if( list == link ) return n;
    ++n;
    list = dlist_next( list );
  }

  return -1;
}

int dlist_index( struct dlist *list, const void *data )
{
  int n = 0;

  while( list )
  {
    if( list->data == data ) return n;
    ++n;
    list = dlist_next( list );
  }

  return -1;
}

struct dlist *dlist_find( struct dlist *list, const void *data )
{
  while( list )
  {
    if( list->data == data ) { break; }
    list = dlist_next( list );
  }

  return list;
}


struct dlist *dlist_find_data( struct dlist *list, DLCMPF cmp_func, const void *data )
{
  if( !list || !cmp_func ) return NULL;

  while( list )
  {
    if( ! cmp_func( list->data, data ) ) { return list; }
    list = dlist_next( list );
  }

  return NULL;
}


struct dlist *dlist_append( struct dlist *list, void *data )
{
  struct dlist *node = NULL;
  struct dlist *last = NULL;

  node = __dlist_alloc();
  node->data = data;

  if( list )
  {
    last = dlist_last( list );

    dlist_next( last ) = node;
    dlist_prev( node ) = last;

    return list;
  }

  return node;
}

struct dlist *dlist_prepend( struct dlist *list, void *data )
{
  struct dlist *node  = NULL;
  struct dlist *first = NULL;

  node = __dlist_alloc();
  node->data = data;

  if( list )
  {
    first = dlist_first( list );

    dlist_prev( first ) = node;
    dlist_next( node )  = first;

    return node;
  }

  return node;
}

struct dlist *dlist_insert( struct dlist *list, void *data, int position )
{
  struct dlist *node  = NULL;
  struct dlist *ptr   = NULL;

  if( position  < 0 ) { return dlist_append( list, data );  }
  if( position == 0 ) { return dlist_prepend( list, data ); }

  ptr = dlist_nth( list, position );
  if( !ptr ) { return dlist_append( list, data ); }

  node = __dlist_alloc();
  node->data = data;

  node->prev = ptr->prev;
  ptr->prev->next = node;
  node->next = ptr;
  ptr->prev = node;

  return list;
}

struct dlist *dlist_insert_sorted( struct dlist *list, DLCMPF cmp_func, void *data )
{
  struct dlist *node  = NULL;
  struct dlist *ptr   = list;
  int cmp;

  if( !cmp_func ) return list;

  if( !list )
  {
    node = __dlist_alloc();
    node->data = data;
    return node;
  }

  cmp = cmp_func( data, ptr->data );

  while( (ptr->next) && (cmp > 0) )
  {
    ptr = ptr->next;
    cmp = cmp_func( data, ptr->data );
  }

  node = __dlist_alloc();
  node->data = data;

  if( (!ptr->next) && (cmp > 0) )
  {
    ptr->next = node;
    node->prev = ptr;
    return list;
  }

  if( ptr->prev )
  {
    ptr->prev->next = node;
    node->prev = ptr->prev;
  }
  node->next = ptr;
  ptr->prev = node;

  if( ptr == list )
    return node;
  else
    return list;
}

struct dlist *dlist_insert_sorted_with_data( struct dlist *list, DLCMPDF cmp_func, void *data, void *user_data )
{
  struct dlist *node  = NULL;
  struct dlist *ptr   = list;
  int cmp;

  if( !cmp_func ) return list;

  if( !list )
  {
    node = __dlist_alloc();
    node->data = data;
    return node;
  }

  cmp = cmp_func( data, ptr->data, user_data );

  while( (ptr->next) && (cmp > 0) )
  {
    ptr = ptr->next;
    cmp = cmp_func( data, ptr->data, user_data );
  }

  node = __dlist_alloc();
  node->data = data;

  if( (!ptr->next) && (cmp > 0) )
  {
    ptr->next = node;
    node->prev = ptr;
    return list;
  }

  if( ptr->prev )
  {
    ptr->prev->next = node;
    node->prev = ptr->prev;
  }
  node->next = ptr;
  ptr->prev = node;

  if( ptr == list )
    return node;
  else
    return list;
}

struct dlist *dlist_concat( struct dlist *list1, struct dlist *list2 )
{
  struct dlist *ptr   = NULL;

  if( list2 )
  {
    ptr = dlist_last( list1 );
    if( ptr )
      ptr->next = list2;
    else
      list1 = list2;

    list2->prev = ptr;
  }

  return list1;
}

struct dlist *dlist_insert_list( struct dlist *list1, struct dlist *list2, int position )
{
  struct dlist *last  = NULL;
  struct dlist *ptr   = NULL;

  if( position  < 0 ) { return dlist_concat( list1, list2 ); }
  if( position == 0 ) { return dlist_concat( list2, list1 ); }

  ptr = dlist_nth( list1, position );
  if( !ptr ) { return dlist_concat( list1, list2 ); }

  last = dlist_last( list2 );
  if( last )
  {
    list2->prev = ptr->prev;
    ptr->prev->next = list2;
    last->next = ptr;
    ptr->prev = last;
  }

  return list1;
}

struct dlist *__dlist_remove_link( struct dlist *list, struct dlist *link )
{
  if( link == NULL ) return list;

  if( link->prev )
  {
    if( link->prev->next == link )
    {
        link->prev->next = link->next;
    }
    else
    {
      WARNING( "Corrupted double-linked list detected" );
    }
  }
  if( link->next )
  {
    if( link->next->prev == link )
    {
      link->next->prev = link->prev;
    }
    else
    {
      WARNING( "Corrupted double-linked list detected" );
    }
  }

  if( link == list ) list = list->next;

  link->next = NULL;
  link->prev = NULL;

  return list;
}

struct dlist *dlist_remove( struct dlist *list, const void *data )
{
  struct dlist *ptr = list;

  while( ptr )
  {
    if( ptr->data != data )
    {
      ptr = ptr->next;
    }
    else
    {
      list = __dlist_remove_link( list, ptr );
      free( ptr );

      break;
    }
  }

  return list;
}

struct dlist *dlist_remove_all( struct dlist *list, const void *data )
{
  struct dlist *ptr = list;

  while( ptr )
  {
    if( ptr->data != data )
    {
      ptr = ptr->next;
    }
    else
    {
      struct dlist *next = ptr->next;

      if( ptr->prev )
        ptr->prev->next = next;
      else
        list = next;

      if( next )
        next->prev = ptr->prev;

      free( ptr );

      ptr = next;
    }
  }

  return list;
}

struct dlist *dlist_remove_data( struct dlist *list, DLCMPF cmp_func, DLFUNC free_func, const void *data )
{
  struct dlist *ptr = list;

  if( !cmp_func ) return list;

  while( ptr )
  {
    if( cmp_func( ptr->data, data ) != 0 )
    {
      ptr = ptr->next;
    }
    else
    {
      list = __dlist_remove_link( list, ptr );
      if( free_func ) free_func( ptr->data, (void *)data ); /* free_func() can compare pointers */
      free( ptr );

      break;
    }
  }

  return list;
}

struct dlist *dlist_remove_data_all( struct dlist *list, DLCMPF cmp_func, DLFUNC free_func, const void *data )
{
  struct dlist *ptr = list;

  if( !cmp_func ) return list;

  while( ptr )
  {
    if( cmp_func( ptr->data, data ) != 0 )
    {
      ptr = ptr->next;
    }
    else
    {
      struct dlist *next = ptr->next;

      if( ptr->prev )
        ptr->prev->next = next;
      else
        list = next;

      if( next )
        next->prev = ptr->prev;

      if( free_func ) free_func( ptr->data, (void *)data ); /* free_func() can compare pointers */
      free( ptr );

      ptr = next;
    }
  }

  return list;
}


struct dlist *dlist_copy( struct dlist *list )
{
  struct dlist *copy = NULL;

  while( list )
  {
    copy = dlist_append( copy, list->data );
    list = dlist_next( list );
  }

  return copy;
}

/* It simply switches the next and prev pointers of each element. */
struct dlist *dlist_reverse( struct dlist *list )
{
  struct dlist *last = NULL;

  while( list )
  {
    last = list;
    list = last->next;
    last->next = last->prev;
    last->prev = list;
  }

  return last;
}


static struct dlist *__dlist_sort_merge( struct dlist *l1, struct dlist *l2, DLCMPF cmp_func )
{
  struct dlist list, *l, *lprev;
  int   cmp;

  l = &list; 
  lprev = NULL;

  while( l1 && l2 )
  {
    cmp = ((DLCMPF) cmp_func)( l1->data, l2->data );

    if( cmp <= 0 )
    {
      l->next = l1;
      l1 = l1->next;
    }
    else
    {
      l->next = l2;
      l2 = l2->next;
    }
    l = l->next;
    l->prev = lprev; 
    lprev = l;
  }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static struct dlist *__dlist_sort_merge_with_data( struct dlist *l1, struct dlist *l2, DLCMPDF cmp_func, void *user_data )
{
  struct dlist list, *l, *lprev;
  int   cmp;

  l = &list; 
  lprev = NULL;

  while( l1 && l2 )
  {
    cmp = ((DLCMPDF) cmp_func)( l1->data, l2->data, user_data );

    if( cmp <= 0 )
    {
      l->next = l1;
      l1 = l1->next;
    }
    else
    {
      l->next = l2;
      l2 = l2->next;
    }
    l = l->next;
    l->prev = lprev; 
    lprev = l;
  }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}


static struct dlist *__dlist_sort_real( struct dlist *list, DLCMPF cmp_func )
{
  struct dlist *l1, *l2;

  if( !list )
    return NULL;
  if( !list->next )
    return list;

  l1 = list; 
  l2 = list->next;

  while( (l2 = l2->next) != NULL )
  {
    if( (l2 = l2->next) == NULL )
      break;
    l1 = l1->next;
  }
  l2 = l1->next;
  l1->next = NULL;

  return __dlist_sort_merge( __dlist_sort_real( list, cmp_func ),
                             __dlist_sort_real( l2, cmp_func ),
                             cmp_func );
}

static struct dlist *__dlist_sort_real_with_data( struct dlist *list, DLCMPDF cmp_func, void *user_data )
{
  struct dlist *l1, *l2;

  if( !list )
    return NULL;
  if( !list->next )
    return list;

  l1 = list; 
  l2 = list->next;

  while( (l2 = l2->next) != NULL )
  {
    if( (l2 = l2->next) == NULL )
      break;
    l1 = l1->next;
  }
  l2 = l1->next;
  l1->next = NULL;

  return __dlist_sort_merge_with_data( __dlist_sort_real_with_data( list, cmp_func, user_data ),
                                       __dlist_sort_real_with_data( l2, cmp_func, user_data ),
                                       cmp_func,
                                       user_data );
}


struct dlist *dlist_sort( struct dlist *list, DLCMPF cmp_func )
{
  return __dlist_sort_real( list, cmp_func );
}

struct dlist *dlist_sort_with_data( struct dlist *list, DLCMPDF cmp_func, void *user_data )
{
  return __dlist_sort_real_with_data( list, cmp_func, user_data );
}


void dlist_foreach( struct dlist *list, DLFUNC func, void *user_data )
{
  struct dlist *next = NULL;

  while( list )
  {
    next = dlist_next( list );
    if( func ) { func( list->data, user_data ); }
    list = next;
  }
}


void __dlist_free( struct dlist *list )
{
  struct dlist *next = NULL;

  while( list )
  {
    next = dlist_next( list );
    free( list ); list = NULL;
    list = next;
  }
}

void dlist_free( struct dlist *list, DLFUNC free_func )
{
  dlist_foreach( list, free_func, NULL );
  __dlist_free( list );
}
