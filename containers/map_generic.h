#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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
//
// Usage example:
//
// #define MAP_GENERIC_TYPE double
// #define MAP_GENERIC_KEY_TYPE CustomType :optional, default uint32_t
// #define MAP_GENERIC_PREFIX CustomName   :optional, default MAP_GENERIC_TYPE
// #include "map_generic.h"
// #undef MAP_GENERIC_TYPE
// #undef MAP_GENERIC_KEY_TYPE
// #undef MAP_GENERIC_PREFIX
//
// double_ConMap mymap = double_ConMap_create();

// user didn't specify type, using default
#ifndef MAP_GENERIC_TYPE
#define MAP_GENERIC_TYPE double
#endif

//typedef struct { char name[40]; } Name;

// user didn't specify key type, using default
#ifndef MAP_GENERIC_KEY_TYPE
#define MAP_GENERIC_KEY_TYPE uint32_t
#endif

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)
#ifndef MAP_GENERIC_PREFIX
#define MAP_GENERIC_PREFIX TOKCAT(MAP_GENERIC_TYPE, _)
#endif
#define PRE(name) TOKCAT(MAP_GENERIC_PREFIX, name)

// my types
#define KEY         MAP_GENERIC_KEY_TYPE
#define TYPE        MAP_GENERIC_TYPE
#define CONMAP      PRE(ConMap)     // example: double_ConMap
#define CONMAP_NODE PRE(ConMapNode) // example: double_ConMapNode
#define CONMAP_IT   PRE(ConMapIterator)
#define CON_MAP_NODE_DEFAULT_SIZE 2
#define CON_MAP_DEFAULT_SIZE 8
#define OK 0
#define MIN(a,b) (((a)<(b))?(a):(b))


/// @param[out] index Item index if found
/// @returns error
/// @retval  0 Found
/// @retval -1 Not found
inline static int32_t PRE(find_key) (
    KEY *array, uint32_t _size, KEY value, uint32_t *index)
{
    for (uint32_t i = 0; i < _size; ++i) {
        if (memcmp(&array[i], &value, sizeof(KEY)) == 0) {
            *index = i;
            return OK;
        }
    }; return -1;
}


typedef struct {
    uint32_t   _size;
    uint32_t   capacity;
    KEY  *keys;
    TYPE *values;
} CONMAP_NODE;

static void PRE(ConMapNode_init_node) (CONMAP_NODE *node)
{
    node->capacity = CON_MAP_NODE_DEFAULT_SIZE;
    node->_size = 0;
    node->keys   = (KEY*)malloc(sizeof(KEY) * node->capacity);
    node->values = (TYPE*)malloc(sizeof(TYPE) * node->capacity);
}

static CONMAP_NODE PRE(ConMapNode_create_node) (void)
{
    CONMAP_NODE node = { 0 };
    PRE(ConMapNode_init_node)(&node);
    return node;
}

// TODO: handle realloc error
/// @returns index
static uint32_t PRE(ConMapNode_insert) (CONMAP_NODE *node, KEY key, TYPE value)
{
    if (node->_size >= node->capacity) {
        node->capacity *= 2;
        node->keys   = (KEY*)realloc(node->keys, sizeof(KEY) * node->capacity);
        node->values = (TYPE*)realloc(node->values, sizeof(TYPE) * node->capacity);
    }

    node->keys[node->_size] = key;
    node->values[node->_size] = value;
    ++node->_size;
    return node->_size -1;
}

/// @returns index
static uint32_t PRE(ConMapNode_emplace) (CONMAP_NODE *node, KEY key, TYPE value)
{
    uint32_t index;
    int32_t err = PRE(find_key)(node->keys, node->_size, key, &index);
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
static int32_t PRE(ConMapNode_remove_by_key) (CONMAP_NODE *node, KEY key)
{
    uint32_t index;
    int32_t err = PRE(find_key)(node->keys, node->_size, key, &index);
    if (err != OK) { // not found
        return -1;
    }

    node->keys[index]   = node->keys[node->_size -1];
    node->values[index] = node->values[node->_size -1];
    --node->_size;
    return OK;
}

static void PRE(ConMapNode_clear_preserve_capacity) (CONMAP_NODE *node) {
    node->_size = 0;
}


typedef struct {
    uint32_t pair_count;
    uint32_t _size;
    CONMAP_NODE *_nodes;
} CONMAP;


static void PRE(__ConMap_init) (CONMAP *map, uint32_t _size) {
    map->pair_count = 0;
    map->_size = _size;
    map->_nodes = (CONMAP_NODE*)calloc(sizeof(CONMAP_NODE), map->_size);

    for (uint32_t i = 0; i < map->_size; ++i) {
        CONMAP_NODE *node = &map->_nodes[i];
        PRE(ConMapNode_init_node)(node);
    }
}


static void PRE(ConMap_init) (CONMAP *map) {
    PRE(__ConMap_init)(map, CON_MAP_DEFAULT_SIZE);
}


static CONMAP* PRE(ConMap_create) (void) {
    CONMAP *map = (CONMAP*)calloc(sizeof(CONMAP), 1);
    PRE(__ConMap_init)(map, CON_MAP_DEFAULT_SIZE);
    return map;
}

static void PRE(ConMap_set_pair) (CONMAP *map, KEY key, TYPE value);

static void PRE(ConMap_rehash_map) (CONMAP *map) {
    uint32_t old_size = map->_size;
    CONMAP_NODE *old_nodes = map->_nodes;

    PRE(__ConMap_init)(map, map->_size * 2);

    // reinsert pars

    KEY key;
    TYPE value;

    for (uint32_t i = 0; i < old_size; ++i) {
        CONMAP_NODE *node = &old_nodes[i];

        for (uint32_t j = 0; j < node->_size; ++j) {
            key = node->keys[j];
            value = node->values[j];

            PRE(ConMap_set_pair)(map, key, value);
        }
    }

    // free old memory

    for (uint32_t i = 0; i < old_size; ++i) {
        CONMAP_NODE *node = &old_nodes[i];
        free(node->values);
        free(node->keys);
        node->values = NULL;
        node->keys = NULL;
    }
    free(old_nodes);
    old_nodes = NULL;
}


inline static uint32_t PRE(get_key_node_index) (CONMAP *map, KEY key) {
    uint32_t key_fragment = 0;
    memcpy(&key_fragment, &key, MIN(sizeof(uint32_t), sizeof(KEY)));
    return (uint32_t)key_fragment % map->_size;
    // TODO: Use a better hash
}



static void PRE(ConMap_set_pair) (CONMAP *map, KEY key, TYPE value) {

    uint32_t node_index = PRE(get_key_node_index)(map, key);
    CONMAP_NODE *node = &map->_nodes[node_index];

    uint32_t node_prev_size = node->_size;
    PRE(ConMapNode_emplace)(node, key, value);
    if (node_prev_size != node->_size) {
        ++map->pair_count;
    }

    // TODO: Calculate factor and grow if too high
    float factor = (float)map->pair_count / (float)map->_size;
    if (factor >= 0.75) {
        PRE(ConMap_rehash_map)(map);
    }
}


static bool PRE(ConMap_has_key) (CONMAP *map, KEY key) {
    uint32_t node_index = PRE(get_key_node_index)(map, key);
    CONMAP_NODE *node = &map->_nodes[node_index];
    uint32_t pair_index;
    int32_t err = PRE(find_key)(node->keys, node->_size, key, &pair_index) == OK;
    return err;
}


/// @param[out] value The found value if any
/// @return error
/// @retval     0     OK
/// @retval     1     Not found
static int32_t PRE(ConMap_get) (CONMAP *map, KEY key, TYPE** value) {
    uint32_t node_index = PRE(get_key_node_index)(map, key);
    CONMAP_NODE *node = &map->_nodes[node_index];
    uint32_t pair_index;
    int32_t err = PRE(find_key)(node->keys, node->_size, key, &pair_index);
    if (err != OK) {
        return 1;
    }
    (*value) = &node->values[pair_index];
    return OK;
}


static uint32_t PRE(ConMap_get_key_count) (CONMAP *map) {
    return map->pair_count;
}


/// @retval 0 OK
/// @retval 1 Not found
static int PRE(ConMap_remove_by_key) (CONMAP *map, KEY key) {
    uint32_t node_index = PRE(get_key_node_index)(map, key);
    CONMAP_NODE *node = &map->_nodes[node_index];
    int err = PRE(ConMapNode_remove_by_key)(node, key);
    if (err == OK) {
        --map->pair_count;
    }
    return err;
}


static void PRE(ConMap_clear_preserve_capacity) (CONMAP *map) {
    map->pair_count = 0;
    for (uint32_t i = 0; i < map->_size; ++i) {
        PRE(ConMapNode_clear_preserve_capacity)(&map->_nodes[i]);
    }
}


typedef struct {
    uint32_t node_idx;
    uint32_t pair_idx; // inside the node
    KEY key;
} CONMAP_IT;


/// @param[out] key No longer valid once end is reached
/// @retval       0 OK
/// @retval      -1 End reached
static int32_t PRE(ConMap_iterator_get_next_key) (CONMAP *map, CONMAP_IT *it)
{
    // check correctness

    if (it->node_idx >= map->_size) { return -1; }

    CONMAP_NODE *node = &map->_nodes[it->node_idx];

    while (node->_size == 0) {
        ++it->node_idx;
        if (it->node_idx >= map->_size) { return -1; }
        node = &map->_nodes[it->node_idx];
    }

    // increment iterator, return key

    it->key = node->keys[it->pair_idx];

    ++it->pair_idx;
    if (it->pair_idx >= node->_size) {
        ++it->node_idx;
        it->pair_idx = 0;
    }

    return OK;
}

#undef PRE
#undef TOKCAT_
#undef TOKCAT
#undef TYPE
#undef CONMAP
#undef CONMAP_NODE
#undef CONMAP_IT
#undef CON_MAP_NODE_DEFAULT_SIZE
#undef CON_MAP_DEFAULT_SIZE
// don't undef: user generated
// MAP_GENERIC_TYPE
// MAP_GENERIC_KEY_TYPE
// MAP_GENERIC_PREFIX
