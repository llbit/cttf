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
/* cTTF Open Type debug and test program
 */
#include <stdio.h>

#include "ttf.h"

int main(int argc, const char** argv)
{
	const char*	fn = "font.ttf";
	FILE*		fp;
	ttf_t*		ttf;

	if (argc > 1) {
		fn = argv[1];
	}

	fp = fopen(fn, "rb");
	if (!fp) {
		perror("main");
		return 1;
	}
	ttf = ttf_load(fp);
	if (!ttf) {
		fprintf(stderr, "Error while loading font file %s:\n%s\n",
				fn, ttf_strerror());
	}

	return 0;
}
