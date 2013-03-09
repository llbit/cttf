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
#ifndef CTTF_SHAPE_H
#define CTTF_SHAPE_H

#include <stdio.h>
#include "vector.h"

typedef struct shape	shape_t;

shape_t* new_shape();
void free_shape(shape_t** shape);
void shape_add_vec(shape_t* shape, float x, float y);
void shape_add_seg(shape_t* shape, int n, int m);
shape_t* load_shape(FILE* file);
void write_shape(FILE* file, shape_t* shape);
void render_shape(shape_t* shape);

struct shape {
	vector_t*	vec;
	int		nvec;
	int		maxvec;
	int*		seg;
	int		nseg;
	int		maxseg;
};

#endif

