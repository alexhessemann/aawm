#ifndef AAWM_WINDOW_H
#define AAWM_WINDOW_H

#include <stdbool.h>
#include <xcb/xcb.h>

typedef enum aawm_window_role_t {
	AAWM_ROLE_CLIENT = 0,
	AAWM_ROLE_ROOT = 1,
	AAWM_ROLE_FRAME = 2,
	AAWM_ROLE_CLOSE = 3,
	AAWM_ROLE_UTILITY = 4,
	AAWM_ROLE_MINMAX = 5,
	AAWM_ROLE_RESIZE = 6
} aawm_window_role_t;

typedef struct aawm_window {
	xcb_window_t wid;
	aawm_window_role_t role; // Sort of like _NET_WM_WINDOW_TYPE, but for our own use
	xcb_window_t parent;
	size_t children_count;
	xcb_window_t *children;
} aawm_window_t;

aawm_window_t * aawm_allocate_window( xcb_window_t a_wid, aawm_window_role_t a_role, xcb_window_t a_parent );
bool aawm_window_add_child( aawm_window_t *a_parent, xcb_window_t a_child );
bool aawm_window_delete_child( aawm_window_t *a_parent, xcb_window_t a_child );

#endif
