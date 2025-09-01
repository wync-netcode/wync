#include "wync_private.h"
#include "wync_wrapper.h"

// functions to preform extrapolation / prediction


// ==================================================
// PUBLIC API
// ==================================================


WyncXtrap_ticks WyncXtrap_preparation(WyncCtx *ctx) {
	CoPredictionData *p = &ctx->co_pred;
	if (p->last_tick_received == 0) {
		return (WyncXtrap_ticks) { 0 };
	}

	if (
		p->last_tick_received_at_tick_prev != p->last_tick_received_at_tick
		|| p->last_tick_received > p->first_tick_predicted
	){
		p->pred_intented_first_tick = p->last_tick_received +1;
		p->last_tick_received_at_tick_prev = p->last_tick_received_at_tick;
	} else {
		p->pred_intented_first_tick = p->last_tick_predicted;
	}

	if (p->target_tick <= (i32)ctx->co_ticks.server_ticks) {
		return (WyncXtrap_ticks) { 0 };
	}
	if (p->pred_intented_first_tick - p->max_prediction_tick_threeshold < 0) {
		return (WyncXtrap_ticks) { 0 };
	}

	p->first_tick_predicted = p->pred_intented_first_tick;
	
	p->currently_on_predicted_tick = true;

	return (WyncXtrap_ticks) {
		true,
		(u32)(ctx->co_pred.pred_intented_first_tick
			- ctx->co_pred.max_prediction_tick_threeshold),
		(u32)(ctx->co_pred.target_tick +1)
	};
}

/// Composes a list of ids of entities TO PREDICT THIS TICK
void WyncXtrap_regular_entities_to_predict(WyncCtx *ctx, i32 tick) {
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_pred.global_entity_ids_to_predict);

	i32 entity_last_tick = -1;
	i32 entity_last_predicted_tick = -1;

	// determine if an entity should be predicted

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(
		&ctx->co_pred.predicted_entity_ids, &it) == OK)
	{
		u32 entity_id = *it.item;

		i32 err = ConMap_get(
			&ctx->co_pred.entity_last_received_tick, entity_id, &entity_last_tick);

		// no history
		if (err != OK || entity_last_tick == -1) {
			continue;
		}

		// already have confirmed state + it's regular prop
		if (entity_last_tick >= tick) continue;

		err = ConMap_get(&ctx->co_pred.entity_last_predicted_tick,
				entity_id, &entity_last_predicted_tick);

		// already predicted
		if (err != OK || tick <= entity_last_predicted_tick) continue;

		// else, aprove prediction and assume this tick as predicted
		if (tick > entity_last_predicted_tick) {

			ConMap_set_pair(&ctx->co_pred.entity_last_predicted_tick,
					entity_id, tick);
		}

		u32_DynArr_insert(
				&ctx->co_pred.global_entity_ids_to_predict, entity_id);
	}
}


void WyncXtrap_termination (WyncCtx *ctx) {
	WyncDelta_props_clear_current_delta_events(ctx);
	WyncDelta_predicted_event_props_clear_events (ctx);

	ctx->co_pred.currently_on_predicted_tick = false;
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_pred.global_entity_ids_to_predict);
}


bool WyncXtrap_is_entity_predicted (WyncCtx *ctx, u32 entity_id) {
	//if (!ctx->co_track.entity_has_props
	u32_DynArr *entity_props = NULL;
	i32 err = 
		u32_DynArr_ConMap_get(&ctx->co_track.entity_has_props, entity_id, &entity_props);
	if (err != OK) {
		return false;
	}

	WyncProp *prop = NULL;
	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) { continue; }
		if (prop->xtrap_enabled) return true;
	}

	return false;
}


static void WyncXtrap_props_update_predicted_states_ticks (
	WyncCtx *ctx,
	u32 target_tick,
	u32 *prop_ids,
	u32 prop_id_amount
) {
	for (u32 i = 0; i < prop_id_amount; ++i) {
		u32 prop_id = prop_ids[i];

		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (prop == NULL) {
			continue;
		}
		if (!prop->xtrap_enabled) { // Redundant check?
			continue;
		}

		// update store predicted state metadata
		prop->co_xtrap.pred_prev.server_tick = target_tick -1;
		prop->co_xtrap.pred_curr.server_tick = target_tick;

	}
}


/// TODO: maybe we could compute this every time we get an update?
/// Only predicted props
/// Exclude props I own (Or just exclude TYPE_INPUT?) What about events or
///   delta props?
///
/// @returns out_latest_received_tick
/// @retval -1 Either entity not found, or tick not found
static i32 WyncXtrap_entity_get_last_received_tick_from_pred_props (
	WyncCtx *ctx,
	u32 entity_id
) {
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)) {
		return -1;
	}

	u32_DynArr *entity_props = NULL;
	u32_DynArrIterator it = { 0 };

	i32 err = u32_DynArr_ConMap_get(
		&ctx->co_track.entity_has_props, entity_id, &entity_props);
	if (err != OK) { return -1; }

	i32 last_tick = -1;
	WyncProp *prop = NULL;

	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) { continue; }

		// for rela props, ignore base, instead count auxiliar last tick
		if (prop->relative_sync_enabled) { continue; }

		i32 prop_last_tick = *i32_RinBuf_get_relative(
				&prop->statebff.last_ticks_received, 0);
		if (prop_last_tick == -1) { continue; }

		if (ConMap_has_key(
			&ctx->co_clientauth.client_owns_prop[ctx->common.my_peer_id],
			prop_id))
		{
			continue;
		}

		if (last_tick == -1) {
			last_tick = prop_last_tick;
		} else {
			last_tick = MIN(last_tick, prop_last_tick);
		}
	}

	return last_tick;
}


/// NOTE: assuming snap props always include all snaps for an entity
void WyncXtrap_update_entity_last_tick_received(
	WyncCtx *ctx,
	u32 prop_id
){
	u32 entity_id;
	i32 err = WyncTrack_prop_get_entity(ctx, prop_id, &entity_id);
	if (err != OK) {
		LOG_WAR_C(ctx, "Couldn't find entity_id for prop_id(%d)", prop_id);
		return;
	}

	i32 last_tick =
		WyncXtrap_entity_get_last_received_tick_from_pred_props(
			ctx, entity_id);
	if (last_tick < 0) { return; }

	ConMap_set_pair(&ctx->co_pred.entity_last_received_tick,
		entity_id, last_tick);
}


// ==================================================
// WRAPPER
// ==================================================


WyncXtrap_entities WyncXtrap_tick_init (WyncCtx *ctx, i32 tick) {
	ctx->co_pred.current_predicted_tick = tick;

	// reset predicted inputs / events

	WyncState_reset_all_state_to_confirmed_tick_absolute(
		ctx,
		ctx->co_filter_c.type_input_event__predicted_owned_prop_ids.items, 
		(u32)ctx->co_filter_c.type_input_event__predicted_owned_prop_ids.size, 
		(u32)tick
	);

	// clearing delta events before predicting, predicted delta events will be
	// polled and cached at the end of the predicted tick
	// WyncXtrapInternal.wync_xtrap_delta_props_clear_current_delta_events(ctx)
	WyncXtrap_delta_props_clear_current_delta_events(ctx);

	// ...

	// collect what entities to predict
	WyncXtrap_regular_entities_to_predict(ctx, tick);

	return (WyncXtrap_entities) {
		(u32)ctx->co_pred.global_entity_ids_to_predict.size,
		ctx->co_pred.global_entity_ids_to_predict.items
	};
}


// private
/// Extracts data from predicted props
static void WyncXtrap_props_update_predicted_states_data (
	WyncCtx *ctx,
	u32 *prop_ids,
	u32 prop_id_amount
) {
	WyncWrapper_Getter *getter = NULL;
	WyncWrapper_UserCtx *user_ctx = NULL;
	Wync_NetTickData *pred_curr = NULL;
	Wync_NetTickData *pred_prev = NULL;
	WyncProp *prop = NULL;

	for (u32 i = 0; i < prop_id_amount; ++i) {
		u32 prop_id = prop_ids[i];

		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (prop == NULL) {
			continue;
		}
		if (prop->relative_sync_enabled) {
			continue;
		}

		pred_curr = &prop->co_xtrap.pred_curr;
		pred_prev = &prop->co_xtrap.pred_prev;
		
		getter = &ctx->wrapper->prop_getter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		if (*getter == NULL) { continue; }

		WyncWrapper_Data extracted = (*getter)(*user_ctx);
		if (extracted.data_size == 0 || extracted.data == NULL) {
			continue;
		}

		WyncState_set_from_buffer(
			&pred_prev->data, pred_curr->data.data_size, pred_curr->data.data);
		WyncState_set_from_buffer(
			&pred_curr->data, extracted.data_size, extracted.data);

		WyncWrapper_Data_free(extracted);
	}
}


static void WyncXtrap_save_latest_predicted_state (WyncCtx *ctx, i32 tick) {
	// (invoke on last two iterations)

	i32 store_predicted_states = tick > (ctx->co_pred.target_tick - 1);
	if (!store_predicted_states) return;

	u32_DynArr *entity_props = NULL;
	u32_DynArrIterator it = { 0 };

	while (u32_DynArr_iterator_get_next(
			&ctx->co_pred.predicted_entity_ids, &it) == OK)
	{
		u32 wync_entity_id = *it.item;

		entity_props = NULL;
		i32 err = u32_DynArr_ConMap_get (&ctx->co_track.entity_has_props,
				wync_entity_id, &entity_props);
		if (err != OK) {
			LOG_ERR_C(ctx, "Couldn't find entity (%d)'s props", wync_entity_id);
			continue;
		}

		// TODO: Make this call user-level
		// 1. store predicted states
		// 2. store predicted states

		WyncXtrap_props_update_predicted_states_data( ctx,
			entity_props->items, (u32)entity_props->size);

		WyncXtrap_props_update_predicted_states_ticks( ctx,
			ctx->co_pred.target_tick, entity_props->items, (u32)entity_props->size);
	}
}


static void WyncXtrap_internal_tick_end(WyncCtx *ctx, i32 tick) {
	
	// wync bookkeeping
	// --------------------------------------------------

	// extract / poll for generated predicted _undo delta events_

	u32_DynArrIterator it = { 0 };
	while(u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_state__predicted_delta_prop_ids, &it) == OK)
	{
			// ...
	}

	ctx->co_pred.last_tick_predicted = tick;

	// NOTE: Integration functions would go here
}


void WyncXtrap_tick_end(WyncCtx *ctx, i32 tick) {
	WyncXtrap_save_latest_predicted_state (ctx, tick);
	WyncXtrap_internal_tick_end(ctx, tick);
}



bool WyncXtrap_allowed_to_predict_entity(WyncCtx *ctx, uint entity_id) {
	if (ctx->common.is_client) {
		return u32_DynArr_has(
				&ctx->co_pred.global_entity_ids_to_predict, entity_id);
	}
	return true;
}
