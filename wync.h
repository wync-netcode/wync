#ifndef WYNC_H
#define WYNC_H

#define WYNC_WRAPPER

#include "stdbool.h"
#include "stdint.h"

typedef struct WyncCtx WyncCtx;

enum WYNC_PROP_TYPE {
    WYNC_PROP_TYPE_STATE,
    WYNC_PROP_TYPE_INPUT,
    WYNC_PROP_TYPE_EVENT // can only store Array[int]
};

typedef struct {
    uint32_t data_size;
    void *data;
} WyncWrapper_Data;
// TODO: Rename to WyncWrapper_NetDataBlob for user setters and getters

typedef struct {
    void *ctx;
    uint32_t type_size;
} WyncWrapper_UserCtx;

typedef WyncWrapper_Data (*WyncWrapper_Getter)(WyncWrapper_UserCtx ctx);

typedef void (*WyncWrapper_Setter)(WyncWrapper_UserCtx, WyncWrapper_Data data);

typedef WyncWrapper_Data (*WyncWrapper_LerpFunc)(
    WyncWrapper_Data from, WyncWrapper_Data to, float delta);

typedef struct {
    uint32_t event_type_id;
    WyncWrapper_Data data;
} WyncEvent_EventData;

/// if requires_undo is FALSE must return -1,
/// else create an "undo event" and return the EVENT_ID
typedef int (*WyncBlueprintHandler)(
    WyncCtx *ctx, WyncWrapper_UserCtx user_ctx, WyncEvent_EventData event,
    bool requires_undo);

typedef struct {
    bool spawn; // wether to spawn or to dispawn
    uint32_t entity_id;
    uint32_t entity_type_id;
    uint32_t spawn_data_size;
    void *spawn_data;
} Wync_EntitySpawnEvent;

// Packets handed to you for sending through the network
// it includes the destination nete peer

typedef struct {
    uint16_t to_nete_peer_id;
    uint32_t data_size;
    void *data; // WyncPacket
} WyncPacketOut;

/// state data type for WYNC_PROP_TYPE_EVENT
typedef struct {
    uint32_t event_amount;
    uint32_t *event_ids;
} WyncEventList;

/// ---------------------------------------------------------------------------
/// WYNC STATISTICS
/// ---------------------------------------------------------------------------

/// ---------------------------------------------------------------------------
/// WYNC PACKET UTIL
/// ---------------------------------------------------------------------------

/// Sets the data limit in bytes for the current frame. Wync will limit the
/// amount of packages it generates; entities will take turns to be synced.
/// Beware, too much throttling might cause visual glitches.
///
/// @param data_limit_bytes Maximum bytes to generate on a frame
void WyncPacket_set_data_limit_chars_for_out_packets(
    WyncCtx *ctx, uint32_t data_limit_bytes);

/// ---------------------------------------------------------------------------
/// WYNC CLOCK
/// ---------------------------------------------------------------------------

/// Updates the latency a peer is experiencing. Periodically let Wync know
/// the updated latency for a peer for better precision when calculating
/// timing for Interpolation, Extrapolation, Timewarp, etc.
///
/// @param peer_id Wync peer identifier
/// @param latency_ms Perceived latency for peer in milliseconds
void WyncClock_peer_set_current_latency(
    WyncCtx *ctx, uint16_t peer_id, uint16_t latency_ms);

/// Let Wync know about the physics update rate of your game for timing
/// calculations.
///
/// @param tps Updates per second, e.g. 30, 60.
void WyncClock_client_set_physics_ticks_per_second(WyncCtx *ctx, uint16_t tps);

/// Debug function. Add an artificial offset to your perceived time.
void WyncClock_set_debug_time_offset(WyncCtx *ctx, uint64_t time_offset_ms);

/// Debug function. Modify Wync internal tick value.
void WyncClock_set_ticks(WyncCtx *ctx, uint32_t ticks);

/// Debug function. Get Wync internal tick value.
///
/// @returns Ticks
uint32_t WyncClock_get_ticks(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC DEBUG
/// ---------------------------------------------------------------------------

/// Debug function. Gets you a summary of the current state in text format,
/// including monitors of relevant data for debugging Interpolation and
/// Extrapolation.
///
/// @param[out] lines Text buffer of at least 4096 bytes
void WyncDebug_get_info_general_text(
    WyncCtx *server_wctx, WyncCtx *client_wctx, char *lines);

/// Debug function. Gets you a summary in text format of the known props for
/// a specified wync context.
///
/// @param[out] lines Text buffer of at least 4096 bytes
void WyncDebug_get_prop_info_text(WyncCtx *ctx, char *lines);

/// Debug function. Gets you a summary in text format of the amount and kind of
/// packets this instance has consumed.
///
/// @param[out] lines Text buffer of at least 4096 bytes
void WyncDebug_get_packets_received_info_text(
    WyncCtx *ctx, char *lines, uint16_t prop_amount);

/// ---------------------------------------------------------------------------
/// WYNC FLOW
/// ---------------------------------------------------------------------------

/// Feeds a network packet to this instance. Wync will read and consumed it to
/// update it's internal state. You must free it afterwards.
///
/// @param from_nete_peer_id Sender network peer id
int32_t WyncFlow_feed_packet(
    WyncCtx *ctx, uint16_t from_nete_peer_id, uint32_t data_size, void *data);

/// Setups current instance as Wync server
void WyncFlow_server_setup(WyncCtx *ctx);

/// Setups current instance as Wync client
void WyncFlow_client_setup(WyncCtx *ctx);

/// Main server logic. Call at the start of you logic frame, among other things
/// this will set the correct state of your client's inputs.
void WyncFlow_server_tick_start(WyncCtx *ctx);

/// Main server logic end. Call at the end of your logic frame, among other
/// things this will extract data from your game state for synchronization.
void WyncFlow_server_tick_end(WyncCtx *ctx);

/// Main client logic. Call at the end of your logic frame.
void WyncFlow_client_tick_end(WyncCtx *ctx);

/// Creates a cache of packets to be sent to other peers respecting the data
/// limit set.
void WyncFlow_gather_packets(WyncCtx *ctx);

/// Setup packet iterator. Call before iterating packets to be sent through the
/// network.
void WyncFlow_prepare_packet_iterator(WyncCtx *ctx);

/// Obtains next network reliable packet for delivery.
///
/// @param[out] out_pkt Packet to be send through the Network RELIABLY.
///                     Must be instanced.
/// @returns error
/// @retval 0 OK
/// @retval -1 End reached
int32_t WyncFlow_get_next_reliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);

/// Obtains next network unreliable packet for delivery.
///
/// @param[out] out_pkt Packet to be send through the Network UNRELIABLY.
///                     Must be instanced.
/// @returns error
/// @retval 0 OK
/// @retval -1 End reached
int32_t
WyncFlow_get_next_unreliable_packet(WyncCtx *ctx, WyncPacketOut *out_pkt);

/// Cleans network packet cache. Call after getting all packets.
void WyncFlow_packet_cleanup(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC INIT
/// ---------------------------------------------------------------------------

/// Creates a new empty Wync Context Instance. Must later be setup as Server or
/// Client
WyncCtx *WyncInit_create_context(void);

/// ---------------------------------------------------------------------------
/// WYNC INPUT
/// ---------------------------------------------------------------------------

/// Assigns ownership of an Input Prop to a specified Peer. This peer will have
/// authority over the Prop's state. Sending updates periodically.
///
/// @param prop_id Prop identifier
/// @param client_id Wync Peer identifier
int32_t WyncInput_prop_set_client_owner(
    WyncCtx *ctx, uint32_t prop_id, uint16_t client_id);

/// ---------------------------------------------------------------------------
/// WYNC JOIN
/// ---------------------------------------------------------------------------

/// Whether the specified instance is connected to a server. Returns always
/// True for a server.
bool WyncJoin_is_connected(WyncCtx *ctx);

bool WyncJoin_is_client(WyncCtx *ctx);

/// Whether a specified peer is connected on this server.
///
/// @param[out] out_peer_id Wync Peer Identifier. Must be instanced.
/// @returns error
int32_t WyncJoin_is_peer_registered(
    WyncCtx *ctx, uint16_t nete_peer_id, uint16_t *out_wync_peer_id);

/// Sets the Network Peer Identifier for the current instance
///
/// @param nete_peer_id Network Peer Identifier
void WyncJoin_set_my_nete_peer_id(WyncCtx *ctx, uint16_t nete_peer_id);

/// (Client only) Sets the target server's Network Peer Identifier
///
/// @param nete_peer_id Network Peer Identifier
void WyncJoin_set_server_nete_peer_id(WyncCtx *ctx, uint16_t nete_peer_id);

/// (Server only) Setup peer iterator. Call before iterating peers pending to
/// be setup.
void WyncJoin_pending_peers_setup_iteration(WyncCtx *ctx);

/// (Server only) Gets the next client peer to be setup.
///
/// @returns Network Peer ID which is pending to setup
/// @retval -1 End reached, no more peers
int32_t WyncJoin_pending_peers_get_next(WyncCtx *ctx);

/// Call after getting all pending peers
void WyncJoin_pending_peers_clear(WyncCtx *ctx);

/// Setup peer iterator. Call before iterating all active peers.
void WyncJoin_active_peers_setup_iteration(WyncCtx *ctx);

typedef struct {
    int32_t wync_peer_id;
    int32_t network_peer_id;
} WyncPeer_ids;

/// Gets next active peer.
///
/// @param[out] Struct including both `Wync ID` and `Network ID` for Peer.
///             Must be instanced.
/// @retval -1 End reached, no more peers
int32_t
WyncJoin_active_peers_get_next(WyncCtx *ctx, WyncPeer_ids *out_peer_ids);

int32_t WyncJoin_get_wync_peer_id_from_nete_peer_id(
    WyncCtx *ctx, uint16_t nete_peer_id, uint16_t *out_wync_peer_id);

int32_t WyncJoin_get_nete_peer_id_from_wync_peer_id(
    WyncCtx *ctx, uint16_t wync_peer_id, int32_t *out_nete_peer_id);

/// ---------------------------------------------------------------------------
/// WYNC LERP
/// ---------------------------------------------------------------------------

/// Configures the desired amount of lerping.
///
/// @param server_tick_rate Rate at which updates are being received from the
///                         Server. (e.g. 60, 30, 10). Pass 0 for automatic.
/// @param lerp_ms Milliseconds to interpolate in the past.
void WyncLerp_client_set_lerp_ms(
    WyncCtx *ctx, float server_tick_rate, uint32_t lerp_ms);

/// Configures the maximum factor for interpolation. Describes how much the
/// interpolation is allowed to extend (extrapolate) when data is missing. This
/// range will be applied symmetrically: How much to go in the past, and how
/// much to go in the future.
///
/// @param max_lerp_factor_symmetric
void WyncLerp_set_max_lerp_factor_symmetric(
    WyncCtx *ctx, float max_lerp_factor_symmetric);

/// Register a data type for performing interpolation.
///
/// @param user_type_id User defined Identifier for this data type
///                     there is a limit of WYNC_MAX_USER_TYPES.
/// @param lerp_func Pointer to the lerping function:
///                  (value1, value2, delta) => result.
void WyncLerp_register_lerp_type(
    WyncCtx *ctx, uint16_t user_type_id, WyncWrapper_LerpFunc lerp_func);

void WyncLerp_interpolate_all(WyncCtx *ctx, float delta_lerp_fraction);

/// ---------------------------------------------------------------------------
/// WYNC PROP
/// ---------------------------------------------------------------------------

/// Enable Extrapolation for a specified Prop
///
/// @returns error
int32_t WyncProp_enable_prediction(WyncCtx *ctx, uint32_t prop_id);

/// Enable Interpolation for a specified Prop
///
/// @param user_type_id Previously register lerp data type identifier.
/// @param setter_lerp Pointer to function that sets the interpolated state to
///                    the game state. It will use the user context passed to
///                    the `Prop register function`:
///                    (user_contex, lerped_state) => void.
/// @returns error
int32_t WyncProp_enable_interpolation(
    WyncCtx *ctx, uint32_t prop_id, uint16_t user_data_type,
    WyncWrapper_Setter setter_lerp);

/// ---------------------------------------------------------------------------
/// WYNC SPAWN
/// ---------------------------------------------------------------------------

/// Gets next spawn event for spawning user defined game entities.
///
/// @param[out] out_spawn_event Struct with info about what to spawn. Struct
///                             must be instanced.
/// @returns error
int32_t WyncSpawn_get_next_entity_event_spawn(
    WyncCtx *ctx, Wync_EntitySpawnEvent *out_spawn_event);

/// Call after successfully consuming an spawn event
void WyncSpawn_finish_spawning_entity(WyncCtx *ctx, uint32_t entity_id);

/// Clear all spawning events. Call after consuming all.
void WyncSpawn_system_spawned_props_cleanup(WyncCtx *ctx);

/// ---------------------------------------------------------------------------
/// WYNC STATE SEND
/// ---------------------------------------------------------------------------

/// ---------------------------------------------------------------------------
/// WYNC STATE SET
/// ---------------------------------------------------------------------------

/// ---------------------------------------------------------------------------
/// WYNC STATE STORE
/// ---------------------------------------------------------------------------

/// ---------------------------------------------------------------------------
/// WYNC THROTTLE
/// ---------------------------------------------------------------------------

/// (Server only) Add an entity to client's "vision", will start synchronization
/// of this entity for the client. Spawn and Snapshot packets will be generated.
///
/// @param client_id Wync Peer Identifier
/// @param entity_id Wync Entity Identifier
int32_t WyncThrottle_client_now_can_see_entity(
    WyncCtx *ctx, uint16_t client_id, uint32_t entity_id);

/// (Server only) Add an entity to everyone's "vision". Will start
/// Synchronization of this entity for all connected client peers.
///
/// @param entity_id Wync Entity Identifier
void WyncThrottle_everyone_now_can_see_entity(WyncCtx *ctx, uint32_t entity_id);

/// (Server only) Sets arbitrary data for helping setup an Spawning entity.
/// Clients will receive this data along the regular Spawn Event.
///
/// @param data_size Spawn data bytes
/// @param data      Pointer to data buffer
void WyncThrottle_entity_set_spawn_data(
    WyncCtx *ctx, uint32_t entity_id, uint32_t data_size, void *data);

int32_t WyncThrottle_client_no_longer_sees_entity(
    WyncCtx *ctx, uint16_t client_id, uint32_t entity_id);

/// ---------------------------------------------------------------------------
/// WYNC TICK COLLECTION
/// ---------------------------------------------------------------------------

/// ---------------------------------------------------------------------------
/// WYNC TRACK
/// ---------------------------------------------------------------------------

///
int32_t WyncTrack_track_entity(
    WyncCtx *ctx, uint32_t entity_id, uint32_t entity_type_id);

void WyncTrack_untrack_entity(WyncCtx *ctx, uint32_t entity_id);

bool WyncTrack_is_entity_tracked(WyncCtx *ctx, uint32_t entity_id);

int32_t WyncTrack_prop_register_minimal(
    WyncCtx *ctx, uint32_t entity_id, const char *name_id,
    enum WYNC_PROP_TYPE data_type, uint32_t *out_prop_id);

int32_t WyncTrack_entity_get_prop_id(
    WyncCtx *ctx, uint32_t entity_id, const char *prop_name_id,
    uint32_t *out_prop_id);

/// Note: Better have a structure for direct access instead of searching
/// @param[out] out_entity_id Pointer to Instance
/// @returns error
int32_t WyncTrack_prop_get_entity(
    WyncCtx *ctx, uint32_t prop_id, uint32_t *out_entity_id);

int32_t WyncTrack_wync_add_local_existing_entity(
    WyncCtx *ctx, uint16_t wync_client_id, uint32_t entity_id);

int32_t WyncTrack_find_owned_entity_by_entity_type_and_prop_name(
    WyncCtx *ctx, uint32_t entity_type_to_find, const char *prop_name_to_find);

/// ---------------------------------------------------------------------------
/// WYNC XTRAP
/// ---------------------------------------------------------------------------

typedef struct {
    bool should_predict;
    uint32_t tick_start;
    uint32_t tick_end;
} WyncXtrap_ticks;

typedef struct {
    uint32_t entity_ids_amount;
    uint32_t *entity_ids_to_predict;
} WyncXtrap_entities;

WyncXtrap_ticks WyncXtrap_preparation(WyncCtx *ctx);

WyncXtrap_entities WyncXtrap_tick_init(WyncCtx *ctx, int32_t tick);

void WyncXtrap_tick_end(WyncCtx *ctx, int32_t tick);

void WyncXtrap_termination(WyncCtx *ctx);

bool WyncXtrap_allowed_to_predict_entity(WyncCtx *ctx, uint32_t entity_id);

/// ---------------------------------------------------------------------------
/// WYNC WRAPPER UTIL
/// ---------------------------------------------------------------------------

void WyncWrapper_set_prop_callbacks(
    WyncCtx *ctx, uint32_t prop_id, WyncWrapper_UserCtx user_ctx,
    WyncWrapper_Getter getter, WyncWrapper_Setter setter);

/// ---------------------------------------------------------------------------
/// WYNC DELTA SYNC
/// ---------------------------------------------------------------------------

uint32_t WyncDelta_create_blueprint(WyncCtx *ctx);

/// @returns error
int WyncDelta_blueprint_register_event(
    WyncCtx *ctx, uint32_t delta_blueprint_id, uint32_t event_type_id,
    WyncBlueprintHandler handler);

int WyncEventUtils_new_event_wrap_up(
    WyncCtx *ctx, uint16_t event_user_type_id, uint32_t data_size,
    void *event_data, uint32_t *out_event_id);

int WyncProp_enable_relative_sync(
    WyncCtx *ctx, uint32_t entity_id, uint32_t prop_id,
    uint32_t delta_blueprint_id, bool predictable);

int WyncDelta_prop_push_event_to_current(
    WyncCtx *ctx, uint32_t prop_id, uint32_t event_type_id, uint32_t event_id);

int WyncDelta_merge_event_to_state_real_state(
    WyncCtx *ctx, uint32_t prop_id, uint32_t event_id);

int32_t WyncEventUtils_publish_global_event_as_client(
    WyncCtx *ctx, uint16_t channel, uint32_t event_id);

int32_t WyncEventUtils_get_events_from_channel_from_peer(
    WyncCtx *ctx, uint16_t wync_peer_id, uint16_t channel, uint32_t tick,
    WyncEventList *out_event_list);

int WyncEventUtils_get_event_data(
    WyncCtx *ctx, uint32_t event_id, WyncEvent_EventData *out_event_data);

int WyncConsumed_global_event_consume_tick(
    WyncCtx *ctx, uint32_t wync_peer_id, uint32_t channel, uint32_t tick,
    uint32_t event_id);

typedef struct {
	char name[40];
} WyncName;

bool WyncAction_already_ran_on_tick (
	WyncCtx *ctx,
	uint32_t predicted_tick,
	WyncName action_id
);

void WyncAction_mark_as_ran_on_tick (
	WyncCtx *ctx,
	uint32_t predicted_tick,
	WyncName action_id
);

void WyncAction_tick_history_reset (
	WyncCtx *ctx,
	uint32_t predicted_tick
);

/// ---------------------------------------------------------------------------
/// WYNC TIMEWARP
/// ---------------------------------------------------------------------------

int WyncProp_enable_timewarp(WyncCtx *ctx, uint32_t prop_id);

uint32_t
WyncTimewarp_get_peer_latency_stable(WyncCtx *ctx, uint32_t wync_peer_id);

uint32_t WyncTimewarp_get_peer_lerp_ms(WyncCtx *ctx, uint32_t wync_peer_id);

bool WyncTimewarp_can_we_timerwarp_to_this_tick(WyncCtx *ctx, uint32_t tick);

void WyncTimewarp_cache_current_state_timewarpable_props(WyncCtx *ctx);

int WyncTimewarp_warp_to_tick(WyncCtx *ctx, uint32_t tick, float delta_lerp_ms);

int WyncTimewarp_warp_entity_to_tick(
    WyncCtx *ctx, uint32_t entity_id, uint32_t tick_left, float lerp_delta_ms);

void WyncTimewarp_restore_present_state (WyncCtx *ctx);

#endif // !WYNC_H
