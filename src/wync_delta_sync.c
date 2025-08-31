#include "assert.h"
#include "wync_private.h"
#include "wync_wrapper.h"


// Relative Synchronization functions
// Blueprint functions are part of the Wrapper


// ==================================================
// WRAPPER
// ==================================================




// Delta Blueprints Setup
// Should be setup only at the beginning
// @returns delta blueprint id
uint WyncDelta_create_blueprint (WyncCtx *ctx) {
	if (ctx->wrapper->delta_blueprint_id_counter >= WYNC_MAX_BLUEPRINTS) {
		assert(false);
	}

	WyncWrapper_DeltaBlueprint *blueprint =
		&ctx->wrapper->delta_blueprints[
			ctx->wrapper->delta_blueprint_id_counter];
	blueprint->event_handlers = *WyncBlueprintHandler_ConMap_create();

	return ctx->wrapper->delta_blueprint_id_counter++;
}


bool WyncDelta_blueprint_exists (WyncCtx *ctx, uint delta_blueprint_id) {
	return delta_blueprint_id < ctx->wrapper->delta_blueprint_id_counter;
}


WyncWrapper_DeltaBlueprint *WyncDelta_get_blueprint (
	WyncCtx *ctx, uint delta_blueprint_id
) {
	return &ctx->wrapper->delta_blueprints[delta_blueprint_id];
}


/// @returns error
int WyncDelta_blueprint_register_event (
	WyncCtx *ctx,
	uint delta_blueprint_id,
	uint event_type_id,
	WyncBlueprintHandler handler
) {
	if (!WyncDelta_blueprint_exists(ctx, delta_blueprint_id)) {
		return -1;
	}

	WyncWrapper_DeltaBlueprint *blueprint = WyncDelta_get_blueprint (
		ctx, delta_blueprint_id);
	WyncBlueprintHandler_ConMap_set_pair(
		&blueprint->event_handlers, event_type_id, handler);
	return OK;
}


// Prop Utils
// ================================================================


/// commits a delta event to this tick
///
/// @returns error
int WyncDelta_prop_push_event_to_current (
	WyncCtx *ctx,
	uint prop_id,
	uint event_type_id,
	uint event_id
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) { return -1; }
	if (!prop->relative_sync_enabled) { return -2; }

	WyncWrapper_DeltaBlueprint *blueprint =
			WyncDelta_get_blueprint(ctx, prop->co_rela.delta_blueprint_id);

	// event belongs to this blueprint
	if (!WyncBlueprintHandler_ConMap_has_key(
		&blueprint->event_handlers, event_type_id)) { return -3; }

	u32_DynArr_insert(
		&prop->co_rela.current_delta_events, event_id);

	return OK;
}


/// @param[out] out_undo_event_id Generated undo event
/// @returns error
static int WyncDelta_internal_merge_event_to_state (
	WyncCtx *ctx,
	WyncProp *prop,
	uint event_id,
	WyncWrapper_UserCtx user_ctx, // Not sure about this: og: "state: Variant"
	bool requires_undo,
	uint *out_undo_event_id
) {
	
	// get event handler function

	if (!WyncEvent_ConMap_has_key (&ctx->co_events.events, event_id)) {
		LOG_ERR_C(ctx, "deltasync | couldn't find event id(%u)", event_id);
		return -14;
	}

	WyncEvent *event;
	WyncEvent_ConMap_get(
		&ctx->co_events.events, event_id, &event);

	// Note: Maybe confirm this prop's blueprint supports this event_type

	WyncWrapper_DeltaBlueprint *blueprint =
			WyncDelta_get_blueprint(ctx, prop->co_rela.delta_blueprint_id);

	WyncBlueprintHandler *handler;
	int error = WyncBlueprintHandler_ConMap_get(
		&blueprint->event_handlers, event->data.event_type_id, &handler);
	if (error != OK) {
		LOG_ERR_C(ctx, "deltasync | invalid event id %u for blueprint %u",
			event->data.event_type_id, prop->co_rela.delta_blueprint_id);
		return -15;
	}

	int handler_result = (*handler)(
		ctx, user_ctx, event->data, requires_undo
	);
	if (!requires_undo) {
		return OK;
	}

	if (handler_result < 0) { // expected undo_event_id, yet didn't get it
		LOG_ERR_C(ctx, "deltasync | expected undo_event_id yet didn't get it"
			" blueprint %u event_type_id %u",
			event->data.event_type_id, prop->co_rela.delta_blueprint_id);
		return -16;
	}

	*out_undo_event_id = (uint)handler_result;
	return OK;
}


/// @returns error
int WyncDelta_merge_event_to_state_real_state (
	WyncCtx *ctx,
	uint prop_id,
	uint event_id
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) { return -1; }
	if (!prop->relative_sync_enabled) { return -2; }

	if (ctx->common.is_client
		&& !prop->xtrap_enabled
		&& ctx->co_pred.currently_on_predicted_tick) { return OK; }

	// If merging a _delta event_ WHILE PREDICTING (that is, not when merging
	// received data) we make sure to always produce an _undo delta event_ 

	bool is_client_predicting = ctx->common.is_client
		&& prop->xtrap_enabled
		&& ctx->co_pred.currently_on_predicted_tick;

	WyncWrapper_UserCtx user_ctx = ctx->wrapper->prop_user_ctx[prop_id];

	// Note: it seems wer're generating undo events for the server too, don't.

	uint out_undo_event_id = 0;
	int error = WyncDelta_internal_merge_event_to_state(
		ctx, prop, event_id, user_ctx, true, &out_undo_event_id);
	if (error != OK) {
		return -3;
	}

	// chacke undo event
	if (is_client_predicting) {
		u32_DynArr_insert(
			&prop->co_rela.current_undo_delta_events, out_undo_event_id);
		LOG_OUT_C(ctx, "deltasync, produced undo delta event %u", out_undo_event_id);
	}

	return OK;
}


// TODO: Make a separate version for only "clear_events_nonpredicted_owned_events"
// So that it doens't interfere with Xtrap tick end
void WyncDelta_predicted_event_props_clear_events (WyncCtx *ctx) {
	WyncWrapper_Setter *setter;
	WyncWrapper_UserCtx *user_ctx;
	u32_DynArrIterator it = { 0 };
	WyncWrapper_Data event_list_zeroed_blob = WyncEventUtil_event_get_zeroed();

	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_event__predicted_prop_ids, &it) == OK)
	{
		uint prop_id = *it.item;

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		if (*setter == NULL) continue;

		(*setter)(*user_ctx, event_list_zeroed_blob);
	}

	// TODO: Index prop_ids which are owned of event type
	it = (u32_DynArrIterator) { 0 };
	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_input_event__owned_prop_ids, &it) == OK)
	{
		uint prop_id = *it.item;
		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		if (prop == NULL) continue;
		if (prop->prop_type != WYNC_PROP_TYPE_EVENT) continue;

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		if (*setter == NULL) continue;

		(*setter)(*user_ctx, event_list_zeroed_blob);
	}
}


// ==================================================
// Private ?
// ==================================================

void WyncDelta_props_clear_current_delta_events(WyncCtx *ctx) {

	u32_DynArrIterator it = { 0 };

	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_s.filtered_delta_prop_ids, &it) == OK)
	{
		uint prop_id = *it.item;

		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		u32_DynArr_clear_preserving_capacity(
			&prop->co_rela.current_delta_events);
		u32_DynArr_clear_preserving_capacity(
			&prop->co_rela.current_undo_delta_events);
	}
}


bool WyncDelta_is_event_healthy (WyncCtx *ctx, uint event_id) {
	if (!WyncEvent_ConMap_has_key(
		&ctx->co_events.events, event_id))
	{
		LOG_ERR_C(ctx, "deltasync, couldn't find event (id %u)", event_id);
		return false;
	}

	return true;
}


// High level functions related to logic cycles
void WyncDelta_system_client_send_delta_prop_acks (WyncCtx *ctx) {
	uint prop_amount = 0;
	
	static u32_DynArr delta_prop_ids = { 0 };
	static i32_DynArr last_tick_received = { 0 };

	if (delta_prop_ids.capacity == 0) {
		delta_prop_ids = u32_DynArr_create();
	}
	if (last_tick_received.capacity == 0) {
		last_tick_received = i32_DynArr_create();
	}

	ConMap *delta_props_last_tick = 
		&ctx->co_track.client_has_relative_prop_has_last_tick[
			ctx->common.my_peer_id];

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(
			&ctx->co_filter_c.type_state__delta_prop_ids, &it) == OK)
	{
		uint prop_id = *it.item;

		int last_tick = 0;
		int error = ConMap_get(delta_props_last_tick, prop_id, &last_tick);
		if (error != OK || last_tick < 0) {
			continue;
		}

		u32_DynArr_insert(&delta_prop_ids, prop_id);
		i32_DynArr_insert(&last_tick_received, last_tick);
		++prop_amount;
	}

	if (prop_amount == 0) { return; }

	WyncPacketOut packet_out = { 0 };
	WyncPktDeltaPropAck pkt = { 0 };
	pkt.prop_amount = prop_amount;
	pkt.delta_prop_ids = delta_prop_ids.items;
	pkt.last_tick_received = last_tick_received.items;

	WyncPacket_wrap_and_queue(
		ctx,
		WYNC_PKT_DELTA_PROP_ACK,
		&pkt,
		SERVER_PEER_ID,
		UNRELIABLE,
		false
	);

	// free

	WyncPacketOut_free(&packet_out);
}


/// @return error
int WyncDelta_handle_pkt_delta_prop_ack (
	WyncCtx *ctx,
	WyncPktDeltaPropAck pkt,
	u16 from_nete_peer_id
) {
	// TODO: check client is healthy
	// NOTE: shouldn't this be checked upstream?
	u16 wync_client_id = 0;
	if (WyncJoin_is_peer_registered(
		ctx, from_nete_peer_id, &wync_client_id) != OK)
	{
		LOG_ERR_C(ctx, "client %u is not registered", wync_client_id);
		return -1;
	}

	ConMap *client_relative_props =
		&ctx->co_track.client_has_relative_prop_has_last_tick[wync_client_id];

	// update latest 'delta prop' acked tick

	for (uint i = 0; i < pkt.prop_amount; ++i) {
		uint prop_id = pkt.delta_prop_ids[i];
		int last_tick = pkt.last_tick_received[i];

		if (last_tick < 0) {
			LOG_ERR_C(ctx, "W: last_tick is < 0 prop(%u) tick(%u)",
				prop_id, last_tick);
			continue;
		}

		if ((uint)last_tick > ctx->common.ticks) {
			LOG_ERR_C(ctx, "W: last_tick is in the future prop(%u) tick(%u)",
				prop_id, last_tick);
			continue;
		}

		WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) {
			LOG_OUT_C(ctx, "W: Couldn't find this prop %u", prop_id);
			continue;
		}

		int saved_last_tick = 0;
		ConMap_get(
			client_relative_props, prop_id, &saved_last_tick);
		ConMap_set_pair(
			client_relative_props, prop_id, MAX(last_tick, saved_last_tick));
	}

	return OK;
}
