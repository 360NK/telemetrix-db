#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/gtfs-realtime.pb-c.h"
#include "../../include/buffer.h"

void parse_and_queue(uint8_t* buffer, size_t len, RingBuffer* rb) {
    TransitRealtime__FeedMessage *msg;
    msg = transit_realtime__feed_message__unpack(NULL, len, buffer);
    if (msg == NULL) {
        fprintf(stderr, "[Parser] Error unpacking incoming message\n");
        return;
    }

    int pushed_count = 0;
    for (size_t i = 0; i < msg->n_entity; i++) {
        TransitRealtime__FeedEntity *entity = msg->entity[i];

        if (entity->vehicle && entity->vehicle->position) {
            VehicleData item;

            const char *raw_fleet = (entity->vehicle->vehicle && entity->vehicle->vehicle->label) ? entity->vehicle->vehicle->label : (entity->id ? entity->id : "UNKNOWN");
            strncpy(item.fleet_number, raw_fleet, sizeof(item.fleet_number) - 1);
            item.fleet_number[sizeof(item.fleet_number) - 1] = '\0';

            const char *raw_internal_id = (entity->vehicle->vehicle && entity->vehicle->vehicle->id) ? entity->vehicle->vehicle->id : "UNKNOWN";
            strncpy(item.internal_id, raw_internal_id, sizeof(item.internal_id) - 1);
            item.internal_id[sizeof(item.internal_id) - 1] = '\0';

            const char *raw_route = (entity->vehicle->trip && entity->vehicle->trip->route_id) ? entity->vehicle->trip->route_id : "UNKNOWN";
            strncpy(item.route_id, raw_route, sizeof(item.route_id) - 1);
            item.route_id[sizeof(item.route_id) - 1] = '\0';

            item.lat = entity->vehicle->position->latitude;
            item.lon = entity->vehicle->position->longitude;

            item.speed = (entity->vehicle->position->has_speed) ? entity->vehicle->position->speed : 0.0;

            // --- GRAB THE PASSENGER DATA ---
            // 7 corresponds to TRANSIT_REALTIME__VEHICLE_POSITION__OCCUPANCY_STATUS__NO_DATA_AVAILABLE
            item.occupancy_status = (entity->vehicle->has_occupancy_status) ? 
                                     entity->vehicle->occupancy_status : 7;
            
            // We use -1 to indicate the TTC didn't send an exact percentage
            item.occupancy_percentage = (entity->vehicle->has_occupancy_percentage) ? 
                                         entity->vehicle->occupancy_percentage : -1;

            // --- GRAB THE BEARING (Compass Direction) ---
            item.bearing = (entity->vehicle->position->has_bearing) ? 
                            entity->vehicle->position->bearing : 0.0;

            // --- GRAB THE HARDWARE TIMESTAMP ---
            item.timestamp = (entity->vehicle->has_timestamp) ? 
                              entity->vehicle->timestamp : 0;

            // --- GRAB THE CURRENT STATUS (0=Incoming, 1=Stopped, 2=In Transit) ---
            // If missing, protobuf defaults typically suggest 'In Transit To' (2)
            item.current_status = (entity->vehicle->has_current_status) ? 
                                   entity->vehicle->current_status : 2;

            // --- GRAB THE CONGESTION LEVEL (0=Unknown, 1=Smooth, 4=Severe) ---
            item.congestion_level = (entity->vehicle->has_congestion_level) ? 
                                     entity->vehicle->congestion_level : 0;

            // --- GRAB THE STOP ID ---
            const char *raw_stop = (entity->vehicle->stop_id) ? entity->vehicle->stop_id : "UNKNOWN";
            strncpy(item.stop_id, raw_stop, sizeof(item.stop_id) - 1);
            item.stop_id[sizeof(item.stop_id) - 1] = '\0';

            if (buffer_push(rb, item)) {
                pushed_count++;
            } else {
                break; 
            }
        }
    }
    printf("[Parser] Successfully pushed %d vehicles to the Ring Buffer.\n", pushed_count);
    transit_realtime__feed_message__free_unpacked(msg, NULL);
}
