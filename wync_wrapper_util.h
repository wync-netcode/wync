#ifndef WYNC_WRAPPER_UTIL_H
#define WYNC_WRAPPER_UTIL_H

#include "wync/wync_state_store.h"
#include "wync/wync_track.h"
#include "wync/wync_wrapper.h"
#include "wync_typedef.h"
#include <stdlib.h>

void WyncWrapper_initialize(WyncCtx *ctx) {
	ctx->wrapper = calloc(sizeof(WyncWrapperCtx), 1);
}

void WyncWrapper_set_prop_callbacks(
	WyncCtx *ctx,
	u32 prop_id,
	WyncWrapper_UserCtx user_ctx,
	WyncWrapper_Getter getter,
	WyncWrapper_Setter setter
) {
	ctx->wrapper->prop_user_ctx[prop_id] = user_ctx;
	ctx->wrapper->prop_getter[prop_id] = getter;
	ctx->wrapper->prop_setter[prop_id] = setter;
}

// ==================================================
// Wrapper functions that aren't worth creating a 
// wrapper version of their respective modules
// ==================================================

void WyncWrapper_buffer_inputs(WyncCtx *ctx) {
	// Buffer state (extract) from props we own

	u32_DynArrIterator it = { 0 };
	while(u32_DynArr_iterator_get_next(
		&ctx->co_filter_c.type_input_event__owned_prop_ids, &it) == OK)
	{
		u32 prop_id = *it.item;
		WyncProp *input_prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		WyncWrapper_UserCtx user_ctx = ctx->wrapper->prop_user_ctx[prop_id];
		WyncWrapper_Getter getter = ctx->wrapper->prop_getter[prop_id];

		WyncWrapper_Data new_state = getter(user_ctx);
		//WyncStateStore
	}

}

void WyncWrapper_extract_data_to_tick(WyncCtx *ctx, u32 save_on_tick) {
	WyncProp *prop = NULL;
	WyncProp *prop_aux = NULL;
	WyncWrapper_Getter *getter = NULL;
	WyncWrapper_Setter *setter = NULL;
	WyncWrapper_UserCtx *user_ctx = NULL;

	// save state history per tick

	u32_DynArrIterator it = { 0 };
	while(u32_DynArr_iterator_get_next(
		&ctx->co_filter_s.filtered_regular_extractable_prop_ids, &it) == OK) {
		u32 prop_id = *it.item;
		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);
		getter = &ctx->wrapper->prop_getter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];

		WyncWrapper_Data data = (*getter)(*user_ctx);

		WyncStore_prop_state_buffer_insert(
			ctx, prop, save_on_tick, (WyncState){data.data_size, data.data});
	}

	// TODO: Delta props here ....
}

void WyncWrapper_input_props_set_tick_value (WyncCtx *ctx) {
	WyncProp *prop = NULL;
	u32_DynArrIterator it = { 0 };
	WyncWrapper_Setter *setter = NULL;
	WyncWrapper_UserCtx *user_ctx = NULL;

	while (u32_DynArr_iterator_get_next(
		&ctx->co_filter_s.filtered_clients_input_and_event_prop_ids, &it) == OK)
	{
		u32 prop_id = *it.item;
		prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		WyncState input = WyncState_prop_state_buffer_get(prop, ctx->common.ticks);
		if (input.data_size == 0 || input.data == NULL) {
			LOG_OUT_C(ctx, "couldn't find input (%s) for tick (%u)", prop->name_id, ctx->common.ticks);
			continue;
		}

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
		(*setter)(*user_ctx, (WyncWrapper_Data){input.data_size, input.data});
	}
}

// TODO: Move to WyncFlow
void WyncWrapper_server_filter_prop_ids(WyncCtx *ctx) {
	if (!ctx->common.was_any_prop_added_deleted) return;
	ctx->common.was_any_prop_added_deleted = false;

	LOG_OUT_C(ctx, "Filtering props");

	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_s.filtered_clients_input_and_event_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_s.filtered_delta_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_s.filtered_regular_extractable_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_s.filtered_regular_timewarpable_prop_ids);

	WyncProp *prop = NULL;

	u32 peer_amount = (u32)i32_DynArr_get_size(&ctx->common.peers);
	for (u16 client_id = 1; client_id < peer_amount; ++client_id) {

		ConMapIterator it = { 0 };
		while (ConMap_iterator_get_next_key(
			&ctx->co_clientauth.client_owns_prop[client_id], &it) == OK)
		{
			u32 prop_id = it.key;
			prop = WyncTrack_get_prop(ctx, prop_id);
			if (prop->prop_type != WYNC_PROP_TYPE_INPUT &&
			    prop->prop_type != WYNC_PROP_TYPE_EVENT) {
				continue;
			}
			
			u32_DynArr_insert(
				&ctx->co_filter_s.filtered_clients_input_and_event_prop_ids, prop_id);
		}
	}

	ConMapIterator it = { 0 };
	while (ConMap_iterator_get_next_key(
		&ctx->co_track.active_prop_ids, &it) == OK)
	{
		u32 prop_id = it.key;
		prop = WyncTrack_get_prop(ctx, prop_id);

		if (prop->prop_type != WYNC_PROP_TYPE_STATE &&
			prop->prop_type != WYNC_PROP_TYPE_INPUT) {
			continue;
		}

		if (prop->relative_sync_enabled &&
			prop->prop_type == WYNC_PROP_TYPE_STATE
		) {
			u32_DynArr_insert(
				&ctx->co_filter_s.filtered_delta_prop_ids, prop_id);
			continue;
		}

		if (prop->prop_type == WYNC_PROP_TYPE_STATE) {
			u32_DynArr_insert(
				&ctx->co_filter_s.filtered_regular_extractable_prop_ids, prop_id);
		}

		if (prop->timewarp_enabled) {
			u32_DynArr_insert(
				&ctx->co_filter_s.filtered_regular_timewarpable_prop_ids, prop_id);

			if (prop->lerp_enabled) {
				u32_DynArr_insert
					(&ctx->co_filter_s.filtered_regular_timewarpable_interpolable_prop_ids, prop_id);
			}
		}
	}
}

void WyncWrapper_client_filter_prop_ids (WyncCtx *ctx) {
	if (!ctx->common.was_any_prop_added_deleted) return;
	ctx->common.was_any_prop_added_deleted = false;

	LOG_OUT_C(ctx, "Filtering props");

	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_input_event__owned_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_input_event__predicted_owned_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_event__predicted_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_state__delta_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_state__predicted_delta_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_state__predicted_regular_prop_ids);
	u32_DynArr_clear_preserving_capacity(
		&ctx->co_filter_c.type_state__interpolated_regular_prop_ids);

	u32_DynArr_clear_preserving_capacity(
		&ctx->co_pred.predicted_entity_ids);

	// Note: Consider drop concept of 'active' props, they're the same?

	WyncProp *prop;
	ConMapIterator it = { 0 };

	while (ConMap_iterator_get_next_key(
		&ctx->co_clientauth.client_owns_prop[ctx->common.my_nete_peer_id], &it) == OK)
	{
		u32 prop_id = it.key;
		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) continue;

		if (prop->prop_type == WYNC_PROP_TYPE_STATE) continue;

		u32_DynArr_insert(
			&ctx->co_filter_c.type_input_event__owned_prop_ids, prop_id);

		if (prop->xtrap_enabled) {
			u32_DynArr_insert(
				&ctx->co_filter_c.type_input_event__predicted_owned_prop_ids, prop_id);
		}

	}

	it = (ConMapIterator) { 0 };
	while (ConMap_iterator_get_next_key(
		&ctx->co_track.active_prop_ids, &it) == OK)
	{
		u32 prop_id = it.key;
		prop = WyncTrack_get_prop(ctx, prop_id);

		if (prop->prop_type == WYNC_PROP_TYPE_EVENT && prop->xtrap_enabled) {
			u32_DynArr_insert(
				&ctx->co_filter_c.type_event__predicted_prop_ids, prop_id);
		}

		if (prop->prop_type == WYNC_PROP_TYPE_STATE) {
			if (prop->relative_sync_enabled) {
				u32_DynArr_insert(
					&ctx->co_filter_c.type_state__delta_prop_ids, prop_id);
				if (prop->xtrap_enabled) {
					u32_DynArr_insert(
						&ctx->co_filter_c.type_state__predicted_delta_prop_ids, prop_id);
				}
			}
			// regular props
			else { 
				if (prop->xtrap_enabled) {
					u32_DynArr_insert(
						&ctx->co_filter_c.type_state__predicted_regular_prop_ids, prop_id);
				}
				if (prop->lerp_enabled) {
					u32_DynArr_insert(
						&ctx->co_filter_c.type_state__interpolated_regular_prop_ids, prop_id);
				}
			}
		}
	}

	// TODO: filter predicted entities
	//it = (ConMapIterator) { 0 };
	//while (ConMap_iterator_get_next_key(
		//&ctx->co_track.tracked_entities, &it) == OK)
	//{
		//if (!
	//}
}


#endif // !WYNC_WRAPPER_UTIL_H
