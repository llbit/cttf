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
#ifndef CTTF_TEXT_H
#define CTTF_TEXT_H

#include <stdio.h>

#include "ttf.h"
#include "triangulate.h"
#include "typeset.h"

typedef struct font	font_t;

// the font structure describes a vector font
struct font {
	ttf_t*		ttf;
	shape_t**	cshape;
	edge_list_t**	cedges;
};

// returns non-NULL on success
font_t* new_font(ttf_t* ttf, int ipl);
font_t* load_font(const char* name, int ipl);
font_t* load_font_file(FILE* fp, int ipl);
void free_font(font_t** font);

// prepare a character for rendering
void font_prepare_chr(font_t* font, uint16_t chr, int triangulated);

float line_width(font_t* font, const char* str);

float line_height(font_t* font);

void draw_hollow_word(font_t* font, const char* str);
void draw_filled_word(font_t* font, const char* str);
void draw_3d_word(font_t* font, const char* str, float depth);

// draw just the contours of the characters
void draw_hollow_str(font_t* font, const char* str);
void draw_filled_str(font_t* font, const char* str);
void draw_3d_str(font_t* font, const char* str, float depth);

void draw_hollow_text(font_t* font, paragraph_t* text);
void draw_filled_text(font_t* font, paragraph_t* text);
void draw_3d_text(font_t* font, paragraph_t* text, float depth);

#endif

