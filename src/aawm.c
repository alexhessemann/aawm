#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/shape.h>
#include <xcb/xcb_icccm.h>

#include "aawm_ctx.h"
#include "aawm_window.h"
#include "atoms.h"
//#include "circle_menu.xbm"
#include "get_constant_string.h"
#include "utfconv.h"

/* Data tree for VISUALID and PICTFORMAT
 *
 *	xcb_setup_t = xcb_get_setup()
 *	.image_byte_order
 *	.bitmap_format_bit_order
 *	.bitmap_format_scanline_unit
 *	.bitmap_format_scanline_pad
 *	xcb_format_t = xcb_setup_pixmap_formats(xcb_setup_t) // pixmap-formats
 *		.depth
 *		.bits_per_pixel
 *		.scanline_pad
 *	xcb_screen_t = xcb_setup_roots_iterator(xcb_setup_t).data // roots
 *		.root_depth
 *		.root_visual
 * 		xcb_depth_t = xcb_screen_allowed_depths_iterator(xcb_screen_t).data // allowed-depths
 *			.depth
 *			xcb_visualtype_t = xcb_depth_visuals(xcb_depth_t) // visuals
 *				.visual_id <= For non-render pixmaps, to associate with pixmap-format for the given depth.
 *				._class
 *				.bits_per_rgb_value
 *				.red_mask
 *				.green_mask
 *				.blue_mask
 *
 *	xcb_render_query_pict_formats_reply_t = xcb_render_query_pict_formats()
 *		xcb_render_pictforminfo_t = xcb_render_query_pict_formats_formats(xcb_render_query_pict_formats_reply_t) // formats
 *			.id <=
 *			.type
 *			.depth
 *			.direct
 *				.red
 *					.shift
 *					.mask
 *				.green
 *					.shift
 *					.mask
 *				.blue
 *					.shift
 *					.mask
 *				.alpha
 *					.shift
 *					.mask
 *		xcb_render_pictscreen_t = xcb_render_query_pict_formats_screens_iterator(xcb_render_query_pict_formats_reply_t).data // screens
 *			xcb_render_pictdepth_t = xcb_render_pictscreen_depths_iterator(xcb_render_pictscreen_t).data
 *				.depth
 *				xcb_render_pictvisual_t = xcb_render_pictdepth_visuals(xcb_render_pictdepth_t) // visuals
 *					.visual <= visual from xcb_setup_t
 *					.format <= associated format (id) from xcb_render_pictforminfo_t
 *		uint32_t = xcb_render_query_pict_formats_subpixels(xcb_render_query_pict_formats_reply_t) // subpixels
 *
 *	xcb_render_pictforminfo_t.direct and xcb_visualtype_t.*mask should match, but xcb_render_pictforminfo_t also has alpha channel.
 */

int setupscreen( xcb_connection_t *a_conn, xcb_drawable_t a_root )
{
	int i;
//	xcb_get_window_attributes_reply_t *attr;

	/* Get all children. */
	xcb_query_tree_reply_t *reply = xcb_query_tree_reply( a_conn,
		xcb_query_tree( a_conn, a_root ), 0);

	int len = xcb_query_tree_children_length( reply );    
	xcb_window_t *children = xcb_query_tree_children( reply );

	printf( "%d windows:\n", len );

	for (i = 0; i < len; i ++)
	{
		printf( "window %d\n", children[i] );
//		attr = xcb_get_window_attributes_reply( a_conn,
//			xcb_get_window_attributes( a_conn, children[i] ),
//			NULL );
//		free( attr );
	}

	return 0;
}


int setupshape( struct aawm_ctx* a_ctx )
{
	const xcb_query_extension_reply_t *extension = xcb_get_extension_data( a_ctx->conn, &xcb_shape_id );
	if (!extension->present) {
		printf( "No shape extension.\n" );
		return -1;
	}

	a_ctx->shape_base = extension->first_event;
	printf( "Shape base is %d\n", a_ctx->shape_base );

	return a_ctx->shape_base;
}


int setuprender( struct aawm_ctx* a_ctx )
{
	const xcb_query_extension_reply_t *extension = xcb_get_extension_data( a_ctx->conn, &xcb_render_id );
	if (!extension->present) {
		printf( "No render extension.\n" );
		return -1;
	}

	a_ctx->render_base = extension->first_event;
	printf( "Render base is %d\n", a_ctx->render_base );

	xcb_render_query_version_reply_t * version = xcb_render_query_version_reply( a_ctx->conn, xcb_render_query_version( a_ctx->conn, 1, 12 ), NULL );
	printf( "Render version %d.%d\n", version->major_version, version->minor_version );

	return a_ctx->render_base;
}


void map_request_passthrough( struct aawm_ctx* a_ctx, xcb_map_request_event_t *a_ev )
{
	xcb_map_window( a_ctx->conn, a_ev->window );
	xcb_flush( a_ctx->conn );
}


void read_property( struct aawm_ctx* a_ctx, xcb_window_t a_window, xcb_atom_t a_atom, const char * a_atom_name )
{
	xcb_generic_error_t *error;
	xcb_get_property_reply_t *prop_reply = xcb_get_property_reply( a_ctx->conn, xcb_get_property( a_ctx->conn, false, a_window, a_atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 40 ), &error );
	if (error != NULL) {
		printf( "GetProperty ERROR type %d, code %d\n", error->response_type, error->error_code );
		return;
	}

	const char * type_name = get_atom_name( a_ctx, prop_reply->type );
	aawm_atom_enum_t type = get_symbol_from_atom( a_ctx, prop_reply->type );

	printf( "\t%d %s type %s value length %d - %d, bytes after %d", a_atom, a_atom_name, type_name, xcb_get_property_value_length( prop_reply ), prop_reply->value_len, prop_reply->bytes_after );

	switch (type) {
		case XCB_ATOM_STRING:
		case AAWM_ATOM_UTF8_STRING:
			printf( " value \"%.*s\"", prop_reply->value_len, (char*)xcb_get_property_value( prop_reply ) );
			break;
		case XCB_ATOM_CARDINAL:
			{
				int i;
				uint32_t * values = (uint32_t *) xcb_get_property_value( prop_reply );
				for (i = 0; i < (prop_reply->value_len < 10 ? prop_reply->value_len : 10); i++ ) {
					printf( " %d (0x%X)", values[i], values[i] );
				}
				if (a_atom == get_atom_from_symbol( a_ctx, AAWM_ATOM_NET_WM_ICON )) {
					// We re-read the whole value to avoid concurrency issues
					xcb_get_property_reply_t *prop_reply2 = xcb_get_property_reply( a_ctx->conn, xcb_get_property( a_ctx->conn, false, a_window, a_atom, XCB_GET_PROPERTY_TYPE_ANY, 0, (163 + prop_reply->bytes_after) / 4 ), &error );
					if (error != NULL) {
						printf( "GetProperty ERROR type %d, code %d\n", error->response_type, error->error_code );
						return;
					}
					printf( "\n\tRe-read says %d %s type %s value length %d - %d, bytes after %d", a_atom, a_atom_name, type_name, xcb_get_property_value_length( prop_reply2 ), prop_reply2->value_len, prop_reply2->bytes_after );
					size_t offset = 0;
					uint32_t * values2 = (uint32_t *) xcb_get_property_value( prop_reply2 );
					int32_t rem_width = a_ctx->screen->width_in_pixels;
					while (offset < prop_reply2->value_len) {
						printf( "\n\t\tIcon %dx%d ", values2[offset], values2[offset+1] );

						xcb_colormap_t cmapid = xcb_generate_id( a_ctx->conn );
						xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_colormap_checked ( a_ctx->conn, XCB_COLORMAP_ALLOC_NONE, cmapid, a_ctx->screen->root, a_ctx->argb_visual ) );
						if (error != NULL) {
							printf( "CreateColormap ERROR type %d, code %d\n", error->response_type, error->error_code );
						}

						xcb_window_t xid = xcb_generate_id( a_ctx->conn );
						rem_width -=  values2[offset];
						uint32_t values3[] = { 0, cmapid };
						error = xcb_request_check( a_ctx->conn, xcb_create_window_checked( a_ctx->conn, 32, xid, a_ctx->screen->root, rem_width, a_ctx->screen->height_in_pixels - values2[offset+1], values2[offset], values2[offset+1], 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, a_ctx->argb_visual, XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP, values3 ) );
						if (error != NULL) {
							printf( "CreateWindow ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
						error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, xid, 0, NULL ) );
						if (error != NULL) {
							printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						xcb_map_window( a_ctx->conn, xid );
						xcb_put_image( a_ctx->conn, XCB_IMAGE_FORMAT_Z_PIXMAP, xid, gc, values2[offset], values2[offset+1], 0, 0, 0, 32, 4 * values2[offset] * values2[offset+1], (uint8_t *) (&values2[offset+2]) );

						offset += 2 + values2[offset] * values2[offset+1];
					}
					xcb_flush( a_ctx->conn );
				}
			}
			break;
		case XCB_ATOM_WINDOW:
			{
				xcb_window_t* win = (xcb_window_t*) xcb_get_property_value( prop_reply );
				printf( " 0x%X", *win );
			}
			break;
		case XCB_ATOM_ATOM:
			{
				int j;
				xcb_atom_t* atoms = (xcb_atom_t*) xcb_get_property_value( prop_reply );
				const char ** name_list = alloc_atom_names_list( a_ctx, prop_reply->value_len, atoms );
				for (j=0; j< prop_reply->value_len; j++) {
//					const char *name = get_atom_name( a_ctx, atoms[j] );
					printf( " %d %s", atoms[j], name_list[j] );
				}
				free( name_list );
			}
			break;
		case XCB_ATOM_WM_HINTS:
			{
//				int width = 0;
				xcb_icccm_wm_hints_t* hints = (xcb_icccm_wm_hints_t*) xcb_get_property_value( prop_reply );
				printf( " flags 0x%X input %d initial state %d icon pixmap 0x%X icon window 0x%X icon position [%d,%d] icon mask 0x%X, window group 0x%X", hints->flags, hints->input, hints->initial_state, hints->icon_pixmap, hints->icon_window, hints->icon_x, hints->icon_y, hints->icon_mask, hints->window_group );
				uint32_t icon_offset = 0;
				if (hints->icon_pixmap) {
					xcb_get_geometry_reply_t *icongeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, hints->icon_pixmap ), NULL );
					printf( " - icon pixmap: %dx%d+%d+%d, depth %d, border %d, root 0x%X", icongeom->width, icongeom->height, icongeom->x, icongeom->y, icongeom->depth, icongeom->border_width, icongeom->root );
//					width = icongeom->width;

					xcb_window_t xid = xcb_generate_id( a_ctx->conn );
					icon_offset += icongeom->width;
					xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, xid, a_ctx->screen->root, 0, a_ctx->screen->height_in_pixels - icongeom->height, icongeom->width, icongeom->height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, 0, NULL );
					xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
					xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, xid, 0, NULL ) );
					if (error != NULL) {
						printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
					}
					xcb_map_window( a_ctx->conn, xid );
					xcb_copy_area( a_ctx->conn, hints->icon_pixmap, xid, gc, 0, 0, 0, 0, icongeom->width, icongeom->height );
				}
				if (hints->icon_window) {
					uint32_t values[] = { icon_offset, a_ctx->screen->height_in_pixels - 64 };
					xcb_configure_window( a_ctx->conn, hints->icon_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values );
					xcb_map_window( a_ctx->conn, hints->icon_window );
					xcb_flush( a_ctx->conn );
					printf( "Tried to display icon window\n" );
				}
			}
			break;
		case XCB_ATOM_WM_SIZE_HINTS:
			{
				xcb_size_hints_t* hints = (xcb_size_hints_t*) xcb_get_property_value( prop_reply );
				printf( " flags 0x%X geom [%d,%d,%d,%d+%d]x[%d,%d,%d,%d+%d]+%d+%d aspect ratio [%d/%d, %d/%d] gravity %d", hints->flags, hints->min_width, hints->base_width, hints->width, hints->max_width, hints->width_inc, hints->min_height, hints->base_height, hints->height, hints->max_height, hints->height_inc, hints->x, hints->y, hints->min_aspect_num, hints->min_aspect_den, hints->max_aspect_num, hints->max_aspect_den, hints->win_gravity );
			}
			break;
		default:
			;
	}
	printf( "\n" );
}


void map_request_reparent( struct aawm_ctx* a_ctx, xcb_map_request_event_t *a_ev )
{
	char *name = NULL;
	int name_len;

	// Create internal structures for child windows

	xcb_window_t xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *frame_win = aawm_allocate_window( xid, AAWM_ROLE_FRAME, a_ctx->screen->root );
	map_add( a_ctx->windows_list, xid, frame_win );
	aawm_window_t *client_win = aawm_allocate_window( a_ev->window, AAWM_ROLE_CLIENT, frame_win->wid );
	map_add( a_ctx->windows_list, a_ev->window, client_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *close_win = aawm_allocate_window( xid, AAWM_ROLE_CLOSE, frame_win->wid );
	map_add( a_ctx->windows_list, xid, close_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *util_win = aawm_allocate_window( xid, AAWM_ROLE_UTILITY, frame_win->wid );
	map_add( a_ctx->windows_list, xid, util_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *minmax_win = aawm_allocate_window( xid, AAWM_ROLE_MINMAX, frame_win->wid );
	map_add( a_ctx->windows_list, xid, minmax_win );
	xid = xcb_generate_id( a_ctx->conn );
	aawm_window_t *resize_win = aawm_allocate_window( xid, AAWM_ROLE_RESIZE, frame_win->wid );
	map_add( a_ctx->windows_list, xid, resize_win );

	aawm_window_add_child( frame_win, client_win->wid );
	aawm_window_add_child( frame_win, close_win->wid );
	aawm_window_add_child( frame_win, util_win->wid );
	aawm_window_add_child( frame_win, minmax_win->wid );
	aawm_window_add_child( frame_win, resize_win->wid );

	// Read, parse and display properties of the client

	xcb_list_properties_reply_t * props = xcb_list_properties_reply( a_ctx->conn, xcb_list_properties( a_ctx->conn, a_ev->window ), NULL );
	int prop_count =  xcb_list_properties_atoms_length( props );
	printf( "Window 0x%X defines %d properties:\n", a_ev->window, prop_count );
	xcb_atom_t *prop_atoms = xcb_list_properties_atoms( props );
	const char ** atom_names_list = alloc_atom_names_list( a_ctx, prop_count, prop_atoms );

	for (int i = 0; i < prop_count; i++) {
		read_property( a_ctx, a_ev->window, prop_atoms[i], atom_names_list[i] );
		
		aawm_atom_enum_t aatom = get_symbol_from_atom( a_ctx, prop_atoms[i] );
		if (aatom == AAWM_ATOM_NET_WM_NAME) {
			printf( "Setting _NET_WM_NAME\n" );

			xcb_generic_error_t *error;
			xcb_get_property_reply_t *prop_reply = xcb_get_property_reply( a_ctx->conn, xcb_get_property( a_ctx->conn, false, a_ev->window, prop_atoms[i], XCB_GET_PROPERTY_TYPE_ANY, 0, 40 ), &error );
			if (error != NULL) {
				printf( "GetProperty ERROR type %d, code %d\n", error->response_type, error->error_code );
			} else {
				name = (char*)xcb_get_property_value( prop_reply );
				name_len = prop_reply->value_len;
			}
		}
	}

	uint32_t event_mask = XCB_CW_EVENT_MASK;
	uint32_t event_mask_values = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
		| XCB_EVENT_MASK_ENTER_WINDOW
		| XCB_EVENT_MASK_LEAVE_WINDOW
		| XCB_EVENT_MASK_FOCUS_CHANGE
		| XCB_EVENT_MASK_PROPERTY_CHANGE
		| XCB_EVENT_MASK_BUTTON_PRESS
		| XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_BUTTON_1_MOTION;

	// used later
	uint32_t mask;
	uint32_t values_m[3];

	xcb_colormap_t cmapid = xcb_generate_id( a_ctx->conn );
	xcb_create_colormap( a_ctx->conn, XCB_COLORMAP_ALLOC_NONE, cmapid, a_ctx->screen->root, a_ctx->argb_visual );

	uint32_t values[] = { 0x0FF, 0x7F00FF00, cmapid };
	uint32_t values2[] = { 0xFF0000 };
	uint32_t values3[] = { 0xFF0000, XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE };
	xcb_get_geometry_reply_t *wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, a_ev->window ), NULL );

	/*xcb_void_cookie_t cookie =*/ xcb_create_window( a_ctx->conn, 32/*XCB_COPY_FROM_PARENT*/, frame_win->wid, a_ctx->screen->root, wgeom->x - 4/*(border_width-1)*/, wgeom->y - 4, wgeom->width + 2 * wgeom->border_width, wgeom->height + 2 * wgeom->border_width + 30, 5 /*border_width*/, XCB_WINDOW_CLASS_INPUT_OUTPUT, a_ctx->argb_visual/*XCB_COPY_FROM_PARENT*/, XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_COLORMAP, values );

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, frame_win->wid, event_mask, &event_mask_values );
	/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
	xcb_flush( a_ctx->conn );

	uint32_t prop_test = XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, client_win->wid, XCB_CW_EVENT_MASK, &prop_test ) );
	xcb_flush( a_ctx->conn );

	xcb_generic_error_t * error;
	xcb_font_t fid = xcb_generate_id( a_ctx->conn );
//	error = xcb_request_check( a_ctx->conn, xcb_open_font_checked( a_ctx->conn, fid, 3, "6x9" ) );
	error = xcb_request_check( a_ctx->conn, xcb_open_font_checked( a_ctx->conn, fid, 56, "-adobe-times-medium-r-normal--0-0-100-100-p-0-iso10646-1" ) );
	if (error) {
		printf( "OpenFont ERROR type %d, code %d\n", error->response_type, error->error_code );
	}

	xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
	uint32_t gc_values[3];
	uint32_t gc_mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
	gc_values[0] = a_ctx->screen->white_pixel;
	gc_values[1] = a_ctx->screen->black_pixel;
	gc_values[2] = fid;
	error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, frame_win->wid, gc_mask, gc_values ) );
	if (error != NULL) {
		printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
	}
//	error = xcb_request_check( a_ctx->conn, xcb_poly_text_8_checked( a_ctx->conn, frame_win->wid, gc, 30, 10, 4, (const unsigned char *)"plop" ) );

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, util_win->wid, frame_win->wid, 0, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values3 );
	xcb_map_window( a_ctx->conn, util_win->wid );

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, minmax_win->wid, frame_win->wid, 20, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, values2 );
	xcb_map_window( a_ctx->conn, minmax_win->wid );

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	values_m[0] = 0xFF0000;
	values_m[1] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_1_MOTION;
	values_m[2] = a_ctx->sizing;

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, resize_win->wid, frame_win->wid, wgeom->width - 19, wgeom->height + 21, 19, 9, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, mask, values_m );
	xcb_map_window( a_ctx->conn, resize_win->wid );

	mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	values_m[0] = 0xFF0000;
	values_m[1] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;
	values_m[2] = a_ctx->pirate;

	xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, close_win->wid, frame_win->wid, wgeom->width - 19, 0, 19, 19, 0, XCB_COPY_FROM_PARENT, XCB_COPY_FROM_PARENT, mask, values_m );
	xcb_map_window( a_ctx->conn, close_win->wid );

	xcb_reparent_window( a_ctx->conn, client_win->wid, frame_win->wid, 0, 20 );
	xcb_map_window( a_ctx->conn, client_win->wid );
	xcb_map_window( a_ctx->conn, frame_win->wid );
	
	if (name) {
	/*	error = xcb_request_check( a_ctx->conn, xcb_image_text_8_checked( a_ctx->conn, 4, frame_win->wid, gc, 30, 10, "plop" ) );
		if (error) {
			printf( "ImageText8 ERROR type %d, code %d\n", error->response_type, error->error_code );
		}*/
		printf( "Printing _NET_WM_NAME\n" );
		uint16_t *name16;
		ssize_t name16_len = alloc_utf16_from_utf8( name, name_len, &name16 );
		int k;
		char *alt_name = (char*) name16;
		for (k = 0; k < name16_len; k++) {
			char tmp = alt_name[2*k];
			alt_name[2*k] = alt_name[2*k+1];
			alt_name[2*k+1] = tmp;
		}
		for (k = 0; k < name16_len; k++) {
			printf( "0x%04X ", name16[k] );
		}
		printf( "\n" );
//		unsigned char polybuffer[7];
		unsigned char *polybuffer = malloc( 2 * name16_len + 2 );
		polybuffer[0] = name16_len; // 5; // len
		polybuffer[1] = 0; // delta
//		memcpy( &polybuffer[2], "plouf", 5 );
		memcpy( &polybuffer[2], name16, 2 * name16_len );
		error = xcb_request_check( a_ctx->conn, xcb_poly_text_16_checked( a_ctx->conn, frame_win->wid, gc, 45, 15, 2*name16_len+2 /*7*/, polybuffer ) );
		if (error) {
			printf( "PolyText8 ERROR type %d, code %d\n", error->response_type, error->error_code );
		}
	}

	xcb_flush( a_ctx->conn );
}


void configure_window_passthrough( xcb_connection_t* a_conn, xcb_configure_request_event_t *a_ev )
{
	printf( "Requested configuration: %d%sx%d%s+%d%s+%d%s, border %d%s, stacking %d%s to sibling %d%s\n", a_ev->width, (a_ev->value_mask & 4) ? "!" : "", a_ev->height, (a_ev->value_mask & 8) ? "!" : "", a_ev->x, (a_ev->value_mask & 1) ? "!" : "", a_ev->y, (a_ev->value_mask & 2) ? "!" : "", a_ev->border_width, (a_ev->value_mask & 16) ? "!" : "", a_ev->stack_mode, (a_ev->value_mask & 64) ? "!" : "", a_ev->sibling, (a_ev->value_mask & 32) ? "!" : "" );

	int value_count = 0;
	int count_mask;
	for (count_mask = 1; count_mask < 256; count_mask <<= 1) {
		if (a_ev->value_mask & count_mask) value_count++;
	}

	uint32_t *values = malloc( value_count * sizeof( uint32_t ) );

	int idx = 0;
	if (a_ev->value_mask & 1) values[idx++] = a_ev->x;
	if (a_ev->value_mask & 2) values[idx++] = a_ev->y;
	if (a_ev->value_mask & 4) values[idx++] = a_ev->width;
	if (a_ev->value_mask & 8) values[idx++] = a_ev->height;
	if (a_ev->value_mask & 16) values[idx++] = a_ev->border_width;
	if (a_ev->value_mask & 32) values[idx++] = a_ev->sibling;
	if (a_ev->value_mask & 64) values[idx++] = a_ev->stack_mode;

	xcb_configure_window( a_conn, a_ev->window, a_ev->value_mask, values );
	xcb_flush( a_conn );
	free( values );
}


void resize_request_passthrough( xcb_connection_t* a_conn, xcb_resize_request_event_t *a_ev, xcb_screen_t *a_screen )
{
	uint32_t values[] = { a_ev->width, a_ev->height };
	xcb_configure_window( a_conn, a_screen->root, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values );
	xcb_flush( a_conn );
}


void create_cursors( struct aawm_ctx *a_ctx )
{
	static const char font_name[] = "cursor";
	a_ctx->cursor_font = xcb_generate_id( a_ctx->conn );
	xcb_open_font( a_ctx->conn, a_ctx->cursor_font, strlen( font_name ), font_name );

	a_ctx->x_cursor = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->x_cursor, a_ctx->cursor_font, a_ctx->cursor_font, 0, 1, 0,0,0,0xFFFF,0xFFFF,0xFFFF );

	a_ctx->fleur = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->fleur, a_ctx->cursor_font, a_ctx->cursor_font, 52, 53, 0,0,0,0xFFFF,0xFFFF,0xFFFF );

	a_ctx->pirate = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->pirate, a_ctx->cursor_font, a_ctx->cursor_font, 88, 89, 0,0,0,0xFFFF,0xFFFF,0xFFFF );

	a_ctx->sizing = xcb_generate_id( a_ctx->conn );
	xcb_create_glyph_cursor( a_ctx->conn, a_ctx->sizing, a_ctx->cursor_font, a_ctx->cursor_font, 120, 121, 0,0,0,0xFFFF,0xFFFF,0xFFFF );

	xcb_flush( a_ctx->conn );
}


void events( struct aawm_ctx *a_ctx )
{
	while (1) {
		xcb_generic_event_t *ev = xcb_wait_for_event( a_ctx->conn );
		switch (ev->response_type & ~0x80) {

			case 0:
			{
				xcb_generic_error_t *e = (xcb_generic_error_t *) ev;
				printf( "Received ERROR%s %d\n", (e->response_type & 0x80) ? " (S)" : "", e->error_code );
			}
			break;

			// Substructure redirect mask

			case XCB_MAP_REQUEST:
			{
				xcb_map_request_event_t *e = (xcb_map_request_event_t *) ev;
				printf( "Received map request%s from 0x%X for 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window );
//				map_request_passthrough( a_ctx, e );
				map_request_reparent( a_ctx, e );
			}
			break;

			case XCB_CONFIGURE_REQUEST:
			{
				xcb_configure_request_event_t *e = (xcb_configure_request_event_t *) ev;
				printf( "Received configure request%s from 0x%X for 0x%X, %dx%d+%d+%d, mask 0x%04x\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window, e->width, e->height, e->x, e->y, e->value_mask );
				configure_window_passthrough( a_ctx->conn, e );
			}
			break;

			// (Sub)Structure notify mask

			case XCB_CIRCULATE_REQUEST:
			{
				xcb_circulate_request_event_t *e = (xcb_circulate_request_event_t *)ev;
				printf("Received circulate request%s from 0x%X for 0x%X, ignoring\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window );
			}
			break;

			case XCB_CONFIGURE_NOTIFY:
			{
				xcb_configure_notify_event_t *e = (xcb_configure_notify_event_t *) ev;
				printf( "Received configure notify%s from 0x%X for 0x%X, %dx%d+%d+%d, above_sibling=0x%X, border_width=%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->width, e->height, e->x, e->y, e->above_sibling, e->border_width, e->override_redirect );
			}
			break;

			case XCB_CREATE_NOTIFY:
			{
				xcb_create_notify_event_t *e = (xcb_create_notify_event_t *) ev;
				printf( "Received create notify%s from 0x%X for 0x%X, %dx%d+%d+%d, border_width=%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->parent, e->window, e->width, e->height, e->x, e->y, e->border_width, e->override_redirect );
			}
			break;

			case XCB_DESTROY_NOTIFY:
			{
				xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
				printf( "Received destroy notify%s from 0x%X for 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window );
				aawm_window_t *win = map_lookup( a_ctx->windows_list, e->window );
				if (!win) {
					printf( "Destroyed window (0x%X) not registered ?!\n", e->window ); 
				} else {
					if (win->parent != e->event) {
						printf( "Catched by root instead of frame ?!\n" );
					}
					printf( "Destroying window 0x%X\n", win->parent );
					xcb_destroy_window( a_ctx->conn, win->parent );
					// TODO: Delete in windows_list, too.
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// TODO: Gravity

			case XCB_MAP_NOTIFY:
			{
				xcb_map_notify_event_t *e = (xcb_map_notify_event_t *) ev;
				printf( "Received map notify%s from 0x%X for 0x%X, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->override_redirect );
			}
			break;

			case XCB_REPARENT_NOTIFY:
			{
				xcb_reparent_notify_event_t *e = (xcb_reparent_notify_event_t *) ev;
				printf( "Received reparent notify%s from 0x%X for 0x%X to be reparented by 0x%X at +%d+%d, override_redirect=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->parent, e->x, e->y, e->override_redirect );
			}
			break;

			case XCB_UNMAP_NOTIFY:
			{
				// We receive synthesized of these ones even with only XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT in the mask. Is it because XSendEvent is targeted ?
				xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *) ev;
				printf( "Received unmap notify%s from 0x%X for 0x%X from_configure(%d)\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->window, e->from_configure );
			}
			break;

			// ResizeRedirectMask

			case XCB_RESIZE_REQUEST:
			{
				xcb_resize_request_event_t *e = (xcb_resize_request_event_t *) ev;
				printf( "Reveived resize request%s from 0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->window );
				resize_request_passthrough( a_ctx->conn, e, a_ctx->screen );
			}
			break;

			// FocusChangeMask

			case XCB_FOCUS_IN:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus in%s for 0x%X, detail %s, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, get_detail_string( e->detail ), e->mode );
			}
			break;

			case XCB_FOCUS_OUT:
			{
				xcb_focus_in_event_t *e = (xcb_focus_in_event_t *) ev;
				printf( "Received focus out%s for 0x%X, detail %s, mode %d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, get_detail_string( e->detail ), e->mode );
			}
			break;

			// PropertyChangeMask

			case XCB_PROPERTY_NOTIFY:
			{
				xcb_property_notify_event_t *e = (xcb_property_notify_event_t *) ev;
				const char *atom_name = get_atom_name( a_ctx, e->atom );
				printf( "Received property notify%s for 0x%X, atom %d (%s), state %d, at time %d\n", (e->response_type & 0x80) ? " (S)" : "", e->window, e->atom, atom_name, e->state, e->time );
				read_property( a_ctx, e->window, e->atom, atom_name );
			}
			break;

			// EnterWindowMask

			case XCB_ENTER_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received enter notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %s, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, get_detail_string( e->detail ), e->mode, e->same_screen_focus, e->state );
				xcb_get_input_focus_reply_t *focus = xcb_get_input_focus_reply( a_ctx->conn, xcb_get_input_focus( a_ctx->conn ), NULL );
				printf( "Focus currently on %d, revert to %d\n", focus->focus, focus->revert_to );
				
				if (e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
					uint32_t mask = XCB_CW_BORDER_PIXEL;
					uint32_t value = 0x80FFFFFF;
					xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value );
					/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
					value = XCB_STACK_MODE_ABOVE;
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_STACK_MODE, &value );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// LeaveWindowMask

			case XCB_LEAVE_NOTIFY:
			{
				xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *) ev;
				printf( "Received leave notify%s for 0x%X (+%d+%d), child 0x%X, root 0x%X (+%d+%d) at time %d, detail %s, mode %d, same_screen_focus=0x%X, state=0x%X\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->child, e->root, e->root_x, e->root_y, e->time, get_detail_string( e->detail ), e->mode, e->same_screen_focus, e->state );
				if (e->detail != XCB_NOTIFY_DETAIL_INFERIOR) {
					uint32_t mask = XCB_CW_BORDER_PIXEL;
					uint32_t value = 0x7F00FF00;
					xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value );
					/*xcb_generic_error_t *error =*/ xcb_request_check( a_ctx->conn, cookie );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// ButtonPressMask

			case XCB_BUTTON_PRESS:
			{
				xcb_button_press_event_t *e = (xcb_button_press_event_t *) ev;
				printf( "Received button press%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, button %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );
				aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
				if (win) {
					switch (win->role) {
					case AAWM_ROLE_CLIENT: printf( "Button press in client window\n" ); break;
					case AAWM_ROLE_ROOT: printf( "Button press in root window\n" ); break;
					case AAWM_ROLE_FRAME: printf( "Button press in frame window\n" ); break;
					case AAWM_ROLE_CLOSE: printf( "Button press in close gadget\n" ); break;
					case AAWM_ROLE_UTILITY: printf( "Button press in utility gadget\n" ); break;
					case AAWM_ROLE_MINMAX: printf( "Button press in minmax gadget\n" ); break;
					case AAWM_ROLE_RESIZE: printf( "Button press in resize gadget\n" ); break;
					default: printf( "Button press in unknown type of window\n" );
					}
				} else {
					printf( "Button press in unknown window\n" );
					debug_map_list( a_ctx->windows_list );
				}
				if (e->detail == 1) {
					if (win && (win->role == AAWM_ROLE_RESIZE || win->role == AAWM_ROLE_FRAME)) {
						a_ctx->motion_origin_x = e->root_x;
						a_ctx->motion_origin_y = e->root_y;
						xcb_get_geometry_reply_t *wgeom;
						if (win && win->role == AAWM_ROLE_RESIZE) {
							aawm_window_t *frame_win = map_lookup( a_ctx->windows_list, win->parent );
							if (frame_win && frame_win->role == AAWM_ROLE_FRAME) {
								wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, win->parent ), NULL );
							} else {
								printf( "Couldn't find frame window, crash imminent\n" );
							}
						} else {
							wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, e->event ), NULL );
						}
						a_ctx->window_origin_x = wgeom->x;
						a_ctx->window_origin_y = wgeom->y;
						a_ctx->window_origin_width = wgeom->width;
						a_ctx->window_origin_height = wgeom->height;
						// e->event_x and e->event_y are useless for resize because relative to the gadget, not the frame window
						a_ctx->motion_rel_x = e->root_x - wgeom->x;
						a_ctx->motion_rel_y = e->root_y - wgeom->y;
						// Need to take border into account for these offsets
						a_ctx->window_width_offset = wgeom->x + wgeom->width - e->root_x;
						a_ctx->window_height_offset = wgeom->y + wgeom->height - e->root_y;
						printf( "Relative coordinates: (%d,%d), to the bottom-right corner: (%d,%d)\n", a_ctx->motion_rel_x, a_ctx->motion_rel_y, a_ctx->window_width_offset, a_ctx->window_height_offset );
					} else if (win && win->role == AAWM_ROLE_UTILITY) {
						xcb_get_geometry_reply_t *wgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, e->event ), NULL );
						aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
						xcb_get_geometry_reply_t *pgeom = xcb_get_geometry_reply( a_ctx->conn, xcb_get_geometry( a_ctx->conn, win->parent ), NULL );

						xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
						xcb_pixmap_t circle_pix = xcb_generate_id( a_ctx->conn );
						xcb_window_t menu = xcb_generate_id( a_ctx->conn );
						int pix_value = 0xffff00;
						xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, menu, a_ctx->screen->root, (pgeom->x + wgeom->x + wgeom->width / 2) - 50, (pgeom->y + wgeom->y + wgeom->height / 2) - 50, 100, 100, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &pix_value );
						xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_pixmap_checked( a_ctx->conn, 1, circle_pix, a_ctx->screen->root, 100, 100 ) );
						if (error != NULL) {
							printf( "CreatePixmap ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, circle_pix /*a_ctx->screen->root*/, 0, NULL ) );
						if (error != NULL) {
							printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
						}
						int gc_value = 0;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_rectangle_t rect = { 0, 0, 100, 100 };
						xcb_poly_fill_rectangle( a_ctx->conn, circle_pix, gc, 1, &rect );
						gc_value = 1;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_arc_t arc = { 0, 0, 100, 100, 0, 360*64 };
						xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc );
						gc_value = 0;
						xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
						xcb_arc_t arc2 = { 40, 40, 20, 20, 0, 360*64 };
						xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc2 );
						
						xcb_shape_mask( a_ctx->conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, menu, 0, 0, circle_pix );
						xcb_map_window( a_ctx->conn, menu );
						xcb_flush( a_ctx->conn );
					}
				} else if (e->detail == 3) {
					xcb_gcontext_t gc = xcb_generate_id( a_ctx->conn );
					xcb_pixmap_t circle_pix = xcb_generate_id( a_ctx->conn );
					xcb_window_t menu = xcb_generate_id( a_ctx->conn );
					int pix_value = 0xffff00;
					xcb_create_window( a_ctx->conn, XCB_COPY_FROM_PARENT, menu, a_ctx->screen->root, 200, 200, 100, 100, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL, &pix_value );
					xcb_generic_error_t * error = xcb_request_check( a_ctx->conn, xcb_create_pixmap_checked( a_ctx->conn, 1, circle_pix, a_ctx->screen->root, 100, 100 ) );
					if (error != NULL) {
						printf( "CreatePixmap ERROR type %d, code %d\n", error->response_type, error->error_code );
					}
					error = xcb_request_check( a_ctx->conn, xcb_create_gc_checked( a_ctx->conn, gc, circle_pix /*a_ctx->screen->root*/, 0, NULL ) );
					if (error != NULL) {
						printf( "CreateGC ERROR type %d, code %d\n", error->response_type, error->error_code );
					}
					int gc_value = 0;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_rectangle_t rect = { 0, 0, 100, 100 };
					xcb_poly_fill_rectangle( a_ctx->conn, circle_pix, gc, 1, &rect );
					gc_value = 1;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_arc_t arc = { 0, 0, 100, 100, 0, 360*64 };
					xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc );
					gc_value = 0;
					xcb_change_gc( a_ctx->conn, gc, XCB_GC_FOREGROUND, &gc_value );
					xcb_arc_t arc2 = { 40, 40, 20, 20, 0, 360*64 };
					xcb_poly_fill_arc( a_ctx->conn, circle_pix, gc, 1, &arc2 );
					
//					error = xcb_request_check( a_ctx->conn, xcb_put_image_checked( a_ctx->conn, XCB_IMAGE_FORMAT_XY_BITMAP, circle_pix, gc, 96 /*100*/, 96 /*100*/, 0, 0, 0, 1, 1152 /*1300*/ /*sizeof( circle_menu_bits )*/, circle_menu_bits ) );
/*					if (error != NULL) {
						printf( "PutImage ERROR type %d, code %d\n", error->response_type, error->error_code );
					}*/
					xcb_shape_mask( a_ctx->conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, menu, 0, 0, circle_pix );
					xcb_map_window( a_ctx->conn, menu );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// ButtonReleaseMask

			case XCB_BUTTON_RELEASE:
			{
				xcb_button_press_event_t *e = (xcb_button_press_event_t *) ev;
				printf( "Received button release%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, button %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );

				if (a_ctx->moving) {
					printf( "We were moving\n" );
					a_ctx->moving = false;
					// then unset pointer
					uint32_t mask = XCB_CW_CURSOR;
					uint32_t value = 0; // None
					xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value ) );
				} else {
					aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
					if (win->role != AAWM_ROLE_FRAME) {
						aawm_window_t *frame = map_lookup( a_ctx->windows_list, win->parent );
						aawm_window_t *client;
						int i;
						bool found = false;
						for (i = 0; !found && i < frame->children_count; i++ ) {
							client = map_lookup( a_ctx->windows_list, frame->children[i] );
							if (client->role == AAWM_ROLE_CLIENT) found = true;
						}

						if (win && win->role == AAWM_ROLE_CLOSE) {
							printf( "Received close request, sending to 0x%X\n", client->wid );
							xcb_client_message_event_t * msg = calloc( 32, 1 );
							msg->response_type = XCB_CLIENT_MESSAGE;
							msg->format = 32;
							msg->window = client->wid;
							msg->type = get_atom_from_symbol( a_ctx, AAWM_ATOM_WM_PROTOCOLS );
							msg->data.data32[0] = get_atom_from_symbol( a_ctx, AAWM_ATOM_WM_DELETE_WINDOW );
							msg->data.data32[1] = e->time; // XCB_CURRENT_TIME;
							xcb_send_event( a_ctx->conn, 0, client->wid, 0, (char*)msg );
							free( msg );
							xcb_flush( a_ctx->conn );
						}
					}
				}
			}
			break;

			// Button1MotionMask

			case XCB_MOTION_NOTIFY:
			{
				xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *) ev;
				printf( "Received motion notify%s from 0x%X (+%d+%d), with root 0x%X (+%d+%d) and child 0x%X, at time %d, is_hint %d, state=%d, same_screen=%d\n", (e->response_type & 0x80) ? " (S)" : "", e->event, e->event_x, e->event_y, e->root, e->root_x, e->root_y, e->child, e->time, e->detail, e->state, e->same_screen );
				aawm_window_t *win = map_lookup( a_ctx->windows_list, e->event );
				if (win) {
					switch (win->role) {
					case AAWM_ROLE_CLIENT: printf( "Motion in client window\n" ); break;
					case AAWM_ROLE_ROOT: printf( "Motion in root window\n" ); break;
					case AAWM_ROLE_FRAME: printf( "Motion in frame window\n" ); break;
					case AAWM_ROLE_CLOSE: printf( "Motion in close gadget\n" ); break;
					case AAWM_ROLE_UTILITY: printf( "Motion in utility gadget\n" ); break;
					case AAWM_ROLE_MINMAX: printf( "Motion in minmax gadget\n" ); break;
					case AAWM_ROLE_RESIZE: printf( "Motion in resize gadget\n" ); break;
					default: printf( "Motion in unknown type of window\n" );
					}
				} else {
					printf( "Motion in unknown window\n" );
					debug_map_list( a_ctx->windows_list );
				}

				if (win && win->role == AAWM_ROLE_FRAME) {
					if (!a_ctx->moving) {
						a_ctx->moving = true;
						uint32_t mask = XCB_CW_CURSOR;
						uint32_t value = a_ctx->fleur;
						xcb_request_check( a_ctx->conn, xcb_change_window_attributes_checked( a_ctx->conn, e->event, mask, &value ) );
					}

					uint32_t values[] = { a_ctx->window_origin_x + e->root_x - a_ctx->motion_origin_x , a_ctx->window_origin_y + e->root_y - a_ctx->motion_origin_y };
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values );
					xcb_flush( a_ctx->conn );
				} else if (win && win->role == AAWM_ROLE_RESIZE) {
					aawm_window_t *parent = map_lookup( a_ctx->windows_list, win->parent );
					int i;
					int found = 0;
					aawm_window_t *temp, *close, *client;
					for (i = 0; found != 2 && i < parent->children_count; i++) {
						temp = map_lookup( a_ctx->windows_list, parent->children[i] );
						if (temp->role == AAWM_ROLE_CLOSE) { close = temp; found++; }
						if (temp->role == AAWM_ROLE_CLIENT) { client = temp; found++; }
					}
					if (found) {
						printf( "Close found at index %d, ID 0x%X / 0x%X\n", i, parent->children[i], close->wid );
					} else {
						printf( "Close not found!\n" );
					}
					// wgeom->width + 2 * wgeom->border_width, wgeom->height + 2 * wgeom->border_width + 30
					uint32_t values1[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y };
					uint32_t values2[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x - 19, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y - 9 };
					uint32_t value3 = e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x - 19;
					uint32_t values4[] = { e->root_x + a_ctx->window_width_offset - a_ctx->window_origin_x, e->root_y + a_ctx->window_height_offset - a_ctx->window_origin_y - 30 };
					xcb_configure_window( a_ctx->conn, win->parent, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values1 );
					xcb_configure_window( a_ctx->conn, e->event, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values2 );
					xcb_configure_window( a_ctx->conn, close->wid, XCB_CONFIG_WINDOW_X, &value3 );
					xcb_configure_window( a_ctx->conn, client->wid, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values4 );
					xcb_flush( a_ctx->conn );
				}
			}
			break;

			// GCGraphicsExposure

			case XCB_NO_EXPOSURE:
			{
				xcb_no_exposure_event_t *e = (xcb_no_exposure_event_t *) ev;
				printf( "Received no exposure%s for drawable 0x%X, opcode %d.%d\n", (e->response_type & 0x80) ? " (S)" : "", e->drawable, e->major_opcode, e->minor_opcode );
			}
			break;

			// Other: These have no corresponding masks, so we receive them in any case.

			case XCB_CLIENT_MESSAGE:
			{
				xcb_client_message_event_t *e = (xcb_client_message_event_t *) ev;

				const char *name = get_atom_name( a_ctx, e->type );
				printf( "Received client message%s from 0x%X, atom %s format %d\n", (e->response_type & 0x80) ? " (S)" : "", e->window, name, e->format );

				if (e->type == AAWM_ATOM_NET_WM_STATE) {
					const char* action = e->data.data32[0] == 0 ? "_NET_WM_STATE_REMOVE" :  e->data.data32[0] == 1 ? "_NET_WM_STATE_ADD" : e->data.data32[0] == 2 ? "_NET_WM_STATE_TOGGLE" : "<unknown>";

					const char * prop1 = get_atom_name( a_ctx, e->data.data32[1] );
					const char * prop2 = get_atom_name( a_ctx, e->data.data32[2] ); // 0 would mean NONE rather than ANY

					const char* source = e->data.data32[3] == 0 ? "unspecified" :  e->data.data32[3] == 1 ? "application" : e->data.data32[3] == 2 ? "direct user action" : "<unknown>";

					printf( "\taction: %s, property 1: %s, property 2: %s, source: %s\n", action, prop1, prop2, source );
				}
			}
			break;

			case XCB_MAPPING_NOTIFY:
			{
				xcb_mapping_notify_event_t* e = (xcb_mapping_notify_event_t*) ev;
				const char* reqtype = e->request == 0 ? "MappingModifier" : e->request == 1 ? "MappingKeyboard" : e->request == 2 ? "MappingPointer" : "<unsupported>";
				printf( "Received mapping notify%s, request type %s\n", (e->response_type & 0x80) ? " (S)" : "", reqtype );
				if (e->request == 1) {
					printf( "\tFirst keycode %d, count %d\n", e->first_keycode, e->count );
				}
			}
			break;

			case XCB_SELECTION_CLEAR:
			{
				xcb_selection_clear_event_t* e = (xcb_selection_clear_event_t*) ev;
				const char *name = get_atom_name( a_ctx, e->selection );
				printf( "Received selection clear%s from 0x%X, at time %d, atom %s\n", (e->response_type & 0x80) ? " (S)" : "", e->owner, e->time, name );
			}
			break;

			case XCB_SELECTION_NOTIFY:
			{
				xcb_selection_notify_event_t* e = (xcb_selection_notify_event_t *) ev;

				xcb_atom_t atom_list[] = { e->selection, e->target, e->property };
				const char ** name_list = alloc_atom_names_list( a_ctx, 3, atom_list );

				printf( "Received selection notify%s from 0x%X, at time %d, selection %s, target type %s, in property %s\n", (e->response_type & 0x80) ? " (S)" : "", e->requestor, e->time, name_list[0], name_list[1], name_list[2] );

				free( name_list );
			}
			break;

			case XCB_SELECTION_REQUEST:
			{
				xcb_selection_request_event_t* e = (xcb_selection_request_event_t *) ev;

				xcb_atom_t atom_list[] = { e->selection, e->target, e->property };
				const char ** name_list = alloc_atom_names_list( a_ctx, 3, atom_list );

				printf( "Received selection request%s from 0x%X for 0x%X, at time %d, selection %s, target type %s, in property %s\n", (e->response_type & 0x80) ? " (S)" : "", e->requestor, e->owner, e->time, name_list[0], name_list[1], name_list[2] );

				free( name_list );
			}
			break;

			// Unhandled

			default:
				printf( "Received unhandled event type %s%d\n", (ev->response_type & 0x80) ? "S" : "", ev->response_type & ~0x80 );
			break;
		}

		free( ev );
	}
}


int main()
{
	int idx;
	struct aawm_ctx ctx;
	ctx.moving = false;
	ctx.windows_list = map_create();
	ctx.atom_names = map_create();
	ctx.argb_visual = 0;
	memset( ctx.atom_map, 0, (AAWM_LAST_MAPPED_ATOM - AAWM_LAST_X11_PREDEFINED_ATOM) * sizeof(xcb_atom_t ) );
	
	int scrno = 0;
	ctx.conn = xcb_connect( NULL, &scrno );

	printf("screen no set to %d\n", scrno );

	const struct xcb_setup_t * setup = xcb_get_setup( ctx.conn );
	printf( "resource id: base 0x%X, mask 0x%X; motion buffer size %d, maximum request length %d, image byte order %d, bitmap format: bit order %d, scanline unit: %d, scanline pad %d\n", setup->resource_id_base, setup->resource_id_mask, setup->motion_buffer_size, setup->maximum_request_length, setup->image_byte_order, setup->bitmap_format_bit_order, setup->bitmap_format_scanline_unit, setup->bitmap_format_scanline_pad );

	xcb_format_t *pixmap_formats = xcb_setup_pixmap_formats( setup );
	for (int i = 0; i < xcb_setup_pixmap_formats_length( setup ); i++) {
		printf( "\tdepth %d, bits per pixel %d, scanline pad %d\n", pixmap_formats[i].depth, pixmap_formats[i].bits_per_pixel, pixmap_formats[i].scanline_pad );
	}

	/* Find our screen. */
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator( xcb_get_setup( ctx.conn ) );
	for (int i = 0; i < scrno; ++i) {
		xcb_screen_next( &iter );
	}

	ctx.screen = iter.data;
	xcb_drawable_t root = ctx.screen->root;
	printf( "Screen info: root 0x%X, default colormap 0x%X, white 0x%X, black 0x%X, input masks 0x%X, dimensions in pixels %dx%d, in mm %dx%d, installed maps [%d, %d], root visual 0x%X, backing stores %d, save-unders %d, root depth %d, number of depths %d/%d\n", ctx.screen->root, ctx.screen->default_colormap, ctx.screen->white_pixel, ctx.screen->black_pixel, ctx.screen->current_input_masks, ctx.screen->width_in_pixels, ctx.screen->height_in_pixels, ctx.screen->width_in_millimeters, ctx.screen->height_in_millimeters, ctx.screen->min_installed_maps, ctx.screen->max_installed_maps, ctx.screen->root_visual, ctx.screen->backing_stores, ctx.screen->save_unders, ctx.screen->root_depth, ctx.screen->allowed_depths_len, xcb_screen_allowed_depths_length( ctx.screen ) );
	for (
		xcb_depth_iterator_t sdepth_iter = xcb_screen_allowed_depths_iterator( ctx.screen );
		sdepth_iter.rem;
		xcb_depth_next( &sdepth_iter )
	) {
		xcb_depth_t *sdepth_data = sdepth_iter.data;
		printf( "\tdepth %d, %d/%d visuals\n", sdepth_data->depth, sdepth_data->visuals_len, xcb_depth_visuals_length( sdepth_data ) );
		for (
			xcb_visualtype_iterator_t visal_iter = xcb_depth_visuals_iterator( sdepth_data );
			visal_iter.rem;
			xcb_visualtype_next( &visal_iter )
		) {
			xcb_visualtype_t *visual_data = visal_iter.data;
			if (sdepth_data->depth == 32 && !ctx.argb_visual) { ctx.argb_visual = visual_data->visual_id; }
			printf( "\t\tvisual id 0x%X: class %d bits per rgb value %d colormap entries %d red mask 0x%X green mask 0x%X blue mask 0x%X\n", visual_data->visual_id, visual_data->_class, visual_data->bits_per_rgb_value, visual_data->colormap_entries, visual_data->red_mask, visual_data->green_mask, visual_data->blue_mask );
		}
	}
	printf( "Screen size: %dx%d\nRoot window: 0x%X\n", ctx.screen->width_in_pixels, ctx.screen->height_in_pixels, ctx.screen->root );

	setupscreen( ctx.conn, root );
	setupshape( &ctx );
	setuprender( &ctx );

	xcb_render_query_pict_formats_reply_t *render_reply = xcb_render_query_pict_formats_reply( ctx.conn, xcb_render_query_pict_formats( ctx.conn ), NULL );
	printf( "%d formats, %d screens, %d depths, %d visuals, %d subpixel / %d pictforminfo, %d pictscreen, %d subpixel\n", render_reply->num_formats, render_reply->num_screens, render_reply->num_depths, render_reply->num_visuals, render_reply->num_subpixel, xcb_render_query_pict_formats_formats_length( render_reply ), xcb_render_query_pict_formats_screens_length( render_reply ), xcb_render_query_pict_formats_subpixels_length( render_reply ) );

	xcb_render_pictforminfo_t *formats = xcb_render_query_pict_formats_formats( render_reply );
	for (idx = 0; idx < xcb_render_query_pict_formats_formats_length( render_reply ); idx++) {
		printf( "\tpictformat 0x%X: type %d depth %d directformat [R(%d,0x%X), G(%d,0x%X), B(%d,0x%X), A(%d,0x%X)] colormap 0x%X\n", formats[idx].id, formats[idx].type, formats[idx].depth, formats[idx].direct.red_shift, formats[idx].direct.red_mask, formats[idx].direct.green_shift, formats[idx].direct.green_mask, formats[idx].direct.blue_shift, formats[idx].direct.blue_mask, formats[idx].direct.alpha_shift, formats[idx].direct.alpha_mask, formats[idx].colormap );
	}

	xcb_render_pictscreen_iterator_t pictscreen_iter = xcb_render_query_pict_formats_screens_iterator( render_reply );
	xcb_render_pictscreen_t *pictscreen_data = pictscreen_iter.data;
	printf( "\t%d/%d depths, fallback is 0x%X\n", xcb_render_pictscreen_depths_length( pictscreen_data ), pictscreen_data->num_depths, pictscreen_data->fallback );

	for (
		xcb_render_pictdepth_iterator_t depth_iter = xcb_render_pictscreen_depths_iterator( pictscreen_data );
		depth_iter.rem;
		xcb_render_pictdepth_next( &depth_iter )
	) {
		xcb_render_pictdepth_t* depth_data = depth_iter.data;
		printf( "\tdepth %d, %d visuals\n", depth_data->depth, depth_data->num_visuals );
		for (
			xcb_render_pictvisual_iterator_t pict_iter = xcb_render_pictdepth_visuals_iterator( depth_data );
			pict_iter.rem;
			xcb_render_pictvisual_next( &pict_iter )
		) {
			xcb_render_pictvisual_t* pict_data = pict_iter.data;
			printf( "\t\tvisual 0x%X, pictformat 0x%X\n", pict_data->visual, pict_data->format );
		}
	}

	create_cursors( &ctx );
	// Set a cursor on root

	uint32_t mask = XCB_CW_EVENT_MASK | XCB_CW_CURSOR;
	uint32_t values[2];

	/*	XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT: equivalent to Xlib's SubstructureRedirectMask,
			covers events CirculateRequest (Z order),
			ConfigureRequest (size, position, border, Z order)
			and MapRequest (request to actually display a window)
		XCB_EVENT_MASK_STRUCTURE_NOTIFY: equivalent to Xlib's StructureNotifyMask,
			covers events CirculateNotify, ConfigureNotify, DestroyNotify, GravityNotify,
			MapNotify, ReparentNotify and UnmapNotify and applies to the target window.
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY: equivalent to Xlib's SubstructureNotifyMask,
			covers event CreateNotify in addition to StructureNotifyMask ones,
			and applies to the subwindows of the target window.
	*/

	values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
		| XCB_EVENT_MASK_STRUCTURE_NOTIFY
		| XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
//		| XCB_EVENT_MASK_ENTER_WINDOW
//		| XCB_EVENT_MASK_LEAVE_WINDOW
		| XCB_EVENT_MASK_FOCUS_CHANGE;
	values[1] = ctx.x_cursor;

	xcb_void_cookie_t cookie = xcb_change_window_attributes_checked( ctx.conn, root, mask, values );
	xcb_generic_error_t *error = xcb_request_check( ctx.conn, cookie );
	xcb_flush( ctx.conn );

	if (NULL != error) {
		printf( "Request failed.\n" );
	}

	events( &ctx );

	xcb_disconnect( ctx.conn );
	return 0;
}

