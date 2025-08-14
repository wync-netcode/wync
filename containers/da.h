// Usage example:
// #define DYN_ARR_TYPE uint
// #undef  DYN_ARR_PREFIX            (optional)
// #define DYN_ARR_PREFIX CustomName (optional)
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


PRE(DynArr) PRE(DynArr_create) (void) {
    PRE(DynArr) da = { 0 };
    da.capacity = 2;
    da.items = (TYPE *)malloc(sizeof(TYPE) * da.capacity);
    return da;
}


/// @param   item  Item passed as value
/// @returns index
size_t PRE(DynArr_insert) (PRE(DynArr) *da, TYPE item) {
    if (da->size >= da->capacity) {
        da->capacity *= 2;
        da->items = (TYPE *)realloc(da->items, sizeof(TYPE) * da->capacity);
    }

    da->items[da->size] = item;
    ++da->size;
    return da->size -1;
}

/// @returns error
int PRE(DynArr_insert_at) (PRE(DynArr) *da, size_t index, TYPE item) {
    if (index >= da->size) {
        return -1;
    }

    da->items[index] = item;
    return OK;
}


/// @returns TYPE
TYPE *PRE(DynArr_get) (PRE(DynArr) *da, size_t index) {
    return &da->items[index];
}


size_t PRE(DynArr_get_size) (PRE(DynArr) *da) {
    return da->size;
}


/// Removing while iterating is Undefined Behaviour
/// Removing will alter the order
/// @retval  0 OK
/// @retval -1 Not found
int PRE(DynArr_remove_at) (PRE(DynArr) *da, size_t index) {
    if (index < 0 || index >= da->size){
        return -1;
    }

    da->items[index] = da->items[da->size -1];
    --da->size;
    return OK;
}


/// Clears the array but preserves the allocated memory
void PRE(DynArr_clear_preserving_capacity) (PRE(DynArr) *da) {
    da->size = 0;
}


typedef struct {
    size_t  __next_index;
    size_t  index;
    TYPE   *item;
} PRE(DynArrIterator);


PRE(DynArrIterator) PRE(DynArr_make_iterator) (void) {
    PRE(DynArrIterator) it = { 0 };
    return it;
}


/// @param[out] it iterator
/// @retval  0 OK
/// @retval -1 End reached
int PRE(DynArr_iterator_get_next) (PRE(DynArr) *da, PRE(DynArrIterator) *it)
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

#undef PRE
//#undef TYPE
//#undef DYN_ARR_PREFIX
