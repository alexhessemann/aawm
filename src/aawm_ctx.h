#ifndef AAWM_CTX_H
#define AAWM_CTX_H

#include <stdbool.h>
#include <xcb/xcb.h>

#include "atoms.h"
#include "map.h"

struct aawm_ctx {
	xcb_connection_t *conn;
	xcb_screen_t *screen;
	uint32_t argb_visual;
	// X extensions => set a structure with the server's configuration
	int shape_base;
	int render_base;
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
	// TODO: Use a table and define an enum
	xcb_cursor_t x_cursor; // 0
	xcb_cursor_t fleur; // 52
	xcb_cursor_t pirate; // 88
	xcb_cursor_t sizing; // 120
};

#endif
