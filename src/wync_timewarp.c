#include "wync_private.h"
#include <math.h>


uint32_t
WyncTimewarp_get_peer_latency_stable(WyncCtx *ctx, uint32_t wync_peer_id) {
	return ctx->common.peer_latency_info[wync_peer_id].latency_stable_ms;
}


uint32_t WyncTimewarp_get_peer_lerp_ms(WyncCtx *ctx, uint32_t wync_peer_id) {
	return ctx->common.client_has_info[wync_peer_id].lerp_ms;
}


bool WyncTimewarp_can_we_timerwarp_to_this_tick(WyncCtx *ctx, uint32_t tick) {
	return (tick > ctx->common.ticks - ctx->max_tick_history_timewarp) ||
		(tick <= ctx->common.ticks);
}


void WyncTimewarp_cache_current_state_timewarpable_props(WyncCtx *ctx) {
	WyncWrapper_extract_prop_snapshot_to_tick (
		ctx,
		ctx->common.ticks,
		(uint)u32_DynArr_get_size(
			&ctx->co_filter_s.filtered_regular_timewarpable_prop_ids
		),
		ctx->co_filter_s.filtered_regular_timewarpable_prop_ids.items
	);
}


int WyncTimewarp_warp_entity_to_tick(
	WyncCtx *ctx, uint entity_id, uint tick_left, float lerp_delta_ms)
{
	float frame_ms = 1000.0 / ctx->common.physic_ticks_per_second;

	uint tick_origin = tick_left + (uint)floorf(lerp_delta_ms/frame_ms);
	bool target_is_current_tick = tick_origin == ctx->common.ticks;

	// Note: we skip this check because this function is often called after
	// already warping to a specific tick
	// if (target_is_current_tick) { return OK; }

	u32_DynArr *entity_props = NULL;
	int error = u32_DynArr_ConMap_get(
		&ctx->co_track.entity_has_props, entity_id, &entity_props);
	if (error != OK) {
		return -1;
	}

	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		uint prop_id = *it.item;
		WyncProp *prop = WyncTrack_get_prop_unsafe(ctx, prop_id);

		if (!prop->timewarp_enabled) { continue; }

		if (prop->lerp_enabled && !target_is_current_tick) {
			WyncLerp_reset_to_interpolated_absolute (
				ctx, &prop_id, 1, tick_left, lerp_delta_ms
			);
		}
		else {
			WyncState_reset_all_state_to_confirmed_tick_absolute (
				ctx, &prop_id, 1, tick_left
			);
		}
	}

	return OK;
}


int WyncTimewarp_warp_to_tick(
	WyncCtx* ctx, uint32_t tick_left, float lerp_delta_ms
) {
	float frame_ms = 1000.0 / ctx->common.physic_ticks_per_second;
	uint tick_origin_target = tick_left + floor(lerp_delta_ms/frame_ms);
	if (tick_origin_target == ctx->common.ticks) { return OK; }

	WyncState_reset_all_state_to_confirmed_tick_absolute (
		ctx,
ctx->co_filter_s.filtered_regular_timewarpable_non_interpolable_prop_ids.items,
ctx->co_filter_s.filtered_regular_timewarpable_non_interpolable_prop_ids.size,
		tick_left
	);
	WyncLerp_reset_to_interpolated_absolute (
		ctx,
ctx->co_filter_s.filtered_regular_timewarpable_interpolable_prop_ids.items,
ctx->co_filter_s.filtered_regular_timewarpable_interpolable_prop_ids.size,
		tick_left,
		lerp_delta_ms
	);
		
	return OK;
}


void WyncTimewarp_restore_present_state (WyncCtx *ctx) {
	WyncState_reset_all_state_to_confirmed_tick_absolute (
		ctx,
		ctx->co_filter_s.filtered_regular_timewarpable_prop_ids.items,
		(uint)ctx->co_filter_s.filtered_regular_timewarpable_prop_ids.size,
		ctx->common.ticks
	);
}
