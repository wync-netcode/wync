#ifndef WYNC_WRAPPER_H
#define WYNC_WRAPPER_H

// TODO: Need generic value MAP<int, Any>
#include "containers/map.h"
#include "src/macro_types.h" // TODO: Rewrite
#include "wync_typedef.h"


typedef struct{

	// Another idea to store Callables:
	// List <handler_id: int>
	// See WyncCtx for where handlers (Callables) are stored
	// Blueprint instances should be scarse, so there won't be any significant repetition
	
	// * Stores Callables
	// * Allows to check if a given event_type_id is supported
	// Map <event_type_id: int, handler: Callable>
	//var event_handlers: Dictionary
	i32 dummy;
	
	// Callable interface
	// 'first time' is another name for 'requires_undo'
	// 'wync_ctx' will only be set if 'requires_undo' is
	// (state: Variant, event: WyncEvent.EventData, requires_undo: bool, ctx: WyncCtx*) -> [err, undo_event_id]:

} WyncWrapper_DeltaBlueprint;


// ------------------------------
// Wrapper

#define WYNC_MAX_USER_TYPES 256
#define WYNC_MAX_BLUEPRINTS 64

typedef struct {
	u32 data_size;
	void *data;
} WyncWrapper_Data;

//typedef void* WyncWrapper_UserCtx;

typedef struct {
	void *ctx;
	size_t type_size;
} WyncWrapper_UserCtx;

typedef WyncWrapper_Data (*WyncWrapper_Getter)(WyncWrapper_UserCtx ctx);

typedef void (*WyncWrapper_Setter)(WyncWrapper_UserCtx, WyncWrapper_Data data);

typedef WyncWrapper_Data (*WyncWrapper_LerpFunc)
	(WyncWrapper_Data from, WyncWrapper_Data to, float delta);

// ^^^ FUTURE: A method with less indirections using void* with cached size


typedef struct WyncWrapperCtx{
	WyncWrapper_UserCtx prop_user_ctx[MAX_PROPS];
	WyncWrapper_Getter prop_getter[MAX_PROPS];
	WyncWrapper_Setter prop_setter[MAX_PROPS]; // Maybe use a b-tree set?

	// Array<user_type_id: int, Any>
	// use lerp_function[index] directly
	//u16 lerp_type_to_lerp_function[WYNC_MAX_USER_TYPES];
	WyncWrapper_LerpFunc lerp_function[WYNC_MAX_USER_TYPES];

	// Array<delta_blueprint_id: int, Blueprint>
	WyncWrapper_DeltaBlueprint delta_blueprints[WYNC_MAX_BLUEPRINTS];

	// TODO(Future): Physics integration functions
	//// Maybe this is the user's responsibility
	//// Map<entity_id: int, sim_fun_id>
	//var entity_has_integrate_fun: Dictionary
	//// Syncs entity transform with physic server 
	//// Array<sim_fun_id: int, Callable>
	//var integration_functions: Array[Callable]
} WyncWrapperCtx;

#endif // !WYNC_WRAPPER_H
