// functions WyncEventUtils_create_event   wync_event__create_event
// types     WyncCtx, WyncCtx_Group_IntMap wync_map_node__insert
// enums     WYNC_PACKET_pkt_login         WyncMap__find
// const     WYNC_MAX_PEERS                ConMapNode
//
// * A good practice: library name + module name + action + subject
//   * If a part is not relevant just skip it
//   * but at least a module name and an action always should be presented.
//   * Examples: function name: os_task_set_prio, list_get_size, avg_get 

// PREFIX: Con -> "my CONtainers library"
// Integer Map implementation:
// * separate chaining
// * main array and buckets are dynamic arrays
// * keys are of type int
// * values are of type int
// Concepts:
// * Map:  The whole thing, array of Node
// * Node: Array of Slot
// * Pair: Represents a pair of Key and Value

#ifndef CON_MAP_H
#define CON_MAP_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define CON_MAP_NODE_DEFAULT_SIZE 2
#define CON_MAP_DEFAULT_SIZE 8
#define OK 0

#ifndef i32
#define i32 int32_t
#endif
#ifndef u32
#define u32 uint32_t
#endif

#define TYPE int32_t // gets undefined at the bottom

typedef struct {
    u32   size;
    u32   capacity;
    u32  *keys;     // negative keys are allowed
    TYPE *values;
} ConMapNode;

static void ConMapNode_init_node (ConMapNode *node)
{
    node->capacity = CON_MAP_NODE_DEFAULT_SIZE;
    node->size = 0;
    node->keys   = (u32*) malloc(sizeof(u32) * node->capacity);
    node->values = (TYPE*) malloc(sizeof(TYPE) * node->capacity);
}

static ConMapNode ConMapNode_create_node (void)
{
    ConMapNode node = { 0 };
    ConMapNode_init_node(&node);
    return node;
}

/// @param[out] index Item index if found
/// @returns error
/// @retval  0 Found
/// @retval -1 Not found
static i32 ConMap_array_find (u32 *array, u32 size, u32 value, u32 *index)
{
    for (u32 i = 0; i < size; ++i) {
        if (array[i] == value) {
            *index = i;
            return OK;
        }
    }; return -1;
}

// TODO: handle realloc error
/// @returns index
static u32 ConMapNode_insert (ConMapNode *node, u32 key, TYPE value)
{
    if (node->size >= node->capacity) {
        node->capacity *= 2;
        node->keys   = (u32*) realloc(node->keys, sizeof(u32) * node->capacity);
        node->values = (TYPE*) realloc(node->values, sizeof(TYPE) * node->capacity);
    }

    node->keys[node->size] = key;
    node->values[node->size] = value;
    ++node->size;
    return node->size -1;
}

/// @returns index
static u32 ConMapNode_emplace (ConMapNode *node, u32 key, TYPE value)
{
    u32 index;
    i32 err = ConMap_array_find(node->keys, node->size, key, &index);
    if (err != OK) {
        index = ConMapNode_insert (node, key, value);
    }
    else {
        node->values[index] = value;
    }
    return index;
}

/// @retval  0 OK
/// @retval -1 Not found
static i32 ConMapNode_remove_by_key (ConMapNode *node, u32 key)
{
    u32 index;
    i32 err = ConMap_array_find(node->keys, node->size, key, &index);
    if (err != OK) { // not found
        return -1;
    }

    node->keys[index]   = node->keys[node->size -1];
    node->values[index] = node->values[node->size -1];
    --node->size;
    return OK;
}

static void ConMapNode_clear_preserve_capacity (ConMapNode *node) {
    node->size = 0;
}


typedef struct {
    u32 pair_count;
    u32 size;
    ConMapNode *nodes;
} ConMap;


static void __ConMap_init (ConMap *map, u32 size) {
    map->pair_count = 0;
    map->size = size;
    map->nodes = (ConMapNode*) calloc(sizeof(ConMapNode), map->size);

    for (u32 i = 0; i < map->size; ++i) {
        ConMapNode *node = &map->nodes[i];
        ConMapNode_init_node(node);
    }
}


static void ConMap_init (ConMap *map) {
    __ConMap_init(map, CON_MAP_DEFAULT_SIZE);
}


static ConMap* ConMap_create (void) {
    ConMap *map = (ConMap*) calloc(sizeof(ConMap), 1);
    __ConMap_init(map, CON_MAP_DEFAULT_SIZE);
    return map;
}

static void ConMap_set_pair (ConMap *map, u32 key, TYPE value);

static void ConMap_rehash_map (ConMap *map) {
    u32 old_size = map->size;
    ConMapNode *old_nodes = map->nodes;

    __ConMap_init(map, map->size * 2);

    // reinsert pars

    u32 key;
    TYPE value;

    for (u32 i = 0; i < old_size; ++i) {
        ConMapNode *node = &old_nodes[i];

        for (u32 j = 0; j < node->size; ++j) {
            key = node->keys[j];
            value = node->values[j];

            ConMap_set_pair(map, key, value);
        }
    }

    // free old memory

    for (u32 i = 0; i < old_size; ++i) {
        ConMapNode *node = &old_nodes[i];
        free(node->values);
        free(node->keys);
        node->values = NULL;
        node->keys = NULL;
    }
    free(old_nodes);
    old_nodes = NULL;
}

static void ConMap_set_pair (ConMap *map, u32 key, TYPE value) {
    u32 node_index = (u32)key % map->size;
    ConMapNode *node = &map->nodes[node_index];

    u32 node_prev_size = node->size;
    ConMapNode_emplace(node, key, value);
    if (node_prev_size != node->size) {
        ++map->pair_count;
    }

    // TODO: Calculate factor and grow if too high
    float factor = (float)map->pair_count / (float)map->size;
    if (factor >= 0.75) {
        ConMap_rehash_map(map);
    }
}


static bool ConMap_has_key (ConMap *map, u32 key) {
    u32 node_index = (u32)key % map->size;
    ConMapNode *node = &map->nodes[node_index];
    u32 pair_index;
    i32 err = ConMap_array_find(node->keys, node->size, key, &pair_index) == OK;
    return err;
}


/// @param[out] value The found value if any
/// @retval     0     OK
/// @retval     1     Not found
static int ConMap_get (ConMap *map, u32 key, int* value) {
    u32 node_index = (u32)key % map->size;
    ConMapNode *node = &map->nodes[node_index];
    u32 pair_index;
    i32 err = ConMap_array_find(node->keys, node->size, key, &pair_index);
    if (err != OK) {
        return 1;
    }
    *value = node->values[pair_index];
    return OK;
}


static u32 ConMap_get_key_count (ConMap *map) {
    return map->pair_count;
}

/// @retval 0 OK
/// @retval 1 Not found
static int ConMap_remove_by_key (ConMap *map, u32 key) {
    u32 node_index = (u32)key % map->size;
    ConMapNode *node = &map->nodes[node_index];
    int err = ConMapNode_remove_by_key(node, key);
    if (err == OK) {
        --map->pair_count;
    }
    return err;
}


static void ConMap_clear_preserve_capacity (ConMap *map) {
    map->pair_count = 0;
    for (u32 i = 0; i < map->size; ++i) {
        ConMapNode_clear_preserve_capacity(&map->nodes[i]);
    }
}


typedef struct {
    u32 __node_idx;
    u32 __pair_idx; // inside the node
    u32 key;
} ConMapIterator;


/// @param[out] key No longer valid once end is reached
/// @retval       0 OK
/// @retval      -1 End reached
static i32 ConMap_iterator_get_next_key (ConMap *map, ConMapIterator *it)
{
    // check correctness

    if (it->__node_idx >= map->size) { return -1; }

    ConMapNode *node = &map->nodes[it->__node_idx];

    while (node->size == 0) {
        ++it->__node_idx;
        if (it->__node_idx >= map->size) { return -1; }
        node = &map->nodes[it->__node_idx];
    }

    // increment iterator, return key

    it->key = node->keys[it->__pair_idx];

    ++it->__pair_idx;
    if (it->__pair_idx >= node->size) {
        ++it->__node_idx;
        it->__pair_idx = 0;
    }

    return OK;
}


#undef TYPE
#endif // !CON_MAP_H
