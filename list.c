/**
 * Copyright (c) 2011 Jesper Öqvist <jesper@llbit.se>
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
#include <assert.h>
#include <stdlib.h>
#include "list.h"

/* Appends the new object to the end of the list
 */
void list_add(list_t** list, void* object)
{
	assert(list != NULL);
	if (*list == NULL) {
		*list = malloc(sizeof(list_t));
		(*list)->data = object;
		(*list)->succ = *list;
		(*list)->pred = *list;
	} else {
		list_t*	new = malloc(sizeof(list_t));
		new->data = object;
		new->succ = *list;
		new->pred = (*list)->pred;
		(*list)->pred = new;
		new->pred->succ = new;
	}
}

/* Remove the first element of the list, if the list is
 * non-empty and return the data value for that element.
 * If the list was empty, the return value is NULL.
 */
void* list_remove(list_t** list)
{
	void* data;

	assert(list != NULL);

	if (*list == NULL) return NULL;

	data = (*list)->data;

	if ((*list)->succ == *list) {
		free(*list);
		*list = NULL;
	} else {
		list_t*	pred = (*list)->pred;
		list_t*	succ = (*list)->succ;
		pred->succ = succ;
		succ->pred = pred;
		free(*list);
		*list = succ;
	}

	return data;
}

/* Remove first element that has a data pointer
 * equal to the data argument from the list.
 *
 * This function may remove 1 or 0 elements from
 * the list depending on if the specified data is
 * in the list or not. If it is in the list, the
 * first list element containing the data will
 * be removed.
 *
 * The return value is the number of list elements
 * that were removed.
 */
int list_remove_item(list_t** list, void* data)
{
	list_t*	p;

	assert(list != NULL);

	p = *list;
	if (p)
	do {
		if (p->data == data) {
			if (p->succ == p) {
				free(p);
				*list = NULL;
				return 1;
			} else {
				list_t*	pred = p->pred;
				list_t*	succ = p->succ;
				pred->succ = succ;
				succ->pred = pred;
				free(p);
				if (p == *list) {
					*list = succ;
				}
				return 1;
			}
		}
		p = p->succ;
	} while (p != *list);

	// no item was removed
	return 0;
}

/* Delete all items from the list.
 *
 * Returns the numer of items that were deleted.
 */
int free_list(list_t** list)
{
	int	count = 0;
	list_t*	p;
	list_t*	h;

	if (list == NULL) return 0;
	if (*list == NULL) return 0;

	p = h = *list;
	do {
		list_t*	t = p;
		p = p->succ;
		free(t);
		count++;
	} while (p != h);
	*list = NULL;
	return count;
}

int list_length(list_t* list)
{
	list_t*	p = list;
	int	len = 0;
	if (p)
	do {
		p = p->succ;
		len += 1;
	} while (p != list);
	return len;
}

int list_contains(list_t* list, void* data)
{
	list_t*	p = list;
	if (p)
	do {
		if (p->data == data)
			return 1;
		p = p->succ;
	} while (p != list);
	return 0;
}

