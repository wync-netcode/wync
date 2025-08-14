#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// Generic Map implementation
// * PREFIX: Con -> "my CONtainers library"
// * separate chaining
// * main array and buckets are dynamic arrays
// * keys are of type int
// * values are of GENERIC type
// Concepts:
// * Map:  The whole thing, array of Node
// * Node: Array of Slot
// * Pair: Represents a pair of Key and Value

// Usage example:
// #define MAP_GENERIC_TYPE double
// #define MAP_GENERIC_PREFIX CustomName  (optional)
// #include "map_generic.h"
// double_ConMap mymap = double_ConMap_create();

// user didn't specify type, using default
#ifndef MAP_GENERIC_TYPE
#define MAP_GENERIC_TYPE double
#endif

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)
#ifndef MAP_GENERIC_PREFIX
#define MAP_GENERIC_PREFIX TOKCAT(MAP_GENERIC_TYPE, _)
#endif
#define PRE(name) TOKCAT(MAP_GENERIC_PREFIX, name)

// my types
#define TYPE        MAP_GENERIC_TYPE
#define CONMAP      PRE(ConMap)     // example: double_ConMap
#define CONMAP_NODE PRE(ConMapNode) // example: double_ConMapNode
#define CONMAP_IT   PRE(ConMapIterator)
#define CON_MAP_NODE_DEFAULT_SIZE 2
#define CON_MAP_DEFAULT_SIZE 8
#define OK 0
#define i32 int32_t
#define u32 uint32_t

#ifndef MAP_GENERIC_H
#define MAP_GENERIC_H

/// @param[out] index Item index if found
/// @returns error
/// @retval  0 Found
/// @retval -1 Not found
inline static i32 ConMap_buffer_find_val (i32 *array, u32 size, i32 value, u32 *index)
{
    for (u32 i = 0; i < size; ++i) {
        if (array[i] == value) {
            *index = i;
            return OK;
        }
    }; return -1;
}

#endif // !MAP_GENERIC_H


typedef struct {
    u32 node_idx;
    u32 pair_idx; // inside the node
} CONMAP_IT;

CONMAP_IT PRE(ConMap_make_iterator) (void)
{
    CONMAP_IT it = { 0, 0 };
    return it;
}


typedef struct {
    u32   size;
    u32   capacity;
    u32  *keys;     // negative keys are OK, casted to u32
    TYPE *values;
} CONMAP_NODE;

void PRE(ConMapNode_init_node) (CONMAP_NODE *node)
{
    node->capacity = CON_MAP_NODE_DEFAULT_SIZE;
    node->size = 0;
    node->keys   = (u32*)malloc(sizeof(u32) * node->capacity);
    node->values = (TYPE*)malloc(sizeof(TYPE) * node->capacity);
}

CONMAP_NODE PRE(ConMapNode_create_node) (void)
{
    CONMAP_NODE node = { 0 };
    PRE(ConMapNode_init_node)(&node);
    return node;
}

// TODO: handle realloc error
/// @returns index
u32 PRE(ConMapNode_insert) (CONMAP_NODE *node, u32 key, TYPE value)
{
    if (node->size >= node->capacity) {
        node->capacity *= 2;
        node->keys   = (u32*)realloc(node->keys, sizeof(u32) * node->capacity);
        node->values = (TYPE*)realloc(node->values, sizeof(TYPE) * node->capacity);
    }

    node->keys[node->size] = key;
    node->values[node->size] = value;
    ++node->size;
    return node->size -1;
}

/// @returns index
u32 PRE(ConMapNode_emplace) (CONMAP_NODE *node, u32 key, TYPE value)
{
    u32 index;
    i32 err = ConMap_buffer_find_val(node->keys, node->size, key, &index);
    if (err != OK) {
        index = PRE(ConMapNode_insert) (node, key, value);
    }
    else {
        node->values[index] = value;
    }
    return index;
}

/// @retval  0 OK
/// @retval -1 Not found
i32 PRE(ConMapNode_remove_by_key) (CONMAP_NODE *node, u32 key)
{
    u32 index;
    i32 err = ConMap_buffer_find_val(node->keys, node->size, key, &index);
    if (err != OK) { // not found
        return -1;
    }

    node->keys[index]   = node->keys[node->size -1];
    node->values[index] = node->values[node->size -1];
    --node->size;
    return OK;
}


typedef struct {
    u32 pair_count;
    u32 size;
    CONMAP_NODE *nodes;
} CONMAP;


static void PRE(__ConMap_init) (CONMAP *map, u32 size) {
    map->pair_count = 0;
    map->size = size;
    map->nodes = (CONMAP_NODE*)calloc(sizeof(CONMAP_NODE), map->size);

    for (u32 i = 0; i < map->size; ++i) {
        CONMAP_NODE *node = &map->nodes[i];
        PRE(ConMapNode_init_node)(node);
    }
}


void PRE(ConMap_init) (CONMAP *map) {
    PRE(__ConMap_init)(map, CON_MAP_DEFAULT_SIZE);
}


CONMAP* PRE(ConMap_create) (void) {
    CONMAP *map = (CONMAP*)calloc(sizeof(CONMAP), 1);
    PRE(__ConMap_init)(map, CON_MAP_DEFAULT_SIZE);
    return map;
}

void PRE(ConMap_set_pair) (CONMAP *map, u32 key, TYPE value);

static void PRE(ConMap_rehash_map) (CONMAP *map) {
    u32 old_size = map->size;
    CONMAP_NODE *old_nodes = map->nodes;

    PRE(__ConMap_init)(map, map->size * 2);

    // reinsert pars

    u32 key;
    TYPE value;

    for (u32 i = 0; i < old_size; ++i) {
        CONMAP_NODE *node = &old_nodes[i];

        for (u32 j = 0; j < node->size; ++j) {
            key = node->keys[j];
            value = node->values[j];

            PRE(ConMap_set_pair)(map, key, value);
        }
    }

    // free old memory

    for (u32 i = 0; i < old_size; ++i) {
        CONMAP_NODE *node = &old_nodes[i];
        free(node->values);
        free(node->keys);
    }
    free(old_nodes);
}

void PRE(ConMap_set_pair) (CONMAP *map, u32 key, TYPE value) {
    u32 node_index = (u32)key % map->size;
    CONMAP_NODE *node = &map->nodes[node_index];

    u32 node_prev_size = node->size;
    PRE(ConMapNode_emplace)(node, key, value);
    if (node_prev_size != node->size) {
        ++map->pair_count;
    }

    // TODO: Calculate factor and grow if too high
    float factor = (float)map->pair_count / (float)map->size;
    if (factor >= 0.75) {
        PRE(ConMap_rehash_map)(map);
    }
}


bool PRE(ConMap_has_key) (CONMAP *map, u32 key) {
    u32 node_index = (u32)key % map->size;
    CONMAP_NODE *node = &map->nodes[node_index];
    u32 pair_index;
    return ConMap_buffer_find_val(node->keys, node->size, key, &pair_index) == OK;
}


/// @param[out] value The found value if any
/// @retval     0     OK
/// @retval     1     Not found
i32 PRE(ConMap_get) (CONMAP *map, u32 key, TYPE* value) {
    u32 node_index = (u32)key % map->size;
    CONMAP_NODE *node = &map->nodes[node_index];
    u32 pair_index;
    i32 err = ConMap_buffer_find_val(node->keys, node->size, key, &pair_index);
    if (err != OK) {
        return 1;
    }
    *value = node->values[pair_index];
    return OK;
}


u32 PRE(ConMap_get_key_count) (CONMAP *map) {
    return map->pair_count;
}

/// @retval 0 OK
/// @retval 1 Not found
int PRE(ConMap_remove_by_key) (CONMAP *map, u32 key) {
    u32 node_index = (u32)key % map->size;
    CONMAP_NODE *node = &map->nodes[node_index];
    int err = PRE(ConMapNode_remove_by_key)(node, key);
    if (err == OK) {
        --map->pair_count;
    }
    return err;
}

/// @param[out] key No longer valid once end is reached
/// @retval       0 OK
/// @retval      -1 End reached
i32 PRE(ConMap_iterator_get_next_key) (CONMAP *map, CONMAP_IT *it, u32 *key)
{
    // check correctness

    if (it->node_idx < 0 || it->node_idx >= map->size) { return -1; }

    CONMAP_NODE *node = &map->nodes[it->node_idx];

    if (it->pair_idx < 0) { return -1; }

    if (it->pair_idx >= node->size) {

        // skip empty nodes

        while (it->node_idx < map->size) {
            node = &map->nodes[it->node_idx];
            if (node->size > 0) break;
            ++it->node_idx;
        }

        it->pair_idx = 0;
        
        if (it->node_idx >= map->size) { return -1; }
    }

    // increment iterator, return key

    *key = node->keys[it->pair_idx];

    ++it->pair_idx;
    if (it->pair_idx >= node->size) {
        ++it->node_idx;
        it->pair_idx = 0;
    }

    return OK;
}

#undef PRE
//#undef MAP_GENERIC_TYPE
//#undef MAP_GENERIC_PREFIX
//#undef TYPE
//#undef CONMAP
//#undef CONMAP_NODE
//#undef CONMAP_IT

