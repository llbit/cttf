/**
 * Copyright (c) 2012 Jesper Öqvist <jesper@llbit.se>
 *
 * This file is part of cTTF.
 *
 * cTTF is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * cTTF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cTTF; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
/*
 * Binary Search Tree
 */
#ifndef CTTF_BSTREE_H
#define CTTF_BSTREE_H

#include "list.h"

typedef struct binary_search_tree bstree_t;
typedef int (*comparator_t)(void*, void*);

void free_bstree(bstree_t** tree);
void bstree_insert(bstree_t** tree, void* v, comparator_t cmp);
void bstree_remove(bstree_t** tree, void* v, comparator_t cmp);
void bstree_remove_if(bstree_t** tree, void* v, comparator_t cmp);
void* bstree_find_left(bstree_t* tree, void* v, comparator_t cmp);
void* bstree_find_right(bstree_t* tree, void* v, comparator_t cmp);
void bstree_find_all(bstree_t* tree, void* v, comparator_t cmp, list_t** list);
unsigned bstree_size(bstree_t* tree);

// TODO: make red-black tree
struct binary_search_tree {
	bstree_t*	parent;
	void*		value;
	void*		rvalue;
	void*		min;
	void*		max;
	bstree_t*	left;
	bstree_t*	right;
};

#endif

