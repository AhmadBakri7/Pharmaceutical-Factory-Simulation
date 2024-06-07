#include "headers.h"
#include "functions.h"

MemoryCell* shared_memory;

struct sembuf acquire = {0, -1, SEM_UNDO};
struct sembuf release = {0, 1, SEM_UNDO};

pthread_mutex_t created_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t non_defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t inspection_overflow_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packaging_overflow_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty_created_medicine_queue_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_non_defected_queue_cv = PTHREAD_COND_INITIALIZER;

int num_medicine_types;
int production_line_number;

PillMedicineSpecs specs[200];
Pill pill_specs[200];

int num_inspectors;
int num_packagers;

Queue created_medicine_queue;
Queue non_defected_medicine_queue[100];

int max_months_before_expiry;
int min_months_before_expiry;

int max_num_pills_per_container;
int min_num_pills_per_container;

int max_num_plastic_containers_per_pill_medicine;
int min_num_plastic_containers_per_pill_medicine;

int max_time_for_inspection;
int min_time_for_inspection;

int max_time_for_packaging;
int min_time_for_packaging;

int serial_counter = 0;

int missing_pills_defect_rate, pill_color_defect_rate, pill_size_defect_rate, expire_date_defect_rate;

float speed_threshold;
// float my_inspection_speed;
// float my_packaging_speed;
int inspection_overflow = 0;
int packaging_overflow = 0;

int feedback_queue_id, emp_transfer_queue_id, drawer_queue_id;
int speed_sem_id, speed_shmem_id;


void cleanup_handler(void *arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mutex);
}

void* inspection(void* data) {
    PlasticContainer medicine;

    int my_number = *((int*) data);

    int last_state, last_type;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state); 
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_type);

    while (1) {

        bool defect = false;

        pthread_mutex_lock(&created_medicine_queue_mutex);

        pthread_cleanup_push(cleanup_handler, &created_medicine_queue_mutex);

        if (isEmpty(&created_medicine_queue))
            pthread_cond_wait(&empty_created_medicine_queue_cv, &created_medicine_queue_mutex);
        
        pthread_cleanup_pop(0);

        if (dequeue(&created_medicine_queue, &medicine) == -1) {
            pthread_mutex_unlock(&created_medicine_queue_mutex);
            pthread_testcancel();
            continue;
        }
        pthread_mutex_lock(&inspection_overflow_mutex);
        inspection_overflow = getSize(&created_medicine_queue);
        pthread_mutex_unlock(&inspection_overflow_mutex);

        pthread_mutex_unlock(&created_medicine_queue_mutex);

        DrawerMessage drawer_msg;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.operation_type = INSPECTION_START;
        drawer_msg.medicine_type = PILL;
        drawer_msg.worker_index = my_number;
        memcpy(&drawer_msg.medicine.plastic_container, &medicine, sizeof(medicine));

        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
            perror("Child: msgsend Production");
            pthread_exit( (void*) -1 );
        }

        /* year/month/day */
        int production_year = atoi(strtok(medicine.production_date, "-"));
        int production_month = atoi(strtok('\0', "-"));

        production_month = production_month + specs[medicine.type].remaining_months_for_expiry;

        while (production_month > 12) {
            production_year++;
            production_month -= 12;
        }

        int printed_year = atoi(strtok(medicine.expire_date, "-"));
        int printed_month = atoi(strtok('\0', "-"));

        sleep( select_from_range(min_time_for_inspection, max_time_for_inspection) );

        // check each spec
        if (medicine.num_pills != specs[medicine.type].num_pills
            || production_year != printed_year || production_month != printed_month)
        {
            defect = true;
            drawer_msg.operation_type = INSPECTION_FAILED;
        }

        for (int i = 0; i < medicine.num_pills; i++) { /* check each pill */

            if (medicine.pills[i].colorR != specs[medicine.type].pill_colorR
                || medicine.pills[i].colorG != specs[medicine.type].pill_colorG
                || medicine.pills[i].colorB != specs[medicine.type].pill_colorB
                || medicine.pills[i].size != specs[medicine.type].pill_size)
            {
                defect = true;
                drawer_msg.operation_type = INSPECTION_FAILED;
                break;
            }
        }

        if (defect) {
            printf("An (INSPECTOR): %d, put a medicine [%d] in Trash\n", my_number, medicine.serial_number);
            fflush(stdout);

            drawer_msg.operation_type = INSPECTION_FAILED;

            // send to main.c (parent process) that a medicine is defected, using a message queue.
            FeedBackMessage msg;
            msg.message_type = DEFECTED_PILL_MEDICINE;

            if ( msgsnd(feedback_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }
        } else {
            pthread_mutex_lock(&non_defected_medicine_queue_mutex);
            enqueue(&non_defected_medicine_queue[medicine.type], &medicine);

            printf("An (INSPECTOR): %d, put a medicine of type %d in Non Defected array\n", my_number, medicine.type);
            fflush(stdout);

            drawer_msg.operation_type = INSPECTION_SUCCESSFUL;

            pthread_mutex_lock(&packaging_overflow_mutex);
            int s;

            if ( (s = getSize(&non_defected_medicine_queue[medicine.type])) > specs[medicine.type].num_plastic_containers )
                packaging_overflow += (s - specs[medicine.type].num_plastic_containers);

            pthread_mutex_unlock(&packaging_overflow_mutex);

            // wake a packager to package a medicine
            pthread_cond_signal(&empty_non_defected_queue_cv);

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.worker_index = my_number;
        memcpy(&drawer_msg.medicine.plastic_container, &medicine, sizeof(medicine));

        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
            perror("Child: msgsend Production");
            pthread_exit( (void*) -1 );
        }
        pthread_testcancel();
    }
}

void* packaging(void* data) {
    PlasticContainer medicine;
    PillPackage package;

    int my_number = *((int*) data);

    int last_state, last_type;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_type);

    while (1) {
        pthread_mutex_lock(&non_defected_medicine_queue_mutex);

        bool no_medicine_to_package = true, medicine_was_packages = false;

        for (int i = 0; i < num_medicine_types; i++) {
            if (non_defected_medicine_queue[i].size >= specs[i].num_plastic_containers)
                no_medicine_to_package = false;
        }

        pthread_cleanup_push(cleanup_handler, &non_defected_medicine_queue_mutex);

        if (no_medicine_to_package)
            pthread_cond_wait(&empty_non_defected_queue_cv, &non_defected_medicine_queue_mutex);

        pthread_cleanup_pop(0);

        DrawerMessage drawer_msg;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.medicine_type = PILL;

        for (int i = 0; i < num_medicine_types; i++) {

            if (non_defected_medicine_queue[i].size >= specs[i].num_plastic_containers) {

                medicine_was_packages = true;

                for (int j = 0; j < specs[i].num_plastic_containers; j++) {

                    if (dequeue(&non_defected_medicine_queue[i], &medicine) != -1) {
                        drawer_msg.operation_type = PACKAGING_START;
                        drawer_msg.worker_index = my_number;
                        memcpy(&drawer_msg.medicine.plastic_container, &medicine, sizeof(medicine));

                        memcpy(&package.containers[j], &medicine, sizeof(medicine));

                        printf("(PACKAGER) %d put medicine [%d] of type %d in a package\n", my_number, medicine.serial_number, specs[i].type);
                        fflush(stdout);

                        // start packaging
                        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
                            perror("Child: msgsend Production");
                            pthread_exit( (void*) -1 );
                        }
                    }
                }
                pthread_mutex_lock(&packaging_overflow_mutex);

                if (getSize(&non_defected_medicine_queue[i]) > specs[i].num_plastic_containers)
                    packaging_overflow -= specs[i].num_plastic_containers;
                else
                    packaging_overflow -= getSize(&non_defected_medicine_queue[i]);

                pthread_mutex_unlock(&packaging_overflow_mutex);

                break;
            }
        }
        pthread_mutex_unlock(&non_defected_medicine_queue_mutex);

        if (!medicine_was_packages) {
            pthread_testcancel();
            continue;
        }

        strcpy(package.prescription, "Please Follow Your Doctor's Instructions");

        sleep( select_from_range(min_time_for_packaging, max_time_for_packaging) );

        // finish packaging
        drawer_msg.operation_type = PACKAGING_END;
        memcpy(&drawer_msg.medicine.plastic_container, &medicine, sizeof(medicine));
        
        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
            perror("Child: msgsend Production");
            pthread_exit( (void*) -1 );
        }

        FeedBackMessage msg;
        msg.message_type = PRODUCED_PILL_MEDICINE;
        msg.medicine_type = medicine.type;

        if ( msgsnd(feedback_queue_id, &msg, sizeof(msg), 0) == -1 ) {
            perror("Child: msgsend");
            pthread_exit( (void*) -1 );
        }
        pthread_testcancel();
    }
}

void initialize_specs() {

    printf("Specs: \n");
    fflush(stdout);

    for (int i = 0; i < num_medicine_types; i++) {
        
        srand(time(NULL) + getpid() + i);

        // set the specs for the plastic containers.
        specs[i].type = i;
        specs[i].num_pills = select_from_range(min_num_pills_per_container, max_num_pills_per_container);
        specs[i].num_plastic_containers = select_from_range(min_num_plastic_containers_per_pill_medicine,max_num_plastic_containers_per_pill_medicine);
        specs[i].remaining_months_for_expiry = select_from_range(min_months_before_expiry, max_months_before_expiry);

        // set the specs for the pills.
        specs[i].pill_colorR = select_from_range(50, 200);
        specs[i].pill_colorG = select_from_range(50, 200);
        specs[i].pill_colorB = select_from_range(50, 200);
        specs[i].pill_size = select_from_range(2, 10);

        printf( /* container specs */
            "[Type: %d]: num_pills: %d, num_plastic_containers: %d, remaining_months: %d, pill_size: %d, Color(R=%d, G=%d, B=%d)\n",
            specs[i].type, specs[i].num_pills, specs[i].num_plastic_containers, specs[i].remaining_months_for_expiry, specs[i].pill_size,
            specs[i].pill_colorR, specs[i].pill_colorG, specs[i].pill_colorB
        );
        fflush(stdout);
    }
    printf("--------------\n");
    fflush(stdout);
}

/* Randomly produce a medicine */
PlasticContainer produce_medicine() {
    PlasticContainer created_medicine;
    bool flag = false;

    srand(time(NULL) + getpid());
    
    int selected_medicine_type = select_from_range(0, num_medicine_types-1);

    // select features for the plastic container.
    created_medicine.type = selected_medicine_type;
    created_medicine.serial_number = serial_counter;
    serial_counter++;

    int num_pills = specs[selected_medicine_type].num_pills;

    bool defect = select_from_range(1, 100) < missing_pills_defect_rate;

    if (defect) {
        int random_missing_pills = select_from_range(1, num_pills-1);
        num_pills -= random_missing_pills;
        flag = true;
    }
    created_medicine.num_pills = num_pills;
    created_medicine.pills = (Pill*) malloc(sizeof(Pill) * num_pills);

    // create each pill in the plastic container.
    for (int i = 0; i < num_pills; i++) {
        created_medicine.pills[i].type = selected_medicine_type; /* choose the pill type equal to the selected type */

        // pill color
        defect = select_from_range(1, 100) < pill_color_defect_rate;

        if (defect) {
            created_medicine.pills[i].colorR = abs(specs[selected_medicine_type].pill_colorR + 30 * ((rand() % 2 == 0)? 1 : -1));
            created_medicine.pills[i].colorG = abs(specs[selected_medicine_type].pill_colorG + 30 * ((rand() % 2 == 0)? 1 : -1));
            created_medicine.pills[i].colorB = abs(specs[selected_medicine_type].pill_colorB + 30 * ((rand() % 2 == 0)? 1 : -1));
            flag = true;

        } else {
            created_medicine.pills[i].colorR = specs[selected_medicine_type].pill_colorR;
            created_medicine.pills[i].colorG = specs[selected_medicine_type].pill_colorG;
            created_medicine.pills[i].colorB = specs[selected_medicine_type].pill_colorB;
        }

        // pill size
        defect = select_from_range(1, 100) < pill_size_defect_rate;

        if (defect) {
            created_medicine.pills[i].size = abs(specs[selected_medicine_type].pill_size + ((rand() % 2 == 0)? 1 : -1));
            flag = true;

        } else {
            created_medicine.pills[i].size = specs[selected_medicine_type].pill_size;
        }
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(created_medicine.production_date, "%d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon+1, tm.tm_mday);

    int months = (tm.tm_mon + 1) + specs[selected_medicine_type].remaining_months_for_expiry;
    int year = tm.tm_year + 1900;

    defect = select_from_range(1, 100) < expire_date_defect_rate;

    if (defect) {
        // months = abs( months + 5 * ((rand() % 2 == 0)? 1 : -1) );
        months += 5;
        flag = true;
    }

    while (months > 12) {
        year++;
        months -= 12;
    }

    sprintf(created_medicine.expire_date, "%d-%02d-%02d", year, months, tm.tm_mday);

    printf("-----\n");
    fflush(stdout);

    printf(
        "(PRODUCED: %d) [Serial Number: %d, Type: %d] num_pills: %d, production Date: %s, expire Date: %s\n",
        production_line_number, created_medicine.serial_number, created_medicine.type,
        created_medicine.num_pills,created_medicine.production_date, created_medicine.expire_date
    );
    fflush(stdout);

    for (int i = 0; i < num_pills; i++) {
        printf(
            "Pill[%d]: Type: %d Color(%d, %d, %d) size: %d\n", i, created_medicine.pills[i].type,
            created_medicine.pills[i].colorR, created_medicine.pills[i].colorG, created_medicine.pills[i].colorB,
            created_medicine.pills[i].size
        );
        fflush(stdout);
    }

    printf("Defected: %d\n", flag);
    fflush(stdout);

    return created_medicine;
}

void write_speed_to_shmem(int production_line_number, float my_inspection_speed, float my_packaging_speed) {

    shared_memory[production_line_number].inspection_speed = my_inspection_speed;
    shared_memory[production_line_number].packaging_speed = my_packaging_speed;

    printf(
        "(LINE: %d) My Inspection Speed: %.2f, My Packaging Speed: %.2f, Memory Inspection: %.2f, Memory Packaging: %.2f\n",
        production_line_number, my_inspection_speed, my_packaging_speed, shared_memory[production_line_number].inspection_speed,
        shared_memory[production_line_number].packaging_speed
    );
    fflush(stdout);
}

void write_and_check_speeds(int production_line_number, float speed_threshold, int num_production_lines,
                            int* num_sent_inspectors, int* num_sent_packagers)
{
    float max_inspection_speed = 0, min_inspection_speed = 1000.0;
    float max_packaging_speed = 0, min_packaging_speed = 1000.0;
    int index_with_min_inspection_speed = 0, index_with_min_packaging_speed = 0;

    pthread_mutex_lock(&inspection_overflow_mutex);
    float my_inspection_speed = 1. / (inspection_overflow + 1e-3);
    pthread_mutex_unlock(&inspection_overflow_mutex);

    pthread_mutex_lock(&packaging_overflow_mutex);
    float my_packaging_speed = 1. / (packaging_overflow + 1e-3);
    pthread_mutex_unlock(&packaging_overflow_mutex);

    semaphore_acquire(0, speed_sem_id, &acquire);

    /* Critical Section */
    // Write the speed to the shared memory
    write_speed_to_shmem(production_line_number, my_inspection_speed, my_packaging_speed);

    // check max and min speeds
    for (int i = 0; i < num_production_lines; i++) {

        if (shared_memory[i].inspection_speed > max_inspection_speed)
            max_inspection_speed = shared_memory[i].inspection_speed;
        
        if (shared_memory[i].inspection_speed < min_inspection_speed) {
            min_inspection_speed = shared_memory[i].inspection_speed;
            index_with_min_inspection_speed = i;
        }

        if (shared_memory[i].packaging_speed > max_packaging_speed)
            max_packaging_speed = shared_memory[i].packaging_speed;

        if (shared_memory[i].packaging_speed < min_packaging_speed) {
            min_packaging_speed = shared_memory[i].packaging_speed;
            index_with_min_packaging_speed = i;
        }
    }

    printf(
        "(LINE %d): max_inspection speed: %.2f, min_inspection_speed: %.2f, min_index: %d, Packaging(max: %.2f,min: %.2f,min_index: %d)\n",
        production_line_number, max_inspection_speed, min_inspection_speed, index_with_min_inspection_speed,
        max_packaging_speed, min_packaging_speed, index_with_min_packaging_speed
    );
    fflush(stdout);

    if (max_inspection_speed == my_inspection_speed && index_with_min_inspection_speed != production_line_number) {

        if (min_inspection_speed < speed_threshold && num_inspectors > 1) {
            shared_memory[index_with_min_inspection_speed].inspection_speed += speed_threshold;

            /* Transfer an inspector */
            EmployeeTransferMessage msg;

            msg.production_line_number = index_with_min_inspection_speed + 1;
            msg.num_inspectors = 1;
            msg.num_packagers = 0;

            *num_sent_inspectors = msg.num_inspectors;

            printf("(LINE: %d) Sending %d inspectors\n", production_line_number, *num_sent_inspectors);
            fflush(stdout);

            if ( msgsnd(emp_transfer_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend Inspection");
                semaphore_release(0, speed_sem_id, &release);
                pthread_exit( (void*) -1 );
            }
        }
    }
    if (min_packaging_speed < speed_threshold && max_packaging_speed == my_packaging_speed) {

        if (index_with_min_packaging_speed != -1 && num_packagers > 1) {
            shared_memory[index_with_min_packaging_speed].packaging_speed += speed_threshold;

            /* Transfer a packager */
            EmployeeTransferMessage msg;
            
            msg.production_line_number = index_with_min_packaging_speed + 1;
            msg.num_inspectors = 0;
            msg.num_packagers = 1;

            *num_sent_packagers = msg.num_packagers;

            printf("(LINE: %d) Sending %d Packagers\n", production_line_number, *num_sent_packagers);
            fflush(stdout);

            if ( msgsnd(emp_transfer_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend Packager");
                semaphore_release(0, speed_sem_id, &release);
                pthread_exit( (void*) -1 );
            }
        }
    }
    /* End of Critical Section */
    semaphore_release(0, speed_sem_id, &release);
}

/**
 * Detach this Process from shared memory, then exit.
*/
void end_program() {
    detach_memory(shared_memory);
    freeQueue(&created_medicine_queue);

    for (int i = 0; i< num_medicine_types; i++)
        freeQueue(&non_defected_medicine_queue[i]);
    
    exit(-1);
}

int main(int argc, char** argv) {

    if (argc < 17) {
        perror("Not enough args Pill Production Line");
        exit(-1);
    }

    int min_inspectors = atoi(strtok(argv[1], "-"));
    int max_inspectors = atoi(strtok('\0', "-"));

    int min_packagers = atoi(strtok(argv[2], "-"));
    int max_packagers = atoi(strtok('\0', "-"));

    srand(getpid());

    num_inspectors = select_from_range(min_inspectors, max_inspectors); /* Number of threads */
    num_packagers = select_from_range(min_packagers, max_packagers);    /* Number of threads */

    num_medicine_types = atoi(argv[3]);

    int min_time_between_each_production = atoi(strtok(argv[4], "-"));
    int max_time_between_each_production = atoi(strtok('\0', "-"));

    min_months_before_expiry = atoi(strtok(argv[5], "-"));
    max_months_before_expiry = atoi(strtok('\0', "-"));

    missing_pills_defect_rate = atof(argv[6]);
    pill_color_defect_rate = atof(argv[7]);
    pill_size_defect_rate = atoi(argv[8]);
    expire_date_defect_rate = atoi(argv[9]);

    min_time_for_inspection = atoi(strtok(argv[10], "-"));
    max_time_for_inspection = atoi(strtok('\0', "-"));

    min_time_for_packaging = atoi(strtok(argv[11], "-"));
    max_time_for_packaging = atoi(strtok('\0', "-"));

    min_num_pills_per_container = atoi(strtok(argv[12], "-"));
    max_num_pills_per_container = atoi(strtok('\0', "-"));

    min_num_plastic_containers_per_pill_medicine = atoi(strtok(argv[13], "-"));
    max_num_plastic_containers_per_pill_medicine = atoi(strtok('\0', "-"));

    production_line_number = atoi(argv[14]);
    int number_of_production_lines = atoi(argv[15]);
    speed_threshold = atof(argv[16]);
    
    key_t feedback_queue_key = ftok(".", 'Q');
    key_t drawer_queue_key = ftok(".", 'D');
    key_t emp_transfer_queue_key = ftok(".", 'E');
    key_t speed_shmem_key = ftok(".", 'M');
    key_t speed_sem_key = ftok(".", 'S');

    if ( (feedback_queue_id = msgget(feedback_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    if ( (emp_transfer_queue_id = msgget(emp_transfer_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    if ( (drawer_queue_id = msgget(drawer_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    // create or retrieve the semaphore
    if ( (speed_sem_id = semget(speed_sem_key, 1, IPC_CREAT | 0660)) == -1 ) {
        perror("semget: IPC_CREAT | 0660");
        exit(1);
    }

    // Create or retrieve the shared memory segment
    if ((speed_shmem_id = shmget(speed_shmem_key, 1, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }

    // attach the shared memory
    if ((shared_memory = shmat(speed_shmem_id, NULL, 0)) == (MemoryCell*) -1) {
        perror("shmat");
        exit(1);
    }

    // initialize specs, and produced medicine queue
    initialize_specs();
    initQueue(&created_medicine_queue, sizeof(PlasticContainer));

    for (int i = 0; i < num_medicine_types; i++) {
        initQueue(&non_defected_medicine_queue[i], sizeof(PlasticContainer));
    }

    // create the threads
    pthread_t inspectors[MAX_THREADS];
    pthread_t packagers[MAX_THREADS];

    int inspectors_numbers[MAX_THREADS];
    int packagers_numbers[MAX_THREADS];

    for (int i = 0; i < MAX_THREADS; i++)
        inspectors_numbers[i] = i;

    for (int i = 0; i < MAX_THREADS; i++)
        packagers_numbers[i] = i;

    // create inspectors
    for (int i = 0; i < num_inspectors; i++)
        pthread_create(&inspectors[i], NULL, inspection, (void*) &inspectors_numbers[i]);

    // create packagers
    for (int i = 0; i < num_packagers; i++)
        pthread_create(&packagers[i], NULL, packaging, (void*) &packagers_numbers[i]);

    // initialize the shared memory
    semaphore_acquire(0, speed_sem_id, &acquire);

    shared_memory[production_line_number].inspection_speed = speed_threshold;
    shared_memory[production_line_number].packaging_speed = speed_threshold;

    semaphore_release(0, speed_sem_id, &release);

    printf("(LINE_PILL: %d)Inspectors: %d, Packagers: %d\n", production_line_number, num_inspectors, num_packagers);
    fflush(stdout);

    InitialMessage init_msg;
    init_msg.type = INITIAL;
    init_msg.production_line_number = production_line_number;
    init_msg.num_inspectors = num_inspectors;
    init_msg.num_packagers = num_packagers;

    if ( msgsnd(drawer_queue_id, &init_msg, sizeof(init_msg), 0) == -1 ) {
        perror("Child: msgsend Production Init");
        pthread_exit( (void*) -1 );
    }

    // producing medicine
    while(1) {
        PlasticContainer created_plastic_container = produce_medicine();

        DrawerMessage drawer_msg;
        drawer_msg.operation_type = PRODUCTION;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.medicine_type = PILL;
        drawer_msg.worker_index = 0;
        memcpy(&drawer_msg.medicine.plastic_container, &created_plastic_container, sizeof(created_plastic_container));

        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
            perror("Child: msgsend Production PILLLLLLS");
            pthread_exit( (void*) -1 );
        }

        pthread_mutex_lock(&created_medicine_queue_mutex);

        enqueue(&created_medicine_queue, &created_plastic_container);
        printf("%d\n", getSize(&created_medicine_queue));
        printf("---------------------------------\n");
        fflush(stdout);

        pthread_mutex_lock(&inspection_overflow_mutex);
        inspection_overflow = getSize(&created_medicine_queue);
        pthread_mutex_unlock(&inspection_overflow_mutex);

        sleep(1);

        pthread_cond_signal(&empty_created_medicine_queue_cv);

        pthread_mutex_unlock(&created_medicine_queue_mutex);

        srand(time(NULL) + getpid());

        sleep( select_from_range(min_time_between_each_production, max_time_between_each_production) );

        // read from message queue if there is an employee transferred to this production line
        EmployeeTransferMessage msg;

        while ( msgrcv(emp_transfer_queue_id, &msg, sizeof(msg), production_line_number+1, IPC_NOWAIT) != -1 ) {
            printf("(MESSAGE: %d) Inspectors: %d, Packagers: %d\n", production_line_number, msg.num_inspectors, msg.num_packagers);
            fflush(stdout);

            for (int i = 0; i < msg.num_inspectors; i++) {
                pthread_t new_inspector;

                pthread_create(&new_inspector, NULL, inspection, (void*) &inspectors_numbers[num_inspectors]);

                DrawerMessage drawer_msg;
                drawer_msg.operation_type = INSPECTOR_RECEIVED;
                drawer_msg.production_line_number = production_line_number;

                if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
                    perror("Child: msgsend Production Mahmoud");
                    pthread_exit( (void*) -1 );
                }

                inspectors[num_inspectors] = new_inspector;
                num_inspectors++;
            }

            for (int i = 0; i < msg.num_packagers; i++) {
                pthread_t new_packager;

                pthread_create(&new_packager, NULL, packaging, (void*) &packagers_numbers[num_packagers]);

                DrawerMessage drawer_msg;
                drawer_msg.operation_type = PACKAGER_RECEIVED;
                drawer_msg.production_line_number = production_line_number;

                if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
                    perror("Child: msgsend Production Mahmoud");
                    pthread_exit( (void*) -1 );
                }

                packagers[num_packagers] = new_packager;
                num_packagers++;
            }
        }

        if (errno == ENOMSG) {
            int num_sent_inspectors = 0, num_sent_packager = 0;

            write_and_check_speeds(
                production_line_number, speed_threshold, number_of_production_lines,
                &num_sent_inspectors, &num_sent_packager
            );

            srand(time(NULL) + getpid());

            for (int i = 0; i < num_sent_inspectors; i++) {
                
                if (num_inspectors < 2) 
                    break;

                int random_inspector = select_from_range(0, num_inspectors - 1);

                pthread_cancel(inspectors[random_inspector]);

                DrawerMessage drawer_msg;
                drawer_msg.operation_type = INSPECTOR_CANCELLED;
                drawer_msg.production_line_number = production_line_number;
                drawer_msg.worker_index = random_inspector;

                if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
                    perror("Child: msgsend Production Mahmoud");
                    pthread_exit( (void*) -1 );
                }

                // delete that thread from memory
                inspectors[random_inspector] = inspectors[num_inspectors-1];
                num_inspectors--;
            }

            for (int i = 0; i < num_sent_packager; i++) {

                if (num_packagers < 2) 
                    break;

                int random_packager = select_from_range(0, num_packagers - 1);

                pthread_cancel(packagers[random_packager]);

                DrawerMessage drawer_msg;
                drawer_msg.operation_type = PACKAGER_CANCELLED;
                drawer_msg.production_line_number = production_line_number;
                drawer_msg.worker_index = random_packager;

                if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
                    perror("Child: msgsend Production Mahmoud");
                    pthread_exit( (void*) -1 );
                }

                // delete that thread from memory
                packagers[random_packager] = packagers[num_inspectors-1];
                num_packagers--;
            }
        }
    }

    // wait for threads
    for (int i = 0; i < num_inspectors; i++)
        pthread_join(inspectors[i], NULL);  

    for (int i = 0; i < num_packagers; i++)
        pthread_join(packagers[i], NULL);

    end_program();

    return 0;
}
