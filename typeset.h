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
#ifndef TTF_TYPESET_H
#define TTF_TYPESET_H

#include "ttf.h"

typedef struct {
	ttf_t*		typeface;
	char**		words;
	int*		wc;
	float*		spw;
	unsigned	nwords;
	unsigned	max;
	float		mincwidth;
	float		spacewidth;
} paragraph_t;

paragraph_t* new_paragraph(ttf_t* typeface);
void free_paragraph(paragraph_t**);
void typeset(paragraph_t* text, float M, int justify);
void add_words(paragraph_t*, const char*);

#endif

