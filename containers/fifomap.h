#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// FIFOMap

// Usage example:
// #define FIFOMAP_TYPE double
// #define FIFOMAP_PREFIX CustomName  (optional)
// #include "fifomap.h"
// double_FIFOMap myfifo = double_FIFOMap_create();

// user didn't specify type, using default
#ifndef FIFOMAP_TYPE
#define FIFOMAP_TYPE double
#endif

// extra containers

#ifndef u32_FIFORING_H
#define u32_FIFORING_H
#define FIFORING_TYPE u32
#define FIFORING_PREFIX u32_
#include "fiforing.h"
#endif // !u32_FIFORING_H

#define MAP_GENERIC_TYPE FIFOMAP_TYPE
#define MAP_GENERIC_PREFIX TOKCAT(FIFOMAP_TYPE, _internal_)
#include "map_generic.h"
#define MAP_G_PREFIX(name) TOKCAT(MAP_GENERIC_PREFIX, name)

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)
#ifndef FIFOMAP_PREFIX
#define FIFOMAP_PREFIX TOKCAT(TYPE, _)
#endif
#define PRE(name) TOKCAT(FIFOMAP_PREFIX, name)

// my types
#define TYPE        FIFOMAP_TYPE
#define FIFOMAP     PRE(FIFOMap)         // example: double_FIFOMap
#define MAP_GENERIC MAP_G_PREFIX(ConMap) // example: double_FIFOMap
#define i32 int32_t
#define u32 uint32_t
#define i64 int64_t
#define u64 uint64_t
#define OK 0


typedef struct {
    u32          max_size;
	u32_FIFORing ring;
    MAP_GENERIC  map_internal;
} FIFOMAP;


/// @param p_max_size Default 0
static FIFOMAP PRE(FIFOMap_init_calloc) (u32 p_max_size) {
	FIFOMAP map = { 0 };

	map.max_size = p_max_size;
	u32_FIFORing_init(p_max_size);
	MAP_G_PREFIX(ConMap_init)(&map.map_internal);

	return map;
}

static void PRE(FIFOMap_push_head_hash_and_item) (FIFOMAP *map, u64 item_hash, TYPE item) {
	// if not enough room -> pop

	if (map->ring.size >= map->max_size) {
		u64 cached_item_hash = *u32_FIFORing_pop_tail(&map->ring);
		MAP_G_PREFIX(ConMap_remove_by_key)
			(&map->map_internal, (i32)cached_item_hash);
	}

	u32_FIFORing_push_head(&map->ring, item_hash);
	MAP_G_PREFIX(ConMap_set_pair)
		(&map->map_internal, (i32)item_hash, item);
}


static bool PRE(FIFOMap_has_item_hash) (FIFOMAP *map, u32 item_hash) {
	return MAP_G_PREFIX(ConMap_has_key)(&map->map_internal, (i32)item_hash);
}


static TYPE *PRE(FIFOMap_get_item_by_hash) (FIFOMAP *map, u32 item_hash) {
	TYPE *item = NULL;
	i32 error = MAP_G_PREFIX(ConMap_get)(&map->map_internal, (i32)item_hash, &item);
	if (error != OK) {
		return NULL;
	}
	return item;
}

#undef PRE
//#undef FIFOMAP_TYPE
//#undef FIFOMAP_PREFIX
//#undef TYPE
//#undef FIFOMAP

