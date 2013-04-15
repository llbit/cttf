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
 * Polygon triangulation test program.
 * 
 * Renders one polygon (or shape) and either interactively or
 * directly triangulates it.
 */
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <wchar.h>
#include <wctype.h>
#include <assert.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <pthread.h>
#include <locale.h>
#include <float.h>

#include "shape.h"
#include "triangulate.h"
#include "bstree.h"
#include "qsortv.h"
#include "ttf.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846	/* pi */
#endif
#ifndef M_PI_2
#define M_PI_2		1.57079632679489661923	/* pi/2 */
#endif

#define NCOLORS	(7)

static pthread_mutex_t	mutex;
static pthread_cond_t	cond;

static float		colors[NCOLORS][3] = {
	{0.6, 0.6, 1},
	{0.2, 0.2, 0.8},
	{0.18, 0.18, 0.50},
	{0.08, 0.08, 0.49},
	{0.08, 0.08, 0.32},
	{0.08, 0.08, 0.22},
	{0.08, 0.08, 0.10},
};

static float		edge_colors[NCOLORS][3] = {
	{1, 0.6, 1},
	{1, 0.2, 0.8},
	{0.9, 0.8, 0.20},
	{0.5, 0.8, 0.3},
	{0.3, 0.8, 0.82},
	{0.1, 0.08, 0.72},
	{0.68, 0.68, 0.67},
};

static int		g_edge_dir = 0;
static int		g_debug = 0;
static int		g_planar = 0;
static int		g_only_triangulate = 0;
static int		g_outer = 0;

static int		fsel = -1;

FILE*			out = NULL;

static shape_t*		shape = NULL;
static edge_list_t*	edge_list = NULL;
static edge_t*		incident_edge = NULL;
static ttf_t*		ttf = NULL;

SDL_Surface*	screen;
static bool	running = true;

#define WINDOW_W (700)
#define WINDOW_H (700)

static float	xoffset = 0.5f;
static float	yoffset = 0.5f;
static float	zoom = 1;
static float	cx = -1;
static float	cy = -1;

static int	dbg_step = 0;

static void render();
static void handle_event(SDL_Event* event);
static void on_left_click(float x, float y);
static void on_key_down(SDLKey key, int down);
static void triangulate_shape(shape_t* shape);
static void render_component(edge_t* edge);
/*static void render_face(face_t* face);*/
static void load_resources(const char* fn, const char* chr);
static void free_resources();

static float from_screen_x(int x);
static float from_screen_y(int y);

static void setup_video()
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

}

static void fit_view_to_shape(shape_t* shape)
{
	/* calculate center coordinates */
	float xmin = FLT_MAX;
	float xmax = FLT_MIN;
	float ymin = FLT_MAX;
	float ymax = FLT_MIN;
	int i;
	for (i = 0; i < shape->nvec; ++i) {
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

void load_resources(const char* fn, const char* chr)
{
	FILE*	fp = fopen(fn, "rb");
	if (!fp) {
		fprintf(stderr, "could not open file: %s\n", fn);
		exit(1);
	}

	ttf = ttf_load(fp);
	if (ttf != NULL) {
		wchar_t	wc;
		fclose(fp);
		if (chr)
			mbtowc(&wc, chr, MB_CUR_MAX);
		else
			mbtowc(&wc, "a", MB_CUR_MAX);
		ttf->interpolation_level = 3;
		shape = ttf_export_chr_shape(ttf, wc);
		if (shape == NULL) {
			free_ttf(&ttf);
			fprintf(stderr, "character not available: %x\n", wc);
			exit(1);
		}

		if (out) {
			write_shape(out, shape);
			printf("wrote shape file\n");
			free_resources();
			exit(0);
		}
		fit_view_to_shape(shape);
		triangulate_shape(shape);
		free_ttf(&ttf);
	} else {
		fseek(fp, 0, SEEK_SET);
		shape = load_shape(fp);
		fclose(fp);
		if (shape == NULL) {
			fprintf(stderr, "could not load shape: %s\n", fn);
			exit(1);
		}
		fit_view_to_shape(shape);
		triangulate_shape(shape);
	}
}

static void free_resources()
{
	free_shape(&shape);
	free_edgelist(&edge_list);
	free_ttf(&ttf);

	if (out)
		fclose(out);

	if (g_debug)
		pthread_mutex_destroy(&mutex);
}

static void print_help()
{
	printf("usage: ftest FONT [CHR] [OPTIONS]\n");
	printf("  where FONT is either the filename of a TrueTypeFont or SHAPE file\n");
	printf("  and CHR is the character to render (if no shape is specified) [default: 'a']\n");
	printf("  and OPTIONS is one of\n");
	printf("    -h            show help\n");
	printf("    -e            display direction of edges\n");
	printf("    -d            DEBUG mode (single stepped algorithm)\n");
	printf("    -p            only make planar graph\n");
	printf("    -o <TARGET>   write the character to shape file TARGET\n");
	printf("    -t            only triangulate the shape then exit\n");
}

void parse_args(int argc, const char** argv, const char** font, const char** fn)
{
	int i;
	for (i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-h")) {
				print_help();
				exit(0);
			} else if (!strcmp(argv[i], "-o")) {

				if (i+1 == argc) {
					fprintf(stderr, "You must specify "
							"output target!\n");
					print_help();
					exit(1);
				}
				out = fopen(argv[i+1], "w");
				if (!out) {
					fprintf(stderr, "Could not open %s "
							"for writing\n",
							argv[i+1]);
				}
				i += 1;
			} else if (!strcmp(argv[i], "-e")) {
				g_edge_dir = 1;
			} else if (!strcmp(argv[i], "-d")) {
				g_debug = 1;
			} else if (!strcmp(argv[i], "-p")) {
				g_planar = 1;
			} else if (!strcmp(argv[i], "-t")) {
				g_only_triangulate = 1;
			} else {
				fprintf(stderr, "Illegal command-line "
						"option: %s\n",
						argv[i]);
				print_help();
				exit(1);
			}
		} else {
			if (!*font) {
				*font = argv[i];
			} else if (!*fn) {
				*fn = argv[i];
			} else {
				fprintf(stderr, "Unexpected command-line "
						"argument: %s\n",
						argv[i]);
				print_help();
				exit(1);
			}
		}
	}
	
	if (!*font) {
		fprintf(stderr, "You must specify a font file to use!\n");
		print_help();
		exit(1);
	}

}

int main(int argc, const char** argv)
{
	const char*	font = NULL;
	const char*	fn = NULL;

	setlocale(LC_ALL, "");

	parse_args(argc, argv, &font, &fn);
	load_resources(font, fn);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	atexit(SDL_Quit);

	setup_video();
	while (running) {
		SDL_Event event;

		render();

		SDL_GL_SwapBuffers();
		while (SDL_PollEvent(&event)) {
			handle_event(&event);
		}
	}

	free_resources();

	return 0;
}

static int render_tree(bstree_t* tree, int i)
{
	if (tree == NULL) return i;

	if (tree->left == NULL && tree->right == NULL) {
		glColor3f(colors[i][0], colors[i][1], colors[i][2]);
		glBegin(GL_LINES);
		glVertex3f(((edge_t*)tree->value)->origin->vec.x,
				((edge_t*)tree->value)->origin->vec.y, 0);
		glVertex3f(((edge_t*)tree->value)->twin->origin->vec.x,
				((edge_t*)tree->value)->twin->origin->vec.y, 0);
		glEnd();
		return (i+1)%NCOLORS;
	} else {
		i = render_tree(tree->left, i);
		i = render_tree(tree->right, i);
		return i;
	}
}

void render_edge(edge_t* e)
{
	float	x1 = e->origin->vec.x;
	float	x2 = e->twin->origin->vec.x;
	float	y1 = e->origin->vec.y;
	float	y2 = e->twin->origin->vec.y;

	/* line */
	glBegin(GL_LINES);
	glVertex3f(x1, y1, 0);
	glVertex3f(x2, y2, 0);
	glEnd();


	if (g_edge_dir) {
		/* figure out angle of rotation: */
		float	angle;
		float	xd = x2-x1;
		float	yd = y2-y1;
		if (x1 == x2) {
			angle = (yd < 0) ? -M_PI_2 : M_PI_2;
		} else {
			angle = atan(yd / xd);
			if (xd < 0)
				angle += M_PI;
		}
		angle -= M_PI_2;
		angle *= 180 / M_PI;


		/* arrow */
		glPushMatrix();
		glTranslatef((x1+x2)/2, (y1+y2)/2, 0);
		glRotatef(angle, 0, 0, 1);
		glColor3f(0.8, 0.8, 0);
		glBegin(GL_LINES);
		glVertex3f(-0.01*zoom, -0.01*zoom, 0);
		glVertex3f(0, +0.01*zoom, 0);
		glVertex3f(0, +0.01*zoom, 0);
		glVertex3f(+0.01*zoom, -0.01*zoom, 0);
		glEnd();

		glPopMatrix();
	}
}

void render_component(edge_t* edge)
{
	edge_t*	p;
	edge_t*	h;
	p = h = edge;
	assert(p != NULL);
	do {
		edge_t*	e = p;
		p = p->succ;

		render_edge(e);
	} while (p != h);
}

/*static void render_face(face_t* face)
{
	if (g_outer && face->outer_component)
		render_component(face->outer_component);

	if (!g_outer && face->inner_components) {
		list_t* p = face->inner_components;
		do {
			render_component(p->data);
			p = p->succ;
		} while (p != face->inner_components);
	}
}*/

static void render()
{
	list_t*	p;
	list_t*	h;
	int i;

	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1/zoom, 1/zoom, 1);
	glTranslatef(-xoffset+zoom/2, -yoffset+zoom/2, 0);

	glColor3f(0.5,0.5,0.5);
	glBegin(GL_LINES);
	glVertex3f(0.5f, 0, 0);
	glVertex3f(0.5f, 1, 0);
	glVertex3f(0, 0.5f, 0);
	glVertex3f(1, 0.5f, 0);
	glEnd();

	pthread_mutex_lock(&mutex);

	if (edge_list == NULL)
		return;

	if (g_planar) {
		int findex = 0;
		p = h = edge_list->edges;
		if (p) do {
			if (fsel < 0 || findex == fsel) {
				glColor3f(edge_colors[findex%NCOLORS][0],
						edge_colors[findex%NCOLORS][1],
						edge_colors[findex%NCOLORS][2]);

				render_component(p->data);
			}
			findex += 1;
			p = p->succ;
		} while (p != h);
	} else {
		int findex = 0;
		p = h = edge_list->cycles;
		if (p) do {
			if (fsel < 0 || findex == fsel) {
				glColor3f(edge_colors[findex%NCOLORS][0],
						edge_colors[findex%NCOLORS][1],
						edge_colors[findex%NCOLORS][2]);

				render_component(p->data);
				/*render_face(p->data);*/
			}
			findex += 1;
			p = p->succ;
		} while (p != h);
	}

	for (i = 0; i < edge_list->nvert; ++i) {
		vertex_t*	v = edge_list->vertices[i];

		unsigned mode = GL_FILL;

		float	x1;
		float	x2;
		float	y1;
		float	y2;

		switch (v->vtype) {
			case START_VERTEX:
				mode = GL_LINE;
				glColor3f(0.8, 0, 0);
				break;
			case END_VERTEX:
				glColor3f(0, 0, 0.8);
				break;
			case SPLIT_VERTEX:
				glColor3f(0, 0, 0.8);
				break;
			case MERGE_VERTEX:
				glColor3f(0.8, 0, 0);
				break;
			case REGULAR_VERTEX:
			default:
				mode = GL_LINE;
				glColor3f(0, 0.6, 0);
				break;
		}
		glPolygonMode(GL_FRONT_AND_BACK, mode);
		/*if (i < dbg_step)
			glColor3f(0.8f, 0.8f, 0.8f);*/

		x1 = v->vec.x - 0.006*zoom;
		x2 = v->vec.x + 0.006*zoom;
		y1 = v->vec.y - 0.006*zoom;
		y2 = v->vec.y + 0.006*zoom;

		switch (v->vtype) {
			case START_VERTEX:
			case END_VERTEX:
				glBegin(GL_QUADS);
				glVertex3f(x1, y1, 0);
				glVertex3f(x1, y2, 0);
				glVertex3f(x2, y2, 0);
				glVertex3f(x2, y1, 0);
				glEnd();
				break;
			case SPLIT_VERTEX:
				glBegin(GL_TRIANGLES);
				glVertex3f(x2, y1, 0);
				glVertex3f(x1, y1, 0);
				glVertex3f((x1+x2)/2, y2, 0);
				glEnd();
				break;
			case MERGE_VERTEX:
				glBegin(GL_TRIANGLES);
				glVertex3f(x1, y2, 0);
				glVertex3f(x2, y2, 0);
				glVertex3f((x1+x2)/2, y1, 0);
				glEnd();
				break;
			default:
			case REGULAR_VERTEX:
				glBegin(GL_LINES);
				glVertex3f(x1, y1, 0);
				glVertex3f(x2, y2, 0);
				glVertex3f(x1, y2, 0);
				glVertex3f(x2, y1, 0);
				glEnd();
				continue;
		}
	}

	if (incident_edge) {
		vertex_t*	origin;
		vertex_t*	end;
		float	x1;
		float	y1;
		float	x2;
		float	y2;
		float	angle;
		float	xd;
		float	yd;

		glColor3f(0.2, 0.3, 0.6);
		render_component(incident_edge);

		origin = incident_edge->origin;
		end = incident_edge->twin->origin;
		x1 = origin->vec.x;
		y1 = origin->vec.y;
		x2 = end->vec.x;
		y2 = end->vec.y;
		/* figure out angle of rotation: */
		xd = x2-x1;
		yd = y2-y1;
		if (x1 == x2) {
			angle = (yd < 0) ? -M_PI_2 : M_PI_2;
		} else {
			angle = atan(yd / xd);
			if (xd < 0)
				angle += M_PI;
		}
		angle -= M_PI_2;
		angle *= 180 / M_PI;


		/* arrow */
		glColor3f(0.2, 0.5, 0.9);
		glBegin(GL_LINES);
		glVertex3f(x1, y1, 0);
		glVertex3f(x2, y2, 0);
		glEnd();
		glPushMatrix();
		glTranslatef((x1+x2)/2, (y1+y2)/2, 0);
		glRotatef(angle, 0, 0, 1);
		glBegin(GL_LINES);
		glVertex3f(-0.01*zoom, -0.01*zoom, 0);
		glVertex3f(0, +0.01*zoom, 0);
		glVertex3f(0, +0.01*zoom, 0);
		glVertex3f(+0.01*zoom, -0.01*zoom, 0);
		glEnd();

		glPopMatrix();
	}

	render_tree(edge_list->etree, 0);

#if 0
	//  TEST CODE
	glColor3f(1, 1, 1);
	//glBegin(GL_LINES);
	//glVertex3f(0.532, 0.964, 0);
	//glVertex3f(0.454, 0.742, 0);
	//glEnd();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	float	x1 = 0.722 - 0.007;
	float	x2 = 0.722 + 0.007;
	float	y1 = 0.980 - 0.007;
	float	y2 = 0.980 + 0.007;
	glBegin(GL_QUADS);
	glVertex3f(x1, y1, 0);
	glVertex3f(x1, y2, 0);
	glVertex3f(x2, y2, 0);
	glVertex3f(x2, y1, 0);
	glEnd();
#endif

	pthread_mutex_unlock(&mutex);
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
		case SDL_MOUSEMOTION:
			cx = from_screen_x(event->motion.x);
			cy = from_screen_y(event->motion.y);
			break;
		case SDL_MOUSEBUTTONDOWN:
			mx = from_screen_x(event->motion.x);
			my = from_screen_y(event->motion.y);
			if (event->button.button == SDL_BUTTON_LEFT)
				on_left_click(mx, my);
			else if (event->button.button == SDL_BUTTON_WHEELUP)
				zoom /= 1.2f;
			else if (event->button.button == SDL_BUTTON_WHEELDOWN)
				zoom *= 1.2f;
			break;
		case SDL_KEYDOWN:
			on_key_down(event->key.keysym.sym, 1);
			break;
		default:;
	}
}

static void print_closest_vertex(float fx, float fy)
{
	float		d;
	float		mind = 0.006*zoom;
	vertex_t*	closest = NULL;
	int		i;

	for (i = 0; i < edge_list->nvert; ++i) {
		vertex_t*	v = edge_list->vertices[i];

		float	dx = fx - v->vec.x;
		float	dy = fy - v->vec.y;
		d = dx*dx + dy*dy;
		if (d < mind) {
			closest = v;
			mind = d;
		}
	}
	if (closest != NULL) {
		printf("%d (%f, %f)\n", closest->id,
				closest->vec.x, closest->vec.y);
	} else {
		printf("no closest vertex\n");
	}
}

/* Find closest vertex and display it's incident edge */
static void on_left_click(float fx, float fy)
{
	float		d;
	float		mind = 0.006*zoom;
	vertex_t*	closest = NULL;
	int		ind;
	int		i;

	for (i = 0; i < edge_list->nvert; ++i) {
		vertex_t*	v = edge_list->vertices[i];

		float	dx = fx - v->vec.x;
		float	dy = fy - v->vec.y;
		d = dx*dx + dy*dy;
		if (d < mind) {
			closest = v;
			ind = i;
			mind = d;
		}
	}

	if (closest != NULL) {
		printf("%d: %f, %f\n", ind,
				closest->vec.x,
				closest->vec.y);
		printf("incident_edge: %p\n", (void*)closest->incident_edge);
		if (closest->incident_edge != NULL)
			printf("left face: %p\n", (void*)closest->incident_edge->left_face);
		incident_edge = closest->incident_edge;
	} else {
		incident_edge = NULL;
	}
}

void on_key_down(SDLKey key, int down)
{
	switch(key) {
		case SDLK_1:
			print_closest_vertex(cx, cy);
			break;
		case SDLK_i:
			g_outer = 0;
			printf("rendering inner components\n");
			break;
		case SDLK_o:
			g_outer = 1;
			printf("rendering only outer components\n");
			break;
		case SDLK_h:
			fsel -= 1;
			if (fsel >= 0) {
				printf("rendering face: %d\n", fsel);
			} else {
				printf("rendering all faces\n");
				fsel = -1;
			}
			break;
		case SDLK_l:
			fsel += 1;
			if (fsel >= 0)
				printf("rendering face: %d\n", fsel);
			else
				printf("rendering all faces\n");
			break;
		case SDLK_q:
			running = false;
			break;
		case SDLK_SPACE:
			if (g_debug) {
				pthread_mutex_lock(&mutex);
				pthread_cond_signal(&cond);
				dbg_step += 1;
				pthread_mutex_unlock(&mutex);
			}
			break;
		case SDLK_DOWN:
			yoffset -= 0.1f*zoom;
			break;
		case SDLK_UP:
			yoffset += 0.1f*zoom;
			break;
		case SDLK_LEFT:
			xoffset -= 0.1f*zoom;
			break;
		case SDLK_RIGHT:
			xoffset += 0.1f*zoom;
			break;
		case SDLK_EQUALS:
			zoom /= 1.2f;
			break;
		case SDLK_MINUS:
			zoom *= 1.2f;
			break;
		default:;
	}
}

static void* work(void* data)
{
	if (g_planar) {
		edge_list = make_planar(data);
		return NULL;
	}
	edge_list = triangulate(data);

	return NULL;
}

static void triangulate_shape(shape_t* shape)
{
	pthread_t	thread;

	if (g_debug) {
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond, NULL);
		if (-1 == pthread_create(&thread, NULL, &work, shape))
			perror("pthread_create");
	} else {
		work(shape);
		if (g_only_triangulate) {
			free_resources();
			exit(0);
		}
	}
}

