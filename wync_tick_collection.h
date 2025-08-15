#ifndef WYNC_TICK_COLLECTION
#define WYNC_TICK_COLLECTION

#include "wync_typedef.h"


void WyncOffsetCollection_replace_value(CoTicks *co_ticks, i32 find_value, i32 new_value) {
	for (u32 i = 0; i < SERVER_TICK_OFFSET_COLLECTION_SIZE; ++i) {
		Wync_i32Pair *pair = &co_ticks->server_tick_offset_collection[i];
		if (pair->a == find_value) {
			pair->a = new_value;
			pair->b = 1;
			return;
		}
	}
}

void WyncOffsetCollection_increase_value(CoTicks *co_ticks, i32 find_value) {
	for (u32 i = 0; i < SERVER_TICK_OFFSET_COLLECTION_SIZE; ++i) {
		Wync_i32Pair *pair = &co_ticks->server_tick_offset_collection[i];
		if (pair->a == find_value) {
			++pair->b;
			return;
		}
	}
}

bool WyncOffsetCollection_value_exists(CoTicks *co_ticks, i32 ar_value) {
	for (u32 i = 0; i < SERVER_TICK_OFFSET_COLLECTION_SIZE; ++i) {
		Wync_i32Pair *pair = &co_ticks->server_tick_offset_collection[i];
		if (pair->a == ar_value) return true;
	}
	return false;
}

i32 WyncOffsetCollection_get_most_common(CoTicks *co_ticks) {
	i32 highest_count = 0;
	i32 current_value = 0;
	for (u32 i = 0; i < SERVER_TICK_OFFSET_COLLECTION_SIZE; ++i) {
		Wync_i32Pair *pair = &co_ticks->server_tick_offset_collection[i];
		if (pair->b > highest_count) {
			highest_count = pair->b;
			current_value = pair->a;
		}
	}
	return current_value;
}

i32 WyncOffsetCollection_get_less_common(CoTicks *co_ticks) {
	i32 lowest_count = -1;
	i32 current_value = 0;
	for (u32 i = 0; i < SERVER_TICK_OFFSET_COLLECTION_SIZE; ++i) {
		Wync_i32Pair *pair = &co_ticks->server_tick_offset_collection[i];
		if (pair->b < lowest_count || lowest_count == -1) {
			lowest_count = pair->b;
			current_value = pair->a;
		}
	}
	return current_value;
}

void WyncOffsetCollection_add_value(CoTicks *co_ticks, i32 new_value) {
	if (WyncOffsetCollection_value_exists(co_ticks, new_value)){
		WyncOffsetCollection_increase_value(co_ticks, new_value);
	} else {
		i32 less_common_value = WyncOffsetCollection_get_less_common(co_ticks);
		WyncOffsetCollection_replace_value(co_ticks, less_common_value, new_value);
	}
}

#endif // !WYNC_TICK_COLLECTION
