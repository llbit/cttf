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
#include <stdlib.h>
#include <assert.h>
#include "treeset.h"

void set_add(set_t** set, void* object)
{
	assert(set != NULL);
	if (*set == NULL) {
		*set = malloc(sizeof(set_t));
		(*set)->data = object;
		(*set)->left = NULL;
		(*set)->right = NULL;
	} else if ((*set)->data > object) {
		set_add(&(*set)->left, object);
	} else if ((*set)->data < object) {
		set_add(&(*set)->right, object);
	}
}

static void* set_rem_rightmost(set_t** set)
{
	assert(set != NULL);
	assert(*set != NULL);
	
	if ((*set)->left == NULL && (*set)->right == NULL) {
		set_t*	s = *set;
		*set = NULL;
		return s;
	} else if ((*set)->right != NULL) {
		return set_rem_rightmost(&(*set)->right);
	} else {
		return set_rem_rightmost(&(*set)->left);
	}
}

static void* set_rem_leftmost(set_t** set)
{
	assert(set != NULL);
	assert(*set != NULL);
	
	if ((*set)->left == NULL && (*set)->right == NULL) {
		set_t*	s = *set;
		*set = NULL;
		return s;
	} else if ((*set)->left != NULL) {
		return set_rem_leftmost(&(*set)->left);
	} else {
		return set_rem_leftmost(&(*set)->right);
	}
}

void* set_remove(set_t** set)
{
	void*	data;

	assert(set != NULL);

	if (*set == NULL) return NULL;

	data = (*set)->data;

	if ((*set)->left != NULL) {
		/* get rightmost child of left subtree and make it the new root */
		set_t*	s = set_rem_rightmost(&(*set)->left);
		s->left = (*set)->left;
		s->right = (*set)->right;
		free(*set);
		*set = s;
	} else if ((*set)->right != NULL) {
		/* get leftmost child of right subtree and make it the new root */
		set_t*	s = set_rem_leftmost(&(*set)->right);
		s->left = (*set)->left;
		s->right = (*set)->right;
		free(*set);
		*set = s;
	} else {
		free(*set);
		*set = NULL;
	}

	return data;
}

void set_join(set_t** a, set_t** b)
{
	while (*b != NULL) {
		set_add(a, set_remove(b));
	}
}

int free_set(set_t** set)
{
	set_t*	left;
	set_t*	right;
	int n;
	assert(set != NULL);
	if (*set == NULL) return 0;
	n = 1;
	left = (*set)->left;
	right = (*set)->right;
	free(*set);
	*set = NULL;
	n += free_set(&left);
	n += free_set(&right);
	return n;
}

int set_contains(set_t* set, void* data)
{
	if (set == NULL)
		return 0;
	else if (set->data == data)
		return 1;
	else if (set->data > data)
		return set_contains(set->left, data);
	else
		return set_contains(set->right, data);
}
