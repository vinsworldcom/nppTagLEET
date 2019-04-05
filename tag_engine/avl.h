/*  Copyright 2013-2014, Gur Stavi, gur.stavi@gmail.com  */

/*
    This file is part of TagLEET.

    TagLEET is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    TagLEET is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with TagLEET.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AVL_H_
#define _AVL_H_

#ifdef __cplusplus
#define AVL_API extern "C"
#else
#define AVL_API
#endif

typedef struct avl_root_s avl_root_t;
typedef struct avl_node_s avl_node_t;
typedef struct avl_loc_s avl_loc_t;

/* Compare callback. Compare the key of n with 'key'.
 * Obtain the key of n by using CONTAINING_RCORD.
 * Return 0 if keys are equal, negative if n should be before 'key',
 * positive if n should be after 'key' */
typedef int (*avl_comp_func_t)(void *ctx, avl_node_t *n, void *key);

AVL_API avl_node_t *avl_lookup(avl_root_t *r, void *key, avl_loc_t *loc);
AVL_API void avl_insert(avl_root_t *r, avl_loc_t *loc, avl_node_t *n);
AVL_API void avl_remove(avl_root_t *r, avl_node_t *n);
AVL_API avl_node_t *avl_prev(avl_root_t *r, avl_node_t *n);

AVL_API avl_node_t *avl_lookup_nearest(avl_root_t *r, void *key, int after);
/* Following a lookup, get the node previous to the lookup key. This will also
 * work if the lookup returned NULL.
 * Returns NULL if the lookup key is (or should be) the first in the tree. */
AVL_API avl_node_t *avl_get_prev_node(const avl_root_t *r, const avl_loc_t *loc);
AVL_API avl_node_t *avl_get_next_node(const avl_loc_t *loc);

#define AVL_CONTREC(ptr,strct,field) \
  ((strct *)(void *)((char *)(ptr) - ((char *)&((strct *)0)->field - (char *)0)))

struct avl_root_s {
  avl_node_t *root;
  avl_node_t *list;
  avl_comp_func_t comp;
  void *ctx;
};

struct avl_node_s {
  avl_node_t *lr[2];
  avl_node_t *up, *next;
  unsigned char flags;
  /* Use 'type' if you want to store multiple types in a single avl_root.
   * Following a lookup 'type' would tell you what kind of CONTAINING_RECORD to
   * use */
  unsigned char type;
};

struct avl_loc_s {
  avl_node_t *up;
  avl_node_t **prev;
  unsigned char lr_idx;
};

#endif /* _AVL_H_ */

