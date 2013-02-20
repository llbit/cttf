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
#ifndef CTTF_TREESET_H
#define CTTF_TREESET_H

typedef struct set	set_t;
struct set {
	set_t*		left;
	set_t*		right;
	void*		data;
};

/* If *set == NULL, create a new set otherwise append */
void set_add(set_t** set, void* object);

/* Remove one item from the set */
void* set_remove(set_t** set);

/* Add all elements from set b to set a. This empties set b. */
void set_join(set_t** a, set_t** b);

/* remove all items from the set */
int free_set(set_t** set);

/* returns nonzero if set contains data */
int set_contains(set_t* set, void* data);

#endif


