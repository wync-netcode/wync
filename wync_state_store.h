#ifndef WYNC_STATE_STORE_H
#define WYNC_STATE_STORE_H

#include "wync/wync_debug.h"
#include "wync/wync_join.h"
#include "wync/wync_track.h"
#include "wync_typedef.h"


void WyncStore_prop_state_buffer_insert(
	WyncCtx *ctx,
	WyncProp *prop,
	u32 tick,
	WyncState state
);

void WyncStore_prop_state_buffer_insert_in_place(
	WyncCtx *ctx,
	WyncProp *prop,
	u32 tick,
	WyncState state
);

i32 WyncStore_save_confirmed_state(
	WyncCtx *ctx,
	u32 prop_id,
	u32 tick,
	WyncState state
);

void WyncStore_client_update_last_tick_received(WyncCtx *ctx, u32 tick) {
	ctx->co_pred.last_tick_received = MAX(ctx->co_pred.last_tick_received, tick);
	ctx->co_pred.last_tick_received_at_tick = ctx->common.ticks;
}

void WyncStore_handle_pkt_prop_snap(
	WyncCtx *ctx,
	WyncPktSnap pkt
) {

	WyncSnap *snap;

	for (u32 i = 0; i < pkt.snap_amount; ++i) {
		snap = &pkt.snaps[i];

		WyncDebug_received_log_prop_id(ctx, WYNC_PKT_PROP_SNAP, snap->prop_id);

		WyncProp *prop = WyncTrack_get_prop(ctx, snap->prop_id);
		if (prop == NULL) {
			LOG_WAR_C(ctx, "couldn't find prop (%hu) saving as dummy prop...",
				snap->prop_id);
			WyncTrack_prop_register_update_dummy(
				ctx,
				snap->prop_id,
				pkt.tick,
				snap->data.data_size,
				snap->data.data
			);
			continue;
		}

		// avoid flooding the buffer with old late state
		
		i32 last_tick_received =
			*i32_RinBuf_get_relative(&prop->statebff.last_ticks_received, 0);

		if ( !(pkt.tick > (last_tick_received -
			ctx->co_track.REGULAR_PROP_CACHED_STATE_AMOUNT))
		) {
			continue;
		}

		WyncState state_copy = WyncState_copy_from_buffer(
				snap->data.data_size, snap->data.data);

		i32 err = WyncStore_save_confirmed_state(
			ctx, snap->prop_id, pkt.tick, state_copy);

		if (err != OK) {
			WyncState_free(&state_copy);
			continue;
		}

		// update entity last received
		// NOTE: assuming snap props always include all snaps for an entity
		//if WyncXtrap.prop_is_predicted(ctx, snap_prop.prop_id):
		// ..........
		// ..........
	}

	WyncStore_client_update_last_tick_received(ctx, pkt.tick);
}

/// Transfer ownership of the data
///
/// @param state Must be Pre-allocated
/// @returns error
i32 WyncStore_save_confirmed_state(
	WyncCtx *ctx,
	u32 prop_id,
	u32 tick,
	WyncState state
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) {
		LOG_WAR_C(ctx, "couldn't find prop (%hu)", prop_id);
		return -1;
	}

	prop->statebff.just_received_new_state = true;

	i32_RinBuf_push(&prop->statebff.last_ticks_received, tick, NULL, NULL);
	i32_RinBuf_sort(&prop->statebff.last_ticks_received);
	i32_RinBuf_insert_at(
		&prop->statebff.state_id_to_local_tick,
		prop->statebff.saved_states.head_pointer,
		ctx->common.ticks
	);

	WyncStore_prop_state_buffer_insert(ctx, prop, tick, state);

	if (prop->relative_sync_enabled) {
		// FIXME: check for max? what about unordered packets?
		ConMap *delta_props_last_tick =
			&ctx->co_track.client_has_relative_prop_has_last_tick
				[ctx->common.my_peer_id];
		ConMap_set_pair(delta_props_last_tick, prop_id, tick);
	}

	// TODO: update prob prop update rate
	//if prop_id == ctx.co_metrics.PROP_ID_PROB:
		//WyncStats.wync_try_to_update_prob_prop_rate(ctx)

	return OK;
}

void WyncStore_service_cleanup_dummy_props(WyncCtx *ctx) {
	// run every few frames
	if (FAST_MODULUS(ctx->common.ticks, 16) != 0) return;

	u32 curr_tick = ctx->co_ticks.server_ticks;
	Wync_DummyProp *dummy;

	DummyProp_ConMapIterator it = { 0 };
	while(DummyProp_ConMap_iterator_get_next_key(
		&ctx->co_dummy.dummy_props, &it) == OK)
	{
		dummy = NULL;
		u32 prop_id = it.key;

		i32 err = DummyProp_ConMap_get(&ctx->co_dummy.dummy_props, it.key, &dummy);
		assert (err == OK && dummy != NULL);

		if ((curr_tick - dummy->last_tick) < MAX_DUMMY_PROP_TICKS_ALIVE) {
			continue;
		}

		// delete dummy prop

		DummyProp_ConMap_remove_by_key(&ctx->co_dummy.dummy_props, prop_id);
		++ctx->co_dummy.stat_lost_dummy_props;
		Wync_DummyProp_free(dummy);
	}
}

/// Creates and stores it's own copy of the data, user must free 'pkt' data
///
/// @returns error
i32 WyncStore_server_handle_pkt_inputs(
	WyncCtx *ctx,
	WyncPktInputs pkt,
	u16 from_nete_peer_id
) {
	u16 client_id;
	u32 prop_id = pkt.prop_id;

	WyncDebug_received_log_prop_id(ctx, WYNC_PKT_INPUTS, prop_id);

	// client and prop exists

	if (WyncJoin_is_peer_registered(ctx, from_nete_peer_id, &client_id) != OK){
		LOG_ERR_C(ctx, "client %hu is not registered", from_nete_peer_id);
		return -1;
	}

	WyncProp *prop_input = WyncTrack_get_prop(ctx, prop_id);
	if (prop_input == NULL) {
		LOG_ERR_C(ctx, "prop %u not found", prop_id);
		return -2;
	}

	// check client has ownership over this prop

	ConMap *owns_props = &ctx->co_clientauth.client_owns_prop[client_id];
	if (!ConMap_has_key(owns_props, prop_id)) {
		LOG_ERR_C(ctx, "user %hu doesn't own prop %u", client_id, prop_id);
		return -3;
	}

	// save the input in the prop before simulation

	WyncTickDecorator *input;
	for (u32 i = 0; i < pkt.amount; ++i) {
		input = &pkt.inputs[i];

		if (input->state.data_size == 0 || input->state.data == NULL) {
			continue;
		}

		WyncState state_copy =
			WyncState_copy_from_buffer(input->state.data_size, input->state.data);

		WyncStore_prop_state_buffer_insert_in_place(
			ctx, prop_input, input->tick, state_copy);

		// TODO: Reject input that is too old
	}

	return OK;
}


/// Creates and stores it's own copy of the data, user must free 'pkt' data
///
/// @returns error
i32 WyncStore_client_handle_pkt_inputs(
	WyncCtx *ctx,
	WyncPktInputs pkt
) {
	u32 prop_id = pkt.prop_id;

	WyncDebug_received_log_prop_id(ctx, WYNC_PKT_INPUTS, prop_id);

	WyncProp *prop_input = WyncTrack_get_prop(ctx, prop_id);
	if (prop_input == NULL) {
		LOG_WAR_C(ctx, "couldnt find prop %u, dropping input...", prop_id);
		return -1;
	}

	// save the input in the prop before simulation

	u32 max_tick = 0;

	WyncTickDecorator *input;
	for (u32 i = 0; i < pkt.amount; ++i) {
		input = &pkt.inputs[i];

		if (input->state.data_size == 0 || input->state.data == NULL) {
			continue;
		}

		WyncState state_copy =
			WyncState_copy_from_buffer(input->state.data_size, input->state.data);

		WyncStore_prop_state_buffer_insert(
			ctx, prop_input, input->tick, state_copy);
			
		max_tick = MAX(max_tick, input->tick);
	}

	i32_RinBuf_sort(&prop_input->statebff.last_ticks_received);

	WyncStore_client_update_last_tick_received(ctx, max_tick);

	// Update entity last received
	//if WyncXtrap.prop_is_predicted(ctx, data.prop_id): 
	// ......

	return OK;
}


/// Transfers ownership of the data pointers
void WyncStore_prop_state_buffer_insert(
	WyncCtx *ctx,
	WyncProp *prop,
	u32 tick,
	WyncState state
){
	WyncState replaced_state = { 0 };
	size_t state_idx;

	if (state.data_size == 0 || state.data == NULL) {
		LOG_WAR_C(ctx, "Tried to buffer empty state");
		return;
	}

	//
	i32 err = WyncState_RinBuf_push(
			&prop->statebff.saved_states, state,
			&state_idx, &replaced_state);

	if (err != OK) { return; }

	// free whatever old state is found if any
	WyncState_free(&replaced_state);

	i32_RinBuf_insert_at(&prop->statebff.state_id_to_tick, state_idx, tick);
	i32_RinBuf_insert_at(&prop->statebff.tick_to_state_id, tick, (u32)state_idx);
}

/// Transfers ownership of the data pointers
void WyncStore_prop_state_buffer_insert_in_place(
	WyncCtx *ctx,
	WyncProp *prop,
	u32 tick,
	WyncState state
){
	u32 state_id = *i32_RinBuf_get_at(&prop->statebff.tick_to_state_id, tick);
	u32 stored_tick = *i32_RinBuf_get_absolute(&prop->statebff.state_id_to_tick, state_id);
	if (tick != stored_tick) {
		WyncStore_prop_state_buffer_insert(ctx, prop, tick, state);
	}

	i32_RinBuf_insert_at(&prop->statebff.state_id_to_tick, state_id, tick);
	i32_RinBuf_insert_at(&prop->statebff.tick_to_state_id, tick, state_id);
}

/// Transfers ownership of the data pointers
i32 WyncStore_insert_state_to_entity_prop (
	WyncCtx *ctx,
	u32 entity_id,
	const char *prop_name_id,
	u32 tick,
	WyncState state
) {
	WyncProp *prop = WyncTrack_entity_get_prop(ctx, entity_id, prop_name_id);
	if (prop == NULL) {
		LOG_WAR_C(ctx, "couldnt find prop %s for entity %u",
			prop_name_id, entity_id);
		return -1;
	}

	// NOTE: Code below mirrors of 'save_confirmed_state'

	prop->statebff.just_received_new_state = true;

	i32_RinBuf_push(&prop->statebff.last_ticks_received, tick, NULL, NULL);
	i32_RinBuf_sort(&prop->statebff.last_ticks_received);

	WyncStore_prop_state_buffer_insert(ctx, prop, tick, state);

	i32_RinBuf_insert_at(
		&prop->statebff.state_id_to_local_tick,
		prop->statebff.saved_states.head_pointer,
		ctx->common.ticks
	);

	return OK;
}

/// Main method to access stored state
///
/// @returns shared state, copy if needed
WyncState WyncState_prop_state_buffer_get(WyncProp *prop, u32 tick) {
	i32 state_id = *i32_RinBuf_get_at(&prop->statebff.tick_to_state_id, tick);
	if (state_id == -1) {
		return (WyncState) { 0 };
	}

	WyncState stored = *WyncState_RinBuf_get_absolute(
		&prop->statebff.saved_states, state_id);
	return stored;
}

/// TODO: Explain why this cannot be covered by function above ^^^
///
/// @returns shared state, copy if needed
WyncState WyncState_prop_state_buffer_get_throughout (
	WyncProp *prop,
	u32 tick
) {
	// look up tick

	size_t size = WyncState_RinBuf_get_size(&prop->statebff.saved_states);
	for (size_t state_id = 0; state_id < size; ++state_id) {

		u32 saved_tick = *i32_RinBuf_get_absolute(
			&prop->statebff.state_id_to_tick, state_id);

		if (saved_tick == tick) {
			WyncState stored = *WyncState_RinBuf_get_absolute(
				&prop->statebff.saved_states, state_id);
			return stored;
		}
	}

	return (WyncState) { 0 };
}


#endif // !WYNC_STATE_STORE_H
