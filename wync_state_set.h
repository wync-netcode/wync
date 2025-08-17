#ifndef WYNC_STATE_SET
#define WYNC_STATE_SET

#include "wync/wync_state_store.h"
#include "wync/wync_track.h"
#include "wync/wync_typedef.h"
#include "wync/wync_wrapper.h"

/// Used only for resetting inputs in WyncXtrap
/// Note: Could be use for general purpose
void WyncState_reset_all_state_to_confirmed_tick_absolute(
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

		WyncState state = WyncState_prop_state_buffer_get(prop, tick);
		if (state.data_size == 0 || state.data == NULL) {
			continue;
		}

		setter = &ctx->wrapper->prop_setter[prop_id];
		user_ctx = &ctx->wrapper->prop_user_ctx[prop_id];
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
		(*setter)(*user_ctx, (WyncWrapper_Data){ state.data_size, state.data });

		if (prop->relative_sync_enabled) {
			LOG_OUT_C(ctx, "debugdelta, Setted absolute state for prop(%u) %s",
			prop_id, prop->name_id);
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

		if (!prop->relative_sync_enabled) { // regular prop

			u32 entity_id = 0;
			i32 err = WyncTrack_prop_get_entity(ctx, prop_id, &entity_id);
			if (err != OK) {
				LOG_ERR_C(ctx, "Couldn't find entity for prop %u", prop_id);
			}
			i32 last_ticks_received = *i32_RinBuf_get_relative(
				&prop->statebff.last_ticks_received, 0);
			ConMap_set_pair(&ctx->co_pred.entity_last_predicted_tick,
				entity_id, last_ticks_received);

		} else { // rela prop
			// ...
		}
	}

	// rest state to _canonic_

	WyncState_reset_all_state_to_confirmed_tick_relative(
		ctx, ctx->co_filter_c.type_state__newstate_prop_ids.items,
		ctx->co_filter_c.type_state__newstate_prop_ids.size, 0);

	// only rollback if new state was received and is applicable:

	//delta_props_update_and_apply_delta_events(ctx, ctx.co_filter_c.type_state__delta_prop_ids)

}

#endif // !WYNC_STATE_SET
