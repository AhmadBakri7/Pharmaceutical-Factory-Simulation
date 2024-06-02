#include "headers.h"

int select_from_range(int min, int max) {

    if (max <= min)
        return min;
        
    // srand(time(NULL) + getpid());

    int range = max - min + 1;
    int random = (rand() % range) + min;

    return random;
}

void semaphore_acquire(int sem_num, int id, struct sembuf* acquire) {

    acquire->sem_num = sem_num;

    // write the amplitude the shared memory
    if (semop(id, acquire, 1) == -1) {
        perror("semop Child");
        exit(1);
    }
}

void semaphore_release(int sem_num, int id, struct sembuf* release) {

    release->sem_num = sem_num;

    // Release the semaphore (unlock)
    if (semop(id, release, 1) == -1) {
        perror("semop Release");
        exit(1);
    }
}

void detach_memory(void* shared_memory) {

    // Detach from the shared memory segment
    if (shmdt(shared_memory) == -1) {
        perror("shmdt");
        exit(1);
    }
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

bool ArrayIsEmpty(Queue q[100], int size){
    bool flag = false;
    for (int i = 0; i< size; i++){
        if (q[i].size == 0){
            flag = true;
        }
    }
    return flag;
}

void freeQueue(Queue* q) {

    while (!isEmpty(q)) {
        dequeue(q, NULL);
    }
}

int getSize(Queue* q) {
    return q->size;
}
