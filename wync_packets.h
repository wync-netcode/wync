#ifndef WYNC_PACKETS_H
#define WYNC_PACKETS_H

#include "stdlib.h"
#include "buffer.h"
#include "macro_types.h"

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

typedef struct {
	enum WYNC_PKT packet_type_id;
	u32 data_size;
	void *data;
} WyncPacket;

bool WyncPacket_write(NeteBuffer *buffer, WyncPacket *pkt) {
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->packet_type_id, sizeof(u16));
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->data_size, sizeof(u32));
	NETEBUFFER_WRITE_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
bool WyncPacket_read(NeteBuffer *buffer, WyncPacket *pkt) {
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->packet_type_id, sizeof(pkt->packet_type_id));
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->data_size, sizeof(pkt->data_size));

	// TODO: must set a limit on the size
	pkt->data = calloc(sizeof(char), pkt->data_size);

	NETEBUFFER_READ_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
void WyncPacket_free(WyncPacket *pkt) {
	if (pkt->data != NULL) free(pkt->data);
}


// Packets handed to you for sending through the network
// it includes the destination nete peer

typedef struct {
	u16 to_nete_peer_id;
	u32 data_size;
	void *data; // WyncPacket
} WyncPacketOut;

bool WyncPacketOut_write(NeteBuffer *buffer, WyncPacketOut *pkt) {
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->to_nete_peer_id, sizeof(u16));
	NETEBUFFER_WRITE_BYTES(buffer, &pkt->data_size, sizeof(u32));
	NETEBUFFER_WRITE_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
bool WyncPacketOut_read(NeteBuffer *buffer, WyncPacketOut *pkt) {
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->to_nete_peer_id, sizeof(pkt->to_nete_peer_id));
	NETEBUFFER_READ_BYTES
		(buffer, &pkt->data_size, sizeof(pkt->data_size));

	pkt->data = calloc(sizeof(char), pkt->data_size);

	NETEBUFFER_READ_BYTES(buffer, pkt->data, pkt->data_size);
	return true;
}
void WyncPacketOut_free(WyncPacketOut *pkt) {
	if (pkt->data != NULL) free(pkt->data);
}

typedef struct {
	i32 lerp_ms;
} WyncPktClientSetLerpMS;

// what happens if it's expensive to know the size?

bool WyncPktClientSetLerpMS_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktClientSetLerpMS i
) {
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.lerp_ms, sizeof(i.lerp_ms)))
		{ return false; }

	return true;
}

typedef struct {
	u32 tick; // answerer's tick
	u32 time; // answerer's time
	u32 tick_og; // requester's tick
	u32 time_og; // requester's time
} WyncPktClock;

bool WyncPktClock_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktClock i
) {
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.tick, sizeof(i.tick)))
		{ return false; }
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.time, sizeof(i.time)))
		{ return false; }
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.tick_og, sizeof(i.tick_og)))
		{ return false; }
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.time_og, sizeof(i.time_og)))
		{ return false; }
	return true;
}

typedef struct {
	// * Client -> Server
	// * The client notifies the server what was the last update (tick) received
	//   for all relative props that he knows about.
	// * Send this aprox every 3 seconds

	u32 prop_amount;
	u32 *delta_prop_ids;
	u32 *last_tick_received;
} WyncPktDeltaPropAck;

bool WyncPktDeltaPropAck_serialize (
	bool is_reading, NeteBuffer *buffer, WyncPktDeltaPropAck i
) {
	if (!NeteBuffer_bytes_serialize
		(is_reading, buffer, &i.prop_amount, sizeof(i.prop_amount)))
		{ return false; }

	if (is_reading) {
		i.delta_prop_ids = (u32*)calloc(sizeof(*i.delta_prop_ids), i.prop_amount);
		i.last_tick_received = (u32*)calloc(sizeof(*i.delta_prop_ids), i.prop_amount);
	}

	for (u32 k = 0; k < i.prop_amount; ++k) {
		if (!NeteBuffer_bytes_serialize
			(is_reading, buffer, i.delta_prop_ids + k, sizeof(*i.delta_prop_ids)))
			{ goto WyncPktDeltaPropAck_defer; }
	}
	for (u16 k = 0; k < i.prop_amount; ++k) {
		if (!NeteBuffer_bytes_serialize
			(is_reading, buffer, i.last_tick_received + k, sizeof(*i.last_tick_received)))
			{ goto WyncPktDeltaPropAck_defer; }
	}
	return true;

	WyncPktDeltaPropAck_defer:
	if (is_reading) {
		free(i.delta_prop_ids);
		free(i.last_tick_received);
	}
	return false;
}

/*
func duplicate() -> WyncPktDeltaPropAck:
	var i = WyncPktDeltaPropAck.new()
	i.prop_amount = prop_amount
	i.delta_prop_ids = delta_prop_ids.duplicate(true)
	i.last_tick_received = last_tick_received.duplicate(true)
	return i
   */

typedef struct {
	u32 entity_amount;
	u32 *entity_ids;
} WyncPktDespawn;

/*
func _init(size) -> void:
	entity_amount = size
	entity_ids.resize(size)

func duplicate() -> WyncPktDespawn:
	var i = WyncPktDespawn.new(entity_amount)
	i.entity_amount = entity_amount
	i.entity_ids = entity_ids.duplicate(true)
	return i
   */

// NOTE: EventData is different from WyncEvent in that this one is sent
// over the network, so it has an extra property: event_id: int
typedef struct {
	u32 event_id;
	u32 event_type_id;
	u32 data_size;
	void *event_data;
} WyncPktEventData_EventData;

/*
	func duplicate() -> EventData:
		var newi = EventData.new()
		newi.event_id = event_id
		newi.event_type_id = event_type_id
		newi.event_data = WyncMisc.duplicate_any(event_data)
		return newi
   */

typedef struct {
	u32 events_amount;
	WyncPktEventData_EventData *events;
} WyncPktEventData;

/*
func duplicate() -> WyncPktEventData:
	var newi = WyncPktEventData.new()
	newi.events = [] as Array[EventData]
	for event in self.events:
		newi.events.append(event.duplicate())
	return newi
   */

typedef struct {
	u32 tick;
	u32 data_size;
	void *data;
} Wync_NetTickDataDecorator;

/*
	func duplicate() -> NetTickDataDecorator:
		var newi = NetTickDataDecorator.new()
		newi.tick = tick
		newi.data = WyncMisc.duplicate_any(data)
		return newi
   */

typedef struct {
	u32 prop_id;
	u32 amount;
	Wync_NetTickDataDecorator *inputs;
} WyncPktInputs;

void WyncPktInputs_free (WyncPktInputs pkt) {
	Wync_NetTickDataDecorator *input;
	if (pkt.inputs == NULL) return;
	for (u32 i = 0; i < pkt.amount; ++i) {
		input = &pkt.inputs[i];
		if (input->data != NULL) {
			free(input->data);
		}
	}
	free(pkt.inputs);
}

/*
func duplicate() -> WyncPktInputs:
	var newi = WyncPktInputs.new()
	newi.prop_id = prop_id
	newi.amount = amount
	newi.inputs = [] as Array[NetTickDataDecorator]
	for input: NetTickDataDecorator in self.inputs:
		newi.inputs.append(input.duplicate())
	return newi
   */

typedef struct {
	u32 dummy;
} WyncPktJoinReq;

bool WyncPktJoinReq_serialize (
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktJoinReq *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->dummy, sizeof(u32));
	return true;
}

	//bool is_reading, NeteBuffer *buffer, WyncPktClock i

//bool WyncPktJoinReq_write(NeteBuffer *buffer, WyncPktJoinReq *pkt) {
	//NETEBUFFER_WRITE_BYTES(buffer, &pkt->dummy, sizeof(pkt->dummy));
	//return true;
//}
//bool WyncPktJoinReq_read(NeteBuffer *buffer, WyncPktJoinReq *pkt) {
	//NETEBUFFER_READ_BYTES(buffer, &pkt->dummy, sizeof(pkt->dummy));
	//return true;
//}

/*
func duplicate() -> WyncPktJoinReq:
	var i = WyncPktJoinReq.new()
	return i
   */

typedef struct {
	bool approved;
	i32 wync_client_id; // -1
} WyncPktJoinRes;

bool WyncPktJoinRes_serialize (
	bool is_reading,
	NeteBuffer *buffer,
	WyncPktJoinRes *pkt
) {
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->approved, sizeof(bool));
	NETEBUFFER_BYTES_SERIALIZE(is_reading, buffer, &pkt->wync_client_id, sizeof(i32));
	return true;
}

/*
func duplicate() -> WyncPktJoinRes:
	var i = WyncPktJoinRes.new()
	i.approved = approved
	i.wync_client_id = wync_client_id
	return i
   */

typedef struct {
	u32 dummy;
} WyncPacketReqClientInfo;

/*
func duplicate() -> WyncPacketReqClientInfo:
	var i = WyncPacketReqClientInfo.new()
	return i
   */

typedef struct {
	u16 peer_id;
	//var entity_id: int # to validate the entity id exists, unused?
	u16 prop_id;
} WyncPktResClientInfo; // TODO: Rename

/*
func duplicate() -> WyncPktResClientInfo:
	var i = WyncPktResClientInfo.new()
	i.peer_id = peer_id
	#i.entity_id = entity_id
	i.prop_id = prop_id
	return i
   */

typedef struct {
	u16 prop_id;
	u16 state_size;
	void *state; // e.g. Vector2, Quaternion, float, struct
} WyncPktSnap_SnapProp;

/*
## NOTE: Build an optimized packet format exclusive for positional data
class SnapProp:
	func duplicate () -> SnapProp:
		var i = SnapProp.new()
		i.prop_id = self.prop_id
		i.state_size = self.state_size
		i.state = self.state
		return i
*/

typedef struct {
	u32 tick;
	u16 snap_amount;
	WyncPktSnap_SnapProp *snaps;
} WyncPktSnap;

/*
func duplicate() -> WyncPktSnap:
	var i = WyncPktSnap.new()
	i.tick = self.tick
		
	for prop in self.snaps:
		var new_prop = SnapProp.new()
		new_prop.prop_id = prop.prop_id
		new_prop.state_size = prop.state_size
		new_prop.state = prop.state
		i.snaps.append(prop)
			
	return i
   */

typedef struct {
	u16 entity_amount;
	u32 *entity_ids;
	u32 *entity_type_ids;

	u32 *entity_prop_id_start; // authoritative prop id range
	u32 *entity_prop_id_end;

	u32 *entity_spawn_data_sizes; // C buffer info
	void *entity_spawn_data;
} WyncPktSpawn;

/*

func _init(size) -> void:
	resize(size)


func resize(size):
	entity_amount = size
	entity_ids.resize(size)
	entity_type_ids.resize(size)
	entity_prop_id_start.resize(size)
	entity_prop_id_end.resize(size)
	entity_spawn_data_sizes.resize(size)
	entity_spawn_data.resize(size)


func duplicate() -> WyncPktSpawn:
	var i = WyncPktSpawn.new(entity_amount)
	i.entity_amount = entity_amount
	i.entity_ids = entity_ids.duplicate(true)
	i.entity_type_ids = entity_type_ids.duplicate(true)
	i.entity_prop_id_start = entity_prop_id_start.duplicate(true)
	i.entity_prop_id_end = entity_prop_id_end.duplicate(true)
	i.entity_spawn_data_sizes = entity_spawn_data_sizes.duplicate(true)
	for j in range(entity_amount):
		i.entity_spawn_data.append(entity_spawn_data[j])
	return i
   */


#endif // !WYNC_PACKETS_H
