#ifndef __FUNCTIONS__
#define __FUNCTIONS__

int select_from_range(int min, int max);

QueueNode* createNode(void* data, size_t dataSize);
void initQueue(Queue* q, size_t dataSize);
void enqueue(Queue* q, void* data);
int dequeue(Queue* q, void* data);
int peek(Queue* q, void* data);
bool isEmpty(Queue* q);
bool ArrayIsEmpty(Queue* q[100], int size);
void freeQueue(Queue* q);
int getSize(Queue* q);
#endif