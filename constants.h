
typedef struct {
    int liquid_level;
    int colorR;
    int colorG;
    int colorB;
    bool isSealed;
    int label;
    char production_date[50];
    char expire_date[50];
} LiquidMedicine;

typedef struct {
    int min_liquid_level;
    int max_liquid_level;
    int colorR;
    int colorG;
    int colorB;
    int label;
    int remaining_months_for_expiry;
} LiquidSpecs;

// typedef struct {
//     int num_pills;
//     Pill[100];
//     int label;
//     char expire_date[50];
// } PlasticContainer;

// typedef struct {
//     int size;
//     int colorR;
//     int colorG;
//     int colorB;
// } Pill;

// Define the node structure
typedef struct QueueNode {
    void* data;
    struct QueueNode* next;
} QueueNode;

typedef struct Queue {
    QueueNode* front;
    QueueNode* rear;
    int size;
    size_t dataSize; // Size of the data type
} Queue;
