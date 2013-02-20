/**
 * Copyright (c) 2011-2012 Jesper Öqvist <jesper@llbit.se>
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
#ifndef CTTF_LIST_H
#define CTTF_LIST_H

typedef struct list	list_t;
struct list {
	list_t*		succ;
	list_t*		pred;
	void*		data;
};

// if *list == NULL, create a new list otherwise append
void list_add(list_t** list, void* object);

// remove the first item from the list
void* list_remove(list_t** list);

// remove element containing data from list
int list_remove_item(list_t** list, void* data);

// remove all items from the list
int free_list(list_t** list);

int list_length(list_t* list);

// returns nonzero if list contains data
int list_contains(list_t* list, void* data);

#endif

