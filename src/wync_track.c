#include "wync_private.h"
#include "lib/log.h"
#include <string.h>
#include <assert.h>

/// Tracks game data


// Entity / Property functions
// ================================================================


/// @returns error
i32 WyncTrack_track_entity(
	WyncCtx *ctx,
	u32 entity_id, 
	u32 entity_type_id
) {
	if (ConMap_has_key(&ctx->co_track.tracked_entities, entity_id)) {
		LOG_OUT_C(ctx, "entity (id %u, entity_type_id %u) already tracked", entity_id, entity_type_id);
		return -1;
	}

	ConMap_set_pair(&ctx->co_track.tracked_entities, entity_id, true);
	u32_DynArr entity_props = u32_DynArr_create();
	u32_DynArr_ConMap_set_pair
		(&ctx->co_track.entity_has_props, entity_id, entity_props);
	ConMap_set_pair(&ctx->co_track.entity_is_of_type, entity_id, entity_type_id);

	ConMap_set_pair(&ctx->co_pred.entity_last_predicted_tick, entity_id, -1);
	ConMap_set_pair(&ctx->co_pred.entity_last_received_tick, entity_id, -1);
	
	return OK;
}

void WyncTrack_untrack_entity(
	WyncCtx *ctx,
	u32 entity_id
) {
	// TODO
}

void WyncTrack_delete_prop(
	WyncCtx *ctx,
	u32 prop_id
) {
	// TODO
}

bool WyncTrack_is_entity_tracked (
	WyncCtx *ctx,
	u32 entity_id
);


/// @param[out] out_prop_id
/// @returns error
i32 WyncTrack_get_new_prop_id (WyncCtx *ctx, u32 *out_prop_id) {
	for (u32 i = 0; i < MAX_PROPS; ++i) {
		++ctx->co_track.prop_id_cursor;

		if (ctx->co_track.prop_id_cursor >= MAX_PROPS) {
			ctx->co_track.prop_id_cursor = 0;
		}
		if (!ctx->co_track.props[ctx->co_track.prop_id_cursor].enabled) {
			*out_prop_id = ctx->co_track.prop_id_cursor;
			return OK;
		}
	}
	return -1;
}


/// @param[out] out_prop_id
/// @returns error
i32 WyncTrack_prop_register_minimal (
	WyncCtx *ctx,
	u32 entity_id,
	const char* name_id,
	enum WYNC_PROP_TYPE data_type,
	u32 *out_prop_id
) {
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)) {
		return -1;
	}

	bool setted_prop_id = false;
	u32 prop_id;

	// if pending to spawn then extract the prop_id

	bool entity_pending_to_spawn = false;

	if (ctx->common.is_client) {
		entity_pending_to_spawn = EntitySpawnPropRange_ConMap_has_key(
			&ctx->co_spawn.pending_entity_to_spawn_props, entity_id);

		if (entity_pending_to_spawn) {
			EntitySpawnPropRange *prop_range = NULL;

			i32 error = EntitySpawnPropRange_ConMap_get(
				&ctx->co_spawn.pending_entity_to_spawn_props,
				entity_id,
				&prop_range
			);

			assert(error == OK);

			setted_prop_id = true;
			prop_id = prop_range->prop_start + prop_range->curr;
			++prop_range->curr;
		}
	}

	if (!entity_pending_to_spawn) {
		i32 error = WyncTrack_get_new_prop_id(ctx, &prop_id);
		if (error == OK) {
			setted_prop_id = true;
		}
	}

	if (!setted_prop_id) {
		return -1;
	}

	WyncProp *prop = &ctx->co_track.props[prop_id];
	*prop = (WyncProp) { 0 };
	strcpy(prop->name_id, name_id);
	prop->prop_type = data_type;
	prop->enabled = true;

	// initialize statebff
	// TODO: some might not be necessary for all

	prop->statebff.last_ticks_received = i32_RinBuf_create
		(ctx->co_track.REGULAR_PROP_CACHED_STATE_AMOUNT, -1);

	// TODO: Dynamic sized buffer for all owned predicted props?
	// TODO: Only do this if this prop is predicted, move to prop_set_predict ?

	size_t saved_states_size;

	if (data_type == WYNC_PROP_TYPE_INPUT || data_type == WYNC_PROP_TYPE_EVENT) {
		saved_states_size = INPUT_BUFFER_SIZE;
	} else {
		saved_states_size = ctx->co_track.REGULAR_PROP_CACHED_STATE_AMOUNT;
	}

	prop->statebff.saved_states = WyncState_RinBuf_create
		(saved_states_size, (WyncState){ 0 });
	prop->statebff.state_id_to_tick = i32_RinBuf_create
		(saved_states_size, -1);
	prop->statebff.tick_to_state_id = i32_RinBuf_create
		(saved_states_size, -1);
	prop->statebff.state_id_to_local_tick = i32_RinBuf_create
		(saved_states_size, -1); // only for lerp

	// mark new Prop as active

	ConMap_set_pair(&ctx->co_track.active_prop_ids, prop_id, true);

	u32_DynArr *entity_props = NULL;
	i32 error = u32_DynArr_ConMap_get
		(&ctx->co_track.entity_has_props, entity_id, &entity_props);
	assert(error == OK);

	u32_DynArr_insert(entity_props, prop_id);

	ctx->common.was_any_prop_added_deleted = true;

	*out_prop_id = prop_id;

	return OK;
}


WyncProp *WyncTrack_get_prop(WyncCtx *ctx, u32 prop_id);


/// @returns Optional WyncProp
/// @retval NULL Not found
WyncProp *WyncTrack_entity_get_prop(
	WyncCtx *ctx,
	u32 entity_id,
	const char* prop_name_id
){
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)) {
		return NULL;
	}

	u32_DynArr *entity_props = NULL;
	i32 error = u32_DynArr_ConMap_get
		(&ctx->co_track.entity_has_props, entity_id, &entity_props);
	if (error != OK) {
		return NULL;
	}

	WyncProp *prop;
	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) { continue; }
		if (strcmp(prop->name_id, prop_name_id) == OK) {
			return prop;
		}
	}

	return NULL;
}


/// @param[out] out_prop_id if found
/// @returns error
i32 WyncTrack_entity_get_prop_id(
	WyncCtx *ctx,
	u32 entity_id,
	const char *prop_name_id,
	u32 *out_prop_id
){
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)) {
		return -1;
	}

	u32_DynArr *entity_props = NULL;
	i32 error = u32_DynArr_ConMap_get
		(&ctx->co_track.entity_has_props, entity_id, &entity_props);
	if (error != OK) {
		return -2;
	}

	WyncProp *prop;
	u32_DynArrIterator it = { 0 };
	while (u32_DynArr_iterator_get_next(entity_props, &it) == OK) {
		u32 prop_id = *it.item;

		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) { continue; }
		if (strcmp(prop->name_id, prop_name_id) == OK) {
			*out_prop_id = prop_id;
			return OK;
		}
	}

	return -3;
}


/// Note: Better have a structure for direct access instead of searching
/// @param[out] out_entity_id Pointer to Instance
/// @returns error
i32 WyncTrack_prop_get_entity(
	WyncCtx *ctx,
	u32 prop_id,
	u32 *out_entity_id
) {
	ConMap *tracked_entities = &ctx->co_track.tracked_entities;
    ConMapIterator it = { 0 };
    u32 entity_id;
	i32 err;

	// iterate all entities (slow)

    while (ConMap_iterator_get_next_key(tracked_entities, &it) == OK) {
		entity_id = it.key;

		u32_DynArr *entity_props = NULL;
		err = u32_DynArr_ConMap_get
			(&ctx->co_track.entity_has_props, entity_id, &entity_props);
		assert(err == OK);

		// iterate entity props

		u32_DynArrIterator dyna_it = { 0 };
		while (u32_DynArr_iterator_get_next(entity_props, &dyna_it) == OK) {
			u32 here_prop_id = *dyna_it.item;

			if (here_prop_id == prop_id) {
				*out_entity_id = entity_id;
				return OK;
			}
		}
	}

	return -1;
}


bool WyncTrack_is_entity_tracked (
	WyncCtx *ctx,
	u32 entity_id
) {
	return ConMap_has_key(&ctx->co_track.tracked_entities, entity_id);
}


/// @returns Optional WyncProp
/// @retval NULL Not found / Not enabled
WyncProp *WyncTrack_get_prop(WyncCtx *ctx, u32 prop_id) {
	if ( prop_id < MAX_PROPS
		&& ctx->co_track.props[prop_id].enabled
	) {
		return &ctx->co_track.props[prop_id];
	};
	return NULL;
}

WyncProp *WyncTrack_get_prop_unsafe(WyncCtx *ctx, u32 prop_id) {
	return &ctx->co_track.props[prop_id];
}

/// Use everytime we get state from a prop we don't have
/// Dummy props will be naturally deleted over time
/// @returns error
i32 WyncTrack_prop_register_update_dummy (
	WyncCtx *ctx,
	u32 prop_id,
	u32 last_tick,
	u32 data_size,
	void *data
) {
	Wync_DummyProp *dummy = NULL;
	DummyProp_ConMap *dummy_props = &ctx->co_dummy.dummy_props;

	// check if a dummy exists

	i32 found = DummyProp_ConMap_get(dummy_props, prop_id, &dummy) == OK;

	if (found) {
		WyncState_free(&dummy->data);
	} else {
		Wync_DummyProp new_dummy = { 0 };
		DummyProp_ConMap_set_pair(dummy_props, prop_id, new_dummy);
		DummyProp_ConMap_get(dummy_props, prop_id, &dummy);
	}

	dummy->last_tick = last_tick;
	dummy->data = WyncState_copy_from_buffer(data_size, data);
	
	return OK;
}

/// * Use it after setting up an entity and it's props
/// * Use it to add entities that already exist on the server & client
/// * Useful for map provided entities.
/// Make sure to reserve some of your _game entity ids_ for static entities
/// * Once a peer connects, make sure to setup all map _entity ids_ for him.
/// * WARNING: entity_id must be the same on server & client
/// * It will prevent the generation of a Spawn packet for that client
/// because it assumes the client already has it.
/// 
/// @returns error
i32 WyncTrack_wync_add_local_existing_entity (
	WyncCtx *ctx,
	u16 wync_client_id,
	u32 entity_id
) {
	if (ctx->common.is_client)
		{ return -1; }
	if (wync_client_id == SERVER_PEER_ID
		|| wync_client_id >= ctx->common.max_peers)
		{ return -2; } 
	if (!WyncTrack_is_entity_tracked(ctx, entity_id)) {
		// entity exists
		LOG_ERR_C(ctx, "entity (%u) isn't tracked", entity_id);
		return -3;
	}

	ConMap *sees_entities =
		&ctx->co_throttling.clients_sees_entities[wync_client_id];
	ConMap *sees_new_entities =
		&ctx->co_throttling.clients_sees_new_entities[wync_client_id];

	// remove from new entities

	ConMap_set_pair(sees_entities, entity_id, true);
	ConMap_remove_by_key(sees_new_entities, entity_id);

	return OK;
} 


/// @returns Entity id
/// @retval -1 Not found
i32 WyncTrack_find_owned_entity_by_entity_type_and_prop_name (
	WyncCtx *ctx,
	u32 entity_type_to_find,
	const char *prop_name_to_find
) {
	ConMap *owned_props =
		&ctx->co_clientauth.client_owns_prop[ctx->common.my_peer_id];
	WyncProp *prop = NULL;

	ConMapIterator it = { 0 };
	u32 prop_id;
	i32 error;
	while (ConMap_iterator_get_next_key(owned_props, &it) == OK) {
		prop_id = it.key;

		prop = WyncTrack_get_prop(ctx, prop_id);
		if (prop == NULL) continue;

		const char *prop_name = prop->name_id;
		u32 entity_id;
		u32 entity_type;

		error = WyncTrack_prop_get_entity(ctx, prop_id, &entity_id);
		if (error != OK) continue;

		error = ConMap_get(&ctx->co_track.entity_is_of_type, entity_id, &entity_type);
		if (error != OK) continue;

		// find a prop that I own
		// that is called "inputs"
		// that is of type player

		if (strcmp(prop_name, prop_name_to_find) == OK
			&& entity_type == entity_type_to_find)
		{
			return (i32)entity_id;
		}
	}
	return -1;
}

