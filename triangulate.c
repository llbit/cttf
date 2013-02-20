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
/*
 * Polygon triangulation, based on the algorithm given in
 * Computational Geometry: Algorithms and Applications, Third Edition,
 * by Berg, M. and Cheong, O. and Kreveld, M. and Overmars, M.,
 * Springer, 2008.
 *
 * Note that this algorithm can not handle polygons with intersecting
 * segments although simple polygons with holes are supported.
 *
 * The only thing in the current implementation of this algorithm which
 * does not follow the book, is the search tree which is used to find
 * the seg directly left of a point. It is not a balanced search tree
 * and so may have worse running time than log(n). However, for most
 * kinds of polygons there will seldom be more than 3 or 4 edges in
 * this data structure at a time so it's not a biggie.
 *
 * TODO: make the search tree a red-black tree
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdio.h>
#include "triangulate.h"
#include "list.h"
#include "qsortv.h"
#include "stack.h"
#include "treeset.h"

/* Vertex status flags */
#define VERTEX_FLAG1	(1)
#define VERTEX_VISITED	(2)
#define VERTEX_UP	(4)
#define VERTEX_DOWN	(8)

#if 0
#define USE_SHARED
#endif

#if 0
#define MAKE_PLANAR_DBG
#endif

#if 0
#define ZERO_AREA_DBG
#endif

#if 0
#define BUILD_EDGELIST_DBG
#endif

/* pi */
#ifndef PI
#define PI	(3.14159265358979323846)
#endif

static void set_left_face(edge_t* component, face_t* face);
static double angle(vector_t v1, vector_t v2);
static double angle_between(double a1, double a2);
static void handle_start_vertex(edge_list_t* edge_list, vertex_t* v);
static void handle_end_vertex(edge_list_t* edge_list, vertex_t* v);
static void handle_split_vertex(edge_list_t* edge_list, vertex_t* v);
static void handle_merge_vertex(edge_list_t* edge_list, vertex_t* v);
static void handle_regular_vertex(edge_list_t* edge_list, vertex_t* v);
static void add_diagonal(edge_list_t* edge_list, vertex_t* v1, vertex_t* v2);
static vertex_t* helper(edge_t* e);
static void set_helper(edge_t* e, vertex_t* v);
static void align_vertices(edge_t* start);
static void align_face_vertices(face_t* face);

/* Tree comparators */
static int edge_left_of_edge(void* a, void* b);
static int vertex_left_of_edge(void* v, void* b);

/* Free resources used by the given edge list
 */
void free_edgelist(edge_list_t** edge_list)
{
	edge_list_t*	p;
	assert(edge_list != NULL);

	p = *edge_list;
	if (!p) return;

	/* free vertices */
	if (p->vertices) {
		int i;
		for (i = 0; i < p->nvert; ++i)
			free(p->vertices[i]);
		free(p->vertices);
	}

	/* free faces */
	while (p->faces) {
		face_t*	face = p->faces->data;

		while (face->inner_components)
			list_remove(&face->inner_components);
		free(face);
		list_remove(&p->faces);
	}

	/* free edges */
	while (p->edges) {
		free(p->edges->data);
		list_remove(&p->edges);
	}

	/* free cycle list */
	free_list(&p->cycles);

	/* free edge tree */
	free_bstree(&p->etree);

	free(*edge_list);
	*edge_list = NULL;
}

/* debug func */
static void print_edge(edge_t* edge)
{
	printf("%d, %d\n", edge->origin->id, edge->twin->origin->id);
}

/* debug func */
void print_edge_tree(bstree_t* tree)
{
	if (!tree) return;
	if (tree->left == NULL && tree->right == NULL) {
		print_edge(tree->value);
	} else {
		print_edge_tree(tree->left);
		print_edge_tree(tree->right);
	}
}

/* Returns 1 if a is above b
 */
int vec_above(vector_t a, vector_t b)
{
	return a.y > b.y  || (a.y == b.y  && a.x < b.x);
}

/* Returns 1 if a == b
 */
static int vec_eq(vector_t a, vector_t b)
{
	return a.x == b.x && a.y == b.y;
}

#define DIST_EPS (1E-10)

/* Returns 1 if the vectors a and b are very close
 */
static int vec_close(vector_t a, vector_t b)
{
	vector_t	diff;
	diff.x = fabsf(a.x-b.x);
	diff.y = fabsf(a.y-b.y);
	return diff.x < DIST_EPS && diff.y < DIST_EPS;
}

static int event_above(const void* a, const void* b)
{
	const struct event*	ea = *(struct event**)a;
	const struct event*	eb = *(struct event**)b;
	if (vec_above(ea->vec, eb->vec))
		return -1;
	else if (vec_eq(ea->vec, eb->vec))
		return 0;
	else
		return 1;
}

static int vertex_above(const void* a, const void* b)
{
	const vertex_t*	va = *(vertex_t**)a;
	const vertex_t*	vb = *(vertex_t**)b;
	if (vec_above(va->vec, vb->vec))
		return -1;
	else if (vec_eq(va->vec, vb->vec))
		return 0;
	else
		return 1;
}

/* Get helper vertex of an edge
 */
static vertex_t* helper(edge_t* e)
{
	assert(e->helper != NULL);
	return e->helper;
}

static void set_helper(edge_t* e, vertex_t* v)
{
	e->helper = v;
	e->twin->helper = v;
}

/* Create a new vertex
 */
static vertex_t* new_vertex()
{
	vertex_t*	obj = malloc(sizeof(vertex_t));
	obj->vtype = UNCLASSIFIED_VERTEX;
	obj->flags = 0;
	obj->incident_edge = NULL;
	return obj;
}

/* Create a new, empty edge
 */
static edge_t* new_edge()
{
	edge_t*	obj = malloc(sizeof(edge_t));
	obj->origin = NULL;
	obj->left_face = NULL;
	obj->twin = NULL;
	obj->succ = NULL;
	obj->pred = NULL;
	obj->helper = NULL;
	obj->cycle = -1;
	return obj;
}

/* Create a new, empty face
 */
static face_t* new_face(edge_list_t* edge_list)
{
	face_t*	obj = malloc(sizeof(face_t));
	obj->inner_components = NULL;
	obj->outer_component = NULL;
	obj->is_inside = -1;
	list_add(&edge_list->faces, obj);
	return obj;
}

/* Angle of vector v1->v2
 *
 * ( 1, 0) => 0
 * ( 0, 1) => pi/2
 * (-1, 0) => pi
 * ( 0,-1) => pi*3/2
 */
static double angle(vector_t v1, vector_t v2)
{
	double	phi = atan2(v2.y - v1.y, v2.x - v1.x);
	return (phi >= 0) ? phi : phi + 2*PI;
}

/* Calculates angle difference between a1 and a2
 */
static double angle_between(double a1, double a2)
{
	return (a2>=a1) ? a1 + 2*PI - a2 : a1 - a2;
}

/* Set the incident face for a component
 */
static void set_left_face(edge_t* component, face_t* face)
{
	edge_t*	p;
	p = component;
	assert(p != NULL);
	do {
		p->left_face = face;
		p = p->succ;
	} while (p != component);
}

/* Link the in and out edges to the vertex v.
 * 
 * If v already did not have an incident edge we simply
 * set the incident edge to be the out edge, and link the
 * in and out edges.
 *
 * Otherwise, the successor edge to the in edge is the
 * incident edge which immediately follows it in clockwise
 * order.
 */
static void link_edges(vertex_t* v, edge_t* in, edge_t* out)
{
	out->origin = v;

	in->succ = out;
	out->pred = in;
}

/* Returns 1 if start is an edge of a new cycle
 * Returns 0 if start was not on a new cycle
 *
 * If a new cycle is found, each edge in the cycle is given the next cycle id
 */
static int is_new_cycle(edge_t* start)
{
	edge_t* p;
	if (start->cycle != -1)
		return 0;

	p = start;
	do {
		p->cycle = -2;
		p = p->succ;
	} while (p != start);
	return 1;
}

/* Returns the edge originating at the leftmost vertex of an edge cycle
 */
static edge_t* leftmost_edge(edge_t* start)
{
	edge_t* left = start;
	edge_t* p = left->succ;
	while (p != start) {
		vector_t v0 = left->origin->vec;
		vector_t v1 = p->origin->vec;
		/* the leftmost is the one with least x coordinate */
		/* if the x coordinate is the same, it is the one with */
		/* least y coordinate */
		if (v1.x < v0.x || (v1.x == v0.x && v1.y < v0.y)) {
			left = p;
		}
		p = p->succ;
	}
	return left;
}

struct arc {
	edge_t*	a;
	edge_t*	b;
};

#ifdef USE_SHARED
static struct arc* new_arc(edge_t* a, edge_t* b)
{
	struct arc*	arc = malloc(sizeof(struct arc));
	arc->a = a;
	arc->b = b;
	return arc;
}

static void connect_shared(list_t* out, edge_t** cycles,
		int* is_inner, list_t** arcs)
{
	int	prev = -1;
	edge_t*	prev_e = NULL;

	list_t*	p = out;
	if (p) do {
		edge_t*	e = p->data;
		prev = e->twin->cycle;
		prev_e = e->twin;

		p = p->succ;
		e = p->data;

		int	id = e->cycle;
		if (is_inner[id] && prev_e != NULL) {
			if (!is_inner[prev]) {
				void*	arc = new_arc(cycles[id], cycles[prev]);
				list_add(arcs, arc);
#ifdef BUILD_EDGELIST_DBG
				printf("joined: %d, %d\n", id, prev);
#endif
			} else if (id != prev) {
				void*	arc = new_arc(cycles[id], cycles[prev]);
				list_add(arcs, arc);
#ifdef BUILD_EDGELIST_DBG
				printf("joined: %d, %d\n", id, prev);
#endif
			}
		}
	} while (p != out);
}
#endif

static int is_outer(edge_t* cycle, int* is_inner)
{
	return cycle == NULL || !is_inner[cycle->cycle];
}

static int already_connected_to_outer(edge_t* cycle, int* is_inner,
		list_t** arcs)
{
	list_t*	p = *arcs;
	if (p) do {
		struct arc*	arc = p->data;
		if (arc->a == cycle && is_outer(arc->b, is_inner)) {
			return 1;
		} else if (arc->b == cycle && is_outer(arc->a, is_inner)) {
			return 1;
		}
		p = p->succ;
	} while (p != *arcs);
	return 0;
}

/* Connect the components (if any) that vertex v is the leftmost vertex of
 * to the edge left_edge immediately to the left of v.
 */
static void connect_leftmost(vertex_t* v, int ncycle, edge_t** cycles,
		int* is_inner, edge_t* left_edge, list_t** arcs)
{
	int i;
	for (i = 0; i < ncycle; ++i) {
		edge_t*	cycle = cycles[i];
		struct arc*	arc;

		if (v != cycle->origin)
			continue;

		if (is_inner[i]) {
			/* connect to left edge */
			if (is_outer(left_edge, is_inner) &&
					already_connected_to_outer(
						cycle, is_inner, arcs))
				continue;

			arc = malloc(sizeof(struct arc));
			arc->a = cycle;
			if (left_edge)
				arc->b = cycles[left_edge->cycle];
			else
				arc->b = NULL;
			list_add(arcs, arc);
		}
	}
}

/* Align the vertices in a component so that they
 * have the edges as their incident_edge
 */
static void align_vertices(edge_t* start)
{
	edge_t*	p;
	p = start;
	do {
		p->origin->incident_edge = p;
		p = p->succ;
	} while (p != start);
}

static void align_face_vertices(face_t* face)
{
	if (face->outer_component) {
		align_vertices(face->outer_component);
	}
	if (face->inner_components) {
		list_t*	p = face->inner_components;
		do {
			align_vertices(p->data);
			p = p->succ;
		} while (p != face->inner_components);
	}
}

static void find_connecting_faces(list_t** faces, face_t* face,
		edge_t* component)
{
	edge_t*	edge = component;
	if (edge) do {
		face_t* connected = edge->twin->left_face;
		if (connected != NULL && connected->is_inside == -1) {
			connected->is_inside = face->is_inside ^ 1;
			list_add(faces, connected);
		}
		edge = edge->succ;
	} while (edge != component);
}

static void find_inner_faces(face_t* face)
{
	list_t*	inner_faces = NULL;
	/*find_connecting_faces(&inner_faces, face, face->outer_component); */

	list_t*	p = face->inner_components;
	if (p) do {
		find_connecting_faces(&inner_faces, face, p->data);
		p = p->succ;
	} while (p != face->inner_components);

	while (inner_faces) {
		face_t* inner = list_remove(&inner_faces);
		find_inner_faces(inner);
	}
}

/*static int edge_starts_at_vertex(void* v, void* x)
{
	edge_t*	e = x;
	return e->origin == v;
}*/

static int edge_ends_at_vertex(void* v, void* x)
{
	edge_t*	e = x;
	return e->twin->origin == v;
}

static void set_cycle(edge_t* start, int cycle)
{
	edge_t*	p;
	p = start;
	do {
		p->cycle = cycle;
		p = p->succ;
	} while (p != start);
}

/* debug func */
void print_cycle(edge_t* start)
{
	edge_t*	p = start;
	printf("cycle %d:\n", start->cycle);
	do {
		printf("  %d (%f, %f)\n",
				p->origin->id,
				p->origin->vec.x,
				p->origin->vec.y);
		p = p->succ;
	} while (p != start);
}

static void set_not_visited(edge_t* cycle)
{
	/* set all vertices in the cycle to "not visited" */
	edge_t*	p = cycle;
	if (p) do {
		p->origin->flags &= ~VERTEX_VISITED;
		p = p->succ;
	} while (p != cycle);
}

/* debug func */
void print_vertex(vertex_t* v)
{
	printf("%d (%f, %f)\n", v->id, v->vec.x, v->vec.y);
}

/* Build a doubly connected edge list from a shape structure.
 *
 * The edge list will contain vertices and faces. The vertices
 * and faces in turn reference edges which connect the vertices
 * and bound the faces.
 *
 * The vertices will be sorted top-down. If two vertices have
 * the same Y-coordinate the one with smaller X-coordinate
 * is considered "above".
 *
 * NB: Triangulation can not be done for non-simple polygons,
 * so don't attempt to use a shape which describes a non-simple
 * polygon. Polygons with holes are permitted though.
 */
void connect_components(edge_list_t* edge_list)
{
	unsigned		nvert = edge_list->nvert;
	vertex_t**	vertices = edge_list->vertices;
	list_t**	incident_edges;
	list_t*		full_cycles;
	list_t*		split_cycles;
	unsigned i;
	unsigned ncycle;
	edge_t** cycles;
	int* is_inner;
	bstree_t* status = NULL;
	list_t* arcs = NULL;
	list_t* connected = NULL;
	face_t*	unbounded_face = NULL;

	if (nvert < 3) {
		/* nothing to do */
		return;
	}

	incident_edges = malloc(sizeof(list_t*) * nvert);

	/* Identify cycles */
	full_cycles = NULL;
	for (i = 0; i < nvert; ++i) {
		vertex_t*	v = vertices[i];
		edge_t*	p;

		if (!v->incident_edge)
			continue;/* ignore detached vertices */

		incident_edges[i] = NULL;

		p = v->incident_edge;
		if (p) do {

			if (is_new_cycle(p)) {
				list_add(&full_cycles, p);
			}
			if (is_new_cycle(p->twin)) {
				list_add(&full_cycles, p->twin);
			}

			p = p->twin->succ;
		} while (p != v->incident_edge);
	}

	/* Split cycles so each vertex is visited at most one time per cycle.
 	 * Add the final cycles with their leftmost edges into the edge list.
 	 */
	split_cycles = NULL;
	while (full_cycles) {
		edge_t*	cycle = list_remove(&full_cycles);
		edge_t*	p = cycle;

		set_not_visited(cycle);

#ifdef BUILD_EDGELIST_DBG
		printf("cycle: "); print_edge(cycle);
#endif
		do {
			if (p->origin->flags & VERTEX_VISITED) {
				/* We have been here before! Create new cycle: */
				edge_t*	in = p->origin->incident_edge;
				edge_t*	in_pred = in->pred;
				
				/* connect pred(p) -> incident(origin(p)):  */
#ifdef BUILD_EDGELIST_DBG
				printf("connecting:\n");
				printf("  "); print_edge(p->pred);
				printf("->"); print_edge(in);
#endif
				p->pred->succ = in;
				in->pred = p->pred;

				/* pred(incident(origin(p))) -> p */
#ifdef BUILD_EDGELIST_DBG
				printf("connecting:\n");
				printf("  "); print_edge(in_pred);
				printf("->"); print_edge(p);
#endif
				in_pred->succ = p;
				p->pred = in_pred;

				/* Add chain containing incident(origin(p))
				 * to cycle list
				 */
#ifdef USE_SHARED
				list_add(&split_cycles, in);
#endif
				cycle = p;
				set_not_visited(cycle);
			}

			p->origin->incident_edge = p;
			p->origin->flags |= VERTEX_VISITED;
			p = p->succ;
		} while (p != cycle);

		list_add(&split_cycles, cycle);
	}

	/* Remove cycles only consisting of two edges */
#ifdef BUILD_EDGELIST_DBG
	printf("removing line segs\n");
#endif
	ncycle = 0;
	while (split_cycles) {
		edge_t*	cycle = list_remove(&split_cycles);

		if (cycle->succ->succ != cycle) {
			set_cycle(cycle, ncycle);
			list_add(&edge_list->cycles, leftmost_edge(cycle));
			ncycle += 1;
#ifdef BUILD_EDGELIST_DBG
			print_cycle(cycle);
		} else {
			printf("rem: "); print_edge(cycle);
#endif
		}
	}

#ifdef BUILD_EDGELIST_DBG
	printf("ncycle: %d\n", ncycle);
#endif
	/* Build cycle array */
	cycles = malloc(sizeof(edge_t*) * ncycle);
	if (edge_list->cycles) {
		list_t* p = edge_list->cycles;
		for (i = 0; i < ncycle; ++i) {
			cycles[i] = p->data;
			p = p->succ;
		}
	}

	for (i = 0; i < ncycle; ++i) {
		edge_t*	cycle = cycles[i];
		edge_t*	e = cycle;
		do {
			vertex_t*	v = e->origin;
			list_add(&incident_edges[v->id], e);
			e = e->succ;
		} while (e != cycle);
	}

	/* Classify cycles as inner or outer components.
	 * A cycle is an inner component if it's edges are clockwise oriented.
	 */
	is_inner = malloc(sizeof(int) * ncycle);
	for (i = 0; i < ncycle; ++i) {
		vector_t	u = cycles[i]->origin->vec;
		vector_t	u1 = cycles[i]->pred->origin->vec;
		vector_t	u2 = cycles[i]->succ->origin->vec;

		/* if the angle between u1->u and u->u2 is greater than
		 * or equal to pi then this cycle is an inner component, i.e.,
		 * the bounding edge of a hole. Otherwise  it is an outer
		 * component
		 */
		
		double a1 = angle(u, u1); 
		double a2 = angle(u, u2); 
		is_inner[i] = angle_between(a1, a2) > PI;

		/* debug */
		/*printf("%d, %d, %d : %s\n",
				cycles[i]->pred->origin->id,
				cycles[i]->origin->id,
				cycles[i]->succ->origin->id,
				is_inner[i] ? "true" : "false");*/
	}

	/* Sort the vertices in order of decreasing Y-coordinate */
	qsort_verts(vertices, nvert);

	/* Note:
	 * the half-edge going down is always the one on the right hand side
	 */
	
#ifdef BUILD_EDGELIST_DBG
	printf("connecting components:\n");
#endif

	/* Use a sweep line to connect bounding edges that share a face */
	for (i = 0; i < nvert; ++i) {
		vertex_t*	v = vertices[i];
		edge_t*		left_edge = NULL;
		list_t*		out;
		list_t*		p;

#ifdef BUILD_EDGELIST_DBG
		printf("event:\n");
		print_vertex(v);
#endif

		bstree_remove_if(&status, v, &edge_ends_at_vertex);

		left_edge = bstree_find_left(status, v, vertex_left_of_edge);

		if (left_edge != NULL && vertex_left_of_edge(v, left_edge))
			left_edge = NULL;

		/* add downward half-edges to status */
		out = incident_edges[i];
		if ((p = out)) do {
			edge_t*		edge = p->data;
			vector_t	origin = edge->origin->vec;
			vector_t	end = edge->twin->origin->vec;

			if (vec_above(origin, end)) {
				bstree_insert(&status, edge, edge_left_of_edge);
			}
			p = p->succ;
		} while (p != out);

		/* out = edges leaving this vertex */
#ifdef BUILD_EDGELIST_DBG
		printf("out(e):\n");
		if ((p = out)) do {
			printf("  ");print_edge(p->data);
			p = p->succ;
		} while (p != out);
#endif

#ifdef USE_SHARED
		connect_shared(out, cycles, is_inner, &arcs);
		int shared = 0;
		if (left_edge != NULL) {
			int	oc = left_edge->cycle;
			if ((p = out)) do {
				edge_t*	e = p->data;
				if (e->cycle == oc || e->twin->cycle == oc) {
					shared = 1;
					break;
				}
				p = p->succ;
			} while (p != out);
		}
		if (!shared) {
#endif
			connect_leftmost(v, ncycle, cycles,
				is_inner, left_edge, &arcs);
#ifdef USE_SHARED
		}
#endif

#ifdef BUILD_EDGELIST_DBG
		printf("status:\n");
		print_edge_tree(status);
#endif
	}

	for (i = 0; i < nvert; ++i) {
		free_list(&incident_edges[i]);
	}
	free(incident_edges);

	assert(status == NULL);

	/* The arcs list contains the arcs between connected components.
	 * Use the arcs to connect components to faces.
	 */
	while (arcs) {
		struct arc*	arc = list_remove(&arcs);
		edge_t*		a = arc->a;
		edge_t*		b = arc->b;
		set_t*		aset = NULL;
		set_t*		bset = NULL;
		list_t* p;

#ifdef BUILD_EDGELIST_DBG
		printf("arc: ");
		if (a == NULL)
			printf("unbounded, ");
		else
			printf("%d, ", a->cycle);
		if (b == NULL)
			printf("unbounded\n");
		else
			printf("%d\n", b->cycle);
#endif

		free(arc);

		p = connected;
		if (p) do {
			set_t*	set = p->data;
			if (set_contains(set, a))
				aset = set;
			else if (set_contains(set, b))
				bset = set;
			p = p->succ;
		} while (p != connected);

		if (aset == NULL && bset == NULL) {
			set_t* nset = NULL;
			set_add(&nset, a);
			set_add(&nset, b);
			list_add(&connected, nset);
		} else if (aset != NULL && bset == NULL) {
			set_add(&aset, b);
		} else if (aset == NULL && bset != NULL) {
			set_add(&bset, a);
		} else {
			list_remove_item(&connected, bset);
			set_join(&aset, &bset);
		}
	}

	while (connected) {
		set_t*	set = list_remove(&connected);
		face_t*	face = new_face(edge_list);

#ifdef BUILD_EDGELIST_DBG
		printf("connected:\n");
#endif
		while (set) {
			edge_t*	e = set_remove(&set);
			if (e != NULL) {
#ifdef BUILD_EDGELIST_DBG
				printf("  %d\n", e->cycle);
#endif
				e->left_face = face;
				if (is_inner[e->cycle]) {
					list_add(&face->inner_components, e);
				} else {
					face->outer_component = e;
				}
			} else {
#ifdef BUILD_EDGELIST_DBG
				printf("  unbounded\n");
#endif
				unbounded_face = face;
			}
		}
	}

	free(is_inner);/* no longer needed */

	assert(unbounded_face != NULL);

	/* Add unconnected faces */
	for (i = 0; i < ncycle; ++i) {
		edge_t* edge;
		if (cycles[i]->left_face == NULL) {
			cycles[i]->left_face = new_face(edge_list);
			cycles[i]->left_face->outer_component = cycles[i];
		}
		edge = cycles[i];
		do {
			edge->left_face = cycles[i]->left_face;
			edge = edge->succ;
		} while (edge != cycles[i]);
	}

	if (unbounded_face != NULL) {
		/* Classify faces as inside/outside */
		unbounded_face->is_inside = 0;
		find_inner_faces(unbounded_face);

		for (i = 0; i < ncycle; ++i) {
			if (cycles[i]->left_face->is_inside == -1)
				cycles[i]->left_face->is_inside = 0;
		}
	}

	free(cycles);/* no longer needed */
}

static void classify_component(edge_t* component)
{

	edge_t*	e;
	if (!component) return;

	e = component;
	if (e) do {
		vector_t	v;
		vector_t	v_in;
		vector_t	v_out;
		double		angle;
		double		phi;
		double		theta;
		int		x;
		int		y;

		v = e->origin->vec;
		v_in = e->pred->origin->vec;
		v_out = e->succ->origin->vec;

		phi = atan2(v_out.y - v.y,
				v_out.x - v.x);
		theta = atan2(v_in.y - v.y,
				v_in.x - v.x);
		if (phi < theta)
			angle = 2*PI + phi - theta;
		else
			angle = phi - theta;

		x = vec_above(v, v_in);
		y = vec_above(v, v_out);
		if (x && y) {

			if (angle > PI)
				e->origin->vtype = START_VERTEX;
			else
				e->origin->vtype = SPLIT_VERTEX;

		} else if (!x && !y) {

			if (angle > PI)
				e->origin->vtype = END_VERTEX;
			else
				e->origin->vtype = MERGE_VERTEX;

		} else {

			e->origin->vtype = REGULAR_VERTEX;

		}

		e = e->succ;
	} while (e != component);

}

/* Classify the vertices in an edge list.
 * Possible vertex classes are:
 *
 * START_VERTEX
 * SPLIT_VERTEX
 * END_VERTEX
 * MERGE_VERTEX
 * REGULAR_VERTEX
 *
 * This function should only be called if you
 * are using triangulate_debug.
 *
 */
static void classify_face(face_t* face)
{
	list_t*	p;
	classify_component(face->outer_component);

	p = face->inner_components;
	if (p) do {
		classify_component(p->data);
		p = p->succ;
	} while (p != face->inner_components);
}

static float cross2d(vector_t u, vector_t v)
{
	return u.x*v.y - u.y*v.x;
}

/*
 * a: p -> p+r
 * b: q -> q+s
 *
 * t = cross2d(q-p, s) / cross2d(r, s)
 * u = cross2d(q-p, r) / cross2d(r, s)
 */
static vector_t* seg_intersection(seg_t* a, seg_t* b)
{
	vector_t	p = a->origin->vec;
	vector_t	p2 = a->end->vec;
	vector_t	q = b->origin->vec;
	vector_t	q2 = b->end->vec;
	vector_t	r;
	vector_t	s;
	vector_t	q_p;
	float		n1;
	float		n2;
	float		d;

	r.x = p2.x - p.x;
	r.y = p2.y - p.y;
	s.x = q2.x - q.x;
	s.y = q2.y - q.y;
	q_p.x = q.x - p.x;
	q_p.y = q.y - p.y;

	n1 = cross2d(q_p, s);
	n2 = cross2d(q_p, r);
	d = cross2d(r, s);

	if (d == 0) {
		/* parallel */
		return NULL;
	} else {
		float		t = n1 / d;
		float		u = n2 / d;
		vector_t*	vec;

		if (t <= 0 || t >= 1 || u <= 0 || u >= 1)
			return NULL;

		vec = malloc(sizeof(vector_t));
		vec->x = p.x + t*r.x;
		vec->y = p.y + t*r.y;
		return vec;
	}
}

/* TODO: fix edge allocation */
static struct event* new_event(vector_t* x, seg_t* in1, seg_t* in2)
{
	struct event*	event = malloc(sizeof(struct event));
	seg_t*		out1 = malloc(sizeof(seg_t));
	seg_t*		out2 = malloc(sizeof(seg_t));

	event->vec = *x;
	event->in = NULL;
	event->out = NULL;

	out1->origin = event;
	out2->origin = event;

	out1->end = in1->end;
	out2->end = in2->end;

	list_remove_item(&in1->end->in, in1);
	list_remove_item(&in2->end->in, in2);

	in1->end = event;
	in2->end = event;

	list_add(&event->in, in1);
	list_add(&event->in, in2);

	list_add(&event->out, out1);
	list_add(&event->out, out2);

	list_add(&out1->end->in, out1);
	list_add(&out2->end->in, out2);

	return event;
}

/* s : seg_t*
 * e : event_t*
 */
static int seg_ends_at_event(void* e, void* x)
{
	seg_t*	s = x;
	return s->end == e;
}

static int seg_starts_at_event(void* e, void* x)
{
	seg_t*	s = x;
	return s->origin == e;
}

static int event_left_of_seg(void* x, void* y)
{
	seg_t*		s = y;
	struct event*	e = x;

	vector_t	s1 = s->origin->vec;
	vector_t	s2 = s->end->vec;

	float		xv = e->vec.x;
	float		yv = e->vec.y;
	float		xs1 = s1.x;
	float		ys1 = s1.y;
	float		xs2 = s2.x;
	float		ys2 = s2.y;

	float		t = xs1 + (xs2-xs1)*(yv-ys1)/(ys2-ys1);

	return xv < t;
}

static int event_right_of_seg(void* x, void* y)
{
	return !event_left_of_seg(x, y);
}

static int seg_left_of_seg(void* x, void* y)
{
	seg_t*		a = x;
	seg_t*		b = y;
	vector_t	ao = a->origin->vec;
	vector_t	bo = b->origin->vec;
	if (vec_above(bo, ao))
		return event_left_of_seg(a->origin, b);
	else
		return event_left_of_seg(a->end, b);
}

/* debug func */
void print_seg_tree(bstree_t* tree)
{
	if (!tree) return;
	if (tree->left == NULL && tree->right == NULL) {
		seg_t*	s = tree->value;
		printf("  %d, %d\n", s->origin->id, s->end->id);
	} else {
		print_seg_tree(tree->left);
		print_seg_tree(tree->right);
	}
}

static void remove_dup_event(struct event* e, struct event* dup)
{
	while (dup->in) {
		seg_t*	in = list_remove(&dup->in);
		in->end = e;
		list_add(&e->in, in);
	}
	while (dup->out) {
		seg_t*	out = list_remove(&dup->out);
		out->origin = e;
		list_add(&e->out, out);
	}
}

static void print_seg(seg_t* seg)
{
	printf("%d, %d\n", seg->origin->id, seg->end->id);
}

void print_seg_list(list_t* segs)
{
	list_t* p = segs;
	if (p) do {
		seg_t*	s = p->data;
		printf("  ");
		print_seg(s);
		p = p->succ;
	} while (p != segs);
}

#define EPSILON (1E-04f)

/* Removes extremities with area below a threshold
 */
static void remove_zero_area(struct event* e)
{
	list_t*	sorted = NULL;
	list_t*	p;
	list_t*	q;

	if (e->out == NULL)
		return;

	/* Build a sorted list of outgoing edges then iterate through the
	 * list and remove pairs that are too close
	 */

	p = e->out;
	do {
		seg_t*	s0 = p->data;
		int	append = 1;

		if ((q = sorted)) do {
			seg_t*	s = q->data;

			if (seg_left_of_seg(s0, s)) {
				list_add(&q, s0);
				if (q == sorted) {
					sorted = q->pred;
				}
				append = 0;
				break;
			}

			q = q->succ;
		} while (q != sorted);

		if (append) list_add(&sorted, s0);
		p = p->succ;
	} while (p != e->out);

#ifdef ZERO_AREA_DBG
	printf("e%d:\n", e->id);
	printf("sorted:\n");
	print_seg_list(sorted);
#endif
	p = sorted;
	do {
		seg_t*		s = p->data;
		vector_t	ds;
		float ls;
		ds.x = s->end->vec.x - s->origin->vec.x;
		ds.y = s->end->vec.y - s->origin->vec.y;
		ls = (float) sqrt(ds.x*ds.x + ds.y*ds.y);
		ds.x /= ls;
		ds.y /= ls;
		/* TODO: handle really short segments! (l ~= 0) */

		/* for each successor segment that is close to parallel:
		 * remove the successor and make all outgoing segments
		 * from it connected to this first segment
		 * When no more nearly parallel successors are found
		 * we iterate one more step in the outer loop
		 */
		q = p->succ;
		while (q != p && q != sorted) {
			seg_t*		t = q->data;
			float	lt;
			vector_t	dt;
			dt.x = t->end->vec.x - t->origin->vec.x;
			dt.y = t->end->vec.y - t->origin->vec.y;
			lt = (float) sqrt(dt.x*dt.x + dt.y*dt.y);
			dt.x /= lt;
			dt.y /= lt;

			if (fabsf(dt.x-ds.x) < EPSILON &&
					fabsf(dt.y-ds.y) < EPSILON) {

				/* t and q are effectively identical */
				if (s->end == t->end) {
#ifdef ZERO_AREA_DBG
					printf("--[1] ");
					print_seg(t);
#endif
					list_remove_item(&t->end->in, t);
					list_remove_item(&e->out, t);
					free(t);
					list_remove(&q);
				} else if (vec_above(s->end->vec,
							t->end->vec)) {
#ifdef ZERO_AREA_DBG
					printf("--[2] ");
					print_seg(t);
#endif
					t->origin = s->end;
					list_add(&s->end->out, t);
					list_remove_item(&e->out, t);
					list_remove(&q);
				} else {
#ifdef ZERO_AREA_DBG
					printf("--[3] ");
					print_seg(t);
#endif
					/* remove s and set s' = t */
					list_remove_item(&s->origin->out, s);
					s->origin = t->end;
					list_add(&t->end->out, s);

					if (p == sorted) {
						list_remove(&sorted);
						p = sorted;
					} else {
						list_remove(&p);
					}
					p = q;
					ds = dt;
					s = t;
				}
				continue;
			}

			q = q->succ;
		}

		p = p->succ;
	} while (p != sorted);

	free_list(&sorted);
}

void print_event_queue(list_t* eventq)
{
	list_t* p = eventq;
	if (p) do {
		struct event*	e = p->data;
		printf("  %d (%f, %f)\n", e->id, e->vec.x, e->vec.y);
		p = p->succ;
	} while (p != eventq);
}

static void remove_tail(struct event* e)
{
	if (e != NULL) {

		int	num_in = list_length(e->in);
		int	num_out = list_length(e->out);

		if (num_in == 1 && num_out == 0) {
			seg_t*	seg = list_remove(&e->in);
			list_remove_item(&seg->origin->out, seg);
			remove_tail(seg->origin);
		} else if (num_in == 0 && num_out == 1) {
			seg_t*	seg = list_remove(&e->out);
			list_remove_item(&seg->end->in, seg);
			remove_tail(seg->end);
		}
	}
}

static void insert_event(list_t* eventq, list_t* q, int* eventid,
		seg_t* inner, seg_t* outer)
{
	struct event*	ne;
	list_t*		r;

	if (inner && outer && inner->end != outer->end) {
		vector_t* x = seg_intersection(inner, outer);
		if (x != NULL) {
			ne = new_event(x, inner, outer);
			ne->id = (*eventid)++;
			free(x);
			/* insert new event in eventqueue */
			r = q;
			while (r != eventq) {
				struct event*	ev;
				r = r->succ;
				ev = r->data;
				if (vec_above(ne->vec, ev->vec)) {
					list_add(&r, ne);
					break;
				}
			}
		}
	}
}

/* Build a planar graph from a shape and return a doubly connected
 * edge list representation of that planar graph.
 */
edge_list_t* make_planar(shape_t* shape)
{
	struct event**	events;
	list_t*		vertices = NULL;
	list_t*		half_edges = NULL;
	int eventid = 0;
	int i;
	list_t*	eventq = NULL;/* TODO: use balanced binary search tree */
	int		nvert = 0;
	bstree_t*	status = NULL;/* TODO: use balanced tree? */
	list_t*		q;
	edge_list_t*	edge_list;
	list_t*		segments = NULL;
	list_t*		p;

	events = malloc(sizeof(struct event*) * shape->nvec);
	for (i = 0; i < shape->nvec; ++i) {
		events[i] = malloc(sizeof(struct event));
		events[i]->vec = shape->vec[i];
		events[i]->in = NULL;
		events[i]->out = NULL;
		events[i]->id = eventid++;
		events[i]->vertex = NULL;
	}

#ifdef MAKE_PLANAR_DBG
	printf("segments:\n");
#endif
	for (i = 0; i < shape->nseg; ++i) {
		int		i1 = shape->seg[i*2];
		int		i2 = shape->seg[i*2+1];
		vector_t	v1 = shape->vec[i1];
		vector_t	v2 = shape->vec[i2];

		seg_t*	seg = malloc(sizeof(seg_t));
		if (vec_above(v1, v2)) {
			seg->origin = events[i1];
			seg->end = events[i2];
		} else {
			seg->end = events[i1];
			seg->origin = events[i2];
		}

		list_add(&segments, seg);

		list_add(&seg->end->in, seg);
		list_add(&seg->origin->out, seg);

#ifdef MAKE_PLANAR_DBG
		printf("  ");
		print_seg(seg);
#endif
	}

	qsort(events, shape->nvec, sizeof(struct event*), event_above);

#ifdef MAKE_PLANAR_DBG
	printf("sorted events:\n");
	for (i = 0; i < shape->nvec; ++i) {
		struct event*	e = events[i];
		printf("  %d (%f, %f)\n", e->id, e->vec.x, e->vec.y);
	}
#endif

	/* Remove duplicate events */
	for (i = 0; i < shape->nvec-1; i += 1) {
		struct event*	ei = events[i];

		do {
			int		j = i + 1;
			struct event*	ej = events[j];

			if (vec_close(ei->vec, ej->vec)) {
				remove_dup_event(ei, ej);
				events[j] = NULL;
			} else {
				break;
			}

			i = j;
		} while (i < shape->nvec-1);
	}

	/* Remove tails */
	for (i = 0; i < shape->nvec; ++i) {
		struct event*	e = events[i];

		remove_tail(e);
	}

	for (i = 0; i < shape->nvec; ++i) {
		struct event*	e = events[i];

		if (e == NULL)
			continue;

		/* remove extremities with small area */
		remove_zero_area(e);

		if (e->in == NULL && e->out == NULL) {
			free(e);
			continue;
		} else if (e->in == NULL && e->out == e->out->succ) {
			seg_t*	s = list_remove(&e->out);
			list_remove_item(&s->end->in, s);
			free(s);
			free(e);
			continue;
		}

		list_add(&eventq, e);
	}

	/* Event array no longer needed */
	free(events);

#ifdef MAKE_PLANAR_DBG
	printf("eventq:\n");
	print_event_queue(eventq);
#endif

	/* Use sweep line to find intersections and place new vertices at
 	 * intersection points.
 	 *
 	 * Simultaneously we build the doubly connected edge list.
 	 */
	q = eventq;
	if (q) do {
		struct event*	e = q->data;
		seg_t*		inner_left = NULL;
		seg_t*		inner_right = NULL;
		seg_t*		outer_left = NULL;
		seg_t*		outer_right = NULL;
		list_t*		in = NULL;
		list_t*		out = NULL;
		
#ifdef MAKE_PLANAR_DBG
		printf("e%d:\n", e->id);
#endif

		bstree_find_all(status, e, &seg_ends_at_event, &in);

		/* remove edges ending at current event point from status */
		bstree_remove_if(&status, e, &seg_ends_at_event);

		outer_left = bstree_find_left(status, e, event_left_of_seg);
		outer_right = bstree_find_right(status, e, event_right_of_seg);

		/* add outgoing segments to status */
		if ((p = e->out)) do {
			seg_t*	seg = p->data;

			/* insert the segment in the status */
			bstree_insert(&status, seg, seg_left_of_seg);

			if (!inner_left) {
				inner_left = seg;
				inner_right = seg;
			} else {
				if (seg->end->vec.x < inner_left->end->vec.x) {
					inner_left = seg;
				} else if (seg->end->vec.x > inner_right->end->vec.x) {
					inner_right = seg;
				}
			}

			p = p->succ;
		} while (p != e->out);

		bstree_find_all(status, e, &seg_starts_at_event, &out);

		/* create vertex for this event */
		e->vertex = new_vertex();
		e->vertex->vec = e->vec;
		e->vertex->id = e->id;
		list_add(&vertices, e->vertex);

		/* create outgoing edges with twins */
		if ((p = out)) do {
			seg_t*	s = p->data;
			s->edge = new_edge();
			s->edge->twin = new_edge();
			s->edge->twin->twin = s->edge;
			list_add(&half_edges, s->edge);
			list_add(&half_edges, s->edge->twin);
			p = p->succ;
		} while (p != out);

		/* Use incoming and outgoing lists to link half-edges */
		if (in != NULL && out != NULL) {
			seg_t*	upper_left = in->data;
			seg_t*	upper_right = in->pred->data;
			seg_t*	lower_left = out->data;
			seg_t*	lower_right = out->pred->data;

			edge_t*	u_down = upper_right->edge;
			edge_t*	u_up = upper_left->edge->twin;
			edge_t* l_down = lower_right->edge;
			edge_t*	l_up = lower_left->edge->twin;

			/* link outside edges */
			link_edges(e->vertex, l_up, u_up);
			link_edges(e->vertex, u_down, l_down);

		} else if (in != NULL) {
			seg_t*	upper_left = in->data;
			seg_t*	upper_right = in->pred->data;

			edge_t*	u_up = upper_left->edge->twin;
			edge_t*	u_down = upper_right->edge;

			/* link outside edges */
			link_edges(e->vertex, u_down, u_up);

		} else if (out != NULL) {
			seg_t*	lower_left = out->data;
			seg_t*	lower_right = out->pred->data;

			edge_t*	l_down = lower_right->edge;
			edge_t*	l_up = lower_left->edge->twin;

			/* link outside edges */
			link_edges(e->vertex, l_up, l_down);
		}

		/* Link inner edges */
		if (in != NULL) {
			p = in;
			while (p->succ != in) {
				seg_t*	s0 = p->data;
				seg_t*	s1 = p->succ->data;
				edge_t*	down = s0->edge;
				edge_t*	up = s1->edge->twin;
				link_edges(e->vertex, down, up);
				p = p->succ;
			}
		}
		if (out != NULL) {
			p = out;
			while (p->succ != out) {
				seg_t*	s0 = p->data;
				seg_t*	s1 = p->succ->data;
				edge_t*	down = s0->edge;
				edge_t*	up = s1->edge->twin;
				link_edges(e->vertex, up, down);
				p = p->succ;
			}
		}

		if ((p = in)) do {
			seg_t*	s = p->data;
			edge_t*	e = s->edge;
			assert(e->succ != NULL);
			assert(e->pred != NULL);
			assert(e->twin->succ != NULL);
			assert(e->twin->pred != NULL);
			p = p->succ;
		} while (p != in);

		if ((p = out)) do {
			seg_t*	s = p->data;
			edge_t*	e = s->edge;
			assert(e->pred != NULL);
			assert(e->twin->succ != NULL);
			p = p->succ;
		} while (p != out);

		free_list(&in);
		free_list(&out);

		/* Add intersection vertices */
		insert_event(eventq, q, &eventid, inner_left, outer_left);
		insert_event(eventq, q, &eventid, inner_right, outer_right);

#ifdef MAKE_PLANAR_DBG
		print_seg_tree(status);
#endif

		q = q->succ;
	} while (q != eventq);

	/* Free event queue */
	while (eventq) {
		struct event*	event = list_remove(&eventq);

		if (event->in != NULL)
			free_list(&event->in);

		if (event->out != NULL)
			free_list(&event->out);

		free(event);
	}

	/* Free segments */
	if ((p = segments)) do {
		free(p->data);
		p = p->succ;
	} while (p != segments);
	free_list(&segments);

#ifdef MAKE_PLANAR_DBG
	printf("remaining status size: %d\n", bstree_size(status));
	print_seg_tree(status);
#else
	assert(status == NULL);
#endif

	/* Insert the half-edges and vertices created during the
 	 * line sweep pass into an edge list.
 	 */

	nvert = list_length(vertices);
	edge_list = malloc(sizeof(edge_list_t));
	edge_list->nvert = 0;
	edge_list->vertices = NULL;
	edge_list->faces = NULL;
	edge_list->etree = NULL;
	edge_list->edges = NULL;
	edge_list->cycles = NULL;

	edge_list->nvert = nvert;
	edge_list->vertices = malloc(sizeof(vertex_t*)*nvert);
	edge_list->edges = half_edges;

	/* TODO: try to preserve vertex IDs */
	if (vertices) {
		int	i = 0;
		list_t*	p = vertices;
		do {
			vertex_t*	v = p->data;
			v->id = i;
			edge_list->vertices[i] = v;
			i += 1;
			p = p->succ;
		} while (p != vertices);
		free_list(&vertices);
	}

	if (half_edges) {
		list_t*	p = half_edges;
		do {
			edge_t*	e = p->data;
			e->origin->incident_edge = e;
			assert(e->succ != NULL);
			p = p->succ;
		} while (p != half_edges);
	}

	/* sort vertices */
	qsort(edge_list->vertices, nvert, sizeof(vertex_t*), vertex_above);

	return edge_list;
}

/* If the vertices have been classified, the doubly
 * connected edge list can then be triangulated.
 *
 * Triangulation consists of two steps:
 * 1. Divide the polygon into monotone polygons
 * 2. Triangulate the monotone polygons
 *
 * Both steps work by inserting diagonals into the polygon.
 */
edge_list_t* triangulate(shape_t* shape)
{
	/* 1. Construct edge list for the planar graph */
	edge_list_t*	edge_list = make_planar(shape);
	list_t* faces = NULL;
	list_t* p;

	/* 2. Identify and connect the components in the edge list */
	connect_components(edge_list);

	/* 3. For each face in the polygon, triangulate the face if
	 * it is part of the inside of the polygon
	 */
	faces = NULL;
	p = edge_list->faces;
	if (p) do {
		face_t*	face = p->data;
		p = p->succ;

		if (face->is_inside) {
			list_add(&faces, face);
		}
	} while (p != edge_list->faces);

	while (faces) {
		list_t*	worklist;
		face_t*	face = (face_t*)list_remove(&faces);
		int i;

		classify_face(face);
		align_face_vertices(face);

		worklist = NULL;
		for (i = 0; i < edge_list->nvert; ++i) {
			vertex_t*	v = edge_list->vertices[i];

			if (v->incident_edge != NULL &&
					v->incident_edge->left_face == face) {

				list_add(&worklist, v);
			}
		}

		while (worklist) {
			vertex_t*	v = list_remove(&worklist);

			switch (v->vtype) {
			case START_VERTEX:
				handle_start_vertex(edge_list, v);
				break;
			case END_VERTEX:
				handle_end_vertex(edge_list, v);
				break;
			case SPLIT_VERTEX:
				handle_split_vertex(edge_list, v);
				break;
			case MERGE_VERTEX:
				handle_merge_vertex(edge_list, v);
				break;
			case REGULAR_VERTEX:
				handle_regular_vertex(edge_list, v);
				break;
			case UNCLASSIFIED_VERTEX:
				fprintf(stderr, "error: unclassified "
						"vertex in triangulation "
						"input!");
				exit(1);
			}
		}
	}

	/* 5. Triangulate each monotone polygon
	 * remove hole faces and create worklist
	 */
	if (edge_list->faces) {
		list_t*	p = edge_list->faces;
		do {
			face_t*	face = p->data;
			if (face->is_inside) {
				triangulate_face(edge_list, face);
			}
			p = p->succ;
		} while (p != edge_list->faces);
	}

	return edge_list;
}

/* Triangulate a monotone polygon given as a vertex chain
 * beginning at the 'start' edge.
 *
 * If stepping is 1, the algorithm will wait for a pthread cond
 * before each iteration.
 */
void triangulate_face(edge_list_t* edge_list, face_t* face)
{
	stack_t*	stack = NULL;
	int		nvert = 0;
	int		last;
	int		v_type;
	vertex_t**	vertices;
	edge_t*		p;
	edge_t*		start = NULL;
	int i;

	assert(face != NULL);

	if (face->outer_component == NULL)
		return;

	/* count vertices */

	p = face->outer_component;
	do {
		vector_t	v = p->origin->vec;
		vector_t	v_out = p->succ->origin->vec;
		vector_t	v_in = p->pred->origin->vec;
		int		above1 = vec_above(v, v_in);
		int		above2 = vec_above(v, v_out);

		nvert += 1;

		p->origin->incident_edge = p;
		p->origin->flags &= ~VERTEX_UP;
		p->origin->flags &= ~VERTEX_DOWN;
		if (above1 && above2) {
			p->origin->vtype = START_VERTEX;
			start = p;
		} else if (!above1 && !above2) {
			p->origin->vtype = END_VERTEX;
		} else {
			p->origin->vtype = REGULAR_VERTEX;
		}

		p = p->succ;
	} while (p != face->outer_component);

	/* it's already a triangle (or something weird) */
	if (nvert <= 3)
		return;

	assert(start != NULL);

	/* create vertex array */
	vertices = malloc(sizeof(vertex_t*)*nvert);
	v_type = VERTEX_DOWN;
	last = 0;
	p = start;
	do {
		vertices[last++] = p->origin;
		if (p->origin->vtype == END_VERTEX) {
			v_type = VERTEX_UP;
		} else {
			p->origin->flags |= v_type;
		}
		p = p->succ;
	} while (p != start);

	/* sort vertices */
	qsort_verts(vertices, nvert);

	/* triangulate the monotone face */
	stack_push(&stack, vertices[0]);
	stack_push(&stack, vertices[1]);
	for (i = 2; i < nvert; ++i) {
		vertex_t*	v;
		vertex_t*	top;
		int		flag1;
		int		flag2;

		v = vertices[i];
		top = stack_peek(stack);

		flag1 = v->flags & (VERTEX_UP | VERTEX_DOWN);
		flag2 = top->flags & (VERTEX_UP | VERTEX_DOWN);
		if (flag1 != flag2) {

			/* v and vertex on top of the stack
			 * are not on the same chain
			 * pop all vertices on the stack
			 * and insert a diagonal from v to
			 * each of them
			 */

			if (v->vtype == END_VERTEX)
				stack_pop(&stack);

			/* pop all except the last vertex */
			while (stack->next != NULL) {
				vertex_t*	this = stack_pop(&stack);
				add_diagonal(edge_list, this, v);
			}
			stack_pop(&stack);
			stack_push(&stack, vertices[i-1]);
			stack_push(&stack, v);

		} else {
			/* v is on the same chain as the vertex
			 * on the top of the stack
			 * insert diagonals only if the diagonal
			 * is inside the shape - we test this using the angle
			 * between u_i->S_j and u_i->S_j-1 (must be >= 0)
			 */
			
			int up = v->flags & VERTEX_UP;
			while (stack != NULL) {
				vertex_t*	prev;
				vertex_t*	peek;
				double phi;

				prev = stack_pop(&stack);
				if (stack == NULL) {
					stack_push(&stack, prev);
					break;
				}
				peek = stack_peek(stack);
				phi = angle_between(
						angle(v->vec, peek->vec),
						angle(v->vec, prev->vec));
				if ((up && phi < PI) || (!up && phi > PI)) {
					add_diagonal(edge_list, peek, v);
				} else {
					stack_push(&stack, prev);
					break;
				}
			}
			stack_push(&stack, v);
		}
	}

	/* free temporary data */
	free_stack(&stack);
	free(vertices);
}

void handle_start_vertex(edge_list_t* edge_list, vertex_t* v)
{
	edge_t*		e = v->incident_edge;

	bstree_insert(&edge_list->etree, e, edge_left_of_edge);
	set_helper(e, v);
}

void handle_end_vertex(edge_list_t* edge_list, vertex_t* v)
{
	edge_t*		e_1 = v->incident_edge->pred;

	if (helper(e_1)->vtype == MERGE_VERTEX) {
		add_diagonal(edge_list, helper(e_1), v);
	}
	bstree_remove(&edge_list->etree, e_1, edge_left_of_edge);
}

void handle_split_vertex(edge_list_t* edge_list, vertex_t* v)
{
	edge_t*		e = v->incident_edge;
	edge_t*		e_j = bstree_find_left(edge_list->etree, v, vertex_left_of_edge);

	add_diagonal(edge_list, helper(e_j), v);
	set_helper(e_j, v);

	bstree_insert(&edge_list->etree, e, edge_left_of_edge);
	set_helper(e, v);
}

void handle_merge_vertex(edge_list_t* edge_list, vertex_t* v)
{
	edge_t*		e_1 = v->incident_edge->pred;
	edge_t*		e_j;

	if (helper(e_1)->vtype == MERGE_VERTEX) {
		add_diagonal(edge_list, helper(e_1), v);
	}
	bstree_remove(&edge_list->etree, e_1, edge_left_of_edge);

	e_j = bstree_find_left(edge_list->etree, v, vertex_left_of_edge);
	if (helper(e_j)->vtype == MERGE_VERTEX) {
		add_diagonal(edge_list, helper(e_j), v);
	}
	set_helper(e_j, v);
}

void handle_regular_vertex(edge_list_t* edge_list, vertex_t* v)
{
	edge_t*		e = v->incident_edge;

	if (!vec_above(e->twin->origin->vec, v->vec)) {
		/* interior of polygon is to the right of v */
		edge_t*		e_1 = e->pred;

		if (helper(e_1)->vtype == MERGE_VERTEX) {
			add_diagonal(edge_list, helper(e_1), v);
		}
		bstree_remove(&edge_list->etree, e_1, edge_left_of_edge);
		bstree_insert(&edge_list->etree, e, edge_left_of_edge);
		set_helper(e, v);
	} else {
		/* interior of polygon is to the left of v */
		edge_t*		e_j = bstree_find_left(edge_list->etree, v, vertex_left_of_edge);

		if (helper(e_j)->vtype == MERGE_VERTEX) {
			add_diagonal(edge_list, helper(e_j), v);
		}
		set_helper(e_j, v);
	}
}

/* Add an edge (diagonal) between v1 and v2
 * v1 should be above v2
 */
static void add_diagonal(edge_list_t* edge_list, vertex_t* v1, vertex_t* v2)
{
	double	amin;
	double	theta;
	double	phi;
	int	closed;
	edge_t*	p;
	edge_t*	h;
	edge_t*	v1_in;
	edge_t*	v1_out;
	edge_t*	v2_in;
	edge_t*	v2_out;
	edge_t*	up;
	edge_t*	down;
	face_t*	face;

	assert(vec_above(v1->vec, v2->vec));

	/* the angle of vector v1->v2 */
	phi = atan2(v2->vec.y - v1->vec.y,
			v2->vec.x - v1->vec.x);

	/* find edge which has least angle from v1->v2 at v1 */
	amin = 2*PI+1;
	p = h = v1->incident_edge;
	assert(p != NULL);
	do {
		double		angle;
		edge_t*		e = p;
		vector_t	vx = e->twin->origin->vec;
		p = p->twin->succ;

		/* angle vx.v1 */
		theta = atan2(vx.y - v1->vec.y,
				vx.x - v1->vec.x);
		if (phi < theta)
			angle = 2*PI + phi - theta;
		else
			angle = phi - theta;
		
		if (angle < amin) {
			if (angle == 0) return;
			amin = angle;
			v1_out = e;
			v1_in = e->pred;
		}

	} while (p != h);
	
	/* the angle of vector v2->v1 */
	phi = atan2(v1->vec.y - v2->vec.y,
			v1->vec.x - v2->vec.x);

	/* find edge which has least angle from v2->v1 at v2 */
	amin = 2*PI+1;
	p = h = v2->incident_edge;
	assert(p != NULL);
	do {
		double		angle;
		edge_t*		e = p;
		vector_t	vx = e->twin->origin->vec;
		p = p->twin->succ;

		theta = atan2(vx.y - v2->vec.y,
				vx.x - v2->vec.x);
		if (phi < theta)
			angle = 2*PI + phi - theta;
		else
			angle = phi - theta;
		
		if (angle < amin) {
			if (angle == 0) return;
			amin = angle;
			v2_out = e;
			v2_in = e->pred;
		}

	} while (p != h);

	/* we now know the original bounded face */
	face = v1_out->left_face;

	/* are the two vertex chains connected? */
	closed = 0;
	p = h = v1_out;
	do {
		if (p == v2_out) {
			closed = 1;
			break;
		}
		p = p->succ;
	} while (p != h);

	up = new_edge();
	list_add(&edge_list->edges, up);
	up->origin = v2;
	up->left_face = face;
	up->pred = v2_in;
	v2_in->succ = up;
	up->succ = v1_out;
	v1_out->pred = up;

	down = new_edge();
	list_add(&edge_list->edges, down);
	down->origin = v1;
	down->left_face = face;
	down->pred = v1_in;
	v1_in->succ = down;
	down->succ = v2_out;
	v2_out->pred = down;

	up->twin = down;
	down->twin = up;

	/* are we creating a new face? */
	if (closed) {
		/* yes - create a new face */
		face_t*	nface = new_face(edge_list);
		nface->is_inside = face->is_inside;
		nface->outer_component = up;

		/* update incident face for all edges on the chain */
		set_left_face(up, nface);

		face->outer_component = down;

	} else {
		/* all edges are connected - update incident face
		 * TODO: perhaps we don't need to loop through the whole
		 * edge list, if we update incident edge in a smarter way
		 * when a new face is created....
		 */
		set_left_face(up, face);
	}
}

/* Returns 1 if v is left of e
 */
static int vertex_left_of_edge(void* v, void* e)
{
	/* get the x-coordinate where e crosses the line y=v.y */
	vector_t	a = ((vertex_t*)v)->vec;
	vector_t	b1 = ((edge_t*)e)->origin->vec;
	vector_t	b2 = ((edge_t*)e)->twin->origin->vec;

	/* e is horizontal */
	/*if (b2.y - b1.y == 0)
		return 1;*/

	float		x = b1.x + (b2.x-b1.x)*(a.y-b1.y)/(b2.y-b1.y);

	return a.x < x;
}

/* Returns 1 if a is left of b:
 * * both edges share a common origin
 * * origin(a) is above origin(b) and origin(b) is not left of a
 * * origin(b) is below origin(a) and origin(a) is left of b
 */
static int edge_left_of_edge(void* x, void* y)
{
	edge_t*	a = x;
	edge_t*	b = y;

	vector_t	ao = a->origin->vec;
	vector_t	bo = b->origin->vec;

	int result = 0;
	if (vec_above(bo, ao)) {
		result = vertex_left_of_edge(a->origin, b);
	} else {
		result = vertex_left_of_edge(a->twin->origin, b);
	}

#if 0
	printf("left of:\n");
	printf("  ");print_edge(x);
	printf("  ");print_edge(y);
	printf("=>%s\n", result ? "true" : "false");
#endif

	return result;
}
