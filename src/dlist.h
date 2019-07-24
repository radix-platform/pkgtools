
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

#ifndef _DLIST_H_
#define _DLIST_H_

#ifdef __cplusplus
extern "C" {
#endif


struct dlist {
  struct dlist *prev;
  struct dlist *next;

  void   *data;
};

typedef void (*DLFUNC)  ( void *data, void *user_data );
typedef int  (*DLCMPF)  ( const void *a, const void *b );
typedef int  (*DLCMPDF) ( const void *a, const void *b, void *user_data );

#define dlist_prev( list )  ( (list)->prev )
#define dlist_next( list )  ( (list)->next )

extern struct dlist *__dlist_alloc( void );
extern struct dlist *dlist_first( struct dlist *list );
extern struct dlist *dlist_last( struct dlist *list );
extern int dlist_length( struct dlist *list );
extern struct dlist *dlist_nth( struct dlist *list, int n );
extern void *dlist_nth_data( struct dlist *list, int n );
extern int dlist_position( struct dlist *list, struct dlist *link );
extern int dlist_index( struct dlist *list, const void *data );
extern struct dlist *dlist_find( struct dlist *list, const void *data );
extern struct dlist *dlist_find_data( struct dlist *list, DLCMPF func, const void *data );

extern struct dlist *dlist_append( struct dlist *list, void *data );
extern struct dlist *dlist_prepend( struct dlist *list, void *data );
extern struct dlist *dlist_insert( struct dlist *list, void *data, int position );
extern struct dlist *dlist_insert_sorted( struct dlist *list, DLCMPF cmp_func, void *data );
extern struct dlist *dlist_insert_sorted_with_data( struct dlist *list, DLCMPDF cmp_func, void *data, void *user_data );
extern struct dlist *dlist_concat( struct dlist *list1, struct dlist *list2 );
extern struct dlist *dlist_insert_list( struct dlist *list1, struct dlist *list2, int position );

extern struct dlist *__dlist_remove_link( struct dlist *list, struct dlist *link );
extern struct dlist *dlist_remove( struct dlist *list, const void *data );
extern struct dlist *dlist_remove_all( struct dlist *list, const void *data );
extern struct dlist *dlist_remove_data( struct dlist *list, DLCMPF cmp_func, DLFUNC free_func, const void *data );
extern struct dlist *dlist_remove_data_all( struct dlist *list, DLCMPF cmp_func, DLFUNC free_func, const void *data );

extern struct dlist *dlist_copy( struct dlist *list );
extern struct dlist *dlist_reverse( struct dlist *list );

extern struct dlist *dlist_sort( struct dlist *list, DLCMPF cmp_func );
extern struct dlist *dlist_sort_with_data( struct dlist *list, DLCMPDF cmp_func, void *user_data );

extern void dlist_foreach( struct dlist *list, DLFUNC func, void *user_data );


extern void __dlist_free( struct dlist *list );
extern void dlist_free( struct dlist *list, DLFUNC free_func );


#ifdef __cplusplus
}  /* ... extern "C" */
#endif

#endif /* _DLIST_H_ */
