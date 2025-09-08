#include "wync_private.h"
#include "wync_wrapper.h"
#include "assert.h"


bool WyncState_does_delta_prop_has_undo_events (
	WyncProp *prop, uint prop_id, uint tick)
{
	if (*i32_RinBuf_get_at(
		&prop->co_rela.confirmed_states_undo_tick, tick) != tick) {
		return false;
	}

	u32_DynArr *undo_event_id_list = NULL;
	undo_event_id_list = u32_DynArr_RinBuf_get_at(
		&prop->co_rela.confirmed_states_undo, tick);

	return u32_DynArr_get_size(undo_event_id_list) != 0;
}


/// Used only for resetting inputs in WyncXtrap
/// Note: Could be use for general purpose
void WyncState_reset_all_state_to_confirmed_tick_absolute(
	WyncCtx *ctx,
	u32 *prop_ids,
	u32 prop_id_amount,
	u32 tick
) {
	WyncProp *prop;
	WyncWrapper_Setter *setter;
	WyncWrapper_UserCtx *user_ctx;

	for (u32 i = 0; i < prop_id_amount; ++i) {
		u32 prop_id = prop_ids[i];

		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (prop == NULL) {
			continue;
		}

		WyncState state = WyncState_prop_state_buffer_get(prop, tick);
		if (state.data_size == 0 || state.data == NULL) {
			if (!ctx->common.is_client && prop->prop_type != WYNC_PROP_TYPE_STATE) {
				LOG_WAR_C(ctx, "No input for prop %u (%s) tick %d",
						prop_id, prop->name_id, tick);
			}
			continue;
		}

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		if (*setter == NULL) continue;
		(*setter)(*user_ctx, (WyncWrapper_Data){ state.data_size, state.data });
	}
}

void WyncState_reset_all_state_to_confirmed_tick_relative(
	WyncCtx *ctx,
	u32 *prop_ids,
	u32 prop_id_amount,
	u32 tick
) {
	WyncWrapper_Setter *setter;
	WyncWrapper_UserCtx *user_ctx;

	for (u32 i = 0; i < prop_id_amount; ++i) {
		u32 prop_id = prop_ids[i];

		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (prop == NULL) {
			continue;
		}

		i32 last_confirmed_tick = *i32_RinBuf_get_relative(
			&prop->statebff.last_ticks_received, tick);
		if (last_confirmed_tick == -1) {
			continue;
		}

		WyncState state = WyncState_prop_state_buffer_get(prop, last_confirmed_tick);
		if (state.data_size == 0 || state.data == NULL) {
			continue;
		}

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		if (*setter == NULL) continue;
		(*setter)(*user_ctx, (WyncWrapper_Data){ state.data_size, state.data });

		if (prop->relative_sync_enabled) {
			LOG_OUT_C(ctx, "debugdelta, Setted absolute state for prop(%u) %s",
			prop_id, prop->name_id);
		}
	}
}


void WyncState_predicted_delta_props_rollback_to_canonic_state(
	WyncCtx *ctx, WyncProp *prop, uint prop_id
) {
	ConMap *delta_props_last_tick =
		&ctx->co_track.client_has_relative_prop_has_last_tick[
			ctx->common.my_peer_id];

	int last_uptodate_tick = -1;
	int error = ConMap_get(delta_props_last_tick, prop_id, &last_uptodate_tick);
	if (error != OK || last_uptodate_tick < 0) {
		return;
	}

	// apply events in order

	for (int tick = ctx->co_pred.last_tick_predicted;
		tick < last_uptodate_tick; --tick)
	{
		if (*i32_RinBuf_get_at(
			&prop->co_rela.confirmed_states_undo_tick, tick) != tick)
		{
			// NOTE: for now, if we find nothing, we do nothing. We assume
			// this tick just wasn't predicted. Needs more testing to be sure
			// this is safe to ignore. Otherwise we could just store ticks
			// that were actually predicted.
			return;
		}
		
		u32_DynArr *undo_event_id_list = u32_DynArr_RinBuf_get_at(
			&prop->co_rela.confirmed_states_undo, tick);

		// merge state

		u32_DynArrIterator it = { 0 };
		while (u32_DynArr_iterator_get_next(undo_event_id_list, &it) == OK)
		{
			uint event_id = *it.item;

			error = 
			WyncDelta_merge_event_to_state_real_state (ctx, prop_id, event_id);

			assert(error == OK);
		}

		u32_DynArr_clear_preserving_capacity(undo_event_id_list);
	}
}


void WyncState_delta_props_update_and_apply_delta_events (WyncCtx *ctx)
{
	ConMap *delta_props_last_tick = 
	&ctx->co_track.client_has_relative_prop_has_last_tick[ctx->common.my_peer_id];

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_state__delta_prop_ids, &it) == OK)
	{
		uint prop_id = *it.item;
		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		WyncProp *aux_prop =
			WyncTrack_get_prop(ctx, prop->auxiliar_delta_events_prop_id);

		if (aux_prop == NULL) {
			LOG_ERR_C(ctx, "deltasync, couldn't find aux_prop id(%u)",
					prop->auxiliar_delta_events_prop_id);
			continue;
		}

		int last_uptodate_tick = -1;
		int error = ConMap_get(delta_props_last_tick, prop_id, &last_uptodate_tick);
		if (error != OK || last_uptodate_tick < 0) {
			return;
		}

		bool already_rollbacked = false;
		int applied_events_until = -1;
		NeteBuffer buffer = { 0 };

		for (int tick = last_uptodate_tick +1;
			tick < ctx->co_pred.last_tick_received +1; ++tick) 
		{
			WyncState event_list_data =
					WyncState_prop_state_buffer_get(aux_prop, tick);
			if (event_list_data.data == NULL || event_list_data.data_size == 0){
				break;
			}

			buffer.data = event_list_data.data;
			buffer.size_bytes = event_list_data.data_size;
			buffer.cursor_byte = 0;

			WyncEventList event_list = { 0 };
			if (!WyncEventList_serialize(true, &buffer, &event_list)) {
				LOG_ERR_C(ctx, "Couldn't read event_list prop %u", prop_id);
				break;
			}

			// before applying any events: confirm we have the event data for
			// all events on tick

			bool has_all_event_data = true;

			for (uint k = 0; k < event_list.event_amount; ++k) {
				uint event_id = event_list.event_ids[k];
				if (!WyncDelta_is_event_healthy (ctx, event_id)) {
					has_all_event_data = false;
					LOG_ERR_C(ctx,
						"Some delta event data is missing from this tick"
						"(%d), we don't have %u", tick, event_id);
					break;
				}
			}

			if (!has_all_event_data) {
				break;
			}

			// if delta_event_list.size() <= 0: also rollback if this current
			// tick contains undo events...... But what about previous ticks? No
			// problem because ticks are applied sequencially, so no ticks will
			// be skipped.
			// NOTE: if we don't apply any events, then there is no need to
			// modify ctx.entity_last_predicted_tick, that way there is no need
			// to repredict for no reason.

			bool we_modified_or_applied_events = false;

			if (
				prop->xtrap_enabled
				&& event_list.event_amount == 0
				&& !already_rollbacked
				&& WyncState_does_delta_prop_has_undo_events(prop, prop_id, tick)
			) {
				already_rollbacked = true;
				WyncState_predicted_delta_props_rollback_to_canonic_state(
						ctx, prop, prop_id);
				we_modified_or_applied_events = true;

				LOG_OUT_C(ctx, "debugrela applied NO events (and therefore "
						"rollbacked) tick %d UNDO DELTA FOUND", tick);
			}


			if (event_list.event_amount > 0) {
				we_modified_or_applied_events = true;

				if (prop->xtrap_enabled && !already_rollbacked) {
					already_rollbacked = true;
					WyncState_predicted_delta_props_rollback_to_canonic_state(
							ctx, prop, prop_id);
				}

				for (uint k = 0; k < event_list.event_amount; ++k) {
					uint event_id = event_list.event_ids[k];

					error = WyncDelta_merge_event_to_state_real_state(
							ctx, prop_id, event_id);

					// this error is almost fatal, should never happen, it could
					// fail because of not finding the event in that case we're
					// gonna continue in hopes we eventually get the event data
					// TODO: implement measures against this ever happening,
					// write tests againts this.

					if (error != OK) {
						LOG_ERR_C(ctx, "delta sync | VERY BAD, couldn't apply "
								"event id(%u) err(%d)", event_id, error);
						assert(false);
						break;
					}
				}

				LOG_OUT_C(ctx, "debugrela applied %d events (and therefore "
						"rollbacked) tick %d", event_list.event_amount, tick);
			}

			// commit

			applied_events_until = tick;
			ConMap_set_pair(
						delta_props_last_tick, prop_id, applied_events_until);

			// if predicted

			if (!prop->xtrap_enabled) continue;

			uint entity_id = 0;
			error = WyncTrack_prop_get_entity(ctx, prop_id, &entity_id);
			assert(error == OK);

			int last_predicted_tick = -1;
			ConMap_get (&ctx->co_pred.entity_last_predicted_tick,
											entity_id, &last_predicted_tick);

			if (we_modified_or_applied_events
				|| last_predicted_tick < applied_events_until)
			{
				ConMap_set_pair (&ctx->co_pred.entity_last_predicted_tick,
											entity_id, applied_events_until);
			}

			// Note: you have last tick applied per prop and globally, so the
			// user can choose what to use.
		}
	}
}

// sets props state to last confirmed received state
// NOTE: optimize which to reset, by knowing which were modified/new state gotten
// NOTE: reset only when new data is available
	
void WyncState_reset_props_to_latest_value (WyncCtx *ctx) {
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_state__newstate_prop_ids);

	ConMapIterator it = { 0 };
	while(ConMap_iterator_get_next_key(
		&ctx->co_track.active_prop_ids, &it) == OK)
	{
		u32 prop_id = it.key;

		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		if (prop->prop_type != WYNC_PROP_TYPE_STATE) { continue; }
		if (!prop->statebff.just_received_new_state) { continue; }

		prop->statebff.just_received_new_state = false;

		u32_DynArr_insert(
			&ctx->co_filter_c.type_state__newstate_prop_ids, prop_id);

		if (!prop->xtrap_enabled) { continue; }

		/*if (!prop->relative_sync_enabled) { */

			// regular prop

			u32 entity_id = 0;
			i32 err = WyncTrack_prop_get_entity(ctx, prop_id, &entity_id);
			if (err != OK) {
				LOG_ERR_C(ctx, "Couldn't find entity for prop %u", prop_id);
				continue;
			}
			i32 last_ticks_received = *i32_RinBuf_get_relative(
				&prop->statebff.last_ticks_received, 0);
			ConMap_set_pair(&ctx->co_pred.entity_last_predicted_tick,
				entity_id, last_ticks_received);

		/*} else { // rela prop*/
			/*// ...*/
		/*}*/
	}

	// rest state to _canonic_

	WyncState_reset_all_state_to_confirmed_tick_relative(
		ctx, ctx->co_filter_c.type_state__newstate_prop_ids.items,
		ctx->co_filter_c.type_state__newstate_prop_ids.size, 0);

	// only rollback if new state was received and is applicable:

	WyncState_delta_props_update_and_apply_delta_events(ctx);

}
