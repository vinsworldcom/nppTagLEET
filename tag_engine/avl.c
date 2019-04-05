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

#include "avl.h"
#include <stddef.h>
#include <memory.h>

#define LEFT  0
#define RIGHT 1

/* node flags */
#define IS_RIGHT_CHILD  (1)
#define LEAN_LEFT2      (0 << 1)
#define LEAN_LEFT       (1 << 1)
#define BALANCED        (2 << 1)
#define LEAN_RIGHT      (3 << 1)
#define LEAN_RIGHT2     (4 << 1)

#define LR_IDX(n) ((n)->flags & IS_RIGHT_CHILD)

#define LEAN_MASK (7 << 1)
#define IS_LEAN_LEFT(n)   (((n)->flags & LEAN_MASK) == LEAN_LEFT)
#define IS_BALANCED(n)    (((n)->flags & LEAN_MASK) == BALANCED)
#define IS_LEAN_RIGHT(n)  (((n)->flags & LEAN_MASK) == LEAN_RIGHT)

#define VALID_BALANCE(n) (((n)->flags & (3 << 1)) != 0)

#define SET_RIGHT(n) ((n)->flags |= IS_RIGHT_CHILD)
#define SET_LEFT(n)  ((n)->flags &= ~IS_RIGHT_CHILD)
#define SET_LR_IDX(n, lr_idx) \
  (n)->flags ^= ((n)->flags ^ lr_idx) & IS_RIGHT_CHILD
#define COPY_LR_IDX(dstn, srcn) SET_LR_IDX(dstn, (srcn)->flags)

static avl_node_t *avl_roll_right(avl_root_t *r, avl_node_t *a, int helper);
static avl_node_t *avl_roll_left(avl_root_t *r, avl_node_t *c, int helper);

avl_node_t *avl_lookup(avl_root_t *r, void *key, avl_loc_t *loc)
{
  avl_node_t *n = r->root;
  avl_loc_t dummy_loc;
  int res;

  if (loc == NULL)
    loc = &dummy_loc;
  else
    memset(loc, 0, sizeof(*loc));

  if (r->comp == NULL)
    return NULL;

  loc->prev = &r->list;
  while (n != NULL)
  {
    res = r->comp(r->ctx, n, key);
    if (res == 0)
      return n;

    loc->up = n;
    if (res < 0)
    {
      loc->prev = &n->next;
      loc->lr_idx = RIGHT;
      n = n->lr[RIGHT];
    }
    else
    {
      loc->lr_idx = LEFT;
      n = n->lr[LEFT];
    }
  }

  return NULL;
}

static avl_node_t *avl_roll_left(avl_root_t *r, avl_node_t *c, int helper)
{
  avl_node_t *a = c->lr[LEFT];
  avl_node_t *b = a->lr[RIGHT];
  unsigned char a_lean = BALANCED;
  unsigned char c_lean = BALANCED;
  unsigned char lr_idx = LR_IDX(c);

  if (helper)
  {
    if (IS_LEAN_LEFT(a))
      c_lean = LEAN_RIGHT;
  }
  else if (IS_LEAN_RIGHT(a))
  {
    if (IS_LEAN_LEFT(b))
      c_lean = LEAN_RIGHT;

    a = avl_roll_right(r, a, 1);
    b = a->lr[RIGHT];
  }
  else if (IS_BALANCED(a)) /* Only on delete */
  {
    a_lean = LEAN_RIGHT;
    c_lean = LEAN_LEFT;
  }

  a->flags = a_lean | lr_idx;
  a->up = c->up;
  if (a->up)
    a->up->lr[lr_idx] = a;
  else
    r->root = a;

  c->flags = c_lean | RIGHT;
  c->up = a;
  a->lr[RIGHT] = c;

  c->lr[LEFT] = b;
  if (b != NULL)
  {
    SET_LEFT(b);
    b->up = c;
  }
  return a;
}

static avl_node_t *avl_roll_right(avl_root_t *r, avl_node_t *a, int helper)
{
  avl_node_t *c = a->lr[RIGHT];
  avl_node_t *b = c->lr[LEFT];
  unsigned char a_lean = BALANCED;
  unsigned char c_lean = BALANCED;
  unsigned char lr_idx = LR_IDX(a);

  if (helper)
  {
    if (IS_LEAN_RIGHT(c))
      a_lean = LEAN_LEFT;
  }
  else if (IS_LEAN_LEFT(c))
  {
    if (IS_LEAN_RIGHT(b))
      a_lean = LEAN_LEFT;

    c = avl_roll_left(r, c, 1);
    b = c->lr[LEFT];
  }
  else if (IS_BALANCED(c)) /* Only on delete */
  {
    a_lean = LEAN_RIGHT;
    c_lean = LEAN_LEFT;
  }

  c->flags = c_lean | lr_idx;
  c->up = a->up;
  if (c->up)
    c->up->lr[lr_idx] = c;
  else
    r->root = c;

  a->flags = a_lean | LEFT;
  a->up = c;
  c->lr[LEFT] = a;

  a->lr[RIGHT] = b;
  if (b != NULL)
  {
    SET_RIGHT(b);
    b->up = a;
  }
  return c;
}

void avl_insert(avl_root_t *r, avl_loc_t *loc, avl_node_t *n)
{
  static const char lean_update_on_add[2] = {-2, 2};
  avl_node_t *up = loc->up;
  unsigned char lr_idx;

  n->lr[RIGHT] = n->lr[LEFT] = NULL;
  n->flags = loc->lr_idx | BALANCED;
  if (up == NULL)
  {
    if (loc->prev == NULL)
      return;
    r->list = r->root = n;
    n->up = NULL;
    n->next = NULL;
    return;
  }
  /* Add to list */
  n->next = *loc->prev;
  *loc->prev = n;
  /* Insert to tree */
  n->up = up;
  up->lr[loc->lr_idx] = n;
  up->flags += lean_update_on_add[loc->lr_idx];
  /* Test if balance was changed */
  if (IS_BALANCED(up))
    return;

  while(up->up != NULL)
  {
    lr_idx = LR_IDX(up);
    up = up->up;
    up->flags += lean_update_on_add[lr_idx];
    switch (up->flags & LEAN_MASK)
    {
      case LEAN_LEFT:
      case LEAN_RIGHT:
        /* sub-tree changed from balance, continue up the tree */
        continue;
      case LEAN_LEFT2:
        avl_roll_left(r, up, 0);
        return;
      case LEAN_RIGHT2:
        avl_roll_right(r, up, 0);
        return;
      case BALANCED:
        /* sub-tree changed from lean left/right to balanced, no height change */
        return;
    }
  }
}

/* n is a node that its left or right (based on lr_idx) sub tree height was
 * reduced by 1 */
static void avl_fix_after_remove(avl_root_t *r, avl_node_t *n, unsigned char lr_idx)
{
  static const char lean_update_on_del[2] = {2, -2};

  while (n != NULL)
  {
    n->flags += lean_update_on_del[lr_idx];
    lr_idx = LR_IDX(n);
    switch (n->flags & LEAN_MASK)
    {
      case LEAN_LEFT:
      case LEAN_RIGHT:
        /* sub-tree changed from balance, no height change */
        return;
      case BALANCED:
        /* Height reduced continue up the tree */
        break;
      case LEAN_LEFT2:
        n = avl_roll_left(r, n, 0);
        if (!IS_BALANCED(n))
          return;
        break;
      case LEAN_RIGHT2:
        n = avl_roll_right(r, n, 0);
        if (!IS_BALANCED(n))
          return;
        break;
    }
    n = n->up;;
  }
}


static avl_node_t **avl_find_prev(avl_root_t *r, avl_node_t *n)
{
  unsigned char lr_idx;

  /* Lookup for the right-most child in the left sub tree */
  if (n->lr[LEFT] != NULL)
  {
    n = n->lr[LEFT];
    while (n->lr[RIGHT] != NULL)
      n = n->lr[RIGHT];

    return &n->next;
  }
  /* Otherwise a parent for which we are a right child */
  for(;;)
  {
    lr_idx = LR_IDX(n);
    n = n->up;
    if (n == NULL)
      return &r->list;
    if (lr_idx == RIGHT)
      return &n->next;
  }
}

avl_node_t *avl_prev(avl_root_t *r, avl_node_t *n)
{
  avl_node_t **prev = avl_find_prev(r, n);
  return prev == &r->list ? NULL : AVL_CONTREC(prev, avl_node_t, next);
}

void avl_remove(avl_root_t *r, avl_node_t *n)
{
  unsigned char lr_idx = LR_IDX(n);
  avl_node_t **up_ptr;
  avl_node_t **prev;
  avl_node_t *p;
  avl_node_t *fix;
  unsigned char fix_lr_idx;

  prev = avl_find_prev(r, n);
  up_ptr = n->up != NULL ? &n->up->lr[lr_idx] : &r->root;
  if (n->lr[RIGHT] != NULL)
  {
    p = n->next;
    p->up->lr[LR_IDX(p)] = p->lr[RIGHT];
    if (p->lr[RIGHT] != NULL)
    {
      p->lr[RIGHT]->up = p->up;
      COPY_LR_IDX(p->lr[RIGHT], p);
    }
  }
  else if (n->lr[LEFT] != NULL)
  {
    p = AVL_CONTREC(prev, avl_node_t, next);
    p->up->lr[LR_IDX(p)] = p->lr[LEFT];
    if (p->lr[LEFT] != NULL)
    {
      p->lr[RIGHT]->up = p->up;
      COPY_LR_IDX(p->lr[LEFT], p);
    }
  }
  else /* No children, just delete n */
  {
    *up_ptr = NULL;
    *prev = n->next;
    avl_fix_after_remove(r, n->up, lr_idx);
    return;
  }

  fix = p->up == n ? p : p->up;
  fix_lr_idx = LR_IDX(p);

  p->up = n->up;
  *up_ptr = p;

  p->lr[LEFT] = n->lr[LEFT];
  if (p->lr[LEFT] != NULL)
    p->lr[LEFT]->up = p;

  p->lr[RIGHT] = n->lr[RIGHT];
  if (p->lr[RIGHT] != NULL)
    p->lr[RIGHT]->up = p;

  p->flags = n->flags;
  *prev = n->next;

  avl_fix_after_remove(r, fix, fix_lr_idx);
}

avl_node_t *avl_get_prev_node(const avl_root_t *r, const avl_loc_t *loc)
{
  if (loc->prev != &r->list)
    return AVL_CONTREC(loc->prev, avl_node_t, next);

  return NULL;
}

avl_node_t *avl_get_next_node(const avl_loc_t *loc)
{
  return *loc->prev;
}

avl_node_t *avl_lookup_nearest(avl_root_t *r, void *key, int after)
{
  avl_node_t *n;
  avl_loc_t loc;

  n = avl_lookup(r, key, &loc);
  if (n != NULL)
    return n;

  if (after)
    return *loc.prev;

  return avl_get_prev_node(r, &loc);
}

