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
 * 3D test
 */
#include <stdio.h>
#include <err.h>
#include <math.h>
#include <stdbool.h>
#include <locale.h>
#include <wchar.h>
#include <assert.h>
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <pthread.h>

#include "shape.h"
#include "triangulate.h"
#include "ttf.h"
#include "text.h"

#define WINDOW_W (700)
#define WINDOW_H (700)

static font_t*		font = NULL;
static shape_t*		shape = NULL;
static edge_list_t*	edge_list = NULL;

static const char*	text = NULL;
static float		z_angle = 0;

SDL_Surface*	screen;
static bool	running = true;

static void setup_video();
static void print_help();
static void load_resources(const char* fn, const char* text);
static void free_resources();
static void render();
static void handle_event(SDL_Event* event);
static void on_key_down(SDLKey key, int down);

int main(int argc, const char** argv)
{
	const char*	fn = NULL;

	setlocale(LC_ALL, "");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	atexit(SDL_Quit);

	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (!strcmp(argv[i], "-h")) {
				print_help();
				exit(0);
			} else {
				fprintf(stderr, "illegal option: %s\n", argv[i]);
				print_help();
				exit(1);
			}
		} else {
			if (!fn)
				fn = argv[i];
			else if (!text)
				text = argv[i];
			else {
				fprintf(stderr, "Too many arguments given! Expected "
						"ONE filename and ONE text argument.\n");
				print_help();
				exit(1);
			}
		}
	}

	if (!fn) {
		fprintf(stderr, "No filename given!\n");
		print_help();
		exit(1);
	}

	load_resources(fn, text);

	setup_video();
	while (running) {
		SDL_Event event;

#if 1
		render();
		z_angle += 0.1;
#endif

		SDL_GL_SwapBuffers();
		while (SDL_PollEvent(&event)) {
			handle_event(&event);
		}
	}

	free_resources();

	return 0;
}

void setup_video()
{
	uint32_t	flags = SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_OPENGL;

	screen = SDL_SetVideoMode(WINDOW_W, WINDOW_H, 0, flags);
	if (!screen)
		errx(1, "Failed to set video mode");

	SDL_ShowCursor(SDL_ENABLE);

	glViewport(0, 0, screen->w, screen->h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, 1, 1, 7);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glClearColor(1, 1, 1, 1);

	GLfloat light[4];
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	light[0] = 0;
	light[1] = 0;
	light[2] = 1;
	light[3] = 0;
	glLightfv(GL_LIGHT0, GL_POSITION, light);
	glEnable(GL_NORMALIZE);

}

/* Attempt to load a TrueType font file.
 * If that fails, try to load the shape file instead.
 */
void load_resources(const char* fn, const char* text)
{
	FILE*	fp = fopen(fn, "rb");
	if (!fp) {
		fprintf(stderr, "could not open file: %s\n", fn);
		exit(1);
	}

	ttf_t*	ttf = ttf_load(fp);
	if (ttf != NULL) {
		if (!text) {
			free_ttf(&ttf);
			fprintf(stderr, "You must specify a text to render!\n");
			print_help();
			exit(1);
		}

		font = new_font(ttf, 3);
	} else {
		fseek(fp, 0, SEEK_SET);
		shape = load_shape(fp);
		if (shape == NULL) {
			fprintf(stderr, "could not load shape: %s\n", fn);
			exit(1);
		}
		edge_list = triangulate(shape);
	}

	fclose(fp);
}

void free_resources()
{
	free_font(&font);
}

void print_help()
{
	printf("usage: 3dtext FONT TEXT [OPTIONS]\n");
	printf("  where FONT is either the filename of a TrueTypeFont or SHAPE file\n");
	printf("  and TEXT is the text to render (if no shape is specified)\n");
	printf("  and OPTIONS is one of\n");
	printf("    -h    show help\n");
}

void render_component(face_t* face, edge_t* edge, int color)
{
	assert(edge != NULL);

	glBegin(GL_TRIANGLES);
	glNormal3d(0, 0, 1);

	edge_t*	p;
	p = edge;
	do {
		edge_t*	e = p;
		glVertex3f(e->origin->vec.x,
				e->origin->vec.y, 0);
		p = p->succ;

	} while (p != edge);

	glEnd();
}

void render_face(face_t* face, int color)
{
	if (face->is_inside && face->outer_component)
		render_component(face, face->outer_component, color);
}

void render()
{
	GLfloat light[4];

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(
		0, 0, 4,
		0, 0, 0,
		0, 1, 0);
	glRotatef(-40, 1, 0, 0);
	glRotatef(z_angle, 0, 0, 1);

	light[0] = .2;
	light[1] = .2;
	light[2] = .2;
	light[3] = 0.4;
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, light);
	glColor3f(0, 0, 1);

	if (edge_list) {

		list_t*	p;
		list_t*	h;
		p = h = edge_list->faces;
		int color = 0;
		if (p)
		do {
			render_face(p->data, color++);
			p = p->succ;

		} while (p != h);

		// render the sides
		for (int i = 0; i < shape->nseg; ++i) {
			float	h = 0.15;
			float	x1 = shape->vec[shape->seg[i*2]].x;
			float	y1 = shape->vec[shape->seg[i*2]].y;
			float	x2 = shape->vec[shape->seg[i*2+1]].x;
			float	y2 = shape->vec[shape->seg[i*2+1]].y;

			// normal = (x1, y1, 0) x (x2, y2, -h)
			glNormal3d(-h*(y2-y1), h*(x2-x1), 0);
			glBegin(GL_QUADS);
			glVertex3f(x1, y1, 0);
			glVertex3f(x2, y2, 0);
			glVertex3f(x2, y2, -h);
			glVertex3f(x1, y1, -h);
			glEnd();
		}
	} else {

		glPushMatrix();
		glTranslatef(-line_width(font, text)/2, -0.25, 0);
		draw_3d_str(font, text, 0.15);
		glPopMatrix();
	}
}

void handle_event(SDL_Event* event)
{
	switch(event->type) {
		case SDL_KEYDOWN:
			on_key_down(event->key.keysym.sym, 1);
			break;
		default:;
	}
}

void on_key_down(SDLKey key, int down)
{
	switch(key) {
		case SDLK_q:
			running = false;
			break;
		default:;
	}
}

