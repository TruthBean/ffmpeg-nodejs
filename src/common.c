#include "./common.h"

time_t get_time() {
    time_t result;
    time_t now_seconds;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    now_seconds = tv.tv_sec;

    result = 1000000 * tv.tv_sec + tv.tv_usec;

    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&now_seconds));
    av_log(NULL, AV_LOG_DEBUG, "now: %s\n", buff);

    return result;
}

int push_lock = UNLOCK;
int pop_lock = UNLOCK;

/**
 * 创建一个后进先出的队列
 * @param size
 * @return
 */
LinkedQueue *create_linkedQueue(LinkedQueue *queue, const size_t size) {
    // 申请队列内存长度
    if (queue == NULL)
        queue = malloc(size * sizeof(LinkedQueue));

    queue->head = queue->tail = malloc(sizeof(LinkedQueueNode));

    queue->size = size;
    // head previous always null
    // queue->head->next = NULL;
    // tail next always null
    // queue->tail->next = NULL;

    // empty queue: head next null, tail previous null
    queue->head->next = NULL;
    queue->tail->next = NULL;

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
 * 删除头部的数据
 * @param queue
 * @return
 */
static void __remove_linkedQueue(LinkedQueue *queue) {
    LinkedQueueNode *head = queue->head;
    if (head != NULL) {
        head = queue->head->next;
        if (head != NULL) {
            queue->head->next = head->next;
            if (queue->tail == head) queue->tail = queue->head;
            if (head->data.frame != NULL) {
                av_frame_unref(head->data.frame);
                av_frame_free(&(head->data.frame));
                free(head);
            }
            queue->real_size -= 1;
        }
    }
    
}

static LinkedQueueNodeData copy_nodeData(LinkedQueueNodeData data) {
    LinkedQueueNodeData result = {
            .ret = data.ret,
            .pts = data.pts,
            .error_message = data.error_message
    };

    AVFrame *dist_frame = av_frame_alloc();
    if (av_frame_copy(dist_frame, data.frame) >= 0) av_frame_copy_props(dist_frame, data.frame);
    av_frame_unref(data.frame);
    return result;
}

/**
 * 尾进
 * @param queue
 * @param data
 */
void push_linkedQueue(LinkedQueue *queue, LinkedQueueNodeData data) {
    // if (push_lock == LOCK && pop_lock == LOCK) push_lock = UNLOCK;
    // while (push_lock == LOCK);
    // push_lock = LOCK;
    // fprintf(stdout, "......... push_linkedQueue with data : %ld \n", data.pts);

    if (queue->size > 1 && queue->real_size >= queue->size) {
        __remove_linkedQueue(queue);
    }

    LinkedQueueNode *node = malloc(sizeof(LinkedQueueNode));
    node->data = copy_nodeData(data);
    node->next = NULL;

    queue->tail->next = node;
    queue->tail = node;
    queue->real_size += 1;

    // push_lock = UNLOCK;
    // pop_lock = UNLOCK;
    // fprintf(stdout, "......... queue size: %d \n", queue->real_size);
}

/**
 * 头出
 * @param queue
 * @return
 */
LinkedQueueNodeData pop_linkedQueue(LinkedQueue *queue) {
    // if (push_lock == LOCK && pop_lock == LOCK) pop_lock = UNLOCK;
    // while (pop_lock == LOCK);
    // pop_lock = LOCK;
    fprintf(stdout, "......... pop_linkedQueue 1 \n");

    LinkedQueueNodeData result = {};

    if (queue->real_size == 0 || queue->head == queue->tail) return result;

    LinkedQueueNode *head = queue->head;
    if (!(head == NULL || head->next == NULL)) head = queue->head->next;

    if (head != NULL) {
        result = head->data;

        queue->head->next = head->next;

        if (queue->tail == head) queue->tail = queue->head;
        free(head);
        queue->real_size -= 1;

        fprintf(stdout, "......... after pop queue size: %d \n", queue->real_size);
        //push_lock = UNLOCK;
        // pop_lock = UNLOCK;
    }

    return result;
}
