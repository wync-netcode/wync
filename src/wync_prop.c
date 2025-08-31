#include "wync.h"
#include "wync_private.h"
#include "wync_wrapper.h"
#include "assert.h"

/// @returns error
i32 WyncProp_enable_prediction (WyncCtx *ctx, u32 prop_id){
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL || prop->xtrap_enabled) {
		return -1;
	}

	prop->xtrap_enabled = true;
	return OK;
}


/// * the server needs to know for subtick timewarping
/// * client needs to know for visual lerping
///
/// @param setter_lerp Callback to set the interpolated state
/// @returns error
i32 WyncProp_enable_interpolation (
	WyncCtx *ctx,
	u32 prop_id,
	u16 user_data_type,
	WyncWrapper_Setter setter_lerp
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) {
		return -1;
	}
 	if (prop->lerp_enabled) {
		return OK;
	}

	assert(user_data_type > 0); // avoid accidental default values
	assert(setter_lerp != NULL);
	
	prop->lerp_enabled = true;
	prop->co_lerp.lerp_user_data_type = user_data_type;
	ctx->wrapper->prop_setter_lerp[prop_id] = setter_lerp;
	
	return OK;
}


i32 WyncProp_enable_module_events_consumed (
	WyncCtx *ctx,
	u32 prop_id
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) {
		return -1;
	}
	if (prop->consumed_events_enabled) {
		return OK;
	}
	prop->consumed_events_enabled = true;

	prop->co_consumed.events_consumed_at_tick_tick = i32_RinBuf_create(
			ctx->common.max_age_user_events_for_consumption, -1);

	prop->co_consumed.events_consumed_at_tick = u32_DynArr_RinBuf_create(
			ctx->common.max_age_user_events_for_consumption, (u32_DynArr) { 0 });

	for (u16 i = 0; i < ctx->common.max_age_user_events_for_consumption; ++i) {
		u32_DynArr *events = u32_DynArr_RinBuf_get_absolute(
				&prop->co_consumed.events_consumed_at_tick, i);

		*events = u32_DynArr_create();
	}

	return OK;
}


int WyncProp_enable_relative_sync (
	WyncCtx *ctx,
	uint entity_id,
	uint prop_id,
	uint delta_blueprint_id,
	bool predictable
) {

	// Reference:
	// depending on the features and if it's server or client we'll need
	// different things
	// * delta prop, server side, no timewarp: real state, delta event buffer
	// * delta prop, client side, no prediction: real state, received delta
	//   event buffer
	// * delta prop, client side, predictable: base state, real state, received
	//   delta event buffer, predicted delta event buffer
	// * delta prop, server side, timewarpable: base state, real state, delta
	//   event buffer

	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) {
		return -1;
	}
	if (prop->relative_sync_enabled) {
		return OK;
	}

	if (!WyncDelta_blueprint_exists(ctx, delta_blueprint_id)) {
		LOG_ERR_C(ctx, "delta blueprint (%u) doesn't exist", delta_blueprint_id);
		return -2;
	}

	prop->relative_sync_enabled = true;
	prop->co_rela.delta_blueprint_id = delta_blueprint_id;

	// assuming no timewarpable
	// minimum storage allowed 0 or 2
	// TODO: create new methods for resizing containers, reusing existing memory

	uint buffer_items = 2;
	prop->statebff.saved_states = WyncState_RinBuf_create
		(buffer_items, (WyncState){ 0 });
	prop->statebff.state_id_to_tick = i32_RinBuf_create
		(buffer_items, -1);
	prop->statebff.tick_to_state_id = i32_RinBuf_create
		(buffer_items, -1);
	prop->statebff.state_id_to_local_tick = i32_RinBuf_create
		(buffer_items, -1); // only for lerp

	bool need_undo_events = false;
	if (ctx->common.is_client && predictable) {
		need_undo_events = true;
		WyncProp_enable_prediction(ctx, prop_id);
	}

	// setup auxiliar prop for delta change events

	prop->co_rela.current_delta_events = u32_DynArr_create();
	prop->co_rela.current_undo_delta_events = u32_DynArr_create();

	WyncEventUtil_EventCtx *event_ctx = calloc(1, sizeof(WyncEventUtil_EventCtx));
	event_ctx->list = &prop->co_rela.current_delta_events;

	uint events_prop_id;
	WyncTrack_prop_register_minimal(
		ctx,
		entity_id,
		"aux_delta_events",
		WYNC_PROP_TYPE_EVENT,
		&events_prop_id
	);
	WyncWrapper_set_prop_callbacks(
		ctx,
		events_prop_id,
		(WyncWrapper_UserCtx) {
			.ctx = event_ctx,
			.type_size = sizeof(WyncEventUtil_EventCtx)
		},
		WyncEventUtil_event_getter,
		WyncEventUtil_event_setter
	);

	// undo events are only for prediction and timewarp

	if (need_undo_events) {
		prop->co_rela.confirmed_states_undo_tick =
			i32_RinBuf_create(INPUT_BUFFER_SIZE, -1);
		prop->co_rela.confirmed_states_undo =
			u32_DynArr_RinBuf_create(INPUT_BUFFER_SIZE, (u32_DynArr) { 0 });

		for (uint i = 0; i < INPUT_BUFFER_SIZE; ++i) {
			u32_DynArr *undo_list = u32_DynArr_RinBuf_get_absolute(
				&prop->co_rela.confirmed_states_undo, i);
			*undo_list = u32_DynArr_create();
		}
	}

	// Q: shouldn't we be setting the auxiliar as predicted?
	// A: TODO

	WyncProp *aux_prop = WyncTrack_get_prop_unsafe(ctx, events_prop_id);
	aux_prop->is_auxiliar_prop = true;
	WyncProp_enable_prediction(ctx, events_prop_id);

	aux_prop->auxiliar_delta_events_prop_id = prop_id;
	prop->auxiliar_delta_events_prop_id = events_prop_id;

	return OK;
}
