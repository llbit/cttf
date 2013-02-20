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
#include "qsortv.h"

/* Quicksort utility function
*/
static void qsort_swap(vertex_t** a, unsigned x, unsigned y)
{
	if (x != y) {
		vertex_t* z = a[x];
		a[x] = a[y];
		a[y] = z;
	}
}

/* Main vertex Quicksort function.
 * A vertex p is below a vertex q if
 * py < qy or py == qy and px > qx
 * The vertices are sorted in order of
 * decreasing height.
 */
static void qsort_work(vertex_t** a, unsigned start, unsigned end)
{
	unsigned	pivot;
	unsigned	i;
	vector_t	pv;
	unsigned	store;

	if (start >= end)
		return;
	else if (start == end - 1) {
		if (!vec_above(a[start]->vec, a[end]->vec))
			qsort_swap(a, start, end);
		return;
	}

	// select pivot
	pivot = (start + end) / 2;
	pv = a[pivot]->vec;
	qsort_swap(a, pivot, end);
	store = start;

	for (i = start; i < end; ++i) {

		if (vec_above(a[i]->vec, pv)) {
			qsort_swap(a, i, store);
			store++;
		}
	}

	qsort_swap(a, store, end);

	if (start < store)
		qsort_work(a, start, store-1);

	if (store < end)
		qsort_work(a, store+1, end);


}

/* Vertex Quicksort for polygon triangulation
*/
void qsort_verts(vertex_t** verts, unsigned n)
{
	qsort_work(verts, 0, n-1);
}


