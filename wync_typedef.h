#ifndef WYNC_CTX_H
#define WYNC_CTX_H

#include "wync.h"
#include "string.h"
#include "stdlib.h"
#include "containers/map.h"
#include "src/buffer.h"


// ============================================================
//
// BASE TYPES
//
// ============================================================


#ifndef i16
#define i16 int16_t
#endif
#ifndef u16
#define u16 uint16_t
#endif
#ifndef i32
#define i32 int32_t
#endif
#ifndef u32
#define u32 uint32_t
#endif
#ifndef i64
#define i64 int64_t
#endif
#ifndef u64
#define u64 uint64_t
#endif

#define SIGN(x) ((x) < 0 ? -1 : ((x) > 0 ? +1 : 0))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


typedef struct {
	char name[40];
} WyncName;

typedef struct {
	u32 data_size;
	void *data;
} WyncState;

static void WyncState_free (WyncState *state) {
	free(state->data);
	state->data = NULL;
	state->data_size = 0;
}
static WyncState WyncState_copy_from_buffer (u32 data_size, void *data) {
	WyncState state = { 
		.data_size = data_size,
		.data = calloc(1, data_size)
	};
	memcpy(state.data, data, data_size);
	return state;
}
static void WyncState_set_from_buffer (WyncState *self, u32 data_size, void *data) {
	if (self->data_size != data_size) {
		WyncState_free(self);
		*self = WyncState_copy_from_buffer(data_size, data);
		return;
	}
	memcpy(self->data, data, data_size);
}
static bool WyncState_serialize (
	bool is_reading,
	NeteBuffer *buffer,
	WyncState *state
) {
	NETEBUFFER_BYTES_SERIALIZE(
		is_reading, buffer, &state->data_size, sizeof(u32));
	if (state->data_size == 0) {
		return true;
	}
	if (is_reading) {
		// TODO: add limit
		state->data = calloc(1, state->data_size);
	}
	NETEBUFFER_BYTES_SERIALIZE(
		is_reading, buffer, state->data, state->data_size);
	return true;
}


typedef struct {
	u32 server_tick;
	u32 arrived_at_tick;
	WyncState data;
} Wync_NetTickData;


//Wync_NetTickData Wync_NetTickData_copy_calloc(Wync_NetTickData *self) {
	//Wync_NetTickData i = (Wync_NetTickData) { 0 };
	//memcpy(&i, self, sizeof(Wync_NetTickData));
	
	//i.data = calloc(sizeof(char), i.data_size);
	//memcpy(i.data, self->data, i.data_size);
	//return i;
//}

// user network info feed

#define LATENCY_BUFFER_SIZE 20 // 20 size, 2 polls per second -> 10 seconds worth

typedef struct {
	u32 latency_raw_latest_ms; // Recently polled latency
	u32 latency_stable_ms;     // Stabilized latency
	u32 latency_mean_ms; 
	u32 latency_std_dev_ms;
	u32 latency_buffer[LATENCY_BUFFER_SIZE];
	u32 latency_buffer_head;
	float debug_latency_mean_ms;
} Wync_PeerLatencyInfo;


typedef struct {
	i32 lerp_ms;
} Wync_ClientInfo;


// throttling

typedef struct {
	u16 peer_id;
	u32 entity_id;
} Wync_PeerEntityPair;

typedef struct {
	u16 peer_id;
	u32 prop_id;
} Wync_PeerPropPair;

typedef struct {
	i32 a;
	i32 b;
} Wync_i32Pair;

//typedef struct {
	//bool spawn; // wether to spawn or to dispawn
	////bool already_spawned;
	//u32 entity_id;
	//u32 entity_type_id;
	//u32 spawn_data_size; // Not using WyncState because this is public API
	//void *spawn_data;
//} Wync_EntitySpawnEvent;

typedef struct {
	u32 last_tick;
	WyncState data;
} Wync_DummyProp;

static void Wync_DummyProp_free (Wync_DummyProp *self) {
	WyncState_free(&self->data);
}

static WyncEvent_EventData WyncEvent_EventData_duplicate(
	WyncEvent_EventData *self
) {
	WyncEvent_EventData newi = { 0 };
	memcpy(&newi, self, sizeof(WyncEvent_EventData));

	WyncState data = WyncState_copy_from_buffer(
			self->data.data_size, self->data.data);
	newi.data.data = data.data;
	newi.data.data_size = data.data_size;
	return newi;
}

typedef struct {
	WyncEvent_EventData data;
	u32 data_hash;
} WyncEvent;

//enum WYNC_PROP_TYPE {
	//WYNC_PROP_TYPE_STATE,
	//WYNC_PROP_TYPE_INPUT,
	//WYNC_PROP_TYPE_EVENT // can only store Array[int]
//};

typedef struct {
	u32 prop_start;
	u32 prop_end;
	u32 curr;
} EntitySpawnPropRange;



// ============================================================
// 
// PACKETS
// 
// ============================================================



enum WYNC_PKT {
	WYNC_PKT_JOIN_REQ,
	WYNC_PKT_JOIN_RES,
	WYNC_PKT_RES_CLIENT_INFO,
	WYNC_PKT_CLIENT_SET_LERP_MS,
	WYNC_PKT_CLOCK,

	WYNC_PKT_PROP_SNAP,
	WYNC_PKT_INPUTS,
	WYNC_PKT_EVENT_DATA,
	WYNC_PKT_DELTA_PROP_ACK,

	WYNC_PKT_SPAWN,
	WYNC_PKT_DESPAWN,
	WYNC_PKT_AMOUNT
};

static const char* GET_PKT_NAME (enum WYNC_PKT pkt) {
	static const char* PKT_NAMES[WYNC_PKT_AMOUNT] = {
		"WYNC_PKT_JOIN_REQ",
		"WYNC_PKT_JOIN_RES",
		"WYNC_PKT_RES_CLIENT_INFO",
		"WYNC_PKT_CLIENT_SET_LERP_MS",
		"WYNC_PKT_CLOCK",

		"WYNC_PKT_PROP_SNAP",
		"WYNC_PKT_INPUTS",
		"WYNC_PKT_EVENT_DATA",
		"WYNC_PKT_DELTA_PROP_ACK",

		"WYNC_PKT_SPAWN",
		"WYNC_PKT_DESPAWN"
	};
	return PKT_NAMES[pkt];
}

typedef struct {
	enum WYNC_PKT packet_type_id;
	WyncState data;
} WyncPacket;

static bool WyncPacket_serialize(
	bool is_reading, 
	NeteBuffer *buffer,
	WyncPacket *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->packet_type_id, sizeof(u16));
	if (!WyncState_serialize(is_reading, buffer, &pkt->data)) { return false; }
	return true;
}

static void WyncPacket_free(WyncPacket *pkt) {
	WyncState_free(&pkt->data);
}

static bool WyncPacketOut_write(NeteBuffer *buffer, WyncPacketOut *pkt) {
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->to_nete_peer_id, sizeof(u16));
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->data_size, sizeof(u32));
	NETEBUFFER_WRITE_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
static bool WyncPacketOut_read(NeteBuffer *buffer, WyncPacketOut *pkt) {
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->to_nete_peer_id, sizeof(pkt->to_nete_peer_id));
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->data_size, sizeof(pkt->data_size));

	pkt->data = calloc(sizeof(char), pkt->data_size);

	NETEBUFFER_READ_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
static void WyncPacketOut_free(WyncPacketOut *pkt) {
	free(pkt->data);
	pkt->data = NULL;
	pkt->data_size = 0;
}

typedef struct {
	u32 lerp_ms;
} WyncPktClientSetLerpMS;

// what happens if it's expensive to know the size?

static bool WyncPktClientSetLerpMS_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktClientSetLerpMS *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->lerp_ms, sizeof(u32));
	return true;
}

typedef struct {
	u32 tick;    // answerer's tick
	u32 tick_og; // requester's tick
	u64 time;    // answerer's time
	u64 time_og; // requester's time
} WyncPktClock;

static bool WyncPktClock_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktClock *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->tick, sizeof(u32));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->time, sizeof(u64));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->tick_og, sizeof(u32));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->time_og, sizeof(u64));
	return true;
}

typedef struct {
	// * Client -> Server
	// * The client notifies the server what was the last update (tick) received
	//   for all relative props that he knows about.
	// * Send this aprox every 3 seconds

	u32 prop_amount;
	u32 *delta_prop_ids;
	i32 *last_tick_received;
} WyncPktDeltaPropAck;

static void WyncPktDeltaPropAck_free (WyncPktDeltaPropAck *pkt) {
	free(pkt->delta_prop_ids);
	free(pkt->last_tick_received);

	pkt->prop_amount = 0;
	pkt->delta_prop_ids = NULL;
	pkt->last_tick_received = NULL;
}

static bool WyncPktDeltaPropAck_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktDeltaPropAck *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->prop_amount, sizeof(u32));

	if (is_reading) {
		pkt->delta_prop_ids = (u32*)calloc(sizeof(u32), pkt->prop_amount);
		pkt->last_tick_received = (i32*)calloc(sizeof(i32), pkt->prop_amount);
	}

	for (u32 k = 0; k < pkt->prop_amount; ++k) {
		NETEBUFFER_BYTES_SERIALIZE(
			is_reading, buffer, &pkt->delta_prop_ids[k], sizeof(u32));
	}
	for (u32 k = 0; k < pkt->prop_amount; ++k) {
		NETEBUFFER_BYTES_SERIALIZE(
			is_reading, buffer, &pkt->last_tick_received[k], sizeof(i32));
	}
	return true;
}

typedef struct {
	u32 entity_amount;
	u32 *entity_ids;
} WyncPktDespawn;
static void WyncPktDespawn_allocate(WyncPktDespawn *pkt, u32 size) {
	pkt->entity_amount = size;
	pkt->entity_ids = (u32*) calloc(sizeof(u32), size);
}
static void WyncPktDespawn_free(WyncPktDespawn *pkt) {
	free(pkt->entity_ids);
	pkt->entity_ids = NULL;
	pkt->entity_amount = 0;
}
/// Allocates needed memory
static bool WyncPktDespawn_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktDespawn *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(
		is_reading, buffer, &pkt->entity_amount, sizeof(u32));
	if (is_reading) {
		// TODO: limit
		pkt->entity_ids = (u32*) calloc(sizeof(u32), pkt->entity_amount);
	}
	for (u32 i = 0; i < pkt->entity_amount; ++i) {
		NETEBUFFER_BYTES_SERIALIZE(
			is_reading, buffer, &pkt->entity_ids[i], sizeof(u32));
	}
	return true;
}

// NOTE: EventData is different from WyncEvent in that this one is sent
// over the network, so it has an extra property: event_id: int
typedef struct {
	u32 event_id;
	u32 event_type_id;
	WyncState data;
} WyncPktEventData_EventData;

typedef struct {
	u32 event_amount;
	WyncPktEventData_EventData *events;
} WyncPktEventData;

static void WyncPktEventData_free (WyncPktEventData *pkt) {
	for (u32 i = 0; i < pkt->event_amount; ++i) {
		WyncState_free(&pkt->events[i].data);
	}
	free(pkt->events);
}

typedef struct {
	u32 tick;
	WyncState state;
} WyncTickDecorator;

typedef struct {
	u32 prop_id;
	u32 amount;
	WyncTickDecorator *inputs;
} WyncPktInputs;

static void WyncPktInputs_free (WyncPktInputs *pkt) {
	WyncTickDecorator *input;
	if (pkt->inputs == NULL) return;
	for (u32 i = 0; i < pkt->amount; ++i) {
		input = &pkt->inputs[i];
		WyncState_free(&input->state);
	}
	free(pkt->inputs);
	pkt->inputs = NULL;
	pkt->amount = 0;
}

static bool WyncPktInputs_serialize (
	bool is_reading,
	NeteBuffer *buff,
	WyncPktInputs *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buff, &pkt->prop_id, sizeof(u32));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buff, &pkt->amount, sizeof(u32));

	if (is_reading) {
		pkt->inputs = (WyncTickDecorator*)
			calloc(sizeof(WyncTickDecorator), pkt->amount);
	}

	WyncTickDecorator *input = NULL;
	for (u32 i = 0; i < pkt->amount; ++i) {
		input = &pkt->inputs[i];

		NETEBUFFER_BYTES_SERIALIZE(is_reading, buff, &input->tick, sizeof(u32));
		if (!WyncState_serialize(is_reading, buff, &input->state))
			{ return false; }
	}
	return true;
}

typedef struct {
	u32 dummy;
} WyncPktJoinReq;

static bool WyncPktJoinReq_serialize (
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktJoinReq *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->dummy, sizeof(u32));
	return true;
}

typedef struct {
	bool approved;
	i32 wync_client_id; // -1
} WyncPktJoinRes;

static bool WyncPktJoinRes_serialize (
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktJoinRes *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->approved, sizeof(bool));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->wync_client_id, sizeof(i32));
	return true;
}

typedef struct {
	u32 dummy;
} WyncPacketReqClientInfo;

typedef struct {
	u16 peer_id;
	//var entity_id: int # to validate the entity id exists, unused?
	u32 prop_id;
} WyncPktResClientInfo; // TODO: Rename

static bool WyncPktResClientInfo_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktResClientInfo *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->peer_id, sizeof(u16));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->prop_id, sizeof(u32));
	return true;
}

typedef struct {
	u32 prop_id;
	WyncState data;
} WyncSnap;

static bool WyncSnap_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncSnap *snap
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &snap->prop_id, sizeof(u32));
	if (!WyncState_serialize(is_reading, buffer, &snap->data))
		{ return false; }
	return true;
}
static void WyncSnap_free(WyncSnap *snap) {
	WyncState_free(&snap->data);
}

typedef struct {
	u32 tick;
	u16 snap_amount;
	WyncSnap *snaps;
} WyncPktSnap;

static bool WyncPktSnap_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktSnap *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(
			is_reading, buffer, &pkt->tick, sizeof(u32));
	NETEBUFFER_BYTES_SERIALIZE(
			is_reading, buffer, &pkt->snap_amount, sizeof(u16));
	if (is_reading) {
		// TODO: limit
		pkt->snaps = (WyncSnap*) 
			calloc(sizeof(WyncSnap), pkt->snap_amount);
	}
	for (u16 i = 0; i < pkt->snap_amount; ++i) {
		if (!WyncSnap_serialize(is_reading, buffer, &pkt->snaps[i])) {
			return false;
		}
	}
	return true;
}
static void WyncPktSnap_free(WyncPktSnap *pkt) {
	for (u16 i = 0; i < pkt->snap_amount; ++i) {
		WyncSnap_free(&pkt->snaps[i]);
	}
	free(pkt->snaps);
	pkt->snaps = NULL;
	pkt->snap_amount = 0;
}

typedef struct {
	u16 entity_amount;

	u32 *entity_ids;
	u16 *entity_type_ids;
	u32 *entity_prop_id_start; // authoritative prop id range
	u32 *entity_prop_id_end;
	WyncState *entity_spawn_data;

} WyncPktSpawn;

static void WyncPktSpawn_calloc(WyncPktSpawn *pkt, u16 size) {
	pkt->entity_amount = size;
	pkt->entity_ids = (u32*) calloc(sizeof(u32), size);
	pkt->entity_type_ids = (u16*) calloc(sizeof(u16), size);

	pkt->entity_prop_id_start = (u32*) calloc(sizeof(u32), size);
	pkt->entity_prop_id_end = (u32*) calloc(sizeof(u32), size);
	pkt->entity_spawn_data = (WyncState*) calloc(sizeof(WyncState), size);
}
static void WyncPktSpawn_free(WyncPktSpawn *pkt) {
	if (pkt->entity_ids != NULL) free(pkt->entity_ids);
	if (pkt->entity_type_ids != NULL) free(pkt->entity_type_ids);
	if (pkt->entity_prop_id_start != NULL) free(pkt->entity_prop_id_start);
	if (pkt->entity_prop_id_end != NULL) free(pkt->entity_prop_id_end);

	for (u32 i = 0; i < pkt->entity_amount; ++i) {
		WyncState data = pkt->entity_spawn_data[i];
		WyncState_free(&data);
	}

	if (pkt->entity_spawn_data != NULL) free(pkt->entity_spawn_data);
	pkt->entity_amount = 0;
}
/// Allocates memory when reading
///
/// @returns error, if failed, manually free
static bool WyncPktSpawn_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktSpawn *pkt
) {
	u16 entity_amount = pkt->entity_amount;

	if (is_reading) {
		NETEBUFFER_BYTES_SERIALIZE(is_reading,
			buffer, &pkt->entity_amount, sizeof(u16));
		entity_amount = pkt->entity_amount;
	} else {
		NETEBUFFER_BYTES_SERIALIZE(is_reading,
			buffer, &entity_amount, sizeof(u16));
	}

	if (is_reading){
		// TODO: set limit
		WyncPktSpawn_calloc(pkt, entity_amount);
	}

	#define WYNC_PKT_SPAWN_EASY_LOOP (u16 i = 0; i < entity_amount; ++i)

	for WYNC_PKT_SPAWN_EASY_LOOP { NETEBUFFER_BYTES_SERIALIZE
		(is_reading, buffer, &pkt->entity_ids[i], sizeof(u32));
	}
	for WYNC_PKT_SPAWN_EASY_LOOP { NETEBUFFER_BYTES_SERIALIZE
		(is_reading, buffer, &pkt->entity_type_ids[i], sizeof(u16));
	}
	for WYNC_PKT_SPAWN_EASY_LOOP { NETEBUFFER_BYTES_SERIALIZE
		(is_reading, buffer, &pkt->entity_prop_id_start[i], sizeof(u32));
	}
	for WYNC_PKT_SPAWN_EASY_LOOP { NETEBUFFER_BYTES_SERIALIZE
		(is_reading, buffer, &pkt->entity_prop_id_end[i], sizeof(u32));
	}
	for WYNC_PKT_SPAWN_EASY_LOOP {
		if (!WyncState_serialize(is_reading, buffer, &pkt->entity_spawn_data[i])) {
			return false;
		}
	}
	#undef WYNC_PKT_SPAWN_EASY_LOOP

	return true;
}

/// allocates when reading
///
/// @param pkt Must be an instance
static bool WyncPktEventData_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktEventData *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading,
		buffer, &pkt->event_amount, sizeof(u16));

	if (is_reading) {
		pkt->events = (WyncPktEventData_EventData*)
			calloc(sizeof(WyncPktEventData_EventData), pkt->event_amount);
	}

	for (u32 i = 0; i < pkt->event_amount; ++i)
	{
		WyncPktEventData_EventData *event_data = &pkt->events[i];

		NETEBUFFER_BYTES_SERIALIZE(is_reading,
			buffer, &event_data->event_id, sizeof(u32));
		NETEBUFFER_BYTES_SERIALIZE(is_reading,
			buffer, &event_data->event_type_id, sizeof(u32));
		if (!WyncState_serialize(is_reading, buffer, &event_data->data)) {
			return false;
		}
	}

	return true;
}

static uint WyncEventList_get_size (WyncEventList *list) {
	return sizeof(u32) * (1 + list->event_amount);
}

static void WyncEventList_free (WyncEventList *list){
	list->event_amount = 0;
	free(list->event_ids);
	list->event_ids = NULL;
}

static bool WyncEventList_serialize(
	bool is_reading,
	NeteBuffer *buffer,
	WyncEventList *list
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading,
		buffer, &list->event_amount, sizeof(u32));
	if (is_reading) {
		list->event_ids = (u32*) malloc(sizeof(u32) * list->event_amount);
	}
	for (u32 i = 0; i < list->event_amount; ++i) {
		NETEBUFFER_BYTES_SERIALIZE(is_reading,
			buffer, &list->event_ids[i], sizeof(u32));
	}
	return true;
}


// ============================================================
//
// CONTAINERS
//
// ============================================================



#ifndef i32_DA_H
#define i32_DA_H
#define DYN_ARR_TYPE i32
#define DYN_ARR_PREFIX i32_
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX
#endif // !i32_DA_H

#define DYN_ARR_TYPE u32
#define DYN_ARR_PREFIX u32_
#define DYN_ARR_ENABLE_SORT
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define DYN_ARR_TYPE Wync_PeerPropPair
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define DYN_ARR_TYPE Wync_PeerEntityPair
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define DYN_ARR_TYPE WyncSnap
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define DYN_ARR_TYPE WyncPacketOut
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define DYN_ARR_TYPE WyncTickDecorator
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX

#define RINGBUFFER_TYPE u32_DynArr
#undef  RINGBUFFER_PREFIX
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define RINGBUFFER_TYPE i32
#define RINGBUFFER_PREFIX i32_
#define RINGBUFFER_ENABLE_SORT
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define RINGBUFFER_TYPE u32
#define RINGBUFFER_PREFIX u32_
#define RINGBUFFER_ENABLE_SORT
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define RINGBUFFER_TYPE double
#undef RINGBUFFER_PREFIX
#define RINGBUFFER_ENABLE_SORT
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define RINGBUFFER_TYPE WyncState
#undef RINGBUFFER_PREFIX
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define RINGBUFFER_TYPE ConMap
#undef RINGBUFFER_PREFIX
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define MAP_GENERIC_TYPE u32_DynArr
#undef MAP_GENERIC_PREFIX
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define MAP_GENERIC_TYPE Wync_DummyProp
#define MAP_GENERIC_PREFIX DummyProp_
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define MAP_GENERIC_TYPE WyncState
#undef MAP_GENERIC_PREFIX
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define MAP_GENERIC_TYPE WyncEvent
#undef MAP_GENERIC_PREFIX
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define MAP_GENERIC_TYPE EntitySpawnPropRange
#undef MAP_GENERIC_PREFIX
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define FIFORING_TYPE Wync_EntitySpawnEvent
#define FIFORING_PREFIX SpawnEvent_
#define FIFORING_DISABLE_COMPARISONS
#include "containers/fiforing.h"
#undef FIFORING_TYPE
#undef FIFORING_PREFIX

#ifndef u32_FIFORING_H
#define u32_FIFORING_H
#define FIFORING_TYPE u32
#define FIFORING_PREFIX u32_
#include "containers/fiforing.h"
#undef FIFORING_TYPE
#undef FIFORING_PREFIX
#endif // !u32_FIFORING_H

#define FIFOMAP_TYPE u32
#define FIFOMAP_PREFIX u32_
#include "containers/fifomap.h"
#undef FIFOMAP_TYPE
#undef FIFOMAP_PREFIX

#define MAP_GENERIC_TYPE u32
#define MAP_GENERIC_KEY_TYPE WyncName
#define MAP_GENERIC_PREFIX Name_
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

#define RINGBUFFER_TYPE Name_ConMap
#undef RINGBUFFER_PREFIX
#include "containers/ringbuffer.h"
#undef RINGBUFFER_TYPE
#undef RINGBUFFER_PREFIX

#define MAP_GENERIC_TYPE WyncBlueprintHandler
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX
#include "containers/map_generic.h"
#undef MAP_GENERIC_TYPE
#undef MAP_GENERIC_KEY_TYPE
#undef MAP_GENERIC_PREFIX

// ============================================================
//
// WYNC PROP
//
// ============================================================



// State buffer
// --------------------------------------------------------

typedef struct {
	bool just_received_new_state;

	// Unified
	// states           <state_id, state>
	// statebff.state_id_to_tick <state_id, tick>
	// statebff.tick_to_state_id <tick, state_id>
	// statebff.state_id_to_local_tick <state_id, local_tick>
	
	// RingBuffer<int, Variant>
	WyncState_RinBuf saved_states;
	// RingBuffer<int, int>
	i32_RinBuf state_id_to_tick;
	// RingBuffer<int, int>
	i32_RinBuf tick_to_state_id;
	// RingBuffer<int, int> (only for lerping)
	i32_RinBuf state_id_to_local_tick;
	
	// Note. On predicted entities only the latest value is valid
	// Last-In-First-Out (LIFO)
	// LIFO Queue <arrival_order: int, server_tick: int>
	i32_RinBuf last_ticks_received;
} WyncProp_StateBuffer;


// Extrapolation / Prediction
// --------------------------------------------------------

typedef struct {
	Wync_NetTickData pred_curr;
	Wync_NetTickData pred_prev;
} WyncProp_Xtrap;


// Lerping
// --------------------------------------------------------

typedef struct {

	// For lerping and 'subtick timewarp', don't use for checking prop data type on
	// any other prop
	u16 lerp_user_data_type;
	
	bool lerp_ready;
	bool lerp_use_confirmed_state;
	
	// Precalculate which ticks we're gonna be interpolating between
	// Q: Why store specific tick range instead of using current one
	// A: To support varying update rates per prop
	// Q: Why store state copy?
	// A: To allow the state buffer to fill and be replaced
	
	i32 lerp_left_local_tick;
	i32 lerp_right_local_tick;
	
	i32 lerp_left_canon_tick;
	i32 lerp_right_canon_tick;
	
	WyncState lerp_left_state;
	WyncState lerp_right_state;
} WyncProp_Lerp;


// Relative Synchronization
// --------------------------------------------------------
// Auxiliar props don't get this struct

typedef struct {
	
	// What blueprint does this prop obeys? This dictates relative sync events
	u16 delta_blueprint_id;
	
	// if relative_sync_enabled: points to auxiliar prop
	// if is_auxiliar_prop: points to delta prop
	//u32 auxiliar_delta_events_prop_id;
	
	// A place to insert current delta events
	u32_DynArr current_delta_events;
	
	// A place to insert current undo delta events
	u32_DynArr current_undo_delta_events;

	// Undo events: Only for Prediction and Timewarp
	// --------------------------------------------------------

	// Rela Aux: here we store undo event_ids
	// Ring <tick: id, data: Array[int]>
	u32_DynArr_RinBuf confirmed_states_undo;
	
	// Rela Aux: what tick corresponds to any saved state
	i32_RinBuf confirmed_states_undo_tick;

} WyncProp_Rela;

// Event consumed module
// --------------------------------------------------------

typedef struct {
	// * Note: Currently, if there are duplicated events on a single tick,
	//   only once instance will be executed
	// * Stores the consumed events for the last N ticks
	// * Used mainly for the server to consume late client events
	// Ring <tick: id, event_ids: Array[int]>
	u32_DynArr_RinBuf events_consumed_at_tick;
	i32_RinBuf events_consumed_at_tick_tick;
} WyncProp_Consumed;


typedef struct {
	// Future: Create pseudo-ECS for prop components

	bool enabled;

	char name_id[64];
	enum WYNC_PROP_TYPE prop_type;

	bool lerp_enabled;
	bool xtrap_enabled;
	bool relative_sync_enabled;
	bool consumed_events_enabled;
	bool timewarp_enabled; // (server-side) will keep state history

	bool is_auxiliar_prop;
	// if relative_sync_enabled: points to auxiliar prop
	// if is_auxiliar_prop: points to delta prop
	u32 auxiliar_delta_events_prop_id; // -1

	WyncProp_StateBuffer statebff;
	WyncProp_Lerp        co_lerp;
	WyncProp_Xtrap       co_xtrap;
	WyncProp_Rela        co_rela;
	WyncProp_Consumed    co_consumed;
} WyncProp;


#define DYN_ARR_TYPE WyncProp
#undef  DYN_ARR_PREFIX
#include "containers/da.h"
#undef DYN_ARR_TYPE
#undef DYN_ARR_PREFIX



// ============================================================
//
// WYNC CONTEXT
//
// ============================================================


#define SERVER_TICK_OFFSET_COLLECTION_SIZE 4

#define RELIABLE true
#define UNRELIABLE false
#define OK_BUT_COULD_NOT_FIT_ALL_PACKETS 1
#define SERVER_PEER_ID 0

#define ENTITY_ID_GLOBAL_EVENTS 700
// NOTE: Rename to PRED_INPUT_BUFFER_SIZE
#define INPUT_BUFFER_SIZE 1024 // 2 ** 10
#define INPUT_AMOUNT_TO_SEND 20     // TODO: Make configurable

#define MAX_PROPS 4096              // default to 2**16 (65536)
#define MAX_DUMMY_PROP_TICKS_ALIVE 100 // 1000
#define SERVER_TICK_RATE_SLIDING_WINDOW_SIZE 8
#define ENTITY_ID_PROB_FOR_ENTITY_UPDATE_DELAY_TICKS 699
#define MAX_CHANNELS 8


struct WyncWrapperCtx;

typedef struct {
	u32 ticks;

	// for a "debug_tick_offset" equivalent just set ctx.co_ticks.common.ticks
	// to any value
	u64 debug_time_offset_ms;

	// --------------------------------------------------------
	// Wrapper
	// --------------------------------------------------------
	// Only populated if in C/C++; other languages require
	// a separate wrapper.


	//WyncWrapperCtx wrapper;


	// --------------------------------------------------------
	// General Settings
	// --------------------------------------------------------
	
	u16 physic_ticks_per_second; // default 60

	bool was_any_prop_added_deleted;


	// --------------------------------------------------------
	// Outgoing packets
	// --------------------------------------------------------

	// can be used to limit the production of packets
	i32 out_packets_size_limit;
	i32 out_packets_size_remaining_chars;
	WyncPacketOut_DynArr out_reliable_packets;
	WyncPacketOut_DynArr out_unreliable_packets;
	WyncPacketOut_DynArrIterator rel_pkt_it;
	WyncPacketOut_DynArrIterator unrel_pkt_it;


	// --------------------------------------------------------
	// Peer Management
	// --------------------------------------------------------
	
	u16 max_peers; // default 4
	
	// peer[0] = -1: it's reserved for the server
	// List<wync_peer_id: int, nete_peer_id: int> // NOTE: Should be Ring
	i32_DynArr peers;
	i32_DynArrIterator active_peers_it;
	
	// Array[12] <peer_id: int, PeerLatencyInfo>
	Wync_PeerLatencyInfo *peer_latency_info;
	
	// Note: Might want to merge with PeerLatencyInfo
	// Stores client metadata
	// Array<client_id: int, WyncClientInfo>
	Wync_ClientInfo *client_has_info;
	
	bool is_client;
	
	i32 my_peer_id; // default -1
	i32 my_nete_peer_id; // default -1
	
	// --------------------------------------------------------
	// Client Only??
	// --------------------------------------------------------
	
	bool connected;
	bool prev_connected;


	// --------------------------------------------------------
	// Inputs / Events
	// --------------------------------------------------------
	
	// it could be useful to have a different value for server cache
	u16 max_amount_cache_events; // default 1024
	
	// set to 1 to see if it's working alright 
	u16 max_prop_relative_sync_history_ticks; // default 20 
	// almost two seconds TODO: separate variables per client/server
	u16 max_age_user_events_for_consumption; // default 120 

} Wync_CoCommon;

typedef struct {

	/// how many common.ticks in the past to keep state cache for a regular prop
	u16 REGULAR_PROP_CACHED_STATE_AMOUNT; // default 8
	
	// Map<wync_entity_id: int, unused_value: int>
	// Set<int>
	ConMap tracked_entities;
	
	u32 prop_id_cursor;
	
	// Array<prop_id: int, WyncProp>
	WyncProp *props;
	
	// SizedBufferList[int]
	// Set[int]
	// TODO: Proper set type, TODO: Use u32_ConMap instead
	ConMap active_prop_ids; 
	
	// Map<entity_id: int, Array<prop_id>>
	// TODO: Make it a map of sets
	u32_DynArr_ConMap entity_has_props;
	
	// User defined types, so that they can know what data types to sync
	// Map<entity_id: int, entity_type_id: int>
	// TODO: Rename to make clear this is user data
	ConMap entity_is_of_type;
	
	// used to know two things:
	// 1. If a player has received info about this prop before
	// 2. What was the Last tick we sent prop data (relative sync event) to a
	//    client
	//    (Counts for both fullsnapshot and _delta events_)
	//
	// ctx.co_track.client_has_relative_prop_has_last_tick can only be modified
	// on these co_events.events:
	// 1. (On server) The server sends a fullsnapshot aka 'base state'
	// 2. (On server) The client notifies the server about it
	//    (WYNC_PKT_DELTA_PROP_ACK)
	// 3. (On client) When we receive a fullsnapshot
	// 4. (On client) When we confidently apply a delta event forward
	//
	// Array[12] < client_id: int, Map<prop_id: int, tick: int> >
	ConMap *client_has_relative_prop_has_last_tick;
	// each 10 frames and on prop creation. check for initialization, TODO: Where?
	// each 1 frame. use it to send needed state

} CoStateTrackingCommon;

typedef struct {
	// Map<client_id: int, prop_id: Array[int]>
	// TODO: change to array
	ConMap *client_owns_prop;
	
	bool client_ownership_updated;
} CoClientAuthority;

typedef struct {
	// (client only)
	// Dummy Props: Used to hold state for a prop not yet present;
	// If too much time passes it will be discarded
	
	// Map <prop_id: int, DummyProp*>
	DummyProp_ConMap dummy_props;
	
	// Stat
	u32 stat_lost_dummy_props;
} CoDummyProps;

typedef struct {
	bool was_any_prop_added_deleted;
	u32_DynArr filtered_clients_input_and_event_prop_ids;
	u32_DynArr filtered_delta_prop_ids; // client & server
	u32_DynArr filtered_regular_extractable_prop_ids;
	// either interpolable or not
	u32_DynArr filtered_regular_timewarpable_prop_ids; 
	// to easily do subtick timewarp
	u32_DynArr filtered_regular_timewarpable_interpolable_prop_ids; 
} CoFilterServer;

typedef struct {
	u32_DynArr type_input_event__owned_prop_ids;
	u32_DynArr type_input_event__predicted_owned_prop_ids;
	u32_DynArr type_event__predicted_prop_ids;
	u32_DynArr type_state__delta_prop_ids;
	u32_DynArr type_state__predicted_delta_prop_ids;
	u32_DynArr type_state__predicted_regular_prop_ids;
	u32_DynArr type_state__interpolated_regular_prop_ids;
	// co_track.props that just received new state
	u32_DynArr type_state__newstate_prop_ids;
} CoFilterClient;

typedef struct {
	// I.e. When entities get added or removed
	
	// * Spawn data is only cleared on 'entity untrack'
	// Q: One may want to update this spawn data as the entity evolves over time
	// A: No, Spawn data should be small metadata just to setup an entity,
	//    big data use _delta props_
	// Map <entity_id: int, data: Variant>
	WyncState_ConMap entity_spawn_data;
	
	// User facing variable
	// Client only
	// FIFO<SpawnEvent>
	// Maybe a dynamic FIFORing would be better?
	SpawnEvent_FIFORing out_queue_spawn_events;
	
	// User must call get_next_entity
	// Cleared each tick
	//Wync_EntitySpawnEvent *next_entity_to_spawn;
	
	// Internal list
	// Map <entity_id: int, Tripla[prop_start: int, prop_end: int, curr: int]
	EntitySpawnPropRange_ConMap pending_entity_to_spawn_props;
	
	// Despawned entities
	// List<entity_id: int>
	u32_DynArr despawned_entity_ids;
} CoSpawn;


typedef struct {
	// TODO: Update
	// Sync priorities:
	// * Spawning
	// * Despawning
	// * Queue
	
	
	// * Only add/remove _entity ids_ when a packet is confirmed sent
	//   (WYNC_EXTRACT_WRITE)
	// * Confirmed list of entities the client sees
	// Array <client_id: int, Set[entity_id: int]>
	ConMap *clients_sees_entities;
	
	// Guarantee that all entities here can be spawned...
	// * Every frame we check.. 
	// Array <client_id: int, Set[entity_id: int]>
	ConMap *clients_sees_new_entities;
	
	// Array <client_id: int, Set[entity_id: int]>
	ConMap *clients_no_longer_sees_entities;
	
	// Queues
	// vvv
	
	// * Only refill the queue once it's emptied
	// * Queue entities for eventual synchronization
	// Array <client_id: int, FIFORing[entity_id: int]>
	u32_FIFORing *queue_clients_entities_to_sync;
	
	// Here, add what entities where synced last frame and to which client_id
	// Array <client_id: int, Set[entity_id: int]>
	ConMap *entities_synced_last_time;
	
	// * Recomputed each tick we gather out packets
	// * TODO: Use FIFORing and preallocate all instances (pooling)
	// * TODO: Use DynArr
	// FIFORing < PeerEntityPair[peer: int, entity: int] > [100]
	Wync_PeerEntityPair_DynArr queue_entity_pairs_to_sync;
	
	// * Used to reduce state extraction to only the co_track.props requested
	// Simple dynamic array list
	u32_DynArr rela_prop_ids_for_full_snapshot;
	Wync_PeerPropPair_DynArr pending_rela_props_to_sync_to_peer;
	
	// setup new connected peer
	// Array <order: int, nete_peer_id: int>
	u32_DynArr out_peer_pending_to_setup;
	u32_DynArrIterator pending_peers_it;
	
	// temporary snapshot cache for later packet size optimization
	// Array < client_id: int, DynArr < WyncPacket > >
	WyncSnap_DynArr *clients_cached_reliable_snapshots;
	WyncSnap_DynArr *clients_cached_unreliable_snapshots;
	
	// Array<client_id: int, ordered_set<event_id> >
	ConMap *peers_events_to_sync; // Array[Dictionary]
} CoThrottling; // or Relative Synchronization

typedef struct {
	// TODO: Separate generated co_events.events from CACHED co_events.events
	// Map<event_id: uint, WyncEvent>
	WyncEvent_ConMap events; // Dictionary[int, WyncCtx.WyncEvent]
	
	u32 event_id_counter;
	
	// 24 clients, 12 channels, unlimited event ids
	// Array[24 clients]< Array[12 channels] < Dictionary <int, unused_bool> > >
	// co_events.events can be Dictionary (non-repeating set) or Array (allows
	// duplicates)
	// Array[Array[Array[int]]]
	u32_DynArr (*peer_has_channel_has_events)[MAX_CHANNELS]; // Array[Array]
	
	// TODO: Rename all the table-like containers
	// Array< peer_id: int, Array< channel: int, prop_id: prop_id > >
	u32 (*prop_id_by_peer_by_channel)[MAX_CHANNELS]; // Array[Array]
	
	
	// --------------------------------------------------------
	// Event caching
	// --------------------------------------------------------
	
	// FIFOMap <event_data_hash: int, event_id: int>
	u32_FIFOMap events_hash_to_id;

	// Set <event_id: int>
	//var events_sent: FIFOMap = FIFOMap.new()
	// peers_events_sent: Array< peer_id: int, RingSet <event_id> >
	u32_FIFOMap *to_peers_i_sent_events; // Array[FIFOMap]
	
	// how far in the past we can go
	// Updated every frame, should correspond to current_tick - max_history_ticks
	i32 delta_base_state_tick; // -1
} CoEvents;


typedef struct {
	// Timestamp when WyncCtx was initialized
	u64 start_time_ms;

	u32 server_ticks;

	// Strategy for getting a stable server_tick_offset value:
	// We have a list of of common values and their count
	// value | percentage
	// -199  | 212
	// -201  | 98
	// -202  | 13
	// Then we just pick the most common one, if we encounter
	// a new value just replace the one with less count. Also, there shouldn't
	// be fight between two adyacent values (e.g. -199 & -200) because the
	// code for picking a value prevents fluctuation of one unit

	// Strategy for more accurate stable latency calculation.
	// TODO: Since the main use of this stable latency is to convert this time
	// into the equivalente common.ticks, then better to constantly update stable 
	// latency and use the previous strategy to slowly update the tick number.
	// Trying to have the tick be always bigger (ceil).
	// TLDR: stabilize tick amount instead of latency.
	
	// List<Tuple<int, int>>
	// Array[Array[Variant]]
	Wync_i32Pair *server_tick_offset_collection;
	i32 server_tick_offset;

	// TODO: Move this elsewhere
	// used to (1) lerp and (2) time warp
	float lerp_delta_accumulator_ms;
	u32 last_tick_rendered_left;
	float minimum_lerp_fraction_accumulated_ms;

} CoTicks; // Clock?


typedef struct {

	// --------------------------------------------------------
	// Predicted Clock / Timing
	// --------------------------------------------------------
	
	// target_time -> curr_time + tick_offset * fixed_step
	i32 tick_offset;
	i32 tick_offset_prev;
	i32 tick_offset_desired;
	i32 target_tick; // co_ticks.common.ticks + tick_offset
	// fixed timestamp for current tick
	// It's the point of reference for other common.ticks
	float current_tick_timestamp;
	
	// For calculating clock_offset_mean
	// TODO: Move this to co_ticks
	
	double_RinBuf clock_offset_sliding_window;
	u16 clock_offset_sliding_window_size; // default 16
	double clock_offset_mean;


	// --------------------------------------------------------
	// Extrapolation / Prediction
	// --------------------------------------------------------
	
	// last tick received from the server
	i32 last_tick_received;
	
	bool currently_on_predicted_tick;
	i32 current_predicted_tick; // only for debugging
	
	// tick markers for the prev prediction cycle
	// TODO: Rename, the actual first tick is = first_tick - threeshold
	i32 first_tick_predicted; 
	i32 last_tick_predicted;
	// markers for the current prediction cycle
	i32 pred_intented_first_tick;
	
	// user facing variable, tells the user which entities are safe to predict
	// this tick
	u32_DynArr global_entity_ids_to_predict;
	
	// how many common.ticks before 'last_tick_received' to predict to compensate for
	// throttling
	// * Limited by co_track.REGULAR_PROP_CACHED_STATE_AMOUNT
	// TODO: rename
	i32 max_prediction_tick_threeshold;
	
	// to know if we should extrapolate from the beggining (last received tick)
	// or continue (this implies not getting packets)
	// TODO: rename
	
	u32 last_tick_received_at_tick;
	u32 last_tick_received_at_tick_prev;
	
	
	ConMap entity_last_predicted_tick; // Dictionary[int, int] = {}
	ConMap entity_last_received_tick; // Dictionary[int, int] = {}
	u32_DynArr predicted_entity_ids; // Array[int] = []
	
	
	// --------------------------------------------------------
	// Single time predicted Action
	// --------------------------------------------------------
	// E.g. shooting sound
	
	// This size should be the maximum amount of 'tick_offset' for prediction
	u16 tick_action_history_size; // default 32
	
	// did action already ran on tick?
	// Ring < predicted_tick: int, Set <action_id: String> >
	// RingBuffer < tick: int, Dictionary <action_id: String, unused_bool: bool> >
	// RingBuffer [ Dictionary ]
	Name_ConMap_RinBuf tick_action_history;
} CoPredictionData;

typedef struct {
	
	u16 lerp_ms; // default 50
	u16 lerp_latency_ms;

	// MAYBEDO: use ms as magnitud
	float max_lerp_factor_symmetric;

	// used to (1) lerp and (2) time warp
	float lerp_delta_accumulator_ms;
	u32 last_tick_rendered_left;
	float minimum_lerp_fraction_accumulated_ms;
} CoLerp;


// amount of props, also 0 is reserved for 'total'
#define DEBUG_PACKETS_RECEIVED_MAX 20

typedef struct {
	// : Array <packet_id:int, Array <prop_id:int, amount: int> >
	// : Array[Array[int]]
	u32 *debug_packets_received[WYNC_PKT_AMOUNT];
	
	// mean of how much data is being transmitted each tick
	u32 debug_data_per_tick_sliding_window_size; // defaults to 8
	u32_RinBuf debug_data_per_tick_sliding_window;
	float debug_data_per_tick_total_mean;
	float debug_data_per_tick_sliding_window_mean;
	float debug_data_per_tick_current;
	u32 debug_ticks_sent;
	
	float debug_lerp_prev_curr_time;
	float debug_lerp_prev_target;
	
	
	// (client only)
	// it's described in common.ticks between receiving updates from the server
	// So, 3 would mean it's 1 update every 4 common.ticks. 0 means updates
	// every tick.
	float server_tick_rate;
	u32_RinBuf server_tick_rate_sliding_window;
	u32 tick_last_packet_received_from_server;
	u32 local_tick_last_packet_received_from_server;

	float snap_tick_delay_mean;
	u32_RinBuf snap_tick_delay_window;

	
	// (client only)
	// it's described in common.ticks between receiving updates from the server
	// So, 3 would mean it's 1 update every 4 common.ticks. 0 means updates
	// every tick.
	double low_priority_entity_update_rate;
	i32_RinBuf low_priority_entity_update_rate_sliding_window;
	u32 low_priority_entity_update_rate_sliding_window_size;
	u32 low_priority_entity_tick_last_update;
	u32 PROP_ID_PROB;
} CoMetrics;

struct WyncCtx {
	Wync_CoCommon common;
	struct WyncWrapperCtx *wrapper;

	CoStateTrackingCommon co_track;
	CoEvents co_events;
	CoClientAuthority co_clientauth;
	CoMetrics co_metrics;
	CoSpawn co_spawn;

	// Server only

	CoThrottling co_throttling;
	CoFilterServer co_filter_s;

	// Client only

	CoTicks co_ticks;
	CoPredictionData co_pred;
	CoLerp co_lerp;
	CoDummyProps co_dummy;
	CoFilterClient co_filter_c;

	// Misc

	bool initialized;

	// --------------------------------------------------------
	// Server Settings
	// --------------------------------------------------------

	// --------------------------------------------------------
	// Timewarp
	// --------------------------------------------------------
	// must be a power of two, 64 ~= 1 second at 60 tps
	u32 max_tick_history_timewarp;

	// --------------------------------------------------------
	// Client Settings
	// --------------------------------------------------------
	// ...
	
};

#endif // !WYNC_CTX_H
