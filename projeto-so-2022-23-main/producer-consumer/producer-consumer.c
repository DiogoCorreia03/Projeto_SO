#include "producer-consumer.h"
#include <pthread.h>
#include <stdlib.h>

int pcq_create(pc_queue_t *queue, size_t capacity) {
    // Allocate memory for the buffer
    queue->pcq_buffer = (void **)malloc(capacity * sizeof(void *));
    if (queue->pcq_buffer == NULL) {
        return -1;
    }

    // Initialize the queue properties
    queue->pcq_capacity = capacity;
    queue->pcq_current_size = 0;
    queue->pcq_head = 0;
    queue->pcq_tail = 0;

    // Initialize the locks and condition variables
    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_pusher_condvar, NULL);
    pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_popper_condvar, NULL);

    return 0;
}

int pcq_destroy(pc_queue_t *queue) {
    // Deallocate memory for the buffer
    free(queue->pcq_buffer);

    // Destroy the locks and condition variables
    pthread_mutex_destroy(&queue->pcq_current_size_lock);
    pthread_mutex_destroy(&queue->pcq_head_lock);
    pthread_mutex_destroy(&queue->pcq_tail_lock);
    pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
    pthread_cond_destroy(&queue->pcq_pusher_condvar);
    pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
    pthread_cond_destroy(&queue->pcq_popper_condvar);

    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem) {
    // Acquire the lock on the current size
    pthread_mutex_lock(&queue->pcq_current_size_lock);

    // Wait until the queue has space
    while (queue->pcq_current_size == queue->pcq_capacity) {
        pthread_cond_wait(&queue->pcq_pusher_condvar,
                          &queue->pcq_current_size_lock);
    }

    // Increment the current size
    queue->pcq_current_size++;

    // Release the lock on the current size
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    // Acquire the lock on the head
    pthread_mutex_lock(&queue->pcq_head_lock);

    // Insert the new element at the head of the buffer
    queue->pcq_buffer[queue->pcq_head] = elem;

    // Update the head index
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;

    // Release the lock on the head
    pthread_mutex_unlock(&queue->pcq_head_lock);

    // Wake up the popper thread
    pthread_cond_signal(&queue->pcq_popper_condvar);

    return 0;
}

void *pcq_dequeue(pc_queue_t *queue) {
    void *elem;

    // Acquire the lock on the current size
    pthread_mutex_lock(&queue->pcq_current_size_lock);

    // Wait until the queue has an element
    while (queue->pcq_current_size == 0) {
        pthread_cond_wait(&queue->pcq_popper_condvar,
                          &queue->pcq_current_size_lock);
    }

    // Decrement the current size
    queue->pcq_current_size--;

    // Release the lock on the current size
    pthread_mutex_unlock(&queue->pcq_current_size_lock);

    // Acquire the lock on the tail
    pthread_mutex_lock(&queue->pcq_tail_lock);

    // Remove the element from the tail of the buffer
    elem = queue->pcq_buffer[queue->pcq_tail];

    // Update the tail index
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;

    // Release the lock on the tail
    pthread_mutex_unlock(&queue->pcq_tail_lock);

    // Wake up the pusher thread
    pthread_cond_signal(&queue->pcq_pusher_condvar);

    return elem;
}
