#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <time.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <libavutil/log.h>
#include <libavutil/frame.h>

#define LOCK 1
#define UNLOCK 0

typedef struct LinkedQueueNodeData {
    AVFrame *frame;
    long pts;
    int ret;
    char *error_message;
} LinkedQueueNodeData;

typedef struct LinkedQueueNode {
    struct LinkedQueueNode *next;
    struct LinkedQueueNode *previous;
    LinkedQueueNodeData data;
} LinkedQueueNode;

typedef struct LinkedQueue {
    
    LinkedQueueNode *head;
    LinkedQueueNode *tail;
    // 队列容量大小
    size_t size;
    // 队列实际大小
    int real_size;
} LinkedQueue;

LinkedQueue *create_linkedQueue(LinkedQueue *queue, const size_t size);

void destroy_linkedQueue(LinkedQueue *queue);

void push_linkedQueue(LinkedQueue *queue, LinkedQueueNodeData data);

LinkedQueueNode *pop_linkedQueue(LinkedQueue *queue);