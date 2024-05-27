#include "headers.h"

int select_from_range(int min, int max) {

    if (max < min)
        return min;
        
    // srand(time(NULL) + getpid());

    int range = max - min + 1;
    int random = (rand() % range) + min;

    return random;
}

void initQueue(Queue* q, size_t dataSize) {
    q->front = q->rear = NULL;
    q->size = 0;
    q->dataSize = dataSize;
}

QueueNode* createNode(void* data, size_t dataSize) {
    QueueNode* newNode = (QueueNode*)malloc(sizeof(QueueNode));

    newNode->data = malloc(dataSize);
    memcpy(newNode->data, data, dataSize);
    newNode->next = NULL;

    return newNode;
}

void enqueue(Queue* q, void* data) {
    QueueNode* newNode = createNode(data, q->dataSize);

    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}

int dequeue(Queue* q, void* data) {

    if (q->front == NULL) {
        printf("Queue is empty\n");
        return -1;
    }

    QueueNode* temp = q->front;

    if (data)
        memcpy(data, temp->data, q->dataSize);

    q->front = q->front->next;

    if (q->front == NULL) {
        q->rear = NULL;
    }

    free(temp->data);
    free(temp);
    q->size--;

    return 0;
}

int peek(Queue* q, void* data) {

    if (q->front == NULL) {
        printf("Queue is empty\n");
        return -1;
    }

    memcpy(data, q->front->data, q->dataSize);

    return 0; // success
}

bool isEmpty(Queue* q) {
    return q->size == 0;
}

void freeQueue(Queue* q) {

    while (!isEmpty(q)) {
        dequeue(q, NULL);
    }
}

int getSize(Queue* q) {
    return q->size;
}
