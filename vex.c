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
/**
 * vex - Vector Editor
 */
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <errno.h>
#include <float.h>

#include "shape.h"
#include "list.h"

struct vec;
struct seg;

struct vec {
	float		x;
	float		y;
	int		vindex;
	list_t*		segs;
};

struct seg {
	struct vec*	origin;
	struct vec*	end;
};

static list_t*	vecs;
static list_t*	segs;

static int		shift = 0;
static struct vec*	pred = NULL;
static struct vec*	first = NULL;
static float		cx = -1;
static float		cy = -1;
static float		xoffset = 0.5f;
static float		yoffset = 0.5f;
static float		zoom = 1;

// I/O streams
static FILE*	in = NULL;
static FILE*	out = NULL;

SDL_Surface*	screen;
static bool	running = true;

#define WINDOW_W (700)
#define WINDOW_H (700)

static void setup_video();
static void render();
static void handle_event(SDL_Event* event);
static void handle_key(SDLKey key, int down);
static void on_left_click(float x, float y);
static void on_right_click(float x, float y);
static void on_middle_click(float x, float y);
static void print_help();
static void connect_vertex(float x, float y);
static list_t* closest_vertex(float x, float y);
static struct vec* add_vec(float x, float y);
static struct seg* add_seg(struct vec* a, struct vec* b);
static void remove_segment(struct seg* seg);
static void remove_component(struct vec* vec);
static float from_screen_x(int x);
static float from_screen_y(int y);
static void fit_view_to_shape(shape_t* shape);

int main(int argc, char* argv[])
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	atexit(SDL_Quit);

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-h")) {
				print_help();
				exit(0);
			} else {
				fprintf(stderr, "illegal option: %s\n",
						argv[i]);
				print_help();
				exit(1);
			}
		} else if (!in) {
			in = fopen(argv[1], "r");
			if (!in) {
				fprintf(stderr, "could not open file %s (%s)\n",
						argv[1], strerror(errno));
			}
		} else {
			fprintf(stderr, "too many command line parameters\n");
			print_help();
			exit(1);
		}
	}

	if (in) {
		shape_t* shape = load_shape(in);
		if (!shape) {
			fprintf(stderr, "could not load shape!\n");
			exit(1);
		}

		fit_view_to_shape(shape);

		struct vec** vecv = malloc(sizeof(struct vec)*shape->nvec);

		for (int i = 0; i < shape->nvec; ++i) {
			vecv[i] = malloc(sizeof(struct vec));
			vecv[i]->x = shape->vec[i].x;
			vecv[i]->y = shape->vec[i].y;
			vecv[i]->segs = NULL;
			list_add(&vecs, vecv[i]);
		}

		for (int i = 0; i < shape->nseg; ++i) {
			struct seg*	seg = malloc(sizeof(struct seg));
			seg->origin = vecv[shape->seg[i*2]];
			seg->end = vecv[shape->seg[i*2+1]];
			list_add(&seg->origin->segs, seg);
			list_add(&seg->end->segs, seg);
			list_add(&segs, seg);
		}

		free_shape(&shape);
	}

	if (!out) out = stdout;

	setup_video();

	while (running) {
		SDL_Event event;

		render();

		SDL_GL_SwapBuffers();
		while (SDL_PollEvent(&event)) {
			handle_event(&event);
		}
	}

	// write shape
	int	index = 0;
	list_t*	p;
	list_t*	h;
	p = h = vecs;
	if (p)
	do {
		struct vec*	vec = p->data;
		p = p->succ;

		vec->vindex = index++;
		fprintf(out, "v: %f, %f\n", vec->x, vec->y);
	} while (p != h);
	p = h = segs;
	if (p)
	do {
		struct seg*	seg = p->data;
		p = p->succ;

		fprintf(out, "s: %d, %d\n",
				seg->origin->vindex,
				seg->end->vindex);
	} while (p != h);

	return 0;
}

static void fit_view_to_shape(shape_t* shape)
{
	// calculate center coordinates
	float xmin = FLT_MAX;
	float xmax = FLT_MIN;
	float ymin = FLT_MAX;
	float ymax = FLT_MIN;
	for (int i = 0; i < shape->nvec; ++i) {
		vector_t v = shape->vec[i];
		if (v.x < xmin)
			xmin = v.x;
		if (v.x > xmax)
			xmax = v.x;
		if (v.y < ymin)
			ymin = v.y;
		if (v.y > ymax)
			ymax = v.y;
	}
	if (shape->nvec > 1) {
		xoffset = (xmax+xmin)/2;
		yoffset = (ymax+ymin)/2;
		zoom = xmax-xmin;
		if (ymax-ymin > xmax-xmin)
			zoom = ymax-ymin;
		zoom *= 1.2;
	}
}

void print_help()
{
	printf("usage: ftest [OPTION] [FILE]\n");
	printf("  where FILE is a shape file to edit\n");
	printf("  OPTION may be one of\n");
	printf("    -h         print help\n");
	//printf("    -o TARGET  route output to TARGET\n");
}

void setup_video()
{
	uint32_t	flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL;

	screen = SDL_SetVideoMode(WINDOW_W, WINDOW_H, 0, flags);
	if (!screen) {
		fprintf(stderr, "Failed to set video mode");
		exit(1);
	}

	SDL_ShowCursor(SDL_ENABLE);

	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glColor3f(1.f, 1.f, 1.f);

}

void render()
{
	list_t*	p;

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1/zoom, 1/zoom, 1);
	glTranslatef(-xoffset+zoom/2, -yoffset+zoom/2, 0);

	glColor3f(0.23, 0.43, 0.87);
	glBegin(GL_QUADS);
	p = vecs;
	if (p)
	do {
		struct vec*	vec = p->data;
		float	x1 = vec->x - 0.006*zoom;
		float	x2 = vec->x + 0.006*zoom;
		float	y1 = vec->y - 0.006*zoom;
		float	y2 = vec->y + 0.006*zoom;

		glBegin(GL_QUADS);
		glVertex3f(x1, y1, 0);
		glVertex3f(x1, y2, 0);
		glVertex3f(x2, y2, 0);
		glVertex3f(x2, y1, 0);
		glEnd();

		p = p->succ;
	} while (p != vecs);
	glEnd();

	glColor3f(1, 1, 1);
	glBegin(GL_LINES);
	p = segs;
	if (p)
	do {
		struct seg*	seg = p->data;
		p = p->succ;

		glVertex3f(seg->origin->x, seg->origin->y, 0);
		glVertex3f(seg->end->x, seg->end->y, 0);
	} while (p != segs);
	glEnd();

	if (pred) {
		glColor3f(0.94f, 0.44f, 0.44f);
		glBegin(GL_LINES);
		glVertex3f(pred->x, pred->y, 0.f);
		glVertex3f(cx, cy, 0.f);
		glEnd();
	}
}

static float from_screen_x(int x) {
	return xoffset - zoom/2 + zoom * x / (float) WINDOW_W;
}

static float from_screen_y(int y) {
	return yoffset - zoom/2 + zoom * (WINDOW_H - y) / (float) WINDOW_H;
}

void handle_event(SDL_Event* event)
{
	float mx;
	float my;
	switch(event->type) {
		case SDL_MOUSEBUTTONDOWN:
			mx = from_screen_x(event->motion.x);
			my = from_screen_y(event->motion.y);
			if (event->button.button == SDL_BUTTON_LEFT)
				on_left_click(mx, my);
			else if (event->button.button == SDL_BUTTON_MIDDLE)
				on_middle_click(mx, my);
			else if (event->button.button == SDL_BUTTON_RIGHT)
				on_right_click(mx, my);
			else if (event->button.button == SDL_BUTTON_WHEELUP)
				zoom /= 1.2f;
			else if (event->button.button == SDL_BUTTON_WHEELDOWN)
				zoom *= 1.2f;
			break;
		case SDL_MOUSEMOTION:
			cx = from_screen_x(event->motion.x);
			cy = from_screen_y(event->motion.y);
			break;
		case SDL_KEYDOWN:
			handle_key(event->key.keysym.sym, 1);
			break;
		case SDL_KEYUP:
			handle_key(event->key.keysym.sym, 0);
			break;
		default:;
	}
}

struct vec* add_vec(float x, float y)
{
	struct vec*	vec = malloc(sizeof(struct vec));
	
	vec->x = x;
	vec->y = y;
	vec->segs = NULL;
	list_add(&vecs, vec);

	return vec;
}

struct seg* add_seg(struct vec* a, struct vec* b)
{
	struct seg*	seg = malloc(sizeof(struct seg));

	seg->origin = a;
	seg->end = b;
	list_add(&seg->origin->segs, seg);
	list_add(&seg->end->segs, seg);
	list_add(&segs, seg);
	return seg;
}

/* Add a new segment to the component, or
 * create a new component.
 */
void on_left_click(float x, float y)
{
	if (shift) {
		connect_vertex(x, y);
	} else {
		struct vec*	vec =
			add_vec(x, y);

		if (pred) add_seg(pred, vec);

		if (!first) first = vec;

		pred = vec;
	}
}

/* Connect to closest vertex
 */
void connect_vertex(float x, float y)
{
	list_t*	closest = closest_vertex(x, y);

	if (!pred) {
		pred = closest->data;
	} else {
		add_seg(pred, closest->data);
		pred = NULL;
	}
	first = NULL;
}

/* Close the current component
 */
void on_right_click(float x, float y)
{
	if (first) {
		add_seg(pred, first);
		first = NULL;
		pred = NULL;
	}
}

void print_closest_vertex(float x, float y)
{
	struct vec*	closest = NULL;
	int		id = -1;
	int		i = 0;
	float		min_dist = 3;
	list_t*		p;

	if ((p = vecs)) do {
		struct vec*	vec = p->data;
		float		dx;
		float		dy;
		float		dist;

		dx = vec->x - x;
		dy = vec->y - y;
		dist = dx*dx + dy*dy;
		if (dist < min_dist) {
			min_dist = dist;
			closest = vec;
			id = i;
		}
		p = p->succ;
		i += 1;
	} while (p != vecs);

	printf("v: %d (%f, %f)\n", closest ? id : -1,
			closest->x, closest->y);
}

/* Find the list item for the vertex closest
 * to the given point (x, y)
 */
list_t* closest_vertex(float x, float y)
{
	list_t*	closest = NULL;
	float	min_dist = 3;
	list_t*	p;
	p = vecs;
	if (p)
	do {
		struct vec*	vec = p->data;
		float		dx;
		float		dy;
		float		dist;

		dx = vec->x - x;
		dy = vec->y - y;
		dist = dx*dx + dy*dy;
		if (dist < min_dist) {
			min_dist = dist;
			closest = p;
		}
		p = p->succ;
	} while (p != vecs);
	return closest;
}

/* Delete vertex closest to cursor
 */
void on_middle_click(float x, float y)
{
	list_t*	closest = closest_vertex(x, y);
	if (closest) {
		struct vec*	vec = closest->data;

		while (vec->segs) {
			struct seg*	seg = vec->segs->data;
			list_remove_item(&seg->origin->segs, seg);
			list_remove_item(&seg->end->segs, seg);
			list_remove_item(&segs, seg);
			free(seg);
		}

		list_remove_item(&vecs, vec);
		free(vec);
	}
}

void handle_key(SDLKey key, int down)
{
	list_t* closest;

	switch(key) {
		case SDLK_1:
			if (down) {
				print_closest_vertex(cx, cy);
			}
			break;
		case SDLK_LSHIFT:
			shift = down;
			break;
		case SDLK_q:
			running = false;
			break;
		case SDLK_d:
			if (down) {
				// delete all segments attached to closest vertex
				closest = closest_vertex(cx, cy);
				if (closest) {
					remove_component(closest->data);
				}
			}
			break;
		case SDLK_j:
		case SDLK_DOWN:
			if (down) yoffset -= 0.1f*zoom;
			break;
		case SDLK_k:
		case SDLK_UP:
			if (down) yoffset += 0.1f*zoom;
			break;
		case SDLK_h:
		case SDLK_LEFT:
			if (down) xoffset -= 0.1f*zoom;
			break;
		case SDLK_l:
		case SDLK_RIGHT:
			if (down) xoffset += 0.1f*zoom;
			break;
		case SDLK_EQUALS:
			if (down) zoom /= 1.2f;
			break;
		case SDLK_MINUS:
			if (down) zoom *= 1.2f;
			break;
		default:;
	}
}

/* Remove segment.
 * Remember to remove origin and end vertices if they become detached!
 */
static void remove_segment(struct seg* seg) {
	list_remove_item(&seg->origin->segs, seg);
	list_remove_item(&seg->end->segs, seg);
	list_remove_item(&segs, seg);
	free(seg);
}

/* Remove all linked segments.
 */
static void remove_component(struct vec* vec) {

	list_t* worklist = NULL;
	list_t* p = vec->segs;

	if (p) {
		do {
			list_add(&worklist, p->data);
			p = p->succ;
		} while (p != vec->segs);
	} else {
		list_remove_item(&vecs, vec);
		free(vec);
	}

	while (worklist) {
		struct seg* seg = list_remove(&worklist);
		if (!list_contains(segs, seg))
			continue;

		struct vec* origin = seg->origin;
		struct vec* end = seg->end;

		remove_segment(seg);

		if (!origin->segs) {
			list_remove_item(&vecs, origin);
			free(origin);
		} else {
			struct seg* next = origin->segs->data;
			if (!list_contains(worklist, next))
				list_add(&worklist, next);
		}

		if (!end->segs) {
			list_remove_item(&vecs, end);
			free(end);
		} else {
			struct seg* next = end->segs->data;
			if (!list_contains(worklist, next))
				list_add(&worklist, next);
		}
	}
}
