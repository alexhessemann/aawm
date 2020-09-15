#ifndef MAP_H
#define MAP_H

#include <stdint.h>

// This is actually an associative array. (= map, dictionary)

//struct map_entry;

typedef struct map {
	size_t entry_count;
	struct map_entry * entries;
} map_t;

map_t * map_create();

void map_destroy( map_t * );

// NULL is returned if a_entry->key was found in the set, and on allocation failure.
map_t *map_add( map_t *a_map, uint32_t a_key, void * a_value );

// Does nothing and returns NULL if a_key is not found in the set
// Otherwise replace the value and returns the old value.
void * map_reassign( map_t *a_map, uint32_t a_key, void * a_value );

// The pointed value isn't freed to avoid a double free
// NULL is returned if a_entry->key was not found in the set.
map_t * map_remove( map_t *a_map, uint32_t a_key );

// NULL is returned if a_key not found.
void * map_lookup( map_t *a_map, uint32_t a_key );

map_t *map_set( map_t *a_map, uint32_t a_key, void * a_value );

size_t get_map_size( map_t *a_map );

void debug_map_list( map_t *a_map );

#endif
