#include <stdlib.h>
#include <stdio.h>
#include "../include/buffer.h"

RingBuffer* buffer_init(int capacity) {
    RingBuffer *rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;

    rb->data = malloc(sizeof(VehicleData) * capacity);
    if (!rb->data) {
        free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->shutdown = false;

    pthread_mutex_init(&rb->lock, NULL);
    pthread_cond_init(&rb->not_empty, NULL);
    pthread_cond_init(&rb->not_full, NULL);

    return rb;
}

void buffer_destroy(RingBuffer *rb){
    if (!rb) return;

    pthread_mutex_destroy(&rb->lock);
    pthread_cond_destroy(&rb->not_empty);
    pthread_cond_destroy(&rb->not_full);

    free(rb->data);
    free(rb);
}

bool buffer_push(RingBuffer *rb, VehicleData item) {
    pthread_mutex_lock(&rb->lock);

    // If full and if its shutdown, wait till data is removed by the pop.
    while(rb->count == rb->capacity && !rb->shutdown) {
        pthread_cond_wait(&rb->not_full, &rb->lock);
    }

    if (rb->shutdown) {
        pthread_mutex_unlock(&rb->lock);
        return false;
    }

    rb->data[rb->head] = item;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;

    pthread_cond_signal(&rb->not_empty);

    pthread_mutex_unlock(&rb->lock);
    return true;
}

bool buffer_pop(RingBuffer *rb, VehicleData *item) {
    pthread_mutex_lock(&rb->lock);
    
    // if empty wait till data is added to the buffer.
    while (rb->count == 0 && !rb->shutdown) {
        pthread_cond_wait(&rb->not_empty, &rb->lock);
    }

    if (rb->shutdown && rb->count == 0) {
        pthread_mutex_unlock(&rb->lock);
        return false;
    }

    *item = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;

    pthread_cond_signal(&rb->not_full);

    pthread_mutex_unlock(&rb->lock);
    return true;
}

void buffer_signal_shutdown(RingBuffer *rb){
    pthread_mutex_lock(&rb->lock);

    rb->shutdown = true;

    pthread_cond_broadcast(&rb->not_empty);
    pthread_cond_broadcast(&rb->not_full);

    pthread_mutex_unlock(&rb->lock);
}