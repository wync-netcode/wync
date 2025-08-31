#include "wync_private.h"

// Q: Difference between "Actions" and "Events Consumed"?
// A:
// * Events Consumed: (Server only) A way for the server to support executing
//   client events without repeating them.
// * Actions: (Client only) Used when predicting, a way for the client to know
//   a given action on a repeating predicted tick was already executed.


// ==================================================================
// "Events Consumed" prop module / add-on

/// @returns error
int WyncConsumed_global_event_consume_tick (
	WyncCtx *ctx,
	uint wync_peer_id,
	uint channel,
	uint tick,
	uint event_id
) {
	if (channel < MAX_CHANNELS ||
		wync_peer_id < ctx->common.max_peers
	){ 
		return -1;
	}

	uint prop_id =
			ctx->co_events.prop_id_by_peer_by_channel[wync_peer_id][channel];
	WyncProp *prop_channel = WyncTrack_get_prop_unsafe(ctx, prop_id);

	int consumed_event_ids_tick = *i32_RinBuf_get_at(
			&prop_channel->co_consumed.events_consumed_at_tick_tick, tick);

	if (consumed_event_ids_tick < 0 || tick != (u32)consumed_event_ids_tick) {
		return -1;
	}

	u32_DynArr *consumed_events = u32_DynArr_RinBuf_get_at(
		&prop_channel->co_consumed.events_consumed_at_tick, tick);

	u32_DynArr_insert(consumed_events, event_id);

	return OK;
}


void WyncConsumed_advance_tick(WyncCtx *ctx) {
	u32 tick = ctx->common.ticks;

	// TODO: Index props with "event consume module"

	ConMapIterator it = { 0 };
	while (ConMap_iterator_get_next_key(
		&ctx->co_track.active_prop_ids, &it) == OK)
	{
		int prop_id;
		ConMap_get(&ctx->co_track.active_prop_ids, it.key, &prop_id);

		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (!prop->consumed_events_enabled) {
			continue;
		}
		i32_RinBuf_insert_at(
			&prop->co_consumed.events_consumed_at_tick_tick, tick, tick);

		u32_DynArr_clear_preserving_capacity(
			u32_DynArr_RinBuf_get_at(
				&prop->co_consumed.events_consumed_at_tick, tick));
	}
}


// ==================================================================
// Action functions, not related to Events


bool WyncAction_already_ran_on_tick (
	WyncCtx *ctx,
	uint predicted_tick,
	WyncName action_id
) {
	Name_ConMap *action_set = Name_ConMap_RinBuf_get_at(
		&ctx->co_pred.tick_action_history, predicted_tick);
	return Name_ConMap_has_key(action_set, action_id);
}

/// @returns error
void WyncAction_mark_as_ran_on_tick (
	WyncCtx *ctx,
	uint predicted_tick,
	WyncName action_id
) {
	Name_ConMap *action_set = Name_ConMap_RinBuf_get_at(
		&ctx->co_pred.tick_action_history, predicted_tick);
	Name_ConMap_set_pair(action_set, action_id, 0);
}

/// Run once each game tick
void WyncAction_tick_history_reset (
	WyncCtx *ctx,
	uint predicted_tick
) {
	Name_ConMap *action_set = Name_ConMap_RinBuf_get_at(
		&ctx->co_pred.tick_action_history, predicted_tick);
	Name_ConMap_clear_preserve_capacity(action_set);
}
