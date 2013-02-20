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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <assert.h>
#include <wchar.h>

#include "typeset.h"

static void add_word(paragraph_t* text, const char* word, int wlen);

static float linespill(paragraph_t* text, int i, int j, float M);
static float linecost(float spill, float M, int wc, int justify);

/* text:	paragraph to typeset
 * M:		maximum line width
 * justify:	1 for justified text
 */
void typeset(paragraph_t* text, float M, int justify)
{
	const int	n = text->nwords;
	float*		cost = malloc(sizeof(float)*(n+1));
	float*		spw = malloc(sizeof(float)*(n+1));
	int*		wc = malloc(sizeof(int)*(n+1));
	int i;
	int maxlinewords;

	for (i = 0; i < n+1; ++i) {
		cost[i] = FLT_MAX;
		wc[i] = 0;
		spw[i] = text->spacewidth;
	}

	maxlinewords = (int) (M / (text->mincwidth + text->spacewidth));
	for (i = n; i > 0; --i) {
		if (linespill(text, i, n, M) >= 0) {
			cost[i] = 0.f;
			wc[i] = n+1 - i;
		} else {
			float	min = FLT_MAX;
			float	spill = M;
			int	wc_i = 1;
			int z;

			// Cost for one word
			spill = linespill(text, i, i, M);
			min = linecost(spill, M, 1, justify) + cost[i+1];
			wc_i = 1;

			// Cost for two or more words
			for (z = i+1; z <= i+maxlinewords; ++z) {
				float	lspill = linespill(text, i, z, M);
				float	lcost = linecost(lspill, M, z+1-i,
						justify) + cost[z+1];
				if (lcost < min) {
					min = lcost;
					spill = lspill;
					wc_i = z + 1 - i;
				}
			}
			cost[i] = min;
			spw[i] = text->spacewidth;
			if (justify) spw[i] += spill / (wc_i-1);
			wc[i] = wc_i;
		}
	}

	text->wc = wc;
	text->spw = spw;

	/*printf("cost: %d\n", (int)cost[1]);
	for (int i = 1; i <= n; ) {
		int already = 0;
		
		int j = i;
		i += wc[i];

		while (j < i) {
			if (already) printf(" ");
			printf("%s", text->words[j-1]);

			already = 1;
			j = j+1;
		}
		printf("\n");
	}*/
}

/* Calculate spill space of typesetting words i to j
 * on one line, with maximum line width M
 */
float linespill(paragraph_t* text, int i, int j, float M)
{
	float	width = 0;
	int k;

	for (k = i-1; k < j; ++k) {
		width += ttf_line_width(text->typeface, text->words[k]);
		if (width > M)
			return M-width;

		if (k != j-1)
			width += ttf_char_width(text->typeface, ' ');
	}

	return M-width;

}

/* Typesetting cost function
 */
static float linecost(float spill, float M, int wc, int justify)
{
	float cost;
	if (spill < 0) return FLT_MAX;
	cost = powf(spill, 3);
	if (wc > 1 && justify)
		cost /= wc-1;
	return cost;
}

/* Allocate a new paragraph text object.
 * The typeface parameter is used to typeset
 * the paragraph.
 */
paragraph_t* new_paragraph(ttf_t* typeface)
{
	paragraph_t* new;

	new = malloc(sizeof(paragraph_t));
	new->typeface = typeface;
	new->words = malloc(sizeof(char*)*10);
	new->wc = NULL;
	new->spw = NULL;
	new->nwords = 0;
	new->max = 10;
	new->mincwidth = FLT_MAX;
	new->spacewidth = ttf_char_width(typeface, ' ');

	return new;
}

void free_paragraph(paragraph_t** paragraph)
{
	paragraph_t* p;
	assert(paragraph != NULL);

	p = *paragraph;
	if (p) {
		unsigned i;
		for (i = 0; i < p->nwords; ++i) {
			free(p->words[i]);
		}
		free(p->words);
		if (p->wc) free(p->wc);
		if (p->spw) free(p->spw);
		free(p);
	}
	*paragraph = NULL;
}

#define TYPESET_WORD_BUFF_SZ	(256)
void add_words(paragraph_t* text, const char* words)
{
	char	word[TYPESET_WORD_BUFF_SZ];
	int	wlen = 0;
	const char* p;

	assert(words != NULL);

	p = words;
	while (*p != '\0') {
		if (*p == ' ' && wlen > 0) {
			add_word(text, word, wlen);
			wlen = 0;
			p += 1;
			continue;
		}

		word[wlen++] = *p;
		p += 1;
		if (wlen == TYPESET_WORD_BUFF_SZ) {
			// word length limit reached!
			fprintf(stderr, "Word length limit reached!\n");
			add_word(text, word, wlen);
			wlen = 0;
		}
	}
	if (wlen > 0)
		add_word(text, word, wlen);
}

void add_word(paragraph_t* text, const char* word, int wlen)
{
	paragraph_t* p;
	unsigned i;
	int j;
	char*	wbuf;
	assert(text != NULL);

	p = text;
	if (p->nwords == p->max) {
		char** newwords;
		unsigned newmax;

		newmax = p->max*2;
		newwords = malloc(sizeof(char*)*newmax);

		for (i = 0; i < p->nwords; ++i) {
			newwords[i] = p->words[i];
		}

		free(p->words);
		p->words = newwords;
		p->max = newmax;
	}

	for (j = 0; j < wlen; ++j) {
		wchar_t	wc;
		float	width;

		mbtowc(&wc, word+j, MB_CUR_MAX);
		width = ttf_char_width(text->typeface, wc);
		if (width < text->mincwidth)
			text->mincwidth = width;
	}

	wbuf = malloc(wlen+1);
	memcpy(wbuf, word, wlen);
	wbuf[wlen] = '\0';
	p->words[p->nwords++] = wbuf;
}

