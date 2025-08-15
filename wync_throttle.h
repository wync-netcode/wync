// Functions related to Throttlng and client relative syncronization

#ifndef WYNC_THROTTLE_H
#define WYNC_THROTTLE_H

#include "wync/lib/log.h"
#include "wync/wync_track.h"
#include "wync/wync_typedef.h"
#include <assert.h>

// TODO: rename

void WyncThrottle__wync_remove_entity_from_sync_queue (
	WyncCtx *ctx,
	u16 peer_id,
	u32 entity_id
) {
	ConMap *synced_last_time = &ctx->co_throttling.entities_synced_last_time[peer_id];
	ConMap_set_pair(synced_last_time, entity_id, true);

	u32_FIFORing *entity_queue = &ctx->co_throttling.queue_clients_entities_to_sync[peer_id];
	u32 saved_entity_id = u32_FIFORing_pop_tail(entity_queue);

	// FIXME
	assert(saved_entity_id == entity_id);
}

/// Just appends entity_id's into the queue
///
void WyncThrottle_wync_system_fill_entity_sync_queue (WyncCtx *ctx) {

	i32_DynArrIterator it = { 0 };
	while(i32_DynArr_iterator_get_next(&ctx->common.peers, &it) == OK) {

		if (it.index == 0) continue; // skip server
		i32 client_id = *it.item;

		bool everything_fitted = true;
		u32_FIFORing *entity_queue =
			&ctx->co_throttling.queue_clients_entities_to_sync[client_id];
		ConMap *synced_last_time =
			&ctx->co_throttling.entities_synced_last_time[client_id];
		ConMap *sees_entities =
			&ctx->co_throttling.clients_sees_entities[client_id];

		ConMapIterator map_it = { 0 };
		u32 entity_id;
		i32 error;

		while (ConMap_iterator_get_next_key(sees_entities, &map_it) != OK)
		{
			entity_id = map_it.key;

			// * Note. A check to only sync on value change shouln't be here.
			// Instead, check individual props not the whole entity.
			// * Note. No need to check if entity is tracked. On entity removal
			// the removed entity_id will be removed from all queues / lists / etc.

			if (!ConMap_has_key(synced_last_time, entity_id)
				&& !u32_FIFORing_has_item(entity_queue, entity_id)
			) {
				error = u32_FIFORing_push_head(entity_queue, entity_id);
				if (error != OK) {
					everything_fitted = false;
					break;
				}
			}
		}

		if (!everything_fitted) continue;

		ConMap_clear_preserve_capacity(synced_last_time);

		// give it a second pass

		map_it = (ConMapIterator) { 0 };

		while (ConMap_iterator_get_next_key(sees_entities, &map_it) != OK)
		{
			entity_id = map_it.key;

			if (!u32_FIFORing_has_item(entity_queue, entity_id))
			{
				error = u32_FIFORing_push_head(entity_queue, entity_id);
				if (error != OK) break;
			}
		}
	}
}

/// Queues pairs of client and entity to sync
///
void WyncThrottle_compute_entity_sync_order(WyncCtx *ctx) {

	// clear
	Wync_PeerEntityPair_DynArr_clear_preserving_capacity(
		&ctx->co_throttling.queue_entity_pairs_to_sync);

	// populate / compute

	u32 entity_index = 0;
	bool ran_out_of_entites = false;

	while (!ran_out_of_entites) {
		ran_out_of_entites = true;

		// from each client we get the Nth item in queue (entity_index'th)

		i32_DynArrIterator it = { 0 };
		i32 client_id;
		while(i32_DynArr_iterator_get_next(&ctx->common.peers, &it) == OK) {

			if (it.index == 0) continue; // skip server
			client_id = *it.item;

			u32_FIFORing *entity_queue =
				&ctx->co_throttling.queue_clients_entities_to_sync[client_id];

			u32 entity_queue_size = u32_FIFORing_get_size(entity_queue);
			
			// has it?
			if (entity_index >= entity_queue_size) continue;

			u32 entity_id_key = 
				*u32_FIFORing_get_relative_to_tail(entity_queue, entity_index);
			//if (entity_id_key == -1) continue; // can't remember why this check

			Wync_PeerEntityPair pair = { 0 };
			pair.peer_id = client_id;
			pair.entity_id = entity_id_key;
			
			Wync_PeerEntityPair_DynArr_insert(
				&ctx->co_throttling.queue_entity_pairs_to_sync, pair);

		}

		++entity_index;
	}
}


// Public
// ----------------------------------------------------------------------


/// @returns error
i32 WyncThrottle_client_now_can_see_entity(
	WyncCtx *ctx,
	u16 client_id,
	u32 entity_id
){
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)){
		LOG_ERR_C(ctx, "entity (%u) isn't tracked", entity_id);
		return -1;
	}

	// already viewed
	if (ConMap_has_key(
		&ctx->co_throttling.clients_sees_entities[client_id], entity_id)) {
		return OK;
	}

	ConMap_set_pair(&ctx->co_throttling.clients_sees_new_entities[client_id],
		entity_id, true);
	return OK;
}

void WyncThrottle_everyone_now_can_see_entity(WyncCtx *ctx, u32 entity_id) {
	i32_DynArrIterator it = { .index = 1 };
	while(i32_DynArr_iterator_get_next(&ctx->common.peers, &it) == OK) {
		u16 peer_id = (u16)*it.item;

		WyncThrottle_client_now_can_see_entity(ctx, peer_id, entity_id);
	}
}

void WyncThrottle_entity_set_spawn_data(
	WyncCtx *ctx,
	u32 entity_id,
	WyncState spawn_data
) {
	assert(!WyncState_ConMap_has_key(
		&ctx->co_spawn.entity_spawn_data, entity_id));
	WyncState_ConMap_set_pair(
		&ctx->co_spawn.entity_spawn_data, entity_id, spawn_data);
}

/// @returns error
i32 WyncThrottle_client_no_longer_sees_entity(
	WyncCtx *ctx,
	u16 client_id,
	u32 entity_id
) {
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)){
		LOG_ERR_C(ctx, "entity (%u) isn't tracked", entity_id);
		return -1;
	}

	ConMap_set_pair(&ctx->co_throttling.clients_no_longer_sees_entities[client_id],
		entity_id, true);
	return OK;

}

#endif // !WYNC_THROTTLE_H
