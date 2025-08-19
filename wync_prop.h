#ifndef WYNC_PROP_H
#define WYNC_PROP_H

#include "wync/wync_track.h"
#include "wync/wync_wrapper.h"
#include "wync_typedef.h"

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


#endif // !WYNC_PROP_H
