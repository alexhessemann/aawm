// This provides a subset of 'xinput' util.

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h> // requires xfixes


char *get_atom_name(  xcb_connection_t *a_conn, xcb_atom_t a_atom )
{
	xcb_generic_error_t *error;
	xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply( a_conn, xcb_get_atom_name( a_conn, a_atom ), &error );
	if (reply) {
		int atom_name_len = xcb_get_atom_name_name_length( reply );
		char *atom_name = xcb_get_atom_name_name( reply );
		char *atom_name_z = malloc( atom_name_len + 1 );
		if (atom_name_z) {
			atom_name_z[atom_name_len] = 0;
			memcpy( atom_name_z, atom_name, atom_name_len );
		}
		free( reply );
		return atom_name_z;
	} else if (error != NULL) {
//		fprintf( stderr, "Get atom name ERROR code %d\n", error->error_code );
		char *atom_name_z = malloc( 13 );
		if (atom_name_z) {
			sprintf( atom_name_z, "<0x%X>", a_atom );
		}
		free( error );
		return atom_name_z;
	} else {
		fprintf( stderr, "No reply and no error ?!!\n" );
		return NULL;
	}
}


void print_device_type( uint16_t a_type )
{
	switch (a_type) {
	case XCB_INPUT_DEVICE_TYPE_MASTER_POINTER: printf( "MasterPointer" ); break;
	case XCB_INPUT_DEVICE_TYPE_MASTER_KEYBOARD: printf( "MasterKeyboard" ); break;
	case XCB_INPUT_DEVICE_TYPE_SLAVE_POINTER: printf( "SlavePointer" ); break;
	case XCB_INPUT_DEVICE_TYPE_SLAVE_KEYBOARD: printf( "SlaveKeyboard" ); break;
	case XCB_INPUT_DEVICE_TYPE_FLOATING_SLAVE: printf( "SloatingSlave" ); break;
	default: printf( "<Unsupported>" ); break;
	}
	printf( " (%d)", a_type );
}


void print_device_class( uint16_t a_class )
{
	switch (a_class) {
	case XCB_INPUT_DEVICE_CLASS_TYPE_KEY: printf( "KeyClass" ); break;
	case XCB_INPUT_DEVICE_CLASS_TYPE_BUTTON: printf( "ButtonClass" ); break;
	case XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR: printf( "AxisClass (valuator)" ); break;
	case XCB_INPUT_DEVICE_CLASS_TYPE_SCROLL: printf( "ScrollClass" ); break;
	case XCB_INPUT_DEVICE_CLASS_TYPE_TOUCH: printf( "TouchClass" ); break;
	default: printf( "<Unsupported>" ); break;
	}
	printf( " (%d)", a_class );
}


void print_key_class( const xcb_input_device_class_data_t *a_parsed_data )
{
	printf( "\t\tnum_keys: %u\n\t\tkeycodes: ", a_parsed_data->key.num_keys );
	assert( a_parsed_data->key.num_keys );
	int range_min = a_parsed_data->key.keys[0];
	int range_max = a_parsed_data->key.keys[0];
	for (int i = 1; i < a_parsed_data->key.num_keys; i++) {
		if (a_parsed_data->key.keys[i] == range_max + 1) {
			range_max++;
		} else {
			if (range_min == range_max) {
				printf( "%u, ", range_min );
			} else {
				printf( "[%u, %u], ", range_min, range_max );
			}
			range_min = range_max = a_parsed_data->key.keys[i];
		}
	}
	if (range_min == range_max) {
		printf( "%u\n", range_min );
	} else {
		printf( "[%u, %u]\n", range_min, range_max );
	}
}


void print_button_class( xcb_connection_t *a_conn, const xcb_input_device_class_t * a_class_data, const xcb_input_device_class_data_t *a_parsed_data )
{
	printf( "\t\tbutton state length: %d\n", xcb_input_device_class_data_button_state_length( a_class_data, a_parsed_data ) );
	uint32_t *state = xcb_input_device_class_data_button_state( a_parsed_data );
	printf( "\t\tbutton state: 0x%X\n", *state );
	int labels_count = xcb_input_device_class_data_button_labels_length( a_class_data, a_parsed_data );
	printf( "\t\tbutton labels length: %d\n\t\tlabels: ", labels_count );
	xcb_atom_t *labels = xcb_input_device_class_data_button_labels( a_parsed_data );
	for (int j = 0; j < labels_count; j++) {
		char *atom_name = get_atom_name( a_conn, labels[j] );
		printf( "'%s' ", atom_name );
		free( atom_name );
	}
	printf( "\n" );
}


void print_button_class_manual( xcb_connection_t *a_conn, const xcb_input_device_class_t * a_class_data, const void *a_buffer )
{
	const uint16_t *int16_data = (uint16_t *) a_buffer;
	const uint32_t *int32_data = (uint32_t *) &int16_data[1];
	printf( "\t\tnum_buttons: %d\n", int16_data[0] );
	printf("\t\tstate: 0x%08X\n\t\tlabels: ", int32_data[0] );
	for (int i = 1; i <= a_class_data->len - 3; i++) {
		char *atom_name = get_atom_name( a_conn, int32_data[i] );
		printf( "'%s' ", atom_name );
		free( atom_name );
	}
	printf( "\n" );
}


void print_valuator_class( xcb_connection_t *a_conn, const xcb_input_device_class_data_t *a_parsed_data )
{
	char *atom_name = get_atom_name( a_conn, a_parsed_data->valuator.label );
	printf( "\t\tnumber: %u, label: '%s', min %d.%u, max %d.%u, value %d.%u, resolution: %u, mode: %u\n",
		a_parsed_data->valuator.number, atom_name,
		a_parsed_data->valuator.min.integral, a_parsed_data->valuator.min.frac,
		a_parsed_data->valuator.max.integral, a_parsed_data->valuator.max.frac,
		a_parsed_data->valuator.value.integral, a_parsed_data->valuator.value.frac,
		a_parsed_data->valuator.resolution, a_parsed_data->valuator.mode
	);
	free( atom_name );
}


void print_scroll_class( const xcb_input_device_class_data_t *a_parsed_data )
{
	printf( "\t\tnumber: %u, scroll type: %u, flags: 0x%08X, increment: %d.%u\n",
		a_parsed_data->scroll.number, a_parsed_data->scroll.scroll_type, a_parsed_data->scroll.flags,
		a_parsed_data->scroll.increment.integral, a_parsed_data->scroll.increment.frac
	);
}


// Untested…
void print_touch_class( const xcb_input_device_class_data_t *a_parsed_data )
{
	printf( "\t\tmode: %u, num touches: %u\n", a_parsed_data->touch.mode, a_parsed_data->touch.num_touches );
}


int main()
{
	xcb_connection_t *conn = xcb_connect( NULL, NULL );

	const xcb_query_extension_reply_t *extension = xcb_get_extension_data( conn, &xcb_input_id );
	assert( extension );
	if (!extension->present) {
		printf( "XInput extension absent.\n" );
	} else {
		printf( "First event: %d, first error: %d\n", extension->first_event, extension->first_error );

		xcb_input_xi_query_version_reply_t *version = xcb_input_xi_query_version_reply( conn, xcb_input_xi_query_version( conn, 2, 3 ), NULL );
		printf( "Version %d.%d\n\n", version->major_version, version->minor_version );

		xcb_input_xi_query_device_reply_t *reply = xcb_input_xi_query_device_reply( conn, xcb_input_xi_query_device( conn, 0 /* AllDevices */ ), NULL );
		if (reply) {
			assert( reply->num_infos == xcb_input_xi_query_device_infos_length( reply ) );
			printf( "%d xinput devices:\n\n", reply->num_infos );
			for (
				xcb_input_xi_device_info_iterator_t iter = xcb_input_xi_query_device_infos_iterator( reply );
				iter.rem;
				xcb_input_xi_device_info_next( &iter )
			) {
				xcb_input_xi_device_info_t *data = iter.data;
				assert( data->name_len == xcb_input_xi_device_info_name_length( data ) );
				printf( "Device ID %d '%.*s':\n", data->deviceid, data->name_len, xcb_input_xi_device_info_name( data ) );
				printf( "\ttype " );
				print_device_type( data->type );
				assert( data->num_classes == xcb_input_xi_device_info_classes_length( data ) );
				printf( ", attached to ID %d, %s, providing %d classes:\n", data->attachment, data->enabled ? "enabled" : "disabled", data->num_classes );
				for (
					xcb_input_device_class_iterator_t cl_iter = xcb_input_xi_device_info_classes_iterator( data );
					cl_iter.rem;
					xcb_input_device_class_next( &cl_iter )
				) {
					xcb_input_device_class_t *cl_data = cl_iter.data;
					printf( "\tclass " );
					print_device_class( cl_data->type );
					printf( ", len %d, source id %d\n", cl_data->len, cl_data->sourceid );
					void *class_data = xcb_input_device_class_data( cl_data );
					printf( "\t\tclass_sizeof: %d, class_data_sizeof: %d\n", xcb_input_device_class_sizeof( class_data ), xcb_input_device_class_data_sizeof( class_data, cl_data->type ) );
					xcb_input_device_class_data_t *parsed_data = malloc( sizeof( xcb_input_device_class_data_t ) );
					int result = xcb_input_device_class_data_unpack( class_data, cl_data->type, parsed_data );
					printf( "\t\txcb_input_device_class_data_unpack returned: %d\n", result );

					switch (cl_data->type) {
					case XCB_INPUT_DEVICE_CLASS_TYPE_KEY: print_key_class( parsed_data ); break;
					case XCB_INPUT_DEVICE_CLASS_TYPE_BUTTON:
						print_button_class( conn, cl_data, parsed_data );
						print_button_class_manual( conn, cl_data, class_data);
						break;
					case XCB_INPUT_DEVICE_CLASS_TYPE_VALUATOR: print_valuator_class( conn, parsed_data ); break;
					case XCB_INPUT_DEVICE_CLASS_TYPE_SCROLL: print_scroll_class( parsed_data ); break;
					case XCB_INPUT_DEVICE_CLASS_TYPE_TOUCH: print_touch_class( parsed_data ); break;
					default:
						printf( "\t\t<Non-supported class type>\n" );
					}

					free( parsed_data );

				}
				xcb_input_xi_list_properties_reply_t *prop_list_reply = xcb_input_xi_list_properties_reply( conn, xcb_input_xi_list_properties( conn, data->deviceid ), NULL );
				printf( "\t%u properties:\n", prop_list_reply->num_properties );
				assert( prop_list_reply->num_properties == xcb_input_xi_list_properties_properties_length( prop_list_reply ) );
				xcb_atom_t *atoms = xcb_input_xi_list_properties_properties( prop_list_reply );
				for (int i = 0; i < prop_list_reply->num_properties; i++ ) {
					xcb_input_xi_get_property_reply_t *prop_reply = xcb_input_xi_get_property_reply( conn, xcb_input_xi_get_property( conn, data->deviceid, false, atoms[i], XCB_GET_PROPERTY_TYPE_ANY, 0, 40 ), NULL );
					char *atom_name = get_atom_name( conn, atoms[i] );
					char *type_name = get_atom_name( conn, prop_reply->type );
					printf( "\t\t'%s': type '%s' format %u, num items %u, bytes after: %u, data: ", atom_name, type_name, prop_reply->format, prop_reply->num_items, prop_reply->bytes_after );
					assert( prop_reply->bytes_after == 0 );
					if (!strcmp( type_name, "STRING" )) {
						assert( prop_reply->format == 8 );
						printf( "\"%.*s\"", prop_reply->num_items, (char*) xcb_input_xi_get_property_items( prop_reply ) );
					} else if (!strcmp( type_name, "CARDINAL" )) {
						printf( "[ " );
						switch (prop_reply->format) {
						case 8: {
							uint8_t *data = (uint8_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%u ", data[i] );
						}
						break;
						case 16: {
							uint16_t *data = (uint16_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%u ", data[i] );
						}
						break;
						case 32: {
							uint32_t *data = (uint32_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%u ", data[i] );
						}
						break;
						}
						printf( "]" );
					} else if (!strcmp( type_name, "INTEGER" )) {
						printf( "[ " );
						switch (prop_reply->format) {
						case 8: {
							int8_t *data = (int8_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%d ", data[i] );
						}
						break;
						case 16: {
							int16_t *data = (int16_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%d ", data[i] );
						}
						break;
						case 32: {
							int32_t *data = (int32_t *) xcb_input_xi_get_property_items( prop_reply );
							for (int i = 0; i < prop_reply->num_items; i++ )
								printf( "%d ", data[i] );
						}
						break;
						}
						printf( "]" );
					} else if (!strcmp( type_name, "FLOAT" )) {
						assert( prop_reply->format == 32 );
						printf( "[ " );
						float *data = (float *) xcb_input_xi_get_property_items( prop_reply );
						for (int i = 0; i < prop_reply->num_items; i++ ) {
							printf( "%f ", data[i] );
						}
						printf( "]" );
					} else if (!strcmp( type_name, "ATOM" )) {
						assert( prop_reply->format == 32 );
						printf( "[ " );
						xcb_atom_t *data = (xcb_atom_t *) xcb_input_xi_get_property_items( prop_reply );
						for (int i = 0; i < prop_reply->num_items; i++ ) {
							char *data_name = get_atom_name( conn, data[i] );
							printf( "'%s' ", data_name );
							free( data_name );
						}
						printf( "]" );
					}
					printf( "\n" );
					free( type_name );
					free( atom_name );
				}
				printf( "\n" );
			}
		} else {
			printf( "xi query device failed\n" );
		}
	}
}

