#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>

#include "aawm_ctx.h"
#include "atoms.h"

// For the predefined, could have used <xcb/xcb_atom.h>
// (package libxcb-util0-dev in Debian)

static const char * atom_string[] = {
	"NONE/ANY",
	"PRIMARY",
	"SECONDARY",
	"ARC",
	"ATOM",
	"BITMAP",
	"CARDINAL",
	"COLORMAP",
	"CURSOR",
	"CUT_BUFFER0",
	"CUT_BUFFER1",
	"CUT_BUFFER2",
	"CUT_BUFFER3",
	"CUT_BUFFER4",
	"CUT_BUFFER5",
	"CUT_BUFFER6",
	"CUT_BUFFER7",
	"DRAWABLE",
	"FONT",
	"INTEGER",
	"PIXMAP",
	"POINT",
	"RECTANGLE",
	"RESOURCE_MANAGER",
	"RGB_COLOR_MAP",
	"RGB_BEST_MAP",
	"RGB_BLUE_MAP",
	"RGB_DEFAULT_MAP",
	"RGB_GRAY_MAP",
	"RGB_GREEN_MAP",
	"RGB_RED_MAP",
	"STRING",
	"VISUALID",
	"WINDOW",
	"WM_COMMAND",
	"WM_HINTS",
	"WM_CLIENT_MACHINE",
	"WM_ICON_NAME",
	"WM_ICON_SIZE",
	"WM_NAME",
	"WM_NORMAL_HINTS",
	"WM_SIZE_HINTS",
	"WM_ZOOM_HINTS",
	// BEGIN Built-in font property names
	"MIN_SPACE",
	"NORM_SPACE",
	"MAX_SPACE",
	"END_SPACE",
	"SUPERSCRIPT_X",
	"SUPERSCRIPT_Y",
	"SUBSCRIPT_X",
	"SUBSCRIPT_Y",
	"UNDERLINE_POSITION",
	"UNDERLINE_THICKNESS",
	"STRIKEOUT_ASCENT",
	"STRIKEOUT_DESCENT",
	"ITALIC_ANGLE",
	"X_HEIGHT",
	"QUAD_WIDTH",
	"WEIGHT",
	"POINT_SIZE",
	"RESOLUTION",
	"COPYRIGHT",
	"NOTICE",
	"FONT_NAME",
	"FAMILY_NAME",
	"FULL_NAME",
	"CAP_HEIGHT",
	// END Built-in font property names
	"WM_CLASS",
	"WM_TRANSIENT_FOR",

	// Following strings aren't pre-defined and need to be mapped

	"UTF8_STRING",
	"WM_PROTOCOLS",
	"WM_TAKE_FOCUS",
	"WM_SAVE_YOURSELF",
	"WM_DELETE_WINDOW"
};

struct request {
	size_t idx;
	xcb_get_atom_name_cookie_t cookie;
};

// Map is for a given connection, changing the connection without purging the cache might return wrong results.

// Note that if map_add fails, we leak the memory pointed by atom_name_z.
const char * get_atom_name( struct aawm_ctx * a_ctx, xcb_atom_t a_atom )
{
	const char *result;
	
	if (a_atom <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		result = atom_string[a_atom];
	} else if (!(result = map_lookup( a_ctx->atom_names, a_atom ))) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, xcb_get_atom_name( a_ctx->conn, a_atom ), NULL );
		int atom_name_len = xcb_get_atom_name_name_length( reply );
		char *atom_name = xcb_get_atom_name_name( reply );
		char *atom_name_z = malloc( atom_name_len + 1 );
		if (atom_name_z) {
			atom_name_z[atom_name_len] = 0;
			memcpy( atom_name_z, atom_name, atom_name_len );
			map_add( a_ctx->atom_names, a_atom, atom_name_z );
			fprintf( stderr, "Added name \"%s\" for atom %d\n", atom_name_z, a_atom );
		}
		result = atom_name_z;
	}

	return result;
}

// It sucks that xcb_get_atom_name_cookie_t isn't a scalar, but we won't cheat and won't use xcb_get_atom_name_cookie_t.sequence
// Returns true if an async request has been sent.
static bool get_atom_name_async( struct aawm_ctx * a_ctx, xcb_atom_t a_atom, const char** a_name, xcb_get_atom_name_cookie_t * a_cookie )
{
	bool result = false;
	
	if (a_atom <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		*a_name = atom_string[a_atom];
	} else if (!(*a_name = map_lookup( a_ctx->atom_names, a_atom ))) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		result = true;
		*a_cookie = xcb_get_atom_name( a_ctx->conn, a_atom );
	}

	return result;
}


// It is assumed that each element in a_list is unique.
const char ** alloc_atom_names_list( struct aawm_ctx * a_ctx, size_t a_len, xcb_atom_t *a_list )
{
	const char ** name_list = malloc( a_len * sizeof( const char * ) );
	if (name_list) {
		struct request *request_list = malloc( a_len * sizeof( struct request ) );
		if (request_list) {
			size_t idx;
			size_t request_list_len = 0;
			for (idx = 0; idx < a_len; idx++) {
				bool async = get_atom_name_async( a_ctx, a_list[idx], &name_list[idx], &request_list[request_list_len].cookie );
				if (async) {
					request_list[request_list_len++].idx = idx;
				}
			}
			for (idx = 0; idx < request_list_len; idx++) {
				xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_ctx->conn, request_list[idx].cookie, NULL );
				int atom_name_len = xcb_get_atom_name_name_length( reply );
				char *atom_name = xcb_get_atom_name_name( reply );
				char *atom_name_z = malloc( atom_name_len + 1 );
				if (atom_name_z) {
					atom_name_z[atom_name_len] = 0;
					memcpy( atom_name_z, atom_name, atom_name_len );
					map_add( a_ctx->atom_names, a_list[request_list[idx].idx], atom_name_z );
					fprintf( stderr, "Added name \"%s\" for atom %d\n", atom_name_z, a_list[request_list[idx].idx] );
				}
				name_list[request_list[idx].idx] = atom_name_z;
			}
		} else {
			free( name_list );
			name_list = NULL;
		}
	}
	return name_list;
}


xcb_atom_t get_atom_from_symbol( struct aawm_ctx * a_ctx, int a_symbol )
{
	xcb_atom_t result;
	if (a_symbol <= AAWM_LAST_X11_PREDEFINED_ATOM) {
		result = a_symbol; // For convenience, we also accept pre-defined atoms
	} else if (a_symbol > AAWM_LAST_MAPPED_ATOM) {
		result = XCB_ATOM_NONE;
	} else if (!(result = a_ctx->atom_map[a_symbol - AAWM_LAST_X11_PREDEFINED_ATOM])) {
		// test clause was a little tricky, we enter here if the atom wasn't found
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply( a_ctx->conn, xcb_intern_atom( a_ctx->conn, false, strlen( atom_string[a_symbol] ), atom_string[a_symbol] ), NULL );
		a_ctx->atom_map[a_symbol - AAWM_LAST_X11_PREDEFINED_ATOM] = reply->atom;
		result = reply->atom;
	}

	return result;
}

