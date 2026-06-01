#include "user_Queue.h"

void initQueue(Queue *queue)
{
    uint8_t i;
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
    for (i = 0; i < QUEUE_SIZE; i++)
    {
        queue->data[i] = 0;
    }
}

bool isQueueEmpty(Queue *queue)
{
    return queue->size == 0;
}

bool isQueueFull(Queue *queue)
{
    return queue->size == QUEUE_SIZE;
}

void enqueue(Queue *queue, uint32_t item)
{
    uint8_t insert_index;

    if (isQueueFull(queue))
    {
        printf("Queue full, enqueue failed.\n");
        return;
    }

    insert_index = (uint8_t)queue->size;
    queue->data[insert_index] = item;
    queue->front = 0;
    queue->rear = (int8_t)insert_index;
    queue->size++;
}

uint32_t dequeue(Queue *queue)
{
    uint8_t i;
    if (isQueueEmpty(queue))
    {
        printf("Queue empty, dequeue failed.\n");
        return 0;
    }

    uint32_t item = queue->data[queue->front];

    for (i = 1; i < (uint8_t)queue->size; i++)
    {
        queue->data[i - 1] = queue->data[i];
    }

    queue->size--;
    queue->front = 0;
    queue->rear = (queue->size == 0) ? -1 : (int8_t)(queue->size - 1);
    queue->data[queue->size] = 0U;
    return item;
}
