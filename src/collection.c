#include "./common.h"

int push_lock = 0;
int pop_lock = 0;

/**
 * 创建一个先进先出的队列
 * @param size
 * @return
 */
LinkedQueue *create_linkedQueue(LinkedQueue *queue, const size_t size) {
    // 申请队列内存长度
    queue = malloc(size * sizeof(LinkedQueue));

    queue->size = size;
    queue->head = NULL;
    queue->tail = NULL;
    queue->real_size = 0;
    return queue;
}

void destroy_linkedQueue(LinkedQueue *queue) {
    if (queue != NULL) {
        LinkedQueueNode *node = queue->head;
        LinkedQueueNode *tmp = node;
        do {
            if (node != NULL) {
                if (node->next != NULL)
                    node = node->next;
                else node = NULL;

                tmp->next = NULL;
                free(tmp);
                queue->real_size -= 1;
            }
        } while (node != NULL);
        queue->size = 0;
        queue = NULL;
        free(queue);
    }
}

/**
 * 删除旧数据
 * @param queue
 * @return
 */
static void __remove_linkedQueue(LinkedQueue *queue) {
    LinkedQueueNode *node = queue->head;
    if (queue->real_size == 2) {
        free(node);
        fprintf(stdout, "free\n");
        queue->head = NULL;
        queue->tail->previous = NULL;
        queue->real_size -= 1;
    } else if (queue->real_size > 2) {
        LinkedQueueNode *next = node->next;
        next->previous = NULL;
        queue->head = next;
        free(node);
        fprintf(stdout, "free\n");
        queue->real_size -= 1;
    }

}

/**
 * 入列
 * @param queue
 * @param data
 */
void push_linkedQueue(LinkedQueue *queue, LinkedQueueNodeData data) {
    if (push_lock == LOCK && pop_lock == LOCK) push_lock = UNLOCK;
    while (push_lock == LOCK);
    push_lock = LOCK;
    fprintf(stdout, "......... push_linkedQueue with data : %ld \n", data.pts);

    if (queue->size > 1 && queue->real_size >= queue->size) {
        __remove_linkedQueue(queue);
    }

    LinkedQueueNode *node = malloc(sizeof(LinkedQueueNode));
    node->data = data;
    // node is tail, its next is null
    if (queue->tail == NULL) {
        node->next = NULL;
        node->previous = NULL;
        queue->real_size += 1;
        queue->tail = node;
    } else if (queue->head == NULL) {
        LinkedQueueNode *next_data = queue->tail;
        node->previous = next_data;
        queue->tail = node;
        queue->head = next_data;
        queue->real_size += 1;
    } else {
        LinkedQueueNode *next_data = queue->tail;
        next_data->next = node;
        node->previous = next_data;
        queue->tail = node;
        queue->real_size += 1;
    }
    pop_lock = UNLOCK;
    fprintf(stdout, "......... queue size: %d \n", queue->real_size);   
}

/**
 * 出列
 * @param queue
 * @return
 */
LinkedQueueNode *pop_linkedQueue(LinkedQueue *queue) {
    if (push_lock == LOCK && pop_lock == LOCK) push_lock = UNLOCK;
    while (pop_lock == LOCK);
    pop_lock = LOCK;
    fprintf(stdout, "......... pop_linkedQueue 1 \n");

    if (queue->real_size == 0) return NULL;

    if (queue->real_size == 1) {
        LinkedQueueNode *tail = queue->tail;
        queue->real_size = 0;
        queue->tail = NULL;
        return tail;
    }

    LinkedQueueNode *node = queue->head;
    if (node == NULL || queue->real_size == 0) {
        queue->real_size = 0;
        return NULL;
    }
    fprintf(stdout, "......... pop_linkedQueue %ld\n", node->data.pts);
    // node is head, its previous is null
    if (queue->real_size == 2) {
        queue->head = NULL;
        queue->tail->previous = NULL;
    } else if (queue->real_size > 2) {
        LinkedQueueNode *next = node->next;
        if (next != NULL) {
            next->previous = NULL;
            queue->head = next;
        }
    }

    queue->real_size -= 1;
    node->next = NULL;
    node->previous = NULL;

    fprintf(stdout, "......... after pop queue size: %d \n", queue->real_size);
    push_lock = UNLOCK;
    return node;
}