#ifndef __CONSTANTS__
#define __CONSTANTS__

#define MAX_THREADS 50

enum OPERATION_TYPE {PRODUCED_LIQUID_MEDICINE = 1, PRODUCED_PILL_MEDICINE, DEFECTED_LIQUID_MEDICINE, DEFECTED_PILL_MEDICINE};
enum MEDICINE_TYPE {LIQUID = 1, PILL};

enum DRAWER_OPERATION_TYPES {
    INITIAL = 1, PRODUCTION, INSPECTION_START, INSPECTION_SUCCESSFUL, INSPECTION_FAILED, PACKAGING_START, PACKAGING_END,
    INSPECTOR_CANCELLED, PACKAGER_CANCELLED, INSPECTOR_RECEIVED, PACKAGER_RECEIVED
};

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

typedef struct {
    int type; /* type of medicine */
    int size;
    int colorR;
    int colorG;
    int colorB;
} Pill;

typedef struct {
    int serial_number;
    int type;
    int num_pills;
    Pill* pills;
    char production_date[50];
    char expire_date[50];
} PlasticContainer;

typedef struct {
    int type;
    int pill_size;
    int pill_colorR;
    int pill_colorG;
    int pill_colorB;
    int num_pills;
    int remaining_months_for_expiry;
    int num_plastic_containers;
} PillMedicineSpecs;

typedef struct {
    PlasticContainer containers[100];
    char prescription[50];
} PillPackage;

typedef struct {
    long message_type; /* the type of message */
    int medicine_type; /* the type of medicine */
} FeedBackMessage;

typedef struct {
    long production_line_number; /* number of production line that will receive employees */
    int num_inspectors;          /* number of inspectors to be transferred */
    int num_packagers;           /* number of packagers to be transferred */
} EmployeeTransferMessage;

typedef struct {
    LiquidMedicine medicine;
    float y;
    float x;
    bool draw_this;
} LiquidMedicineGUI;

typedef struct {
    PlasticContainer plastic_container;
    float x;
    float y;
    bool draw_this;
} PlasticContainerGUI;

typedef struct {
    int worker_index;
    int production_line_number;
} InspectorGUI;

typedef struct {
    int worker_index;
    int production_line_number;
} PackagerGUI;

typedef struct {
    long operation_type;
    int production_line_number;
    int worker_index;
    int medicine_type;
    union
    {
        PlasticContainer plastic_container;
        LiquidMedicine liquid_medicine;
    } medicine;
    
} DrawerMessage;

typedef struct {
    long type;
    int production_line_number;
    int num_inspectors;
    int num_packagers;
} InitialMessage;

typedef struct {
    int production_num;
    float inspection_speed;
    float packaging_speed;
} MemoryCell;

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

union semun {
    int              val;
    struct semid_ds *buf;
    ushort          *array;
};

#endif