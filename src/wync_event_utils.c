#ifndef WYNC_EVENT_UTILS_H
#define WYNC_EVENT_UTILS_H

#include "src/wync_private.h"
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

	WyncEvent_ConMap_set_pair(&ctx->co_events.events,
		event_id, event);
	return event_id;
}

/// Copies provided data
///
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

	WyncState state = WyncState_copy_from_buffer(data_size, data);
	event->data.data.data = state.data;
	event->data.data.data_size = state.data_size;
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
	event->data_hash = (u32)rapidhash(event->data.data.data, event->data.data.data_size);

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
	if (err != OK) { /* TODO: cleanup */ }
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

/// @returns error
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


/// This system writes state. It should run just after
/// queue_delta_event_data_to_be_synced_to_peers. All queued events must be sent
/// , they're already commited, so no throttling here
///
/// @returns int: 0 -> OK, 1 -> OK, but couldn't queue all packets, >1 -> Error
static i32 WyncEventUtils_system_send_events_to_peer (
	WyncCtx *ctx,
	u16 wync_peer_id
) {
	ConMap *events = &ctx->co_throttling.peers_events_to_sync[wync_peer_id];
	u32 event_amount = ConMap_get_key_count(events);
	if (event_amount <= 0) {
		return OK;
	}

	i32 error;
	u32 appended_events_count = 0;
	WyncPktEventData data = { 0 };

	data.event_amount = event_amount;
	data.events = calloc(sizeof(WyncPktEventData_EventData), event_amount);

	ConMapIterator it = { 0 };
	while (ConMap_iterator_get_next_key(events, &it) == OK)
	{
		// get event

		u32 event_id = it.key;
		WyncEvent *event = NULL;

		error = WyncEvent_ConMap_get(&ctx->co_events.events, event_id, &event);
		if (error != OK) {
			LOG_ERR_C(ctx, "couldn't find event_id %u", event_id);
			continue;
		}

		// check if peer already has it
		// NOTE: is_serve_cached could be skipped? all events should be cached
		//       on our side...

		u32 *cached_event_id = u32_FIFOMap_get_item_by_hash(
			&ctx->co_events.events_hash_to_id, event->data_hash);

		if (cached_event_id != NULL) {
			bool peer_has_it = u32_FIFOMap_has_item_hash(
				&ctx->co_events.to_peers_i_sent_events[wync_peer_id],
				*cached_event_id);
			if (peer_has_it) continue;
			// see ctx.max_amount_cache_events
		}

		// package it

		WyncPktEventData_EventData event_data = { 0 };
		event_data.event_id = event_id;
		event_data.event_type_id = event->data.event_type_id;
		event_data.data = WyncState_copy_from_buffer(
				event->data.data.data_size, event->data.data.data);

		data.events[appended_events_count] = event_data;
		++appended_events_count;

		// confirm commit (these events are all already commited)
		// since peer doesn't have it, the mark it as sent
		u32_FIFOMap_push_head_hash_and_item(
			&ctx->co_events.to_peers_i_sent_events[wync_peer_id],
			event_id, true);
	}

	if (appended_events_count <= 0) {
		free(data.events);
		return OK;
	}

	WyncPacket_wrap_and_queue(
		ctx,
		WYNC_PKT_EVENT_DATA,
		&data,
		wync_peer_id,
		RELIABLE,
		true
	);

	WyncPktEventData_free(&data);
	
	return OK;
}

void WyncEventUtils_wync_send_event_data (WyncCtx *ctx) {
	/*if (!ctx->common.connected) { return; }*/

	if (ctx->common.is_client) {
		WyncEventUtils_system_send_events_to_peer(ctx, SERVER_PEER_ID);
	} else { // server
		size_t peers_size = i32_DynArr_get_size(&ctx->common.peers);
		for (u16 wync_peer_id = 0; wync_peer_id < peers_size; ++wync_peer_id) {
			WyncEventUtils_system_send_events_to_peer(ctx, wync_peer_id);
		}
	}
}

/// @returns error
i32 WyncEventUtils_handle_pkt_event_data (WyncCtx *ctx, WyncPktEventData data) {
	// TODO: as the server, only receive event data from a client if they own a
	// prop with it. There might not be a very performant way of doing that
	for (u32 i = 0; i < data.event_amount; ++i) {
		WyncEvent event = { 0 };
		event.data.event_type_id = data.events[i].event_type_id;

		WyncState event_data = WyncState_copy_from_buffer(
				data.events[i].data.data_size, 
				data.events[i].data.data);
		event.data.data.data = event_data.data;
		event.data.data.data_size = event_data.data_size;

		WyncEvent_ConMap_set_pair(
			&ctx->co_events.events, 
			data.events[i].event_id, event);

		// NOTE: what if we already have this event data? Maybe it's better to
		// receive it anyway?
	}

	return OK;
}

/// @returns optional<u32_DynArr>
u32_DynArr *WyncEventUtils_get_events_from_event_prop(
	WyncProp *prop, i32 tick
) {
	if (prop->prop_type != WYNC_PROP_TYPE_EVENT) {
		assert(false);
		return NULL;
	}
	WyncState state = WyncState_prop_state_buffer_get(prop, tick);
	if (state.data_size == 0 || state.data == NULL) {
		assert(false);
		return NULL;
	}
	u32_DynArr *event_list = (u32_DynArr*)state.data;

	return event_list;
}

/// @param[out] out_event_list (instance) for returning an event list
/// @returns error
i32 WyncEventUtils_get_events_from_channel_from_peer(
	WyncCtx *ctx,
	u16 wync_peer_id,
	u16 channel,
	u32 tick,
	WyncEventList *out_event_list
) {
	// Q: Is it possible that event_ids accumulate infinitely if they are never
	// consumed? A: No, if events are never consumed they remain in the
	// confirmed_states buffer until eventually replaced.

	static u32_DynArr event_ids = { 0 };
	if (event_ids.capacity == 0) {
		event_ids = u32_DynArr_create();
	}
	u32_DynArr_clear_preserving_capacity(&event_ids);

	u32 prop_id = ctx->co_events.prop_id_by_peer_by_channel[wync_peer_id][channel];
	WyncProp *prop_channel = WyncTrack_get_prop_unsafe(ctx, prop_id);

	i32 consumed_event_ids_tick = *i32_RinBuf_get_at(
		&prop_channel->co_consumed.events_consumed_at_tick_tick, tick);

	if (consumed_event_ids_tick != tick) {
		return -1;
	}

	u32_DynArr *consumed_event_ids = NULL;
	consumed_event_ids = u32_DynArr_RinBuf_get_at(
		&prop_channel->co_consumed.events_consumed_at_tick, tick);

	u32_DynArr *confirmed_event_ids = NULL;
	if (tick != ctx->common.ticks) {
		confirmed_event_ids =
			&ctx->co_events.peer_has_channel_has_events[wync_peer_id][channel];
	} else {
		confirmed_event_ids = WyncEventUtils_get_events_from_event_prop(
				prop_channel, tick);
		if (confirmed_event_ids == NULL) {
			return -1;
		}
	}

	for (size_t i = 0; i < u32_DynArr_get_size(confirmed_event_ids); ++i) {
		u32 event_id = *u32_DynArr_get(confirmed_event_ids, i);
		if (u32_DynArr_has(consumed_event_ids, event_id)) {
			continue;
		}
		if (!WyncEvent_ConMap_has_key(&ctx->co_events.events, event_id)) {
			continue;
		}

		u32_DynArr_insert(&event_ids, event_id);
	}

	*out_event_list = (WyncEventList) {
		.event_amount = event_ids.size,
		.event_ids = event_ids.items
	};
	return OK;
}


// I give you a data BLOB, you give me a data BLOB, simple.
// Optimization Idea: Pass the preallocated space for the user to fill, to avoid
// constant allocations
void WyncEventUtil_event_setter(
	WyncWrapper_UserCtx user_ctx,
	WyncWrapper_Data data
) {
	if (user_ctx.type_size != sizeof(WyncEventUtil_EventCtx))
	{ return; }

	WyncEventUtil_EventCtx event_ctx = *(WyncEventUtil_EventCtx *)user_ctx.ctx;
	u32_DynArr *events = event_ctx.list;
	u32_DynArr_clear_preserving_capacity(events);

	NeteBuffer buffer = { 0 };
	buffer.data = data.data;
	buffer.size_bytes = data.data_size;
	buffer.cursor_byte = 0;

	WyncEventList event_list = { 0 };
	if (!WyncEventList_serialize(true, &buffer, &event_list)) {
		assert(false);
	}

	for (u32 i = 0; i < event_list.event_amount; ++i) {
		u32_DynArr_insert(events, event_list.event_ids[i]);
	}
}

WyncWrapper_Data WyncEventUtil_event_get_zeroed (void) {

	WyncEventList event_list = { 0 };
	WyncWrapper_Data data;
	data.data_size = WyncEventList_get_size(&event_list);
	data.data = malloc(data.data_size);

	NeteBuffer buffer = { 0 };
	buffer.data = data.data;
	buffer.size_bytes = data.data_size;
	buffer.cursor_byte = 0;

	if (!WyncEventList_serialize(false, &buffer, &event_list)) {
		assert(false);
	}
	assert(buffer.cursor_byte == data.data_size);
	free(event_list.event_ids);

	return data;
}

WyncWrapper_Data WyncEventUtil_event_getter (
	WyncWrapper_UserCtx user_ctx
) {
	if (user_ctx.type_size != sizeof(WyncEventUtil_EventCtx))
		{ return (WyncWrapper_Data) { 0 }; }

	WyncEventUtil_EventCtx event_ctx = *(WyncEventUtil_EventCtx *)user_ctx.ctx;
	u32_DynArr *events = event_ctx.list;

	WyncEventList event_list = { 0 };
	event_list.event_amount = (u32)u32_DynArr_get_size(events);
	event_list.event_ids = malloc(sizeof(u32) * event_list.event_amount);

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(events, &it) == OK) {
		event_list.event_ids[it.index] = *it.item;
	}

	WyncWrapper_Data data;
	data.data_size = WyncEventList_get_size(&event_list);
	data.data = malloc(data.data_size);

	NeteBuffer buffer = { 0 };
	buffer.data = data.data;
	buffer.size_bytes = data.data_size;
	buffer.cursor_byte = 0;

	if (!WyncEventList_serialize(false, &buffer, &event_list)) {
		assert(false);
	}
	assert(buffer.cursor_byte == data.data_size);
	free(event_list.event_ids);

	return data;
}

// NOTE: Maybe it's better to initialize all client channels from the start

i32 WyncEventUtils_setup_peer_global_events (WyncCtx *ctx, u32 peer_id) {

	u32 entity_id = ENTITY_ID_GLOBAL_EVENTS + peer_id;
	u32 channel_id = 0;
	char prop_name[40];
	sprintf(prop_name, "channel_%d", channel_id);

	WyncTrack_track_entity(ctx, entity_id, -1);
	
	u32 channel_prop_id;
	i32 error = WyncTrack_prop_register_minimal(
		ctx,
		entity_id,
		prop_name,
		WYNC_PROP_TYPE_EVENT,
		&channel_prop_id
	);
	if (error != OK) { return -1; }

	WyncEventUtil_EventCtx *event_ctx = calloc(1, sizeof(WyncEventUtil_EventCtx));
	event_ctx->list = &ctx->co_events.peer_has_channel_has_events[
		peer_id][channel_id];

	WyncWrapper_set_prop_callbacks(
		ctx,
		channel_prop_id,
		(WyncWrapper_UserCtx) {
			.ctx = event_ctx,
			.type_size = sizeof(WyncEventUtil_EventCtx)
		},
		WyncEventUtil_event_getter,
		WyncEventUtil_event_setter
	);

	// populate ctx var
	ctx->co_events.prop_id_by_peer_by_channel[peer_id][channel_id] = channel_prop_id;

	if (!ctx->common.is_client) {
		// add as local existing prop
		WyncTrack_wync_add_local_existing_entity(ctx, peer_id, entity_id);

		// server module for consuming user events... Q: should this be server only?
		WyncProp_enable_module_events_consumed(ctx, channel_prop_id);
	}

	return OK;
}

#endif // !WYNC_EVENT_UTILS_H
