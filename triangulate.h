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
/*
 * Polygon triangulation.
 *
 * See triangulation.c for details and limitations
 * for the algorithm used.
 */
#ifndef CTTF_TRIANGULATE_H
#define CTTF_TRIANGULATE_H

#include "shape.h"
#include "list.h"
#include "bstree.h"

typedef struct edge		edge_t;
typedef struct vertex		vertex_t;
typedef struct face		face_t;
typedef struct edge_list	edge_list_t;
typedef struct segment		seg_t;
typedef struct segment_tree	stree_t;

edge_list_t* triangulate(shape_t* shape);

void free_edgelist(edge_list_t** edge_list);

/* above-ness relation between vectors */
int vec_above(vector_t v1, vector_t v2);

edge_list_t* make_planar(shape_t* shape);
void connect_components(edge_list_t* edge_list);
void triangulate_face(edge_list_t* edge_list, face_t* face);

typedef enum vtype {
	/* The highest vertex of a monotone polygon */
	START_VERTEX,

	/* The lowest vertex of a monotone polygon */
	END_VERTEX,

	/* A vertex where two monotone polygons separate */
	SPLIT_VERTEX,

	/* A vertex where two monotone polygons meet */
	MERGE_VERTEX,

	/* A regular vertex */
	REGULAR_VERTEX,

	UNCLASSIFIED_VERTEX,
} vtype_t;

struct face {
	list_t*		inner_components;
	edge_t*		outer_component;
	int		is_inside;
};

struct edge {
	vertex_t*	origin;
	face_t*		left_face;
	edge_t*		twin;
	edge_t*		succ;
	edge_t*		pred;
	vertex_t*	helper;
	int		cycle;
};

struct vertex {
	vtype_t		vtype;/* vertex type */
	vector_t	vec;
	edge_t*		incident_edge;

	/* flags for internal state of triangulation algorithm */
	int		flags;

	/* for debugging */
	int		id;
};

struct edge_list {
	int		nvert;
	vertex_t**	vertices;
	list_t*		faces;
	list_t*		cycles;
	bstree_t*	etree;

	/* for convenience - makes freeing memory easier */
	list_t*		edges;
};

/* Sweep line event points */
struct event {
	vector_t	vec;
	list_t*		in;/* segments entering (above) this point */
	list_t*		out;/* segments leaving (below) this point */
	int		id;
	vertex_t*	vertex;
};

struct segment {
	struct event*	origin;
	struct event*	end;
	edge_t*		edge;
};

#endif

