#ifndef AAWM_CTX_H
#define AAWM_CTX_H

#include <stdbool.h>
#include <xcb/xcb.h>

#include "atoms.h"
#include "map.h"

struct aawm_ctx {
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	// X extensions
	int shape_base;
	// Atom names
	map_t * atom_names;
	xcb_atom_t atom_map[AAWM_LAST_MAPPED_ATOM - AAWM_LAST_X11_PREDEFINED_ATOM];
	// Windows
	map_t * windows_list;
	// Not wise to put this here
	bool moving;
	int motion_origin_x;
	int motion_origin_y;
	int motion_rel_x;
	int motion_rel_y;
	int window_origin_x;
	int window_origin_y;
	int window_origin_width;
	int window_origin_height;
	int window_width_offset;
	int window_height_offset;
	xcb_font_t cursor_font;
	xcb_cursor_t fleur;
	xcb_cursor_t pirate;
	xcb_cursor_t sizing;
};

#endif
