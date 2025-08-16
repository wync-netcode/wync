#ifndef WYNC_STATE_SEND
#define WYNC_STATE_SEND

#include "wync/wync_state_store.h"
#include "wync/wync_throttle.h"
#include "wync/wync_track.h"
#include "wync_typedef.h"
#include <stdlib.h>


/// @param[out] out_snap Prop snapshop
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
	out_snap->data = state;
	return OK;
}


/// This services modifies ctx.client_has_relative_prop_has_last_tick
///
void WyncSend_extracted_data(WyncCtx *ctx) {

	u32 data_used = 0;

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

		WyncSnap_DynArrIterator it = { 0 };
		while (WyncSnap_DynArr_iterator_get_next(reliable, &it) == OK) {
			WyncPacket_free(it.item);
		}
		it = (WyncSnap_DynArrIterator){ 0 };
		while (WyncSnap_DynArr_iterator_get_next(reliable, &it) == OK) {
			WyncPacket_free(it.item);
		}

		WyncSnap_DynArr_clear_preserving_capacity(reliable);
		WyncSnap_DynArr_clear_preserving_capacity(unreliable);
	}

	Wync_PeerEntityPair_DynArr *queue =
		&ctx->co_throttling.queue_entity_pairs_to_sync;

	//u32 pairs_amount = u32_FIFORing_get_size
	//Wync_PeerEntityPair *pair = NULL;

	Wync_PeerEntityPair_DynArrIterator it = { 0 };
	while (Wync_PeerEntityPair_DynArr_iterator_get_next(queue, &it) == OK)
	{
		u16 client_id = it.item->peer_id;
		u32 entity_id = it.item->entity_id;

		WyncThrottle__remove_entity_from_sync_queue(
			ctx, client_id, entity_id);

		// fill all the data for the props, the see if it fits

		WyncSnap_DynArr *unreliable =
			&ctx->co_throttling.clients_cached_unreliable_snapshots[client_id];

		u32_DynArr *entity_prop_ids = NULL;
		i32 err = u32_DynArr_ConMap_get(
			&ctx->co_track.entity_has_props, entity_id, entity_prop_ids);
		assert(err == OK);

		u32_DynArrIterator prop_it = { 0 };
		while (u32_DynArr_iterator_get_next(entity_prop_ids, &prop_it) == OK)
		{
			u32 prop_id = *prop_it.item;
			WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
			if (prop == NULL) continue;

			if (prop->prop_type == WYNC_PROP_TYPE_INPUT) continue;

			if (prop->prop_type == WYNC_PROP_TYPE_STATE) {

				// relative syncable Props:
				// ========================================

				if (prop->relative_sync_enabled) {
					if (prop->timewarp_enabled) {
						// NOT supported: relative_syncable + timewarpable
						continue;
					}

					err = -1;
					//err = WyncSend__wync_sync_queue_prop_fullsnap(
						//ctx, prop_id, client_id);
					if (err != OK) {
						LOG_WAR_C(ctx, "Couldn't sync prop %u", prop_id);
						continue;
					}

					data_used += 0; // ??? TODO: Get size
					continue;
				}

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
				data_used += snap_prop.data.data_size + sizeof(u32) * 2;
				continue;
			}

			// event Props:
			// ========================================

			if (!ConMap_has_key(
				&ctx->co_clientauth.client_owns_prop[client_id], prop_id)) {
				continue;
			}

			// TODO: ...

		}

		if (data_used >= ctx->common.out_packets_size_remaining_chars) {
			break;
		}
	}
}


void WyncSend_queue_out_snapshots_for_delivery (WyncCtx *ctx) {

	// TOOD: use shared buffer
	static NeteBuffer buffer = { 0 };
	if (buffer.size_bytes == 0) {
		buffer.size_bytes = 4096;
		buffer.data = calloc(1, buffer.size_bytes);
	}

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);

	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {

		WyncSnap_DynArr *reliable =
			&ctx->co_throttling.clients_cached_reliable_snapshots[client_id];
		WyncSnap_DynArr *unreliable =
			&ctx->co_throttling.clients_cached_unreliable_snapshots[client_id];

		i32 err;
		WyncPktSnap pkt_rel_snap = { 0 };
		WyncPktSnap pkt_unrel_snap = { 0 };
		pkt_rel_snap.tick = ctx->common.ticks;
		pkt_unrel_snap.tick = ctx->common.ticks;

		pkt_rel_snap.snap_amount = (u16)WyncSnap_DynArr_get_size(reliable);
		pkt_unrel_snap.snap_amount = (u16)WyncSnap_DynArr_get_size(unreliable);

		// NOTE: The following segments could be another function

		// reliable

		if (pkt_rel_snap.snap_amount > 0) {

			pkt_rel_snap.snaps =
				calloc(sizeof(WyncSnap), pkt_rel_snap.snap_amount);

			WyncSnap_DynArrIterator it = { 0 };
			while(WyncSnap_DynArr_iterator_get_next(reliable, &it) == OK)
			{
				pkt_rel_snap.snaps[it.index] = *it.item;
			}

			buffer.cursor_byte = 0;
			if (!WyncPktSnap_serialize(false, &buffer, &pkt_rel_snap)) {
				LOG_ERR_C(ctx, "Couldn't serialize packet");
				goto WyncSend_queue_reliable_defer;
			}

			// queue

			WyncPacketOut packet_out = { 0 };
			err = WyncPacket_wrap_packet_out_alloc(
				ctx, client_id,
				WYNC_PKT_PROP_SNAP,
				buffer.cursor_byte,
				buffer.data,
				&packet_out);
			if (err == OK) {
				err = WyncPacket_try_to_queue_out_packet(
					ctx,
					packet_out,
					RELIABLE, true, false
				);
				if (err != OK) {
					LOG_ERR_C(ctx, "Couldn't queue packet");
				}
			} else {
				LOG_ERR_C(ctx, "Couldn't wrap packet");
			}
			WyncPacketOut_free(&packet_out);

			WyncSend_queue_reliable_defer:
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

			buffer.cursor_byte = 0;
			if (!WyncPktSnap_serialize(false, &buffer, &pkt_unrel_snap)) {
				LOG_ERR_C(ctx, "Couldn't serialize packet");
				goto WyncSend_queue_unreliable_defer;
			}

			// queue

			WyncPacketOut packet_out = { 0 };
			err = WyncPacket_wrap_packet_out_alloc(
				ctx, client_id,
				WYNC_PKT_PROP_SNAP,
				buffer.cursor_byte,
				buffer.data,
				&packet_out);
			if (err == OK) {
				err = WyncPacket_try_to_queue_out_packet(
					ctx,
					packet_out,
					UNRELIABLE, true, false
				);
				if (err != OK) {
					LOG_ERR_C(ctx, "Couldn't queue packet");
				}
			} else {
				LOG_ERR_C(ctx, "Couldn't wrap packet");
			}
			WyncPacketOut_free(&packet_out);

			WyncSend_queue_unreliable_defer:
			WyncPktSnap_free(&pkt_unrel_snap);
		}
	}
}


/// This system writes state
//static func wync_get_event_data_packet (ctx: WyncCtx, peer_id: int, event_ids: Array[int]) -> WyncPktEventData:


void WyncSend_system_update_delta_base_state_tick(WyncCtx *ctx) {

	// move base_state_tick forward

	u32 new_base_tick =
		ctx->common.ticks - ctx->common.max_prop_relative_sync_history_ticks +1;

	if (!(ctx->co_events.delta_base_state_tick < new_base_tick)){
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
	static NeteBuffer buffer = { 0 };
	if (buffer.size_bytes == 0) {
		buffer.size_bytes = 4096;
		buffer.data = calloc(1, buffer.size_bytes);
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
			// TODO .....
			//
			//if (input_prop->prop_type == WYNC_PROP_TYPE_EVENT) {
				//input = input as Array
				//for event_id: int in input:
					//event_set[event_id] = true
		}

		// dump collection into packet

		WyncPktInputs_free(&pkt_inputs);
		pkt_inputs = (WyncPktInputs) { 0 };
		pkt_inputs.prop_id = prop_id;
		pkt_inputs.amount = (u32)WyncTickDecorator_DynArr_get_size(&input_list);
		pkt_inputs.inputs = calloc(sizeof(WyncTickDecorator), pkt_inputs.amount);

		while(WyncTickDecorator_DynArr_iterator_get_next(
			&input_list, &input_it) == OK)
		{
			WyncTickDecorator tick_input = *input_it.item;
			pkt_inputs.inputs[input_it.index] = tick_input;
		}

		// wrap and queue

		buffer.cursor_byte = 0;
		i32 err = WyncPktInputs_serialize(false, &buffer, &pkt_inputs);
		if (err != OK) {
			LOG_ERR_C(ctx, "Couldn't queue packet");
			continue;
		}

		WyncPacketOut packet_out = { 0 };
		err = WyncPacket_wrap_packet_out_alloc(
			ctx,
			SERVER_PEER_ID,
			WYNC_PKT_INPUTS,
			buffer.cursor_byte,
			buffer.data,
			&packet_out);
		if (err == OK) {
			err = WyncPacket_try_to_queue_out_packet(
				ctx,
				packet_out,
				RELIABLE, true, false
			);
			if (err != OK) {
				LOG_ERR_C(ctx, "Couldn't queue packet");
			}
		} else {
			LOG_ERR_C(ctx, "Couldn't wrap packet");
		}

		WyncPacketOut_free(&packet_out);
	}
}

#endif // !WYNC_STATE_SEND
