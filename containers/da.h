// Usage example:
// #define DYN_ARR_TYPE uint
// #undef  DYN_ARR_PREFIX            (optional)
// #define DYN_ARR_PREFIX CustomName (optional)
// #define DYN_ARR_ENABLE_SORT       (optional)
// #include "da.h"

// user didn't specify type, using default
#ifndef DYN_ARR_TYPE
#define DYN_ARR_TYPE uint
#endif

// token concatenation
#define TOKCAT_(a, b) a ## b
#define TOKCAT(a, b) TOKCAT_(a, b)

#ifndef DYN_ARR_PREFIX
#define DYN_ARR_PREFIX TOKCAT(DYN_ARR_TYPE, _)
#endif

#define PRE(name) TOKCAT(DYN_ARR_PREFIX, name)

#define TYPE DYN_ARR_TYPE
#define OK 0

#include <stdlib.h>

typedef struct {
    size_t size;
    size_t capacity;
    TYPE *items;
} PRE(DynArr);


static inline PRE(DynArr) PRE(DynArr_create) (void) {
    PRE(DynArr) da = { 0 };
    da.capacity = 2;
    da.items = (TYPE *)malloc(sizeof(TYPE) * da.capacity);
    return da;
}


/// Inserts item at the back
/// @param   item  Item passed as value
/// @returns index
static inline size_t PRE(DynArr_insert) (PRE(DynArr) *da, TYPE item) {
    if (da->size >= da->capacity) {
        da->capacity *= 2;
        da->items = (TYPE *)realloc(da->items, sizeof(TYPE) * da->capacity);
    }

    da->items[da->size] = item;
    ++da->size;
    return da->size -1;
}

/// @returns error
static int PRE(DynArr_insert_at) (PRE(DynArr) *da, size_t index, TYPE item) {
    if (index >= da->size) {
        return -1;
    }

    da->items[index] = item;
    return OK;
}


/// @returns TYPE
static TYPE *PRE(DynArr_get) (PRE(DynArr) *da, size_t index) {
    return &da->items[index];
}


static size_t PRE(DynArr_get_size) (PRE(DynArr) *da) {
    return da->size;
}


/// Removing while iterating is Undefined Behaviour
/// Removing will alter the order
/// @retval  0 OK
/// @retval -1 Not found
static int PRE(DynArr_remove_at) (PRE(DynArr) *da, size_t index) {
    if (index < 0 || index >= da->size){
        return -1;
    }

    da->items[index] = da->items[da->size -1];
    --da->size;
    return OK;
}


/// Clears the array but preserves the allocated memory
static void PRE(DynArr_clear_preserving_capacity) (PRE(DynArr) *da) {
    da->size = 0;
}


typedef struct {
    size_t  __next_index;
    size_t  index;
    TYPE   *item;
} PRE(DynArrIterator);


/// @param[out] it iterator
/// @retval  0 OK
/// @retval -1 End reached
static int PRE(DynArr_iterator_get_next) (PRE(DynArr) *da, PRE(DynArrIterator) *it)
{
    if (it->__next_index < 0 || it->__next_index >= da->size) {
        it->item = NULL;
        return -1;
    }

    it->item = &da->items[it->__next_index];
    it->index = it->__next_index;
    ++it->__next_index;

    return OK;
}

#ifdef DYN_ARR_ENABLE_SORT
typedef struct {
    size_t a;
    size_t b;
} PRE(DynArr_Pair);

static PRE(DynArr_Pair) PRE(DynArr_partition) (TYPE *buffer, size_t from, size_t to) {

    size_t left = from;
    size_t eq = from;
    size_t right = to;
    TYPE pivot = buffer[(from + to) / 2];

    while (eq <= right) {

        if (buffer[eq] < pivot) {
            TYPE swap = buffer[eq];
            buffer[eq] = buffer[left];
            buffer[left] = swap;

            ++left;
            ++eq;
        } else if (buffer[eq] > pivot) {
            TYPE swap = buffer[eq];
            buffer[eq] = buffer[right];
            buffer[right] = swap;

            --right;
        } else {
            ++eq;
        }
    }

    return (PRE(DynArr_Pair)){left, right};
}

static void PRE(DynArr_sort_range) (TYPE *buffer, size_t from, size_t to) {
    if (from >= 0 && from < to) {
        PRE(DynArr_Pair) partition_index = PRE(DynArr_partition)(buffer, from, to);
        if (from +1 < partition_index.a) {
            PRE(DynArr_sort_range) (buffer, from, partition_index.a -1);
        }
        if (partition_index.b +1 < to) {
            PRE(DynArr_sort_range) (buffer, partition_index.b +1, to);
        }
    }
}

// sorts it and repositions head to latest
static void PRE(DynArr_sort) (PRE(DynArr) *r) {
    if (r->size == 0) return;
    PRE(DynArr_sort_range) (r->items, 0, r->size-1);
}
#endif // !DYN_ARR_ENABLE_SORT

#undef PRE
#undef DYN_ARR_ENABLE_SORT
#undef TYPE
// don't undef: user generated
// DYN_ARR_TYPE
// DYN_ARR_PREFIX
