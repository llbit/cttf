/**
 * Copyright (c) 2012 Jesper Öqvist <jesper@llbit.se>
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
 * Binary Search Tree
 */
#include <assert.h>
#include <stdlib.h>
#include "bstree.h"

static void adjust_left(bstree_t** tree);
static void adjust_right(bstree_t** tree);
static int is_leaf(bstree_t* tree);

/* Returns the number of nodes in the binary search tree
 */
unsigned bstree_size(bstree_t* tree)
{
	if (tree == NULL)
		return 0;
	else if (is_leaf(tree))
		return 1;
	else
		return bstree_size(tree->left) + bstree_size(tree->right);
	
}

/* Add a node to the tree
 *
 * cmp(x, y) must return nonzero if x is left of y
 */
void bstree_insert(bstree_t** tree, void* v, comparator_t cmp)
{
	bstree_t*	p;
	assert(tree != NULL);
	
	p = *tree;
	if (p == NULL) {
		p = malloc(sizeof(bstree_t));
		p->value = v;
		p->rvalue = v;
		p->min = v;
		p->max = v;
		p->left = NULL;
		p->right = NULL;
		p->parent = NULL;
		*tree = p;

	} else if (is_leaf(p)) {
		if (cmp(v, p->value)) {
			bstree_insert(&p->left, v, cmp);
			bstree_insert(&p->right, p->value, cmp);

			// adjust tree
			p->value = v;
			p->min = v;
			// rvalue & max unchanged
		} else {
			bstree_insert(&p->left, p->value, cmp);
			bstree_insert(&p->right, v, cmp);

			// adjust tree
			p->rvalue = v;
			p->max = v;
			// value & min unchanged
		}
	} else {
		if (cmp(v, p->value)) {
			bstree_insert(&p->left, v, cmp);

			// adjust tree
			p->value = p->left->max;
			p->min = p->left->min;
			// rvalue & max unchanged
		} else {
			bstree_insert(&p->right, v, cmp);

			// adjust tree
			p->rvalue = p->right->min;
			p->max = p->right->max;
			// value & min unchanged
		}
	}
}

/* Adjust the tree after a left leaf has been removed
 */
static void adjust_left(bstree_t** tree)
{
	bstree_t*	p = *tree;

	// adjust tree
	if (p->left == NULL) {
		if (p->right != NULL) {
			p->left = p->right;
			p->right = NULL;
			p->value = p->max;
			p->rvalue = p->max;
			p->min = p->left->min;
		} else {
			// we have no children - remove p
			free(p);
			*tree = NULL;
		}
	} else {
		p->value = p->left->max;
		p->min = p->left->min;
		if (p->right == NULL) {
			p->max = p->value;
			p->rvalue = p->value;
		} else {
			p->max = p->right->max;
			p->rvalue = p->right->min;
		}
	}
}

/* Adjust the tree after a right leaf has been removed
 */
static void adjust_right(bstree_t** tree)
{
	bstree_t*	p = *tree;

	if (p->right != NULL) {
		p->rvalue = p->right->min;
		p->max = p->right->max;
	} else if (p->left != NULL) {
		p->max = p->left->max;
		p->rvalue = p->left->max;
	}
}

/* Remove a node from a binary search tree
 *
 * cmp(x, y) must return nonzero if x is left of y
 */
void bstree_remove(bstree_t** tree, void* v, comparator_t cmp)
{
	bstree_t*	p;

	assert(tree != NULL);

	p = *tree;

	if (p == NULL) return;

	if (p->value == v) {
		if (is_leaf(p)) {
			free(p);
			*tree = NULL;
		} else {
			bstree_remove(&p->left, v, cmp);
			adjust_left(tree);
		}
	} else if (p->rvalue == v) {
		bstree_remove(&p->right, v, cmp);
		adjust_right(tree);
	} else {
		if (cmp(v, p->value) || cmp(v, p->rvalue)) {
			bstree_remove(&p->left, v, cmp);
			adjust_left(tree);
		} else {
			bstree_remove(&p->right, v, cmp);
			adjust_right(tree);
		}
	}

}

/* Remove all nodes x for which cmp(v, x) returns nonzero
 */
void bstree_remove_if(bstree_t** tree, void* v, comparator_t cmp)
{
	bstree_t*	p;

	assert(tree != NULL);

	p = *tree;

	if (p == NULL) return;

	if (is_leaf(p)) {
		if (cmp(v, p->value)) {
			free(p);
			*tree = NULL;
		}
	} else {
		bstree_remove_if(&p->right, v, cmp);
		adjust_right(tree);
		bstree_remove_if(&p->left, v, cmp);
		adjust_left(tree);
	}

}

/* Find the node immediately to the left of v
 *
 * cmp(v, x) must return nonzero if v is left of x
 */
void* bstree_find_left(bstree_t* tree, void* v, comparator_t cmp)
{
	if (tree == NULL) return NULL;

	if (is_leaf(tree))
		return tree->value;
	else if (tree->right == NULL)
		return bstree_find_left(tree->left, v, cmp);
	else if (tree->left == NULL)
		return bstree_find_left(tree->right, v, cmp);
	else if (cmp(v, tree->value) || cmp(v, tree->rvalue))// TODO: can remove first test?
		return bstree_find_left(tree->left, v, cmp);
	else
		return bstree_find_left(tree->right, v, cmp);
}

/* Find the node immediately to the right of v
 *
 * cmp(v, x) must return nonzero if v is right of x
 */
void* bstree_find_right(bstree_t* tree, void* v, comparator_t cmp)
{
	if (tree == NULL) return NULL;

	if (is_leaf(tree))
		return tree->value;
	else if (tree->right == NULL)
		return bstree_find_right(tree->left, v, cmp);
	else if (tree->left == NULL)
		return bstree_find_right(tree->right, v, cmp);
	else if (cmp(v, tree->value) || cmp(v, tree->rvalue))// TODO: can remove second test?
		return bstree_find_right(tree->right, v, cmp);
	else
		return bstree_find_right(tree->left, v, cmp);
}

/* Find all the nodes that match the comparator
 *
 * cmp(v, x) must return nonzero if v matches x
 */
void bstree_find_all(bstree_t* tree, void* v, comparator_t cmp, list_t** list)
{
	if (tree == NULL) return;

	if (is_leaf(tree)) {
		if (cmp(v, tree->value))
			list_add(list, tree->value);
	}  else {
		if (tree->left != NULL)
			bstree_find_all(tree->left, v, cmp, list);

		if (tree->right != NULL)
			bstree_find_all(tree->right, v, cmp, list);
	}
}

void free_bstree(bstree_t** tree)
{
	assert(tree != NULL);
	if (*tree == NULL) return;
	free_bstree(&(*tree)->left);
	free_bstree(&(*tree)->right);
	free(*tree);
	*tree = NULL;
}

static int is_leaf(bstree_t* tree)
{
	return tree->left == NULL && tree->right == NULL;
}

