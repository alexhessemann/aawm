#include <assert.h>
#include <stdio.h> // DEBUG
#include <stdlib.h>
#include <string.h>

#include "map.h"


#if 0
#pragma mark Structures
#endif


struct map_entry {
	uint32_t key;
	void * value;
};


#if 0
#pragma mark Internal functions
#endif


// We perform a simple dichotomic search.
// If the key has been found, returns its index,
// If not, returns the index where it should be inserted.
static uint32_t map_lookup_index( map_t *a_map, uint32_t a_key )
{
	assert( a_map != NULL );

	uint32_t min_idx = 0;
	uint32_t max_idx = a_map->entry_count;
	uint32_t cur_idx = 0;

	while (min_idx != max_idx) {
		cur_idx = (min_idx + max_idx) / 2; // Note that cur_idx can't equal max_idx, which is what we want.
		if (a_key < a_map->entries[cur_idx].key) {
			max_idx = cur_idx;
		} else if (a_key > a_map->entries[cur_idx].key) {
			min_idx = ++cur_idx; // We need to update cur_idx as we can have min_idx == max_idx
		} else { // a_key == a_map->entries[cur_idx].key
			min_idx = cur_idx;
			max_idx = cur_idx;
		}
	}

	return cur_idx;
}


// Returns NULL on realloc failure
static map_t * map_insert_at( map_t *a_map, size_t a_idx, uint32_t a_key, void * a_value )
{
	assert( a_map != NULL );
	assert( a_idx <= a_map->entry_count );

	map_t * result = NULL;
	struct map_entry * entries = realloc( a_map->entries, (a_map->entry_count + 1) * sizeof( struct map_entry ) );
	if (entries) {
		memmove( &entries[a_idx + 1], &entries[a_idx], (a_map->entry_count - a_idx) * sizeof( struct map_entry ) );
		entries[a_idx].key = a_key;
		entries[a_idx].value = a_value;
		a_map->entries = entries;
		a_map->entry_count++;
		result = a_map;
	}

	return result;
}


#if 0
#pragma mark External functions
#endif


map_t * map_create()
{
	map_t * map = malloc( sizeof( map_t ) );
	if (map) {
		map->entry_count = 0;
		map->entries = NULL;
	}
	return map;
}


void map_destroy( map_t * a_map )
{
	free( a_map->entries );
	free( a_map );
}


map_t * map_add( map_t *a_map, uint32_t a_key, void * a_value  )
{
	map_t * result = NULL;
	uint32_t idx = map_lookup_index( a_map, a_key );
//	printf( "Adding 0x%X at %d\n", a_key, idx );
	if ( idx == a_map->entry_count || a_map->entries[idx].key != a_key) {
		result = map_insert_at( a_map, idx, a_key, a_value );
	}
	return result;
}


void * map_reassign( map_t *a_map, uint32_t a_key, void * a_value )
{
	void * result = NULL;
	uint32_t idx = map_lookup_index( a_map, a_key );
	if (a_map->entries[idx].key == a_key) {
		result = a_map->entries[idx].value;
		a_map->entries[idx].value = a_value;
	}
	return result;
}


// On realloc failure (which shouldn't happen), we keep the old memory block.
map_t * map_remove( map_t *a_map, uint32_t a_key, void ** a_value )
{
	map_t * result = NULL;
	uint32_t idx = map_lookup_index( a_map, a_key );
	if (a_map->entries[idx].key == a_key) {
		if (a_value) { *a_value = a_map->entries[idx].value; }
		if (--a_map->entry_count) {
			memmove( &a_map->entries[idx], &a_map->entries[idx + 1], (a_map->entry_count - idx) * sizeof( struct map_entry ) );
			struct map_entry * entries = realloc( a_map->entries, a_map->entry_count * sizeof( struct map_entry ) );
			if (entries) {
				a_map->entries = entries;
			}
		} else { // empty map
			free( a_map->entries );
			a_map->entries = NULL; // Needed to avoid a realloc fail (map_add) or a double free (map_destroy).
		}
		result = a_map;
	}
	return result;
}

void * map_lookup( map_t *a_map, uint32_t a_key )
{
	void * result = NULL;
	uint32_t idx = map_lookup_index( a_map, a_key );
	if (idx < a_map->entry_count && a_map->entries[idx].key == a_key) {
		result = a_map->entries[idx].value;
	}
	return result;
}


map_t * map_set( map_t *a_map, uint32_t a_key, void * a_value )
{
	map_t * result = NULL;
	uint32_t idx = map_lookup_index( a_map, a_key );
	if (a_map->entries[idx].key == a_key) {
		a_map->entries[idx].value = a_value;
		result = a_map;
	} else {
		result = map_insert_at( a_map, idx, a_key, a_value );
	}
	return result;
}


size_t get_map_size( map_t *a_map )
{
	return a_map->entry_count;
}

void debug_map_list( map_t *a_map )
{
	int i;
	printf( "[" );
	if (a_map->entry_count) {
		printf( " 0x%X", a_map->entries[0].key );
		for (i = 1; i < a_map->entry_count; i++) {
			printf( ", 0x%X", a_map->entries[i].key );
		}
	}
	printf( " ]\n" );
}
