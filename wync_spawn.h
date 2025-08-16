#ifndef WYNC_SPAWN_H
#define WYNC_SPAWN_H

#include "wync/containers/map.h"
#include "wync/wync_packet_util.h"
#include "wync/wync_track.h"
#include "wync/wync_state_store.h"
#include "wync_typedef.h"


void _wync_confirm_client_can_see_entity(
	WyncCtx *ctx, u16 client_id, u32 entity_id);

/// Copies new data, User must free pkt later
/// 
void WyncSpawn_handle_pkt_spawn(WyncCtx *ctx, WyncPktSpawn pkt) {

	for (u32 i = 0; i < pkt.entity_amount; ++i) {
		u32 entity_id = pkt.entity_ids[i];
		u16 entity_type_id = pkt.entity_type_ids[i];
		u32 prop_start = pkt.entity_prop_id_start[i];
		u32 prop_end = pkt.entity_prop_id_start[i];
		WyncState spawn_data = pkt.entity_spawn_data[i];

		spawn_data = WyncState_copy_from_buffer(
			spawn_data.data_size, spawn_data.data);

		// "flag" it
		EntitySpawnPropRange_ConMap_set_pair(
			&ctx->co_spawn.pending_entity_to_spawn_props,
			entity_id,
			(EntitySpawnPropRange) {
				.prop_start = prop_start,
				.prop_end = prop_end,
				.curr = 0
			}
		);

		// queue it to user face variable
		Wync_EntitySpawnEvent spawn_event = { 
			.spawn = true,
			.already_spawned = false,
			.entity_id = entity_id,
			.entity_type_id = entity_type_id,
			.spawn_data = spawn_data
		};

		SpawnEvent_FIFORing_push_head(
			&ctx->co_spawn.out_queue_spawn_events, spawn_event);
	}
}

void WyncSpawn_handle_pkt_despawn(WyncCtx *ctx, WyncPktDespawn pkt) {

	for (u32 i = 0; i < pkt.entity_amount; ++i) {
		u32 entity_id = pkt.entity_ids[i];

		// TODO: untrack only if it exists already
		// NOTE: There might be a bug where we untrack an entity
		// that needed to be respawned

		WyncTrack_untrack_entity(ctx, entity_id);

		// remove from spawn list if found

		if (EntitySpawnPropRange_ConMap_has_key(
			&ctx->co_spawn.pending_entity_to_spawn_props, entity_id
		)) {

			// TODO: Extract as function

			assert(OK == EntitySpawnPropRange_ConMap_remove_by_key(
			&ctx->co_spawn.pending_entity_to_spawn_props, entity_id));

			SpawnEvent_FIFORing *queue = &ctx->co_spawn.out_queue_spawn_events;
			u32 size = (u32)SpawnEvent_FIFORing_get_size(queue);
			
			for (u32 k = 0; k < size; ++k) {

				Wync_EntitySpawnEvent *spawn_event = 
					SpawnEvent_FIFORing_get_relative_to_tail(queue, k);

				if (spawn_event->entity_id == entity_id
					&& spawn_event->spawn
				){
					assert(SpawnEvent_FIFORing_remove_relative_to_tail
						(queue, k) == OK);
				}
			}

		} else {
			Wync_EntitySpawnEvent spawn_event = {
				.spawn = false,
				.entity_id = entity_id
			};
			SpawnEvent_FIFORing_push_head(
				&ctx->co_spawn.out_queue_spawn_events, spawn_event);
		}
	}
}

/// @param[out] out_spawn_event Pointer to an existing intance
/// @returns error
i32 WyncSpawn_get_next_entity_event_spawn (
	WyncCtx *ctx,
	Wync_EntitySpawnEvent *out_spawn_event
) {
	u32 size = SpawnEvent_FIFORing_get_size(
		&ctx->co_spawn.out_queue_spawn_events);
	if (size == 0) {
		return -1;
	}

	Wync_EntitySpawnEvent *spawn_event = SpawnEvent_FIFORing_pop_tail(
		&ctx->co_spawn.out_queue_spawn_events);

	if (spawn_event != NULL) {
		*out_spawn_event = *spawn_event;
	}

	//ctx->co_spawn.next_entity_to_spawn = spawn_event;

	return OK;
}


/// @returns error
void WyncSpawn_finish_spawning_entity(WyncCtx *ctx, u32 entity_id) {

	// remove from 'spawn prop range'

	assert(EntitySpawnPropRange_ConMap_has_key(
		&ctx->co_spawn.pending_entity_to_spawn_props, entity_id) == OK);

	EntitySpawnPropRange prop_range;
	EntitySpawnPropRange_ConMap_get(
		&ctx->co_spawn.pending_entity_to_spawn_props,
		entity_id,
		&prop_range);

	assert((prop_range.prop_end - prop_range.prop_start) == (prop_range.curr -1));

	EntitySpawnPropRange_ConMap_remove_by_key(
		&ctx->co_spawn.pending_entity_to_spawn_props, entity_id);

	LOG_OUT_C(ctx, "spawn, spawned entity %u", entity_id);

	// apply dummy props if any

	u32_DynArr *entity_props = NULL;
	i32 error = u32_DynArr_ConMap_get(
		&ctx->co_track.entity_has_props, entity_id, entity_props);
	assert(error == OK);

	u32_DynArrIterator it = { 0 };
	while(u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		if (!DummyProp_ConMap_has_key(&ctx->co_dummy.dummy_props, prop_id)) {
			continue;
		}

		Wync_DummyProp dummy;
		error =
			DummyProp_ConMap_get(&ctx->co_dummy.dummy_props, prop_id, &dummy);
		assert (error == OK);

		WyncStore_save_confirmed_state(
			ctx, prop_id, dummy.last_tick, dummy.data);

		// clean up
		DummyProp_ConMap_remove_by_key(&ctx->co_dummy.dummy_props, prop_id);
	}
}

// Call after finishing spawning entities
void WyncSpawn_system_spawned_props_cleanup (WyncCtx *ctx) {
	SpawnEvent_FIFORing_clear(&ctx->co_spawn.out_queue_spawn_events);
}


/// This system is throttled
/// Note: as it is the first clients have priority until the out buffer fills
void WyncSpawn_system_send_entities_to_spawn(WyncCtx *ctx) {
	static ConMap ids_to_spawn = { 0 };
	if (ids_to_spawn.size == 0) {
		ConMap_init(&ids_to_spawn);
	}

	u32 data_used = 0;
	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	WyncPktSpawn packet = { 0 };

	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {
		ConMap *new_entities =
			&ctx->co_throttling.clients_sees_new_entities[client_id];
		ConMap *current_entities =
			&ctx->co_throttling.clients_sees_entities[client_id];

		// compile ids to sync

		ConMapIterator it = { 0 };
		while(ConMap_iterator_get_next_key(new_entities, &it) == OK) {
			u32 entity_id = it.key;

			// already in current_entities
			if (ConMap_has_key(current_entities, entity_id)) {
				continue;
			}

			ConMap_set_pair(&ids_to_spawn, entity_id, true);
		}

		// allocate

		u16 entity_amount = (u16)ConMap_get_key_count(&ids_to_spawn);
		if (entity_amount == 0) {
			continue;
		}

		WyncPktSpawn_free(&packet);
		WyncPktSpawn_calloc(&packet, entity_amount);

		i32 i = -1;
		i32 error;
		u32 entity_id;
		u16 entity_type_id;

		// add each new entity

		it = (ConMapIterator){ 0 };
		while(ConMap_iterator_get_next_key(&ids_to_spawn, &it) == OK){

			++i;
			entity_id = it.key;

			error = ConMap_get(
				&ctx->co_track.entity_is_of_type, entity_id, (i32*)&entity_type_id);
			assert (error == OK);

			u32_DynArr prop_ids = { 0 };
			u32_DynArr_ConMap_get(
				&ctx->co_track.entity_has_props, entity_id, &prop_ids);
			u32 prop_amount = (u32)u32_DynArr_get_size(&prop_ids);
			assert (prop_amount > 0);

			packet.entity_ids[i] = entity_id;
			packet.entity_type_ids[i] = entity_type_id;
			packet.entity_prop_id_start[i] = *u32_DynArr_get(&prop_ids, 0);
			packet.entity_prop_id_end[i] = *u32_DynArr_get(&prop_ids, prop_amount -1);

			// Clone spawn data

			if (WyncState_ConMap_has_key(
					&ctx->co_spawn.entity_spawn_data, entity_id)
			){
				WyncState spawn_data = { 0 };

				WyncState_ConMap_get(
					&ctx->co_spawn.entity_spawn_data, entity_id, &spawn_data);

				spawn_data = WyncState_copy_from_buffer(
					spawn_data.data_size, spawn_data.data);

				packet.entity_spawn_data[i] = spawn_data;
			}

			// commit: confirm as _client can see it_

			_wync_confirm_client_can_see_entity(ctx, client_id, entity_id);
		}

		// serialize packet

		u16 truncated_entity_amount = entity_amount;
		if ((i + 1) != entity_amount) {
			truncated_entity_amount = (u16)i + 1;
		}

		// TODO: use shared buffer with configurable packet size
		static NeteBuffer buffer = { 0 };
		if (buffer.size_bytes == 0) {
			buffer.size_bytes = 4096; 
			buffer.data = calloc(1, buffer.size_bytes);
		}
		buffer.cursor_byte = 0;

		if (!WyncPktSpawn_serialize(
				false, &buffer, &packet, truncated_entity_amount)) {
			LOG_ERR_C(ctx, "Couldn't serialize WyncPktSpawn");
			continue;
		}

		// wrap and queue

		WyncPacketOut packet_out = { 0 };
		error = WyncPacket_wrap_packet_out_alloc(
			ctx,
			client_id,
			WYNC_PKT_SPAWN,
			buffer.cursor_byte,
			buffer.data,
			&packet_out);
		if (error == OK) {
			error = WyncPacket_try_to_queue_out_packet(
				ctx,
				packet_out,
				RELIABLE, true, false
			);
			if (error != OK) {
				LOG_ERR_C(ctx, "Couldn't wrap packet");
			}
		} else {
			LOG_ERR_C(ctx, "Couldn't wrap packet");
			continue;
		}
		WyncPacketOut_free(&packet_out);

		// data limit

		data_used += buffer.cursor_byte;
		if (data_used >= ctx->common.out_packets_size_remaining_chars) {
			break;
		}
	}

	WyncPktSpawn_free(&packet);
}


/// This system is not throttled
void WyncSpawn_system_send_entities_to_despawn(WyncCtx *ctx) {

	static u32_DynArr entity_id_list = { 0 };
	static NeteBuffer buffer = { 0 };

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);

	// TODO: use shared buffer with configurable packet size
	if (buffer.size_bytes == 0) {
		buffer.size_bytes = 4096; 
		buffer.data = calloc(1, buffer.size_bytes);
	}

	if (entity_id_list.capacity == 0) {
		entity_id_list = u32_DynArr_create();
	}
	u32_DynArr_clear_preserving_capacity(&entity_id_list);

	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {

		ConMap *current_entities =
			&ctx->co_throttling.clients_sees_entities[client_id];
		u32 entity_amount = 0;
		u32_DynArrIterator it = { 0 };
		WyncPktDespawn packet_despawn = { 0 };
		WyncPacketOut packet_out = { 0 };
		i32 error;

		while (u32_DynArr_iterator_get_next(
			&ctx->co_spawn.despawned_entity_ids, &it) == OK)
		{
			u32 entity_id = *it.item;

			if (!ConMap_has_key(current_entities, entity_id)) continue;

			u32_DynArr_insert(&entity_id_list, entity_id);
			++entity_amount;

			// ATTENTION: Removing entity here
			ConMap_remove_by_key(current_entities, entity_id);

			LOG_OUT_C(ctx,
				"I: spawn, confirmed: client %hu no longer sees entity %u",
				client_id, entity_id);
		}

		if (entity_amount == 0) continue;

		// build packet

		buffer.cursor_byte = 0;

		WyncPktDespawn_allocate(&packet_despawn, entity_amount);

		for (u32 i = 0; i < entity_amount; ++i) {
			packet_despawn.entity_ids[i] = *u32_DynArr_get(&entity_id_list, i);
		}

		error = WyncPktDespawn_serialize(false, &buffer, &packet_despawn);
		if (error != true) {
			LOG_ERR_C(ctx, "Couldn't serialize packet");
			goto WyncSpawn_send_entities_to_despawn__defer;
		}

		// queue

		error = WyncPacket_wrap_packet_out_alloc(
			ctx,
			client_id,
			WYNC_PKT_DESPAWN,
			buffer.cursor_byte,
			buffer.data,
			&packet_out);
		if (error == OK) {
			error = WyncPacket_try_to_queue_out_packet(
				ctx,
				packet_out,
				RELIABLE, true, false
			);
			if (error != OK) {
				LOG_ERR_C(ctx, "Couldn't queue packet");
			}
		} else {
			LOG_ERR_C(ctx, "Couldn't wrap packet");
		}

		// free

		WyncSpawn_send_entities_to_despawn__defer:

		WyncPacketOut_free(&packet_out);
		WyncPktDespawn_free(&packet_despawn);
	}
}

void _wync_confirm_client_can_see_entity(
	WyncCtx *ctx,
	u16 client_id,
	u32 entity_id
) {
	ConMap *sees_entities = &ctx->co_throttling.clients_sees_entities[client_id];
	ConMap_set_pair(sees_entities, entity_id, true);

	u32_DynArr *entity_props = NULL;
	u32_DynArr_ConMap_get(
		&ctx->co_track.entity_has_props, entity_id, entity_props);
	u32_DynArrIterator it = { 0 };

	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) {
			LOG_ERR_C(ctx, "Couldn't find prop(%u) in entity(%u)",
				prop_id, entity_id);
			continue;
		}

		if (prop->relative_sync_enabled) {
			ConMap *delta_prop_last_tick = 
				&ctx->co_track.client_has_relative_prop_has_last_tick[client_id];
			ConMap_set_pair(delta_prop_last_tick, prop_id, -1);
		}
	}

	// remove from new entities

	ConMap *sees_new_entities =
		&ctx->co_throttling.clients_sees_new_entities[client_id];
	ConMap_remove_by_key(sees_new_entities, entity_id);

	LOG_OUT_C(ctx, "spawn, confirmed: client %hu can now see entity %u",
		client_id, entity_id);
}

#endif // !WYNC_SPAWN_H
