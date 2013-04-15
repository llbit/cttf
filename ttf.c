/**
 * Copyright (c) 2002-2011 Jesper Öqvist <jesper@llbit.se>
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
 * cTTF can load OpenType fonts with TrueType font outlines.
 * There are various limitations to cTTF which restrict the
 * kinds of fonts it will successfully load.
 *
 * See http://www.microsoft.com/typography/otspec/default.htm
 * for the OpenType font format specification. Some sections
 * of the specification are referenced throughout this code.
 *
 * A short version history follows
 *
 * 2002-06-23:
 *      File created
 * 2003-01-28:
 * 	Added Resolution and EMPointSize
 * 	Various other small changes
 * 2004-01-29:
 * 2006-03-03:
 * 	Adapted for ScrapHeap
 * 2006-03-15:
 * 	Added swenglish comments
 * 2011-01-11:
 * 	Licensed under GPLv3
 * 	Ported to C99
 * 2011-02-09:
 * 	Added polygon triangulation
 * 	License changed to GPL v2
 * 	Uploaded to Launchpad
 * 2011-02-24:
 *      Refactored ttf_load
 *      One new loading func for each tbl
 *      Improved error handling
*/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "ttf.h"
#include "list.h"

/* All values in an OpenType file are encoded in
 * Motorola style big endian. These macros convert
 * from big endian to the current platform's endianness.
 */
#if 1
#define TTF_ENDIAN_WORD(a) \
	((((0xFF00 & a) >> 8) | ((0x00FF & a) << 8)))
#define TTF_ENDIAN_DWORD(a) \
	(((0xFF000000 & a) >> 24) | ((0x00FF0000 & a) >> 8) | \
	 ((0x0000FF00 & a) << 8) | ((0x000000FF & a) << 24))
#else
#define TTF_ENDIAN_WORD(a) (a)
#define TTF_ENDIAN_DWORD(a) (a)
#endif

#ifdef TTF_DEBUG
#define ttf_dbg_print(...) printf(__VA_ARGS__)
#else
#define ttf_dbg_print(...) 
#endif

static char	ttf_err_buf[1024] = { '\0' };
#if 1
#define ttf_err(...) 
#else
#define ttf_err(...) snprintf(ttf_err_buf, sizeof(ttf_err_buf), __VA_ARGS__)
#endif
#define ttf_warn(...) fprintf(stderr, __VA_ARGS__)

/* TTF constants */

/* magic number and sfnt versions */
#define TTF_MAGIC_NUM	(0xF53C0F5F)
#define TTF_SFNT_1_0	(0x00010000)
#define TTF_SFNT_OTTO	(0x4F54544F)

/* table tags */
#define TTF_CMAP_TAG	(0x636D6170)
#define TTF_GLYF_TAG	(0x676C7966)
#define TTF_HEAD_TAG	(0x68656164)
#define TTF_HHEA_TAG	(0x68686561)
#define TTF_HMTX_TAG	(0x686D7478)
#define TTF_LOCA_TAG	(0x6C6F6361)
#define TTF_MAXP_TAG	(0x6D617870)

/* control points */
#define TTF_ON_CURVE (0x01)
#define TTF_XSHORT (0x02)
#define TTF_YSHORT (0x04)
#define TTF_FLAG_REPEAT (0x08)
#define TTF_XREPEAT (0x10)
#define TTF_YREPEAT (0x20)

#define TTF_WORD_ARGUMENTS (0x0001)
#define TTF_ARGUMENTS_ARE_XY (0x0002)
#define TTF_ROUND_XY_TO_GRID (0x0004)
#define TTF_SCALE (0x0008)
#define TTF_RESERVED (0x0010)
#define TTF_MORE_COMPONENTS (0x0020)
#define TTF_XY_SCALE (0x0040)
#define TTF_MATRIX2 (0x0080)
#define TTF_INSTRUCTIONS (0x0100)
#define TTF_USE_THESE_METRICS (0x0200)

#define TTF_GLYPH_TBL_SIZE (256)

static int ttf_read_gh(FILE* file, ttf_glyph_header_t* gh);
static int ttf_seek_header(FILE* file, ttf_table_header_t* table);
static int ttf_load_headers(FILE* file,
		ttf_t* ttf, const ttf_tbl_directory_t* td);
static int ttf_load_glyf(FILE* file, ttf_t* ttf);
static int ttf_load_cmap(FILE* file, ttf_t* ttf);
static int ttf_load_cmap_subtable(FILE* file,
		ttf_t* ttf, ttf_enctbl_header_t* eth);
static int ttf_load_segmap4(FILE* file, ttf_t* ttf);
static int ttf_load_head(FILE* file, ttf_t* ttf);
static int ttf_load_hhea(FILE* file, ttf_t* ttf);
static int ttf_load_hmtx(FILE* file, ttf_t* ttf);
static int ttf_load_loca(FILE* file, ttf_t* ttf);
static int ttf_load_maxp(FILE* file, ttf_t* ttf);
static uint16_t ttf_interpolate_chr(
		ttf_t*		ttf,
		uint16_t	chr,
		vector_t*	cpoints,
		vector_t*	points,
		uint16_t*	cpind,
		uint16_t	e);

/* Returns the last error string
 */
const char* ttf_strerror()
{
	return ttf_err_buf;
}

ttf_t* new_ttf()
{
	ttf_t*	obj;

	obj = malloc(sizeof(ttf_t));
	obj->glyph_table = NULL;
	obj->glyph_data = NULL;
	obj->plhmtx = NULL;
	obj->plsb = NULL;
	obj->interpolation_level = 1;
	obj->ppem = 12;
	obj->resolution = 96;/* Screen resolution DPI */

	obj->idx2loc = NULL;
	obj->hh = NULL;
	obj->fh = NULL;

	obj->cmap = NULL;
	obj->glyf = NULL;
	obj->head = NULL;
	obj->hhea = NULL;
	obj->hmtx = NULL;
	obj->loca = NULL;
	obj->maxp = NULL;
	return obj;
}

/* Frees a TTF structure
 */
void free_ttf(ttf_t** obj)
{
	ttf_t*	p;
	assert(obj != NULL);

	p = *obj;

	if (!p) return;

	if (p->glyph_table) free(p->glyph_table);

	/* free glyph data structure */
	if (p->glyph_data) {
		int i;
		for (i = 0; i < p->nglyphs; i++) {
			free(p->glyph_data[i].endpoints);
			free(p->glyph_data[i].px);
			free(p->glyph_data[i].py);
			free(p->glyph_data[i].state);
		}
		free(p->glyph_data);
	}

	if (p->plhmtx) free(p->plhmtx);

	if (p->plsb) free(p->plsb);

	/* free indextolocation */
	if (p->idx2loc) free(p->idx2loc);

	/* free horizontal header */
	if (p->hh) free(p->hh);

	/* free font header */
	if (p->fh) free(p->fh);

	/* free table headers */
	if (p->cmap) free(p->cmap);
	if (p->glyf) free(p->glyf);
	if (p->head) free(p->head);
	if (p->hhea) free(p->hhea);
	if (p->hmtx) free(p->hmtx);
	if (p->loca) free(p->loca);
	if (p->maxp) free(p->maxp);

	free(p);
	*obj = NULL;
}

ttf_glyph_data_t* ttf_new_glyph_data()
{
	ttf_glyph_data_t*	obj;

	obj = malloc(sizeof(*obj));
	obj->endpoints = NULL;
	obj->state = NULL;
	obj->px = NULL;
	obj->py = NULL;
	return obj;
}

void ttf_free_glyph_data(ttf_glyph_data_t* gd)
{
}

/* Seek to the position of a header
 *
 * Returns 1 on error
 */
int ttf_seek_header(FILE* file, ttf_table_header_t* table)
{
	if (-1 == fseek(file, table->offset, SEEK_SET)) {
		ttf_err("Read error in file: %s",
				strerror(errno));
		return 1;
	}
	return 0;
}

int ttf_read_gh(FILE* file, ttf_glyph_header_t* gh)
{
	if (1 != fread(gh, sizeof(*gh), 1, file)) {
		ttf_err("Read error in file: %s",
				strerror(errno));
		return 1;
	}
	gh->number_of_contours = TTF_ENDIAN_WORD(gh->number_of_contours);
	gh->xmin = TTF_ENDIAN_WORD(gh->xmin);
	gh->ymin = TTF_ENDIAN_WORD(gh->ymin);
	gh->xmax = TTF_ENDIAN_WORD(gh->xmax);
	gh->ymax = TTF_ENDIAN_WORD(gh->ymax);
	return 0;
}

/* load the required TTF headers
 *
 * Returns 1 on error
 */
int ttf_load_headers(FILE* file, ttf_t* ttf, const ttf_tbl_directory_t* td)
{
	int i;

	ttf_dbg_print("loading table headers\n");

	for (i = 0; i < td->num_tables; i++) {
		ttf_table_header_t*	header =
			malloc(sizeof(ttf_table_header_t));
		if (1 != fread(header, sizeof(*header), 1, file)) {
			ttf_err("Read error in file: %s",
					strerror(errno));
			return 1;
		}
		header->tag = TTF_ENDIAN_DWORD(header->tag);
		header->checksum = TTF_ENDIAN_DWORD(header->checksum);
		header->offset = TTF_ENDIAN_DWORD(header->offset);
		header->length = TTF_ENDIAN_DWORD(header->length);
		switch (header->tag) {
			case TTF_CMAP_TAG:
				ttf_dbg_print("found 'cmap' table\n");
				ttf->cmap = header;
				break;
			case TTF_GLYF_TAG:
				ttf_dbg_print("found 'glyf' table\n");
				ttf->glyf = header;
				break;
			case TTF_HEAD_TAG:
				ttf_dbg_print("found 'head' table\n");
				ttf->head = header;
				break;
			case TTF_HHEA_TAG:
				ttf_dbg_print("found 'hhea' table\n");
				ttf->hhea = header;
				break;
			case TTF_HMTX_TAG:
				ttf_dbg_print("found 'hmtx' table\n");
				ttf->hmtx = header;
				break;
			case TTF_LOCA_TAG:
				ttf_dbg_print("found 'loca' table\n");
				ttf->loca = header;
				break;
			case TTF_MAXP_TAG:
				ttf_dbg_print("found 'maxp' table\n");
				ttf->maxp = header;
				break;
			default:
				free(header);
		}
	}

	/* Check that all required headers were found */
	if (!ttf->cmap) {
		ttf_err("'cmap' table is missing in font file");
		return 1;
	}
	if (!ttf->glyf) {
		ttf_err("'glyf' table is missing in font file");
		return 1;
	}
	if (!ttf->head) {
		ttf_err("'head' table is missing in font file");
		return 1;
	}
	if (!ttf->hhea) {
		ttf_err("'hhea' table is missing in font file");
		return 1;
	}
	if (!ttf->hmtx) {
		ttf_err("'hmtx' table is missing in font file");
		return 1;
	}
	if (!ttf->loca) {
		ttf_err("'loca' table is missing in font file");
		return 1;
	}
	if (!ttf->maxp) {
		ttf_err("'maxp' table is missing in font file");
		return 1;
	}

	return 0;
}

/* Load 'cmap' table - character mappings
 *
 * Returns 1 on error
 */
int ttf_load_cmap(FILE* file, ttf_t* ttf)
{
	ttf_cmap_t	cth;
	int found_mapping = 0;
	ttf_enctbl_header_t* eth;
	int i;

	ttf_dbg_print("loading cmap table\n");

	if (ttf_seek_header(file, ttf->cmap))
		return 1;

	fread(&cth, sizeof(cth), 1, file);
	cth.table_version = TTF_ENDIAN_WORD(cth.table_version);
	cth.num_tables = TTF_ENDIAN_WORD(cth.num_tables);

	if (cth.table_version != 0x0000) {
		ttf_warn("Warning: unexpected cmap table version (%08X)\n",
				cth.table_version);
	}

	if (cth.num_tables == 0) {
		ttf_err("No encoding tables found!");
		return 1;
	}

	eth =
		malloc(sizeof(ttf_enctbl_header_t) * cth.num_tables);
	for (i = 0; i < cth.num_tables; i++) {
		fread(&eth[i], sizeof(ttf_enctbl_header_t), 1, file);
		eth[i].platform_id = TTF_ENDIAN_WORD(eth[i].platform_id);
		eth[i].encoding_id = TTF_ENDIAN_WORD(eth[i].encoding_id);
		eth[i].offset = TTF_ENDIAN_DWORD(eth[i].offset);
	}
	for (i = 0; i < cth.num_tables; i++) {
		if (eth[i].platform_id == 3 && eth[i].encoding_id == 1) {
			/* Unicode BMP (UCS-2) */
			found_mapping = 1;
			if (ttf_load_cmap_subtable(file, ttf, &eth[i]))
				return 1;
			break;
		}
	}
	free(eth);

	if (!found_mapping) {
		ttf_err("No supported encoding table found!");
		return 1;
	} else return 0;
}

int ttf_load_cmap_subtable(FILE* file, ttf_t* ttf, ttf_enctbl_header_t* eth)
{
	uint16_t	format;
	if (-1 == fseek(file, eth->offset + ttf->cmap->offset, SEEK_SET)) {
		ttf_err("Read error in file: %s", strerror(errno));
		return 1;
	}

	if (1 != fread(&format, sizeof(format), 1, file)) {
		ttf_err("Read error in file: %s", strerror(errno));
		return 1;
	}
	format = TTF_ENDIAN_WORD(format);

	/* rewind */
	if (-1 == fseek(file, eth->offset + ttf->cmap->offset, SEEK_SET)) {
		ttf_err("Read error in file: %s", strerror(errno));
		return 1;
	}


	switch (format) {
		case 0:
			// Apple 256-character map
			ttf_warn("Warning: Apple 256 character "
					"mapping not supported\n");
			break;
		case 2:
			// High-byte mapping through table
			ttf_warn("Warning: High-byte mapping through "
					"table not supported\n");
			break;
		case 4:
			// Segment mapping to delta values
			ttf_load_segmap4(file, ttf);
			break;
		case 6:
			// Trimmed table mapping
			ttf_warn("Warning: Trimmed table not supported\n");
			break;
		case 8:
			// mixed 16-bit and 32-bit coverage
			ttf_warn("Warning: Mixed 16/32-bit coverage "
					"not supported\n");
			break;
		case 10:
			// Trimmed array
			ttf_warn("Warning: Trimmed array not supported\n");
			break;
		case 12:
			// Segmented coverage
			ttf_warn("Warning: Segmented coverage not supported\n");
			break;
		case 13:
			// Many-to-one range mappings
			ttf_warn("Warning: Many-to-one range mapping "
					"not supported\n");
			break;
		case 14:
			// Unicode Variation Sequences
			ttf_warn("Warning: UVS not supported\n");
			break;
		default:
			ttf_warn("Warning: unrecognized cmap subtable "
					"format (format %d)\n", format);
	}


	return 0;
}

int ttf_load_segmap4(FILE* file, ttf_t* ttf)
{
	long length;
	int i;
	uint32_t c;
	ttf_mapfmt4_header_t mf4h;

	fread(&mf4h.format, sizeof(mf4h.format), 1, file);
	mf4h.format = TTF_ENDIAN_WORD(mf4h.format);
	assert(mf4h.format == 4);

	fread(&mf4h.length, sizeof(mf4h.length), 1, file);
	fread(&mf4h.version, sizeof(mf4h.version), 1, file);
	fread(&mf4h.seg_count_2, sizeof(mf4h.seg_count_2), 1, file);
	mf4h.seg_count_2 = TTF_ENDIAN_WORD(mf4h.seg_count_2) >> 1;
	fread(&mf4h.search_range, sizeof(mf4h.search_range), 1, file);
	fread(&mf4h.entry_selector, sizeof(mf4h.entry_selector), 1, file);
	fread(&mf4h.range_shift, sizeof(mf4h.range_shift), 1, file);
	mf4h.end_count = malloc(sizeof(uint16_t) * mf4h.seg_count_2);
	fread(mf4h.end_count, sizeof(uint16_t), mf4h.seg_count_2, file);
	fread(&mf4h.reserved_pad, sizeof(mf4h.reserved_pad), 1, file);
	if (mf4h.reserved_pad != 0) {
		ttf_warn("Warning: reservedPad is nonzero");
	}
	mf4h.start_count = malloc(sizeof(uint16_t) * mf4h.seg_count_2);
	fread(mf4h.start_count, sizeof(uint16_t), mf4h.seg_count_2, file);
	mf4h.id_delta = malloc(sizeof(int16_t) * mf4h.seg_count_2);
	fread(mf4h.id_delta, sizeof(int16_t), mf4h.seg_count_2, file);
	length = (TTF_ENDIAN_WORD(mf4h.length) -
		16 + (mf4h.seg_count_2 << 3)) >> 1;
	mf4h.id_range_offset = malloc(sizeof(uint16_t) * length);
	fread(mf4h.id_range_offset, sizeof(uint16_t), length, file);

	/* allocate space for the table */
	ttf->glyph_table = malloc(sizeof(uint32_t) * TTF_GLYPH_TBL_SIZE);
	for (i = 0; i < mf4h.seg_count_2; i++) {
		mf4h.end_count[i] = TTF_ENDIAN_WORD(mf4h.end_count[i]);
		mf4h.start_count[i] = TTF_ENDIAN_WORD(mf4h.start_count[i]);
		mf4h.id_delta[i] = TTF_ENDIAN_WORD(mf4h.id_delta[i]);
	}
	for (i = 0; i < length; i++) {
		mf4h.id_range_offset[i] = TTF_ENDIAN_WORD(mf4h.id_range_offset[i]);
	}
	for (c = 0; c < TTF_GLYPH_TBL_SIZE; c++) {
		int i;
		for (i = 0; i < mf4h.seg_count_2; i++) {
			if (mf4h.end_count[i] >= c) {
				if (mf4h.start_count[i] > c) {
					//mf4h.start_count[i] måste vara <= c
					ttf->glyph_table[c] = 0;
					break;
				}

				if (mf4h.id_range_offset[i] != 0) {
					ttf->glyph_table[c] =
						*((mf4h.id_range_offset[i] >> 1)
						+ (c - mf4h.start_count[i])
						+ &mf4h.id_range_offset[i]);
					if (ttf->glyph_table[c] != 0)
						ttf->glyph_table[c] += mf4h.id_delta[i];
					break;
				} else {
					ttf->glyph_table[c] = mf4h.id_delta[i] + c;
					break;
				}
			}
		}
		// is the last segment 0xFFFF always zero?
		if (i == mf4h.seg_count_2-1)
			ttf->glyph_table[c] = 0;
	}
	free(mf4h.end_count);
	free(mf4h.start_count);
	free(mf4h.id_delta);
	free(mf4h.id_range_offset);

	return 0;
}

/* Load 'glyf' table - glyphs
 *
 * Returns 1 on error
 */
int ttf_load_glyf(FILE* file, ttf_t* ttf)
{
	int i;
	ttf_glyph_header_t gh;

	ttf_dbg_print("loading glyf table\n");

	if (ttf_seek_header(file, ttf->glyf))
		return 1;

	ttf->glyph_data = malloc(sizeof(ttf_glyph_data_t) * ttf->nglyphs);
	for (i = 0; i < ttf->nglyphs; i++) {
		size_t target;
		if (ttf->idx2loc[i] == ttf->idx2loc[i+1]) {
			ttf->glyph_data[i].npoints = 0;
			ttf->glyph_data[i].endpoints = NULL;
			ttf->glyph_data[i].ncontours = 0;
			ttf->glyph_data[i].px = NULL;
			ttf->glyph_data[i].py = NULL;
			ttf->glyph_data[i].state = NULL;
			ttf_set_ls_aw(ttf, &gh, &ttf->glyph_data[i], i);
			continue;
		}
		target = ttf->glyf->offset + ttf->idx2loc[i];
		if (-1 == fseek(file, target, SEEK_SET)) {
			ttf_err("Read error in file: %s",
					strerror(errno));
			return 1;
		}
		if (ttf_read_gh(file, &gh)) return 1;
		if (ttf_read_glyph(ttf, file, &gh,
				&ttf->glyph_data[i],
				ttf->glyf, i))
			return 1;
	}
	
	return 0;
}

/* Load 'head' table - font header
 *
 * Returns 1 on error
 */
int ttf_load_head(FILE* file, ttf_t* ttf)
{
	ttf_dbg_print("loading head table\n");

	if (ttf_seek_header(file, ttf->head))
		return 1;

	ttf->fh = malloc(sizeof(ttf_head_t));
	if (1 != fread(ttf->fh, sizeof(ttf_head_t), 1, file)) {
		ttf_err("Read error in file: %s",
				strerror(errno));
		return 1;
	}

	if (ttf->fh->magic != TTF_MAGIC_NUM) {
		ttf_err("Incorrect magic number: "
				"expected %08X, was %08X",
				TTF_MAGIC_NUM, ttf->fh->magic);
		return 1;
	}

	ttf->upem = TTF_ENDIAN_WORD(ttf->fh->upem);
	ttf->ymin = TTF_ENDIAN_WORD(ttf->fh->ymin);
	ttf->ymax = TTF_ENDIAN_WORD(ttf->fh->ymax);
	ttf->xmin = TTF_ENDIAN_WORD(ttf->fh->xmin);
	ttf->xmax = TTF_ENDIAN_WORD(ttf->fh->xmax);
	ttf->zerobase = 0;
	ttf->zerolsb = 0;

	ttf->fh->flags = TTF_ENDIAN_WORD(ttf->fh->flags);
	if (ttf->fh->flags & 0x0001) ttf->zerobase = 1;
	if (ttf->fh->flags & 0x0002) ttf->zerolsb = 1;

	return 0;
}

/* Load 'hhea' table - horizontal header
 *
 * Returns 1 on error
 */
int ttf_load_hhea(FILE* file, ttf_t* ttf)
{
	ttf_dbg_print("loading hhea table\n");

	if (ttf_seek_header(file, ttf->hhea))
		return 1;

	ttf->hh = malloc(sizeof(ttf_hhea_t));
	fread(ttf->hh, sizeof(ttf_hhea_t), 1, file);
	ttf->hh->num_h_metrics = TTF_ENDIAN_WORD(ttf->hh->num_h_metrics);

	return 0;
}

/* Load 'hmtx' table - horizontal metrics
 *
 * Returns 1 on error
 */
int ttf_load_hmtx(FILE* file, ttf_t* ttf)
{
	int i;

	ttf_dbg_print("loading hmtx table\n");

	if (ttf_seek_header(file, ttf->hmtx))
		return 1;

	ttf->plhmtx = malloc(sizeof(ttf_lhmetrics_t) * ttf->hh->num_h_metrics);
	ttf->nhmtx = ttf->hh->num_h_metrics;
	fread(ttf->plhmtx, sizeof(ttf_lhmetrics_t), ttf->hh->num_h_metrics, file);
	i = ttf->nglyphs - ttf->hh->num_h_metrics;
	ttf->plsb = malloc(sizeof(int16_t) * i);
	if (i) fread(ttf->plsb, sizeof(int16_t), i, file);

	return 0;
}

/* Load 'loca' table - index to location
 *
 * Returns 1 on error
 */
int ttf_load_loca(FILE* file, ttf_t* ttf)
{
	ttf_dbg_print("loading loca table\n");

	if (ttf_seek_header(file, ttf->loca))
		return 1;

	ttf->idx2loc = malloc(sizeof(uint32_t) * (ttf->nglyphs + 1));
	ttf->fh->index_to_loc_format = TTF_ENDIAN_WORD(ttf->fh->index_to_loc_format);
	if (ttf->fh->index_to_loc_format == 0) {
		int i;
		// short offsets
		uint16_t* indextolocation2 = malloc(sizeof(uint16_t) * (ttf->nglyphs + 1));
		fread(indextolocation2, sizeof(uint16_t), (ttf->nglyphs+1), file);
		for (i = 0; i <= ttf->nglyphs; i++)
			ttf->idx2loc[i] = (uint32_t)
				(((0xFF00 & indextolocation2[i]) >> 8) |
				 ((0x00FF & indextolocation2[i]) << 8)) << 1;
		free(indextolocation2);
	} else {
		int i;
		// long offsets
		fread(ttf->idx2loc, sizeof(uint32_t), (ttf->nglyphs+1), file);
		for (i = 0; i <= ttf->nglyphs; i++)
			ttf->idx2loc[i] = TTF_ENDIAN_DWORD(ttf->idx2loc[i]);
	}

	return 0;
}

/* Load 'maxp' table - maximum profiles
 *
 * Returns 1 on error
 */
int ttf_load_maxp(FILE* file, ttf_t* ttf)
{
	ttf_max_profile_tbl_header_t	mpth;

	ttf_dbg_print("loading maxp table\n");

	if (ttf_seek_header(file, ttf->maxp))
		return 1;

	if (1 != fread(&mpth, sizeof(mpth), 1, file)) {
		ttf_err("Read error in file: %s",
				strerror(errno));
		return 1;
	}

	ttf->nglyphs = TTF_ENDIAN_WORD(mpth.numGlyphs);
	return 0;
}

/* Load a TrueType font
 *
 * Returns NULL on error
 * Error messages are returned by ttf_strerror
 */
ttf_t* ttf_load(FILE* file)
{
	ttf_t*	ttf;
	ttf_tbl_directory_t td;

	ttf_dbg_print("loading TrueType font\n");

	if (file == NULL) {
		ttf_err("Can not read from null file");
		return NULL;
	}

	if (1 != fread(&td, sizeof(td), 1, file)) {
		ttf_err("Read error in file: %s",
				strerror(errno));
		return NULL;
	}
	td.sfnt_version = TTF_ENDIAN_DWORD(td.sfnt_version);
	td.num_tables = TTF_ENDIAN_WORD(td.num_tables);
	if (td.sfnt_version == TTF_SFNT_1_0) {
		ttf_dbg_print("sftn version: 1.0\n");
	} else if (td.sfnt_version == TTF_SFNT_OTTO) {
		ttf_dbg_print("sftn version: OTTO\n");
	} else {
		ttf_err("Unrecognized sfnt version: %08X", td.sfnt_version);
		return NULL;
	}

	ttf = new_ttf();

	if (ttf_load_headers(file, ttf, &td)) goto err;
	if (ttf_load_head(file, ttf)) goto err;
	if (ttf_load_maxp(file, ttf)) goto err;
	if (ttf_load_hhea(file, ttf)) goto err;
	if (ttf_load_hmtx(file, ttf)) goto err;
	if (ttf_load_cmap(file, ttf)) goto err;
	if (ttf_load_loca(file, ttf)) goto err;
	if (ttf_load_glyf(file, ttf)) goto err;

	ttf_dbg_print("TrueType font loaded successfully\n");

	return ttf;

err:
	free_ttf(&ttf);
	return NULL;
}

/*
 * Read glyphs.
 *
 * Här laddas de enskilda tecknen "glyphs".
 * Sammansatta tecken är t.ex. ä, sammansatt av ¨ och a i de
 * flesta typsnitt. Detta är dock helt beroende av vad tillverkaren
 * tycker är lämpligt.
 *
 * Alla konverteringar är från TrueType-format till plattformens format.
 * Lägg märke till att endian varierar mellan olika plattformer.
 *
 * Returns 1 on error
*/
int ttf_read_glyph(ttf_t*		ttf,
		FILE*			file,
		ttf_glyph_header_t*	gh,
		ttf_glyph_data_t*	gd,
		ttf_table_header_t*	glyf,
		uint32_t		i)
{
	ttf_simpglyphinfo_t simple;
	int j, numpoints = 0;
	uint8_t*	flagbuff;
	uint16_t	flagind = 0;
	uint32_t	flagpos;
	uint8_t		tmpb;
	uint8_t		tmpb2;
	int16_t* px;
	int16_t* py;
	int* state;
	uint8_t* pointbuff;
	uint32_t pointind = 0;
	int is_short = 0;
	int is_prev = 0;
	int16_t last_point = 0;
	if (gh->number_of_contours < 0) {
		// load composite glyph
		uint16_t		contour;
		uint16_t		flags;
		uint16_t		glyph_index;
		uint16_t		xoff;
		uint16_t		yoff;
		uint16_t		npoints = 0;
		uint16_t		ncontours = 0;
		uint32_t		readpos;
		ttf_glyph_data_t* 	component;
		list_t*			components = NULL;
		int			norigmtx = 0;
		uint16_t*	endpoints;
		int*		state;
		int16_t*	px;
		int16_t*	py;
		uint16_t	point_off = 0;
		uint16_t	contour_off = 0;
		uint16_t	currentPoint;
		int		ccoff;
		int		coff;
		int		olim;
		int		firstrun;
		list_t* p;
		list_t* h;
		int i = 0;
		
		do {
			fread(&flags, sizeof(flags), 1, file);
			flags = TTF_ENDIAN_WORD(flags);
			fread(&glyph_index, sizeof(glyph_index), 1, file);
			glyph_index = TTF_ENDIAN_WORD(glyph_index);

			//Load subglyph{
				readpos = ftell(file);
				fseek(file, glyf->offset + ttf->idx2loc[glyph_index], SEEK_SET);
				if (ttf_read_gh(file, gh)) return 1;
				component = malloc(sizeof(*component));
				ttf_read_glyph(ttf, file, gh, component,
						glyf, glyph_index);
				list_add(&components, component);
				fseek(file, readpos, SEEK_SET);
			//}
			
			// flag out of range check
			assert(flags < 0x800);

			if (flags & TTF_WORD_ARGUMENTS) {
				fread(&xoff, sizeof(xoff), 1, file);
				xoff = TTF_ENDIAN_WORD(xoff);
				fread(&yoff, sizeof(yoff), 1, file);
				yoff = TTF_ENDIAN_WORD(yoff);
			} else {
				uint16_t tmp;
				fread(&tmp, sizeof(tmp), 1, file);
				yoff = 0xFF & tmp;
				xoff = tmp >> 8;
			}
			if (flags & TTF_ARGUMENTS_ARE_XY) {
				for (contour = 0; contour < component->npoints; contour++) {
					component->px[contour] += xoff;
					component->py[contour] += yoff;
				}
			}
			if (flags & TTF_SCALE) {
				float		scale;
				uint16_t	f2dot14;
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				scale = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				scale += ((float) f2dot14) / 16383.f; //16383 == 0x3FFF
				for (contour = 0; contour < component->npoints; contour++) {
					component->px[contour] = (int16_t)(component->px[contour] * scale);
					component->py[contour] = (int16_t)(component->py[contour] * scale);
				}
			} else if (flags & TTF_XY_SCALE) {
				float		xScale;
				float		yScale;
				uint16_t	f2dot14;
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				xScale = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				xScale += ((float) f2dot14) / 16383.f; //16383 == 0x3FFF
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				yScale = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				yScale += ((float) f2dot14) / 16383.f;
				for (contour = 0; contour < component->npoints; contour++)
				{
					component->px[contour] = (int16_t)(component->px[contour] * xScale);
					component->py[contour] = (int16_t)(component->py[contour] * yScale);
				}
			} else if (flags & TTF_MATRIX2) {
				float	transform_11;
				float	transform_12;
				float	transform_21;
				float	transform_22;

				uint16_t f2dot14;
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				transform_11 = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				transform_11 += ((float) f2dot14) / 16383.f; //16383 == 0x3FFF
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				transform_12 = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				transform_12 += ((float) f2dot14) / 16383.f;
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				transform_21 = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				transform_21 += ((float) f2dot14) / 16383.f;
				fread(&f2dot14, sizeof(f2dot14), 1, file);
				f2dot14 = TTF_ENDIAN_WORD(f2dot14);
				//convert f2dot14 to float
				transform_22 = (float) ((0xC000 & f2dot14) >> 14);
				f2dot14 &= 0x3FFF;
				transform_22 += ((float) f2dot14) / 16383.f;
				for (contour = 0; contour < component->npoints; contour++) {
					component->px[contour] = (int16_t)(component->px[contour] * transform_11 + component->py[contour] * transform_21);
					component->py[contour] = (int16_t)(component->py[contour] * transform_12 + component->px[contour] * transform_22);
				}
			}

			if (flags & TTF_USE_THESE_METRICS) {
				norigmtx = 1;
				ttf_set_ls_aw(ttf, gh, gd, glyph_index);
			}
			npoints += component->npoints;
			ncontours += component->ncontours;
		} while (flags & TTF_MORE_COMPONENTS);
		gd->npoints = npoints;
		gd->ncontours = ncontours;

		//use the components list to create a composite glyph
		endpoints = malloc(sizeof(uint16_t)*ncontours);
		state = malloc(sizeof(int)*npoints);
		px = malloc(sizeof(int16_t)*npoints);
		py = malloc(sizeof(int16_t)*npoints);
		point_off = 0;
		contour_off = 0;

		memset(endpoints, 0, ncontours << 1);

		p = h = components;
		do {
			component = p->data;
			olim = component->ncontours - 1;
			firstrun = 1;
			for (ccoff = contour_off, coff = 0; ccoff < ncontours; ccoff++) {
				if (coff < olim) {
					endpoints[ccoff] += component->endpoints[coff++];
				} else {
					if (firstrun) {
						endpoints[ccoff] += component->endpoints[coff];
						firstrun = 0;
					} else {
						endpoints[ccoff] += component->endpoints[coff] + 1;
					}
				}
			}
			currentPoint = component->npoints << 1;
			memcpy(&px[point_off], component->px,
				currentPoint);
			memcpy(&py[point_off], component->py,
				currentPoint);
			memcpy(&state[point_off], component->state,
				currentPoint >> 1);
			point_off += component->npoints;
			contour_off += component->ncontours;

			++i;
			p = p->succ;
		} while (p != h);

		while (components) {
			ttf_glyph_data_t*	gd;
			gd = components->data;
			free(gd->endpoints);
			free(gd->state);
			free(gd->px);
			free(gd->py);
			free(gd);
			list_remove(&components);
		}

		gd->px = px;
		gd->py = py;
		gd->state = state;
		gd->endpoints = endpoints;
		if (norigmtx) return 0;
		ttf_set_ls_aw(ttf, gh, gd, i);
		return 0;
	}

	//Load simple glyph
	simple.endpoints_contour =
		malloc(sizeof(uint16_t) * gh->number_of_contours);
	fread(simple.endpoints_contour, sizeof(uint16_t), gh->number_of_contours, file);
	//calculate the number of points in the glyph
	numpoints = 0;
	for (j = 0; j < gh->number_of_contours; j++) {
		simple.endpoints_contour[j] = TTF_ENDIAN_WORD(simple.endpoints_contour[j]);
		if (simple.endpoints_contour[j] > numpoints)
			numpoints = simple.endpoints_contour[j];
	}
	numpoints++;
	fread(&simple.instruction_length, sizeof(simple.instruction_length), 1, file);
	if (numpoints <= 0) {
		return 0;
	}

	// read all the flags
	simple.instruction_length = TTF_ENDIAN_WORD(simple.instruction_length);
	simple.instructions = malloc(sizeof(uint8_t) * simple.instruction_length);
	fread(simple.instructions, sizeof(uint8_t), simple.instruction_length, file);
	simple.flags = malloc(sizeof(uint8_t)*numpoints);
	flagbuff = malloc(sizeof(uint8_t)*numpoints);
	flagind = 0;
	flagpos = ftell(file);
	fread(flagbuff, sizeof(uint8_t), numpoints, file);
	j = 0;
	while (j < numpoints) {
		tmpb = flagbuff[flagind++];
		simple.flags[j++] = tmpb;
		if (tmpb & TTF_FLAG_REPEAT) {
			unsigned char rt;
			tmpb2 = flagbuff[flagind++];
			for (rt = 0; rt < tmpb2; rt++)
				simple.flags[j++] = tmpb;
		}
	}
	free(flagbuff);
	fseek(file, flagpos+flagind, SEEK_SET);
	px = malloc(sizeof(int16_t) * numpoints);
	py = malloc(sizeof(int16_t) * numpoints);
	state = malloc(sizeof(int) * numpoints);
	/*
		Read in the maximum ammount of data needed for the
		points. This method should be faster than just
		reading point by point.
	*/
	pointbuff = malloc(sizeof(uint8_t) * (numpoints << 2));
	fread(pointbuff, sizeof(uint8_t), numpoints << 2, file);
	pointind = 0;
	is_short = 0;
	is_prev = 0;
	last_point = 0;
	//xpass
	for (j = 0; j < numpoints; j++) {
		//on-curve check {
			if (simple.flags[j] & TTF_ON_CURVE) {
				state[j] = 1;
			} else {
				state[j] = 0;
			}
		//}
		if (simple.flags[j] & TTF_XSHORT) {
			//x-short
			is_short = 1;
		}
		if (simple.flags[j] & TTF_XREPEAT) {
			//x-prev
			is_prev = 1;
		}
		if (is_short) {
			if (is_prev) {
				last_point += pointbuff[pointind++];
				px[j] = last_point;
			} else {
				last_point -= pointbuff[pointind++];
				px[j] = last_point;
			}
		} else {
			if (is_prev) {
				px[j] = last_point;
			} else {
				last_point += (int16_t)(pointbuff[pointind+1] | (pointbuff[pointind] << 8));
				pointind += 2;
				px[j] = last_point;
			}
		}
		//reset
		is_short = is_prev = 0;
	}
	//ypass
	last_point = 0;
	for (j = 0; j < numpoints; j++) {
		if (simple.flags[j] & TTF_YSHORT) {
			//y-short
			is_short = 1;
		}
		if (simple.flags[j] & TTF_YREPEAT) {
			//y-prev
			is_prev = 1;
		}
		if (is_short) {
			if (is_prev) {
				last_point += pointbuff[pointind++];
				py[j] = last_point;
			} else {
				last_point -= pointbuff[pointind++];
				py[j] = last_point;
			}
		} else {
			if (is_prev) {
				py[j] = last_point;
			} else {
				last_point += (int16_t)(pointbuff[pointind+1] | (pointbuff[pointind] << 8));
				pointind += 2;
				py[j] = last_point;
			}
		}
		//reset
		is_short = is_prev = 0;
	}
	free(pointbuff);
	//save information to the GlyphData struct obj.
	gd->npoints = (uint16_t) numpoints;
	gd->endpoints = simple.endpoints_contour;
	gd->ncontours = gh->number_of_contours;
	gd->px = px;
	gd->py = py;
	gd->state = state;
	ttf_set_ls_aw(ttf, gh, gd, i);
	free(simple.flags);
	free(simple.instructions);
	return 0;
}

/* ttf-glyph_table[chr] must be < ttf->nglyphs
 * TODO: add error handling
 */
float ttf_char_width(ttf_t* ttf, uint16_t chr)
{
	assert(ttf != NULL);
	return (float)(ttf->glyph_data[ttf->glyph_table[chr]].aw) / ttf->upem;
}

float ttf_line_width(ttf_t* type, const char* line)
{
	const char* p;
	float	width;
	assert(type != NULL);

	p = line;

	width = 0;
	while (*p != '\0') {
		wchar_t 	wc;
		int		n;

		n = mbtowc(&wc, p, MB_CUR_MAX);
		if (n == -1) break;
		else p += n;

		width += ttf_char_width(type, wc);
	}
	return width;
}

/* ttf->glyph_table[chr] must be < ttf->nglyphs
 * TODO: add better error handling
 *
 * Returns the exported shape, or NULL on failure.
 */
shape_t* ttf_export_chr_shape(ttf_t* ttf, uint16_t chr)
{
	if (ttf->glyph_table[chr] >= ttf->nglyphs)
		return NULL;

	if (ttf->interpolation_level) {
		// interpolate the curves
		shape_t*	shape = new_shape();
		vector_t*	points;
		uint16_t*	endpoints;
		uint16_t	lim = 0;
		uint16_t	e;
		uint16_t	p = 0;
		//uint16_t	p2 = 0;
		uint16_t	origin;

		ttf_interpolate(ttf, chr, &points, &endpoints,
				1.0/ttf->upem);

		for (e = 0; e < ttf->glyph_data[ttf->glyph_table[chr]].ncontours; e++) {
			lim += endpoints[e];
			origin = p;
			for (; p < lim; p++) {
				shape_add_vec(shape, points[p].x, points[p].y);
                		if (p < (lim - 1))
					shape_add_seg(shape, p, p+1);
                		else
					shape_add_seg(shape, p, origin);
			}
		}
		free(points);
		free(endpoints);

		return shape;
	} else {
		return NULL;
	}
}

uint16_t ttf_interpolate_chr(
		ttf_t*		ttf,
		uint16_t	chr,
		vector_t*	cpoints,
		vector_t*	points,
		uint16_t*	cpind,
		uint16_t	e)
{
	/*
 	 * This is a quadric Bezier curve interpolation algorithm using a
 	 * modified version of the de Casteljau algorithm to de-compose the
 	 * curve so that the actual interpolation goes faster.
 	 */
	
	int*		states = ttf->glyph_data[ttf->glyph_table[chr]].state;
	uint16_t	pind;
	uint16_t firstpoint;
	uint16_t lastpoint;
	int ls, cs, ns;
	vector_t	lp;
	vector_t	cp;
	vector_t	np;
	uint16_t	c;
	float		cx;
	float		cy;
	float		dx;
	float		ddx;
	float		dy;
	float		ddy;
	float		oa, ob, oc, oo1, oo2;
	float		m;
	float		mm;
	vector_t	ip1, ip2;
	uint16_t addon;
	int cont;

	if (e)
		pind = ttf->glyph_data[ttf->glyph_table[chr]].endpoints[e - 1] + 1;
	else
		pind = 0;
	firstpoint = pind;
	lastpoint = ttf->glyph_data[ttf->glyph_table[chr]].endpoints[e];

	// if any state is true that means that the current point is on the curve
	ls = states[lastpoint];
	ns = states[firstpoint];
	lp = points[lastpoint];
	np = points[firstpoint];

	m = 1.f / ((float) ttf->interpolation_level);
	mm = m * m;

	oa = mm - 2.f*m;
	ob = 2.f*m - 2.f*mm;
	oc = mm;
	oo1 = 2.f*mm;
	oo2 = -4.f*mm;
	addon = *cpind;
	cont = 1;
	do {
		cs = ns;
		cp = np;
		pind++;
		if (pind > lastpoint) {
			ns = states[firstpoint];
			np = points[firstpoint];
			cont = 0;
		} else {
			ns = states[pind];
			np = points[pind];
		}
		if (!ls) {
			if (!cs) {
				if (!ns) {
					//		off-off-off
					ip1.x = (lp.x + cp.x)/2.f;
					ip1.y = (lp.y + cp.y)/2.f;
					ip2.x = (cp.x + np.x)/2.f;
					ip2.y = (cp.y + np.y)/2.f;
					dx = ip1.x * oa + cp.x * ob + ip2.x * oc;
					dy = ip1.y * oa + cp.y * ob + ip2.y * oc;
					ddx = ip1.x * oo1 + cp.x * oo2 + ip2.x * oo1;
					ddy = ip1.y * oo1 + cp.y * oo2 + ip2.y * oo1;
					cx = ip1.x;
					cy = ip1.y;
					for (c = 0; c < ttf->interpolation_level; c++) {
						cpoints[*cpind].x = cx;
						cpoints[(*cpind)++].y = cy;
						cx += dx;
						cy += dy;
						dx += ddx;
						dy += ddy;
					}
				} else {
					//		off-off-on
					ip1.x = (lp.x + cp.x)/2.f;
					ip1.y = (lp.y + cp.y)/2.f;
					dx = ip1.x * oa + cp.x * ob + np.x * oc;
					dy = ip1.y * oa + cp.y * ob + np.y * oc;
					ddx = ip1.x * oo1 + cp.x * oo2 + np.x * oo1;
					ddy = ip1.y * oo1 + cp.y * oo2 + np.y * oo1;
					cx = ip1.x;
					cy = ip1.y;
					for (c = 0; c < ttf->interpolation_level; c++) {
						cpoints[*cpind].x = cx;
						cpoints[(*cpind)++].y = cy;
						cx += dx;
						cy += dy;
						dx += ddx;
						dy += ddy;
					}
				}
			} else {
				if (ns) {
					cpoints[(*cpind)++] = cp;
				}
			}
		} else {
			if (!cs) {
				if (!ns) {
					//		on-off-off
					ip2.x = (cp.x + np.x)/2.f;
					ip2.y = (cp.y + np.y)/2.f;
					dx = lp.x * oa + cp.x * ob + ip2.x * oc;
					dy = lp.y * oa + cp.y * ob + ip2.y * oc;
					ddx = lp.x * oo1 + cp.x * oo2 + ip2.x * oo1;
					ddy = lp.y * oo1 + cp.y * oo2 + ip2.y * oo1;
					cx = lp.x;
					cy = lp.y;
					for (c = 0; c < ttf->interpolation_level; c++) {
						cpoints[*cpind].x = cx;
						cpoints[(*cpind)++].y = cy;
						cx += dx;
						cy += dy;
						dx += ddx;
						dy += ddy;
					}
				} else {
					//		on-off-on
					dx = lp.x * oa + cp.x * ob + np.x * oc;
					dy = lp.y * oa + cp.y * ob + np.y * oc;
					ddx = lp.x * oo1 + cp.x * oo2 + np.x * oo1;
					ddy = lp.y * oo1 + cp.y * oo2 + np.y * oo1;
					cx = lp.x;
					cy = lp.y;
					for (c = 0; c < ttf->interpolation_level; c++) {
						cpoints[*cpind].x = cx;
						cpoints[(*cpind)++].y = cy;
						cx += dx;
						cy += dy;
						dx += ddx;
						dy += ddy;
					}
				}
					
			} else {
				if (ns) {
					cpoints[(*cpind)++] = cp;
				}
			}
		}
		ls = cs;
		lp = cp;
	} while (cont);
	return *cpind - addon;
}

/*
 * LSB == Left Side Bound
 * AW == Advance Width
 */
void ttf_set_ls_aw(ttf_t* ttf,
		ttf_glyph_header_t* gh,
		ttf_glyph_data_t* gd,
		uint16_t i)
{
	int16_t	xmin;
	int16_t	xmax;

	xmin = gh->xmin;
	xmax = gh->xmax;

	gd->maxwidth = xmax - xmin;
	if (ttf->nhmtx > i) {
		if (ttf->zerolsb) {
			gd->lsb = 0;
			gd->aw = TTF_ENDIAN_WORD(ttf->plhmtx[i].aw);
			return;
		}
		gd->lsb = xmin - TTF_ENDIAN_WORD(ttf->plhmtx[i].lsb);
		gd->aw = TTF_ENDIAN_WORD(ttf->plhmtx[i].aw);
		return;
	}

	if (ttf->zerolsb) {
		gd->lsb = 0;
		gd->aw = TTF_ENDIAN_WORD(ttf->plhmtx[ttf->nhmtx-1].aw);
		return;
	}
	gd->lsb = xmin - TTF_ENDIAN_WORD(ttf->plsb[i-ttf->nhmtx]);
	gd->aw = TTF_ENDIAN_WORD(ttf->plhmtx[ttf->nhmtx-1].aw);
}

/*
 * Transform the interpolated coordinates to the correct unit.
 */
void ttf_interpolate(
		ttf_t*		ttf,
		uint16_t	chr,		// character code
		vector_t**	points,
		uint16_t**	endpoints,
		float		scale)
{
	uint16_t	contour;
	uint16_t	point;
	uint16_t	cpind = 0;

	vector_t*		cpoints;
	ttf_glyph_data_t*	glyph;

	glyph = &(ttf->glyph_data[ttf->glyph_table[chr]]);
	cpoints = malloc(sizeof(vector_t) * glyph->npoints);
	*points = malloc(sizeof(vector_t) * glyph->npoints * ttf->interpolation_level);
	*endpoints = malloc(sizeof(uint16_t) * glyph->ncontours);

	for (contour = 0, point = 0; contour < glyph->ncontours; contour++) {
		for (; point <= glyph->endpoints[contour]; point++) {
			cpoints[point].x = scale * (glyph->px[point] - glyph->lsb);
			cpoints[point].y = scale * glyph->py[point];
		}
		(*endpoints)[contour] = ttf_interpolate_chr(
				ttf, chr, *points,
				cpoints, &cpind, contour);
	}
	free(cpoints);
}
