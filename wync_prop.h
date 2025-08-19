#ifndef WYNC_PROP_H
#define WYNC_PROP_H

#include "wync/wync_track.h"
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
/// @returns error
i32 WyncProp_enable_interpolation (
	WyncCtx *ctx,
	u32 prop_id,
	u16 user_data_type
) {
	WyncProp *prop = WyncTrack_get_prop(ctx, prop_id);
	if (prop == NULL) {
		return -1;
	}
 	if (prop->lerp_enabled) {
		return OK;
	}

	assert(user_data_type > 0); // avoid accidental default values
	
	prop->lerp_enabled = true;
	prop->co_lerp.lerp_user_data_type = user_data_type;
	
	return OK;
}


#endif // !WYNC_PROP_H
