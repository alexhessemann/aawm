#include "aawm_window.h"

#include <stdlib.h>
#include <string.h>


aawm_window_t * aawm_allocate_window( xcb_window_t a_wid, aawm_window_role_t a_role, xcb_window_t a_parent )
{
	aawm_window_t * result = malloc( sizeof( aawm_window_t ) );
	if (result) {
		result->wid = a_wid;
		result->role = a_role;
		result->parent = a_parent;
		result->children_count = 0;
		result->children = NULL;
	}
	return result;
}

bool aawm_window_add_child( aawm_window_t *a_parent, xcb_window_t a_child )
{
	xcb_window_t *result = realloc( a_parent->children, (a_parent->children_count + 1) * sizeof( xcb_window_t ) );
	if (result) {
		a_parent->children = result;
		a_parent->children[a_parent->children_count] = a_child;
		a_parent->children_count++;
		return true;
	} else {
		return false;
	}
}


bool aawm_window_delete_child( aawm_window_t *a_parent, xcb_window_t a_child )
{
	int idx;
	bool found = false;

	for (idx = 0; !found && idx < a_parent->children_count; idx++) {
		if (a_parent->children[idx] == a_child) {
			found = true;
			if (--a_parent->children_count) {
				memmove( &a_parent->children[idx], &a_parent->children[idx + 1], (a_parent->children_count - idx) * sizeof( xcb_window_t ) );
				xcb_window_t * result = realloc( a_parent->children, a_parent->children_count * sizeof( xcb_window_t ) );
				if (result) {
					a_parent->children = result;
				}
			} else { // empty list
				free( a_parent->children );
				a_parent->children = NULL;
			}
		}
	}

	return found;
}
