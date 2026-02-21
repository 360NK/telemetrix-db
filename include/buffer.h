#ifndef BUFFER_H
#define BUFFER_H

#include <pthread.h>
#include <stdbool.h>

typedef struct {
    char fleet_number[16];
    char internal_id[16];
    char route_id[16];
    float lat;
    float lon;
} VehicleData;

typedef struct {
    VehicleData *data;
    int capacity;
    int head;
    int tail;
    int count;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    bool shutdown;
} RingBuffer;


RingBuffer* buffer_init(int capacity);
void buffer_destroy(RingBuffer *rb);
bool buffer_push(RingBuffer *rb, VehicleData item);
bool buffer_pop(RingBuffer *rb, VehicleData *item);
void buffer_signal_shutdown(RingBuffer *rb);

#endif