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
#include <SDL/SDL.h>
#include <SDL/SDL_opengl.h>
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>
#include "text.h"

/* Returns null if ttf is null
 */
font_t* new_font(ttf_t* ttf, int ipl)
{
	font_t*	obj;
	int i;
	if (!ttf) return NULL;

	obj = malloc(sizeof(font_t));
	obj->ttf = ttf;
	ttf->interpolation_level = ipl;
	obj->cshape = malloc(sizeof(shape_t*)*0x10000);
	obj->cedges = malloc(sizeof(edge_list_t*)*0x10000);
	for (i = 0; i < 0x10000; ++i) {
		obj->cshape[i] = NULL;
		obj->cedges[i] = NULL;
	}
	return obj;
}

/* Attempts to load a TrueTypeFont from a file with the
 * path given in name. Returns null on failure.
 */
font_t* load_font(const char* name, int ipl)
{
	FILE*	fp;
	ttf_t*	ttf;
	fp = fopen(name, "rb");
	if (!fp) {
		fprintf(stderr, "Could not find font \"%s\"\n", name);
		return NULL;
	}
	ttf = ttf_load(fp);
	fclose(fp);
	if (ttf) {
		return new_font(ttf, ipl);
	} else {
		fprintf(stderr, "Error while loading "
				"font file: \"%s\":\n%s\n",
				name, ttf_strerror());
		return NULL;
	}
}

void free_font(font_t** font)
{
	font_t*	p;
	int i;
	assert(font != NULL);
	p = *font;
	if (!p) return;
	free_ttf(&p->ttf);
	for (i = 0; i < 0x10000; ++i) {
		free_shape(&p->cshape[i]);
		free_edgelist(&p->cedges[i]);
	}
	free(p->cshape);
	free(p->cedges);
	free(p);
	*font = NULL;
}

void font_prepare_chr(font_t* font, uint16_t chr, int triangulated)
{
	assert(font != NULL);
	assert(font->ttf != NULL);

	if (!font->cshape[chr]) {
		font->cshape[chr] = ttf_export_chr_shape(font->ttf, chr);
	}
	if (font->cshape[chr] && triangulated && !font->cedges[chr]) {
		font->cedges[chr] = triangulate(font->cshape[chr]);
	}
}

float line_width(font_t* font, const char* str)
{
	return ttf_line_width(font->ttf, str);
}

float line_height(font_t* font)
{
	assert(font != NULL);

	return (font->ttf->ymax - font->ttf->ymin) /
		(float) font->ttf->upem;
}

void draw_hollow_word(font_t* font, const char* str)
{
	const char* p;
	assert(font != NULL);

	p = str;
	while (*p != '\0') {
		wchar_t 	wc;
		int		n;
		int	i;
		shape_t*	shape;

		n = mbtowc(&wc, p, MB_CUR_MAX);
		if (n == -1) break;
		else p += n;

		font_prepare_chr(font, wc, 0);
		shape = font->cshape[wc];
		if (!shape) continue;

		glBegin(GL_LINES);
		for (i = 0; i < shape->nseg; ++i) {
			float	x1 = shape->vec[shape->seg[i*2]].x;
			float	y1 = shape->vec[shape->seg[i*2]].y;
			float	x2 = shape->vec[shape->seg[i*2+1]].x;
			float	y2 = shape->vec[shape->seg[i*2+1]].y;

			glVertex3f(x1, y1, 0);
			glVertex3f(x2, y2, 0);
		}
		glEnd();
		// offset to next character
		glTranslatef(ttf_char_width(font->ttf, wc), 0, 0);
	}
}

void draw_filled_word(font_t* font, const char* str)
{
	const char* s;
	assert(font != NULL);

	s = str;
	while (*s != '\0') {
		wchar_t 	wc;
		int		n;
		edge_list_t*	edge_list;
		list_t*	p;
		list_t*	h;

		n = mbtowc(&wc, s, MB_CUR_MAX);
		if (n == -1) break;
		else s += n;

		font_prepare_chr(font, wc, 1);
		edge_list = font->cedges[wc];
		if (!edge_list) continue;

		glBegin(GL_TRIANGLES);
		p = h = edge_list->faces;
		if (p)
		do {
			face_t*	face = p->data;
			edge_t*	edge = face->outer_component;
			p = p->succ;

			if (!face->is_inside || edge == NULL)
				continue;

			if (edge->succ->succ->succ == edge) {

				edge_t*	e = edge;
				glNormal3d(0, 0, 1);
				
				do {
					glVertex3f(e->origin->vec.x,
							e->origin->vec.y, 0);
					e = e->succ;

				} while (e != edge);
			}

		} while (p != h);
		glEnd();

		// offset to next character
		glTranslatef(ttf_char_width(font->ttf, wc), 0, 0);
	}
}

void draw_3d_word(font_t* font, const char* str, float depth)
{
	const char* s;
	assert(font != NULL);

	s = str;
	while (*s != '\0') {
		wchar_t 	wc;
		int		n;
		list_t*	p;
		list_t*	h;
		edge_list_t*	edge_list;
		shape_t*	shape;
		int i;

		n = mbtowc(&wc, s, MB_CUR_MAX);
		if (n == -1) break;
		else s += n;

		font_prepare_chr(font, wc, 1);
		edge_list = font->cedges[wc];
		shape = font->cshape[wc];
		if (!edge_list) continue;

		glBegin(GL_TRIANGLES);
		p = h = edge_list->faces;
		if (p)
		do {
			face_t*	face = p->data;
			edge_t*	edge = face->outer_component;
			edge_t*	e;
			p = p->succ;

			if (!face->is_inside || edge == NULL)
				continue;

			glNormal3d(0, 0, 1);
			e = edge;
			do {
				glVertex3f(e->origin->vec.x,
						e->origin->vec.y, 0);
				e = e->succ;

			} while (e != edge);

		} while (p != h);
		glEnd();

		glBegin(GL_QUADS);
		for (i = 0; i < shape->nseg; ++i) {
			float	x1 = shape->vec[shape->seg[i*2]].x;
			float	y1 = shape->vec[shape->seg[i*2]].y;
			float	x2 = shape->vec[shape->seg[i*2+1]].x;
			float	y2 = shape->vec[shape->seg[i*2+1]].y;

			// normal = (x1, y1, 0) x (x2, y2, -h)
			glNormal3d(-depth*(y2-y1), depth*(x2-x1), 0);
			glVertex3f(x1, y1, 0);
			glVertex3f(x2, y2, 0);
			glVertex3f(x2, y2, -depth);
			glVertex3f(x1, y1, -depth);
		}
		glEnd();

		// offset to next character
		glTranslatef(ttf_char_width(font->ttf, wc), 0, 0);
	}
}

void draw_hollow_str(font_t* font, const char* str)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	draw_hollow_word(font, str);
	glPopMatrix();
}

void draw_filled_str(font_t* font, const char* str)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	draw_filled_word(font, str);
	glPopMatrix();
}

void draw_3d_str(font_t* font, const char* str, float depth)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	draw_3d_word(font, str, depth);
	glPopMatrix();
}

void draw_hollow_text(font_t* font, paragraph_t* text)
{
	int i;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	for (i = 1; i <= text->nwords; ) {
		int first = 1;
		int	j = i;
		int	next = i+text->wc[i];
		glPushMatrix();
		while (j < next) {
			if (!first)
				glTranslatef(text->spw[i], 0, 0);
			draw_hollow_word(font, text->words[j-1]);

			first = 0;
			j = j+1;
		}
		i = next;
		glPopMatrix();
		glTranslatef(0, -line_height(font), 0);
	}
	glPopMatrix();
}

void draw_filled_text(font_t* font, paragraph_t* text)
{
	int i;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	for (i = 1; i <= text->nwords; ) {
		int first = 1;
		int	j = i;
		int	next = i+text->wc[i];
		glPushMatrix();
		while (j < next) {
			if (!first)
				glTranslatef(text->spw[i], 0, 0);
			draw_filled_word(font, text->words[j-1]);

			first = 0;
			j = j+1;
		}
		i = next;
		glPopMatrix();
		glTranslatef(0, -line_height(font), 0);
	}
	glPopMatrix();
}

void draw_3d_text(font_t* font, paragraph_t* text, float depth)
{
	int i;
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	for (i = 1; i <= text->nwords; ) {
		int first = 1;
		int	j = i;
		int	next = i+text->wc[i];
		glPushMatrix();
		while (j < next) {
			if (!first)
				glTranslatef(text->spw[i], 0, 0);
			draw_3d_word(font, text->words[j-1], depth);

			first = 0;
			j = j+1;
		}
		i = next;
		glPopMatrix();
		glTranslatef(0, -line_height(font), 0);
	}
	glPopMatrix();
}

