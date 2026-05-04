#ifndef AAWM_WINDOW_H
#define AAWM_WINDOW_H

#include <stdbool.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

typedef enum aawm_window_role {
	AAWM_ROLE_ROOT = 0, // Root window
	AAWM_ROLE_CLIENT = 1, // Client (non-WM) window
	AAWM_ROLE_FRAME = 2, // WM frame around client
	// Bars
	AAWM_ROLE_TITLEBAR = 3,
	AAWM_ROLE_RESIZEBAR = 4,
	// Frame buttons:
	AAWM_ROLE_CLOSE = 5,
	AAWM_ROLE_UTILITY = 6,
	AAWM_ROLE_MINMAX = 7,
	AAWM_ROLE_RESIZE = 8
} aawm_window_role_t;

typedef struct aawm_window {
	xcb_window_t wid;
	aawm_window_role_t role; // Sort of like _NET_WM_WINDOW_TYPE, but for our own use
	xcb_window_t parent;
	size_t children_count;
	xcb_window_t *children;
	xcb_size_hints_t *size_hints;
} aawm_window_t;

/*typedef struct aawm_client_window {
//	struct aawm_window; // require -fms-extensions in CFLAGS
	int min_x_size;
} aawm_client_window_t;*/

aawm_window_t * aawm_allocate_window( xcb_window_t a_wid, aawm_window_role_t a_role, xcb_window_t a_parent );
bool aawm_window_add_child( aawm_window_t *a_parent, xcb_window_t a_child );
bool aawm_window_delete_child( aawm_window_t *a_parent, xcb_window_t a_child );
void aawm_window_print_children( aawm_window_t *a_parent );

#endif
