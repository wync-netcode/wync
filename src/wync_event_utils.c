#ifndef WYNC_EVENT_UTILS_H
#define WYNC_EVENT_UTILS_H

#include "wync_typedef.h"
#include "assert.h"
#include "math.h"
#include "lib/rapidhash/rapidhash.h"
#include "lib/log.h"

// ==================================================
// Public API
// ==================================================


// Events utilities
// ================================================================
// TODO: write tests for this

/// @returns New event id
u32 WyncEventUtils_get_new_event_id(WyncCtx *ctx) {
	// get my peer id (1 byte, 255 max)
	u32 peer_id = (u32)ctx->common.my_peer_id;
	
	/*
	# alternative approach:
	# 32 24 16 08
	# X  X  X  Y
	# X = event_id, Y = peer_id
	var event_id = ctx.event_id_counter << 8
	event_id += peer_id & (2**8-1)
	*/
	
	// generate an event id ending in my peer id
	ctx->co_events.event_id_counter += 1;
	u32 event_id = peer_id << 24;
	event_id += ctx->co_events.event_id_counter & ((u32)pow(2,24) -1);
	
	// reset event_id_counter every 2^24 (3 bytes)
	if (ctx->co_events.event_id_counter >= ((u32)pow(2,24) -1)) {
		ctx->co_events.event_id_counter = 0;
	}
		
	// return the generated id
	assert (event_id > 0);
	return event_id;
}

/// @returns Event id
u32 WyncEventUtils_instantiate_new_event (
	WyncCtx *ctx,
	u32 event_user_type_id
) {
	//if (!ctx->common.connected) return NULL;

	u32 event_id = WyncEventUtils_get_new_event_id(ctx);
	WyncEvent event = { 0 };
	event.data.event_type_id = event_user_type_id;
	event.data.event_data = NULL;

	WyncEvent_ConMap_set_pair(&ctx->co_events.events,
		event_id, event);
	return event_id;
}

/// @returns error
/// @retval 0 OK
/// @retval -1 error
i32 WyncEventUtils_event_set_data(
	WyncCtx *ctx,
	u32 event_id,
	u32 data_size,
	void *data
) {
	WyncEvent* event = NULL;
	WyncEvent_ConMap_get(&ctx->co_events.events, event_id, &event);
	if (event == NULL) return -1;

	event->data.data_size = data_size;
	event->data.event_data = malloc(data_size);
	memcpy(event->data.event_data, data, data_size);
	return OK;
}

/// @param[out] out_event_id Final event_id
/// @returns error
/// @retval  0 OK
/// @retval -1 error
i32 WyncEventUtils_event_wrap_up(
	WyncCtx *ctx,
	u32 event_id,
	u32* out_event_id
) {
	WyncEvent* event = NULL;
	WyncEvent_ConMap_get(&ctx->co_events.events, event_id, &event);
	if (event == NULL) return -1;

	// Note: Maybe hash the type too
	event->data_hash = rapidhash(event->data.event_data, event->data.data_size);

	// this event is a duplicate
	if (u32_FIFOMap_has_item_hash
		(&ctx->co_events.events_hash_to_id, event->data_hash))
	{
		WyncEvent_ConMap_remove_by_key(&ctx->co_events.events, event_id);
		u32* cached_event_id = u32_FIFOMap_get_item_by_hash
			(&ctx->co_events.events_hash_to_id, (u32)event->data_hash);

		assert(cached_event_id != NULL);
		LOG_OUT_C(ctx, "This event is a duplicate");

		*out_event_id = *cached_event_id;
		return OK;
	}

	// not a duplicate -> cache it
	u32_FIFOMap_push_head_hash_and_item
		(&ctx->co_events.events_hash_to_id, event->data_hash, event_id);
	
	*out_event_id = event_id;
	return OK;
}


/// @param[out] out_event_id
/// @returns error
i32 WyncEventUtils_new_event_wrap_up (
	WyncCtx *ctx,
	u16 event_user_type_id,
	u32 data_size,
	void *event_data,
	u32 *out_event_id
) {
	u32 event_id = WyncEventUtils_instantiate_new_event(ctx, event_user_type_id);
	i32 err = WyncEventUtils_event_set_data(ctx, event_id, data_size, event_data);
	if (err != OK) { /* cleanup? */ }
	err = WyncEventUtils_event_wrap_up(ctx, event_id, out_event_id);
	if (err != OK) { return -1; }
	return OK;
}


/// @returns error
i32 WyncEventUtils_publish_global_event_as_client (
	WyncCtx *ctx,
	u16 channel,
	u32 event_id
) {
	if (channel >= MAX_CHANNELS) {
		return -5;
	}

	// TODO: Improve safety
	u32_DynArr* events = &ctx->co_events.peer_has_channel_has_events
		[ctx->common.my_peer_id][channel];
	u32_DynArr_insert(events, event_id);
	return OK;
}

i32 WyncEventUtils_publish_global_event_as_server (
	WyncCtx *ctx,
	u16 channel,
	u32 event_id
) {
	if (channel >= MAX_CHANNELS) {
		return -5;
	}

	// TODO: Improve safety
	u32_DynArr* events = &ctx->co_events.peer_has_channel_has_events
		[SERVER_PEER_ID][channel];
	u32_DynArr_insert(events, event_id);
	return OK;
}


// ==================================================================
// Sending events

void WyncEventUtils_wync_send_event_data (WyncCtx *ctx) {
	if (!ctx->common.connected) { return; }

	if (ctx->common.is_client) {
		//wync_system_send_events_to_peer(ctx, SERVER_PEER_ID);
	} else { // server
		size_t peers_size = i32_DynArr_get_size(&ctx->common.peers);
		for (u16 wync_peer_id = 0; wync_peer_id < peers_size; ++wync_peer_id) {
			//wync_system_send_events_to_peer(ctx, wync_peer_id);
		}
	}
}


// TODO: ....



#endif // !WYNC_EVENT_UTILS_H
