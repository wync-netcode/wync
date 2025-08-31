#include "wync_private.h"
#include <stdlib.h>
#include "assert.h"


/// @param[out] out_snap Fills it with a copy (new alloc'd)
/// @returns error
i32 WyncSend__wync_sync_regular_prop(
	WyncProp *prop,
	u32 prop_id,
	u32 tick,
	WyncSnap *out_snap
) {
	WyncState state = WyncState_prop_state_buffer_get(prop, tick);
	if (state.data_size == 0 || state.data == NULL) {
		return -1;
	}

	out_snap->prop_id = prop_id;
	out_snap->data = WyncState_copy_from_buffer(state.data_size, state.data);
	return OK;
}

/// @returns error
i32 WyncSend__wync_sync_queue_rela_prop_fullsnap(
	WyncCtx *ctx,
	u32 prop_id,
	uint client_id
) {

	// FIXME: Optimization ideas:
	// 1. (probably not) Adelantar ticks en los que no pasó nada. Es decir
	// automaticamente aumentar el número del tick de un peer
	// 'last_tick_confirmed'. Esto trae problemas por el determinismo, pues no
	// se enviaría ticks intermedios, es decir, el cliente debe saber. send
	// fullsnapshot if client doesn't have history, or if it's too old.
	// 2. Podriamos evitar enviar actualizaciones si se detecta que el cliente
	// está desconectado temporalmente (1-3 mins); Wync actualmente no sabe
	// cuando un peer está sufriendo desconexión temporal; se podría crear un
	// mecanismo para esto o usar _last_tick_.

	Wync_PeerLatencyInfo *lat_info = &ctx->common.peer_latency_info[client_id];
	float latency_ticks = ((float)lat_info->latency_stable_ms) /
		(1000.0f / ctx->common.physic_ticks_per_second);

	ConMap *client_prop_last_tick =
		&ctx->co_track.client_has_relative_prop_has_last_tick[client_id];

	int last_tick = -1;
	ConMap_get(client_prop_last_tick, prop_id, &last_tick);

	if ((float)(ctx->co_events.delta_base_state_tick - last_tick)
			< (latency_ticks * 4)
	){
		return -1;
	}


	// queue to sync later (+ queue for state extraction)

	Wync_PeerPropPair peer_prop_pair = {
		.peer_id = client_id,
		.prop_id = prop_id
	};

	Wync_PeerPropPair_DynArr_insert(
		&ctx->co_throttling.pending_rela_props_to_sync_to_peer, peer_prop_pair);
	u32_DynArr_insert(
		&ctx->co_throttling.rela_prop_ids_for_full_snapshot, prop_id);

	return OK;
}


void WyncSend_send_pending_rela_props_fullsnapshot (WyncCtx *ctx) {
	Wync_PeerPropPair_DynArrIterator it = { 0 };

	while (Wync_PeerPropPair_DynArr_iterator_get_next(
		&ctx->co_throttling.pending_rela_props_to_sync_to_peer, &it) == OK)
	{
		uint prop_id = it.item->prop_id;
		uint peer_id = it.item->peer_id;
		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		WyncSnap snap_prop = { 0 };
		int error = WyncSend__wync_sync_regular_prop(
				prop, prop_id, ctx->common.ticks, &snap_prop);
		if (error != OK) {
			LOG_WAR_C(ctx, "Couldn't extract state for prop_id %u", prop_id);
			continue;
		}

		WyncSnap_DynArr *reliable_snaps = 
			&ctx->co_throttling.clients_cached_reliable_snapshots[peer_id];
		ConMap *rela_prop_last_tick =
			&ctx->co_track.client_has_relative_prop_has_last_tick[peer_id];

		WyncSnap_DynArr_insert(reliable_snaps, snap_prop);
		ConMap_set_pair(rela_prop_last_tick, prop_id, (int)ctx->common.ticks);

		// Note: data consumed was already calculated before when queueing
	}	
}



// TODO: Make a separate version only for _event_ids_ different from _inputs any_
// TODO: Make a separate version only for _delta event_ids_

/// @param[out] out_pkt_inputs Instace, returns filled packet (with new alloc's)
void WyncSend_prop_event_send_event_ids_to_peer(
	WyncCtx *ctx,
	WyncProp *prop,
	uint prop_id,
	WyncPktInputs *out_pkt_inputs
) {
	static NeteBuffer buffer = { 0 };
	static WyncTickDecorator_DynArr input_list = { 0 };
	if (input_list.capacity == 0) {
		input_list = WyncTickDecorator_DynArr_create();
	}
	static WyncPktInputs pkt_inputs = { 0 };

	// collect inputs

	WyncTickDecorator_DynArr_clear_preserving_capacity(&input_list);
	
	for (uint tick = ctx->common.ticks - INPUT_AMOUNT_TO_SEND;
		tick < ctx->common.ticks +1; ++tick)
	{
		WyncState input = WyncState_prop_state_buffer_get(prop, tick);
		if (input.data == NULL || input.data_size == 0) {
			LOG_ERR_C(ctx, "deltasync, Can't extract event data from prop %u", prop_id);
			continue;
		}

		// check is valid

		buffer.data = input.data;
		buffer.size_bytes = input.data_size;
		buffer.cursor_byte = 0;

		WyncEventList event_list = { 0 };
		if (!WyncEventList_serialize(true, &buffer, &event_list)) {
			WyncEventList_free(&event_list);
			LOG_ERR_C(ctx, "deltasync, couldn't read WyncEventList");
			continue;
		}

		// collect events

		WyncTickDecorator tick_input_wrap = { 0 };
		tick_input_wrap.tick = tick;
		tick_input_wrap.state = WyncState_copy_from_buffer(
			input.data_size, input.data);

		WyncTickDecorator_DynArr_insert(&input_list, tick_input_wrap);
	}

	// dump collection into packet

	WyncPktInputs_free(&pkt_inputs);
	pkt_inputs = (WyncPktInputs) { 0 };
	pkt_inputs.prop_id = prop_id;
	pkt_inputs.amount = (u32)WyncTickDecorator_DynArr_get_size(&input_list);
	pkt_inputs.inputs = calloc(sizeof(WyncTickDecorator), pkt_inputs.amount);

	WyncTickDecorator_DynArrIterator input_it = { 0 };
	while(WyncTickDecorator_DynArr_iterator_get_next(
		&input_list, &input_it) == OK)
	{
		WyncTickDecorator tick_input = *input_it.item;
		pkt_inputs.inputs[input_it.index] = tick_input;
	}

	*out_pkt_inputs = pkt_inputs;
}


// This system writes state
WyncPktEventData WyncSend_get_event_data_packet (
	WyncCtx *ctx,
	uint peer_id,
	WyncPktInputs *pkt_input
) {
	static NeteBuffer buffer = { 0 };
	WyncPktEventData pkt_data = { 0 };
	pkt_data.event_amount = pkt_input->amount;
	pkt_data.events = calloc(sizeof(uint), pkt_data.event_amount);

	uint actual_event_count = 0;

	for (uint i = 0; i < pkt_input->amount; ++i) {
		WyncTickDecorator input = pkt_input->inputs[i];
		
		buffer.data = input.state.data;
		buffer.size_bytes = input.state.data_size;
		buffer.cursor_byte = 0;

		WyncEventList event_list = { 0 };
		if (!WyncEventList_serialize(true, &buffer, &event_list)) {
			WyncEventList_free(&event_list);
			LOG_ERR_C(ctx, "deltasync, couldn't read WyncEventList");
			continue;
		}

		for (uint k = 0; k < event_list.event_amount; ++k) {
			uint event_id = event_list.event_ids[k];

			WyncEvent *event;
			int error = WyncEvent_ConMap_get(
				&ctx->co_events.events, event_id, &event);
			if (error != OK) {
				LOG_ERR_C(ctx, "couldn't find event_id %u", event_id);
				continue;
			}
			
			// check if peer already has it (avoid resending)

			uint *cached_event_id = u32_FIFOMap_get_item_by_hash(
				&ctx->co_events.events_hash_to_id, event->data_hash);
			if (cached_event_id != NULL) {
				bool peer_has_it = u32_FIFOMap_has_item_hash(
					&ctx->co_events.to_peers_i_sent_events[peer_id], *cached_event_id);
				if (peer_has_it) {
					continue;
				}
			}

			// package it

			WyncPktEventData_EventData event_data = { 0 };
			event_data.event_id = event_id;
			event_data.event_type_id = event->data.event_type_id;
			event_data.data = WyncState_copy_from_buffer(
				event->data.data.data_size,
				event->data.data.data
			);

			pkt_data.events[actual_event_count] = event_data;
			++actual_event_count;

			// since peer doesn't have it, then mark it as sent
			u32_FIFOMap_push_head_hash_and_item(
				&ctx->co_events.to_peers_i_sent_events[peer_id], event_id, true);
		}
	}

	pkt_data.event_amount = actual_event_count;
	return pkt_data;
}


/// Builds packets
/// This services modifies ctx.client_has_relative_prop_has_last_tick
void WyncSend_extracted_data(WyncCtx *ctx) {

	u32_DynArr_clear_preserving_capacity(
		&ctx->co_throttling.rela_prop_ids_for_full_snapshot);
	Wync_PeerPropPair_DynArr_clear_preserving_capacity(
		&ctx->co_throttling.pending_rela_props_to_sync_to_peer);

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {

		WyncSnap_DynArr *reliable =
			&ctx->co_throttling.clients_cached_reliable_snapshots[client_id];
		WyncSnap_DynArr *unreliable =
			&ctx->co_throttling.clients_cached_unreliable_snapshots[client_id];

		WyncSnap_DynArr_clear_preserving_capacity(reliable);
		WyncSnap_DynArr_clear_preserving_capacity(unreliable);

		// Note: Actual individual packets are freed at
		// WyncSend_queue_out_snapshots_for_delivery
	}

	Wync_PeerEntityPair_DynArr *queue =
		&ctx->co_throttling.queue_entity_pairs_to_sync;

	Wync_PeerEntityPair_DynArrIterator it = { 0 };
	while (Wync_PeerEntityPair_DynArr_iterator_get_next(queue, &it) == OK)
	{
		u16 client_id = it.item->peer_id;
		u32 entity_id = it.item->entity_id;

		WyncThrottle__remove_entity_from_sync_queue(
			ctx, client_id, entity_id);

		// fill all the data for the props, then see if it fits

		WyncSnap_DynArr *unreliable =
			&ctx->co_throttling.clients_cached_unreliable_snapshots[client_id];

		u32_DynArr *entity_prop_ids = NULL;
		i32 err = u32_DynArr_ConMap_get(
			&ctx->co_track.entity_has_props, entity_id, &entity_prop_ids);
		assert(err == OK);

		u32_DynArrIterator prop_it = { 0 };
		while (u32_DynArr_iterator_get_next(entity_prop_ids, &prop_it) == OK)
		{
			u32 prop_id = *prop_it.item;
			WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
			if (prop == NULL) continue;

			if (prop->prop_type == WYNC_PROP_TYPE_INPUT) continue;

			if (prop->prop_type == WYNC_PROP_TYPE_STATE) {

				// ========================================
				// relative syncable Props:
				// ========================================

				if (prop->relative_sync_enabled) {
					if (prop->timewarp_enabled) {
						// NOT supported: relative_syncable + timewarpable
						continue;
					}

					err = WyncSend__wync_sync_queue_rela_prop_fullsnap(
						ctx, prop_id, client_id);
					if (err != OK) {
						LOG_WAR_C(ctx, "Couldn't sync prop %u", prop_id);
						continue;
					}

					continue;
				}

				// ========================================
				// regular declarative Props:
				// ========================================

				WyncSnap snap_prop;
				err = WyncSend__wync_sync_regular_prop (
					prop, prop_id, ctx->common.ticks, &snap_prop);
				if (err != OK) {
					LOG_WAR_C(ctx, "Couldn't sync prop %u", prop_id);
					continue;
				}

				WyncSnap_DynArr_insert(unreliable, snap_prop);
				continue;
			}

			// ========================================
			// event Props:
			// ========================================

			// don't send if client owns this prop

			if (ConMap_has_key(
				&ctx->co_clientauth.client_owns_prop[client_id], prop_id)) {
				continue;
			}

			// this include 'regular' and 'auxiliar' props ???

			WyncPktInputs pkt_input = { 0 };
			WyncSend_prop_event_send_event_ids_to_peer(
					ctx, prop, prop_id, &pkt_input);

			WyncPktEventData pkt_event_data =
				WyncSend_get_event_data_packet(ctx, client_id, &pkt_input);

			WyncPacket_wrap_and_queue(
				ctx,
				WYNC_PKT_INPUTS,
				&pkt_input,
				client_id,
				UNRELIABLE,
				true
			);

			if (pkt_event_data.event_amount == 0) {
				continue;
			}
			WyncPacket_wrap_and_queue(
				ctx,
				WYNC_PKT_EVENT_DATA,
				&pkt_event_data,
				client_id,
				RELIABLE,
				true
			);

		}

		if (ctx->common.out_packets_size_remaining_chars <= 0) {
			break;
		}
	}
}


void WyncSend_queue_out_snapshots_for_delivery (WyncCtx *ctx) {

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);

	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {

		WyncSnap_DynArr *reliable =
			&ctx->co_throttling.clients_cached_reliable_snapshots[client_id];
		WyncSnap_DynArr *unreliable =
			&ctx->co_throttling.clients_cached_unreliable_snapshots[client_id];

		WyncPktSnap pkt_rel_snap = { 0 };
		WyncPktSnap pkt_unrel_snap = { 0 };
		pkt_rel_snap.tick = ctx->common.ticks;
		pkt_unrel_snap.tick = ctx->common.ticks;

		pkt_rel_snap.snap_amount = (u16)WyncSnap_DynArr_get_size(reliable);
		pkt_unrel_snap.snap_amount = (u16)WyncSnap_DynArr_get_size(unreliable);

		// reliable

		if (pkt_rel_snap.snap_amount > 0) {

			pkt_rel_snap.snaps =
				calloc(sizeof(WyncSnap), pkt_rel_snap.snap_amount);

			WyncSnap_DynArrIterator it = { 0 };
			while(WyncSnap_DynArr_iterator_get_next(reliable, &it) == OK)
			{
				pkt_rel_snap.snaps[it.index] = *it.item;
			}

			WyncPacket_wrap_and_queue(
				ctx,
				WYNC_PKT_PROP_SNAP,
				&pkt_rel_snap,
				client_id,
				RELIABLE,
				true
			);

			WyncPktSnap_free(&pkt_rel_snap);
		}

		// unreliable

		if (pkt_unrel_snap.snap_amount > 0) {

			pkt_unrel_snap.snaps =
				calloc(sizeof(WyncSnap), pkt_unrel_snap.snap_amount);

			WyncSnap_DynArrIterator it = { 0 };
			while(WyncSnap_DynArr_iterator_get_next(unreliable, &it) == OK)
			{
				pkt_unrel_snap.snaps[it.index] = *it.item;
			}

			WyncPacket_wrap_and_queue(
				ctx,
				WYNC_PKT_PROP_SNAP,
				&pkt_unrel_snap,
				client_id,
				UNRELIABLE,
				true
			);

			WyncPktSnap_free(&pkt_unrel_snap);
		}

		WyncSnap_DynArr_clear_preserving_capacity(reliable);
		WyncSnap_DynArr_clear_preserving_capacity(unreliable);
	}
}


/// This system writes state
//static func wync_get_event_data_packet (ctx: WyncCtx, peer_id: int, event_ids: Array[int]) -> WyncPktEventData:


void WyncSend_system_update_delta_base_state_tick(WyncCtx *ctx) {

	// move base_state_tick forward

	i32 new_base_tick = (i32)ctx->common.ticks
		- (i32)ctx->common.max_prop_relative_sync_history_ticks +1;

	if (ctx->co_events.delta_base_state_tick >= new_base_tick){
		return;
	}
	ctx->co_events.delta_base_state_tick = new_base_tick;
}


// * Sends inputs/events in chunks
// TODO: either throttle or commit to the packet
// This system writes state (ctx.peers_events_to_sync) but it's naturally
// redundant
// So think about it as if it didn't write state
void WyncSend_client_send_inputs (WyncCtx *ctx) {

	static WyncTickDecorator_DynArr input_list = { 0 };
	if (input_list.capacity == 0) {
		input_list = WyncTickDecorator_DynArr_create();
	}
	static WyncPktInputs pkt_inputs = { 0 };

	ConMap *event_set =
		&ctx->co_throttling.peers_events_to_sync[SERVER_PEER_ID];
	u32_DynArr *owned_props =
		&ctx->co_filter_c.type_input_event__owned_prop_ids;

	WyncProp *input_prop = NULL;
	u32_DynArrIterator it = { 0 };
	WyncTickDecorator_DynArrIterator input_it = { 0 };

	ConMap_clear_preserve_capacity(event_set);

	while(u32_DynArr_iterator_get_next(owned_props, &it) == OK) {

		u32 prop_id = *it.item;
		input_prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		assert(input_prop != NULL);
		WyncTickDecorator_DynArr_clear_preserving_capacity(&input_list);

		// extract stored inputs

		for ( u32 i = ctx->co_pred.target_tick - INPUT_AMOUNT_TO_SEND;
			i < ctx->co_pred.target_tick; ++i
		){
			WyncState input = WyncState_prop_state_buffer_get(input_prop, i);

			if (input.data_size == 0 || input.data == NULL) {
				// TODO: Implement input duplication on frame skip

				LOG_OUT_C(ctx, "prop(%u) don't have input for tick %u", prop_id, i);
				continue;
			}

			WyncTickDecorator tick_input = { .tick = i };
			tick_input.state =
				WyncState_copy_from_buffer(input.data_size, input.data);
			WyncTickDecorator_DynArr_insert(&input_list, tick_input);

			// collect event ids

			if (input_prop->prop_type == WYNC_PROP_TYPE_EVENT) {
				WyncEventList event_list = { 0 };
				NeteBuffer buffer = { 0 };
				buffer.size_bytes = input.data_size;
				buffer.data = input.data;

				if (!WyncEventList_serialize(true, &buffer, &event_list)) {
					LOG_ERR_C(ctx, "Couldn't read PROP_TYPE_EVENT contents");
					continue;
				}

				for (uint k = 0; k < event_list.event_amount; ++k) {
					uint event_id = event_list.event_ids[k];
					ConMap_set_pair(event_set, event_id, true);
				}
			}
		}

		// dump collection into packet

		WyncPktInputs_free(&pkt_inputs);
		pkt_inputs = (WyncPktInputs) { 0 };
		pkt_inputs.prop_id = prop_id;
		pkt_inputs.amount = (u32)WyncTickDecorator_DynArr_get_size(&input_list);
		pkt_inputs.inputs = calloc(sizeof(WyncTickDecorator), pkt_inputs.amount);

		input_it = (WyncTickDecorator_DynArrIterator ) { 0 };

		while(WyncTickDecorator_DynArr_iterator_get_next(
			&input_list, &input_it) == OK)
		{
			WyncTickDecorator tick_input = *input_it.item;
			pkt_inputs.inputs[input_it.index] = tick_input;
		}

		WyncPacket_wrap_and_queue(
			ctx,
			WYNC_PKT_INPUTS,
			&pkt_inputs,
			SERVER_PEER_ID,
			UNRELIABLE,
			true
		);
	}
}
