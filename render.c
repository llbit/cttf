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
#include "shape.h"

void render_shape(shape_t* shape)
{
	int i;

	glBegin(GL_LINES);

	for (i = 0; i < shape->nseg; ++i) {
		int	n;
		int	m;

		n = shape->seg[i*2];
		m = shape->seg[i*2+1];
		glVertex3f(shape->vec[n].x, shape->vec[n].y, 0.f);
		glVertex3f(shape->vec[m].x, shape->vec[m].y, 0.f);
	}

	glEnd();
}

