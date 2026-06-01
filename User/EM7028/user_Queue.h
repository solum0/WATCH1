#ifndef __USER_QUEUE_H__
#define __USER_QUEUE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define QUEUE_SIZE 7

typedef struct {
    int8_t front;
    int8_t rear;
    int8_t size;
    uint32_t data[QUEUE_SIZE];
} Queue;

void initQueue(Queue *queue);
bool isQueueEmpty(Queue *queue);
bool isQueueFull(Queue *queue);
void enqueue(Queue *queue, uint32_t item);
uint32_t dequeue(Queue *queue);

#endif
