#ifndef __CONSTANTS__
#define __CONSTANTS__

enum OPERATION_TYPE {PRODUCED_LIQUID_MEDICINE = 1, PRODUCED_PILL_MEDICINE, DEFECTED_LIQUID_MEDICINE, DEFECTED_PILL_MEDICINE};

typedef struct {
    int serial_number;
    int liquid_level;
    int colorR;
    int colorG;
    int colorB;
    bool isSealed;
    int type;
    int label;
    float label_position;
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
    float label_position;
    int remaining_months_for_expiry;
} LiquidSpecs;

typedef struct {
    LiquidMedicine medicine;
    char prescription[50];
} LiquidPackage;


// typedef struct {
//     int num_pills;
//     Pill[100];
//     int label;
//     char expire_date[50];
// } PlasticContainer;

typedef struct {
    int size;
    int colorR;
    int colorG;
    int colorB;
} Pill;

typedef struct {
    long message_type; /* the type of message */
    int medicine_type; /* the type of medicine */
} Message;

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

#endif