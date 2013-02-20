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
#include <stdlib.h>
#include <assert.h>
#include "stack.h"

void stack_push(stack_t** stack, void* data)
{
	stack_t*	p;
	assert(stack != NULL);

	p = malloc(sizeof(stack_t));
	p->data = data;

	if (*stack == NULL)
		p->next = NULL;
	else
		p->next = *stack;

	*stack = p;
}

void* stack_pop(stack_t** stack)
{
	void*		data;
	stack_t*	next;
	assert(stack != NULL);
	assert(*stack != NULL);

	data = (*stack)->data;
	next = (*stack)->next;
	free(*stack);
	*stack = next;
	return data;
}

void* stack_peek(stack_t* stack)
{
	assert(stack != NULL);

	return stack->data;
}

void free_stack(stack_t** stack)
{
	assert(stack != NULL);

	while (*stack != NULL) {
		stack_t*	p = *stack;
		*stack = p->next;
		free(p);
	}
}

