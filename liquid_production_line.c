#include "headers.h"
#include "functions.h"

MemoryCell* shared_memory;

struct sembuf acquire = {0, -1, SEM_UNDO};
struct sembuf release = {0, 1, SEM_UNDO};

pthread_mutex_t created_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t non_defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty_created_medicine_queue_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_non_defected_queue_cv = PTHREAD_COND_INITIALIZER;

int num_medicine_types;
int production_line_number;

LiquidSpecs specs[200];

Queue created_medicine_queue;
Queue non_defected_medicine_queue;

int num_inspectors;
int num_packagers;

int max_months_before_expiry;
int min_months_before_expiry;

int min_time_for_inspection;
int max_time_for_inspection;

int min_time_for_packaging;
int max_time_for_packaging;

int serial_counter = 0;

int liquid_level_defect_rate, color_defect_rate, sealed_defect_rate,
    expire_date_defect_rate, correct_label_defect_rate, label_place_defect_rate;

float speed_threshold;

int feedback_queue_id, emp_transfer_queue_id, drawer_queue_id;
int speed_sem_id, speed_shmem_id;


void cleanup_handler(void *arg) {
    pthread_mutex_t *mutex = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mutex);
}

void* inspection(void* data) {
    LiquidMedicine medicine;

    int my_number = *((int*) data);

    int last_state, last_type;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state); 
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_type);

    while (1) {
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

        pthread_mutex_unlock(&created_medicine_queue_mutex);

        DrawerMessage drawer_msg;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.operation_type = INSPECTION_START;
        drawer_msg.medicine_type = LIQUID;
        drawer_msg.worker_index = my_number;
        memcpy(&drawer_msg.medicine.liquid_medicine, &medicine, sizeof(medicine));

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
        if (medicine.type != medicine.label || !medicine.isSealed
            || medicine.liquid_level > specs[medicine.type].max_liquid_level
            || medicine.liquid_level < specs[medicine.type].min_liquid_level
            || medicine.label_position != specs[medicine.type].label_position
            || medicine.colorR != specs[medicine.type].colorR || medicine.colorG != specs[medicine.type].colorG
            || medicine.colorB != specs[medicine.type].colorB || production_year != printed_year || production_month != printed_month)
        {
            printf("An (INSPECTOR): %d, put a medicine [%d] in Trash\n", my_number, medicine.serial_number);
            fflush(stdout);

            drawer_msg.operation_type = INSPECTION_FAILED;

            // send to main.c (parent process) that a medicine is defected, using a message queue.
            FeedBackMessage msg;
            msg.message_type = DEFECTED_LIQUID_MEDICINE;

            if (msgsnd(feedback_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }
        } else {
            pthread_mutex_lock(&non_defected_medicine_queue_mutex);
            enqueue(&non_defected_medicine_queue, &medicine);

            printf("An (INSPECTOR): %d, put a medicine [%d] in Non Defected\n", my_number, medicine.serial_number);
            fflush(stdout);

            drawer_msg.operation_type = INSPECTION_SUCCESSFUL;

            pthread_cond_signal(&empty_non_defected_queue_cv);

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.worker_index = my_number;
        memcpy(&drawer_msg.medicine.liquid_medicine, &medicine, sizeof(medicine));

        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
            perror("Child: msgsend Production");
            pthread_exit( (void*) -1 );
        }
        pthread_testcancel();
    }
}

void* packaging(void* data) {
    LiquidMedicine medicine;
    LiquidPackage package;

    int my_number = *((int*) data);

    int last_state, last_type;

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &last_state);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &last_type);

    while (1) {
        pthread_mutex_lock(&non_defected_medicine_queue_mutex);

        pthread_cleanup_push(cleanup_handler, &non_defected_medicine_queue_mutex);

        if (isEmpty(&non_defected_medicine_queue))
            pthread_cond_wait(&empty_non_defected_queue_cv, &non_defected_medicine_queue_mutex);

        pthread_cleanup_pop(0);

        DrawerMessage drawer_msg;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.operation_type = PACKAGING_START;
        drawer_msg.medicine_type = LIQUID;
        drawer_msg.worker_index = my_number;
        memcpy(&drawer_msg.medicine.liquid_medicine, &medicine, sizeof(medicine));

        if (dequeue(&non_defected_medicine_queue, &medicine) != -1) {

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
            
            // start packaging
            if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
                perror("Child: msgsend Production");
                pthread_exit( (void*) -1 );
            }

            sleep( select_from_range(min_time_for_packaging, max_time_for_packaging) );

            memcpy(&package.medicine, &medicine, sizeof(medicine));
            strcpy(package.prescription, "Please Follow Your Doctor's Instructions");

            printf("A (PACKAGER): %d, put medicine [%d] in Packaged Queue\n", my_number, medicine.serial_number);
            fflush(stdout);

            // finish packaging
            drawer_msg.operation_type = PACKAGING_END;
            memcpy(&drawer_msg.medicine.liquid_medicine, &medicine, sizeof(medicine));

            if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(drawer_msg), 0) == -1 ) {
                perror("Child: msgsend Production");
                pthread_exit( (void*) -1 );
            }

            FeedBackMessage msg;
            msg.message_type = PRODUCED_LIQUID_MEDICINE;
            msg.medicine_type = medicine.type;

            if (msgsnd(feedback_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }

        } else {
            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }
        pthread_testcancel();
    }
}

void initialize_specs() {

    printf("Specs: \n");
    fflush(stdout);

    for (int i = 0; i < num_medicine_types; i++) {
        
        srand(time(NULL) + getpid() + i);

        specs[i].min_liquid_level = select_from_range(70, 80);
        specs[i].max_liquid_level = select_from_range(81, 90);
        
        specs[i].label = i;
        specs[i].colorR = select_from_range(50, 200);
        specs[i].colorG = select_from_range(50, 200);
        specs[i].colorB = select_from_range(50, 200);
        specs[i].label_position = select_from_range(0, 10) / 10.0;

        specs[i].remaining_months_for_expiry = select_from_range(min_months_before_expiry, max_months_before_expiry);

        printf(
            "[label: %d]: min_liquid_level: %d, max_liquid_level: %d, remaining_months: %d, Color(R=%d, G=%d, B=%d)\n",
            specs[i].label, specs[i].min_liquid_level, specs[i].max_liquid_level, specs[i].remaining_months_for_expiry,
            specs[i].colorR, specs[i].colorG, specs[i].colorB
        );
        fflush(stdout);
    }
    printf("--------------\n");
    fflush(stdout);
}

LiquidMedicine produce_medicine() {
    LiquidMedicine created_medicine;
    bool flag = false;

    srand(time(NULL) + getpid());
    
    int selected_medicine_type = select_from_range(0, num_medicine_types-1);
    created_medicine.type = specs[selected_medicine_type].label;
    created_medicine.serial_number = serial_counter;
    serial_counter++;

    bool defect = select_from_range(1, 100) < liquid_level_defect_rate;

    if (defect) {
        flag = true;
        created_medicine.liquid_level = select_from_range(
            specs[selected_medicine_type].max_liquid_level + 10,
            specs[selected_medicine_type].max_liquid_level + 20
        );
    }
    else
        created_medicine.liquid_level = select_from_range(
            specs[selected_medicine_type].min_liquid_level,
            specs[selected_medicine_type].max_liquid_level
        );

    defect = select_from_range(1, 100) < color_defect_rate;

    if (defect) {
        flag = true;

        created_medicine.colorR = abs(specs[selected_medicine_type].colorR + 30 * ((rand() % 2 == 0)? 1 : -1));
        created_medicine.colorG = abs(specs[selected_medicine_type].colorG + 30 * ((rand() % 2 == 0)? 1 : -1));
        created_medicine.colorB = abs(specs[selected_medicine_type].colorB + 30 * ((rand() % 2 == 0)? 1 : -1));
    } else {
        created_medicine.colorR = specs[selected_medicine_type].colorR;
        created_medicine.colorG = specs[selected_medicine_type].colorG;
        created_medicine.colorB = specs[selected_medicine_type].colorB;
    }

    defect = select_from_range(1, 100) < correct_label_defect_rate;

    if (defect) {
        flag = true;

        int wrong_label;

        while ( (wrong_label = select_from_range(0, num_medicine_types)) == created_medicine.type );
        created_medicine.label = wrong_label;
    } else {
        created_medicine.label = specs[selected_medicine_type].label;
    }

    defect = select_from_range(1, 100) < label_place_defect_rate;
    
    if (defect) {
        flag = true;
        created_medicine.label_position = abs(specs[selected_medicine_type].label_position + 0.2 * ((rand() % 2 == 0)? 1 : -1));
    } else {
        created_medicine.label_position = specs[selected_medicine_type].label_position;
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(created_medicine.production_date, "%d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon+1, tm.tm_mday);

    int months = (tm.tm_mon + 1) + specs[selected_medicine_type].remaining_months_for_expiry;
    int year = tm.tm_year + 1900;

    while (months > 12) {
        year++;
        months -= 12;
    }

    defect = select_from_range(1, 100) < expire_date_defect_rate;

    if (defect) {
        flag = true;
        sprintf(created_medicine.expire_date, "%d-%02d-%02d", year, months + 5, tm.tm_mday);
    }
    else
        sprintf(created_medicine.expire_date, "%d-%02d-%02d", year, months, tm.tm_mday);

    defect = select_from_range(1, 100) < sealed_defect_rate;

    if (defect) {
        flag = true;
        created_medicine.isSealed = false;
    }
    else
        created_medicine.isSealed = true;

    printf("-----\n");
    fflush(stdout);

    printf(
        "(PRODUCED: %d) [Serial Number: %d, Type: %d] Label: %d, liquid_level: %d, Color(R:%d, G:%d, B:%d) sealed: %d, production Date: %s, expire Date: %s\n",
        production_line_number, created_medicine.serial_number, created_medicine.type,
        created_medicine.label, created_medicine.liquid_level,
        created_medicine.colorR, created_medicine.colorG, created_medicine.colorB,
        created_medicine.isSealed, created_medicine.production_date, created_medicine.expire_date
    );
    fflush(stdout);

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

    pthread_mutex_lock(&created_medicine_queue_mutex);
    float my_inspection_speed = 1. / (getSize(&created_medicine_queue) + 1e-3);
    pthread_mutex_unlock(&created_medicine_queue_mutex);

    pthread_mutex_lock(&non_defected_medicine_queue_mutex);
    float my_packaging_speed = 1. / (getSize(&non_defected_medicine_queue) + 1e-3);
    pthread_mutex_unlock(&non_defected_medicine_queue_mutex);

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
    freeQueue(&non_defected_medicine_queue);
    exit(-1);
}

int main(int argc, char** argv) {

    if (argc < 17) {
        perror("Not enough args Liquid");
        exit(-1);
    }

    int min_inspectors = atoi(strtok(argv[1], "-"));
    int max_inspectors = atoi(strtok('\0', "-"));

    int min_packagers = atoi(strtok(argv[2], "-"));
    int max_packagers = atoi(strtok('\0', "-"));

    srand(getpid());

    num_inspectors = select_from_range(min_inspectors, max_inspectors); /* Number of threads */
    num_packagers = select_from_range(min_packagers, max_packagers);   /* Number of threads */

    num_medicine_types = atoi(argv[3]);

    int min_time_between_each_production = atoi(strtok(argv[4], "-"));
    int max_time_between_each_production = atoi(strtok('\0', "-"));

    min_months_before_expiry = atoi(strtok(argv[5], "-"));
    max_months_before_expiry = atoi(strtok('\0', "-"));

    liquid_level_defect_rate = atof(argv[6]);
    color_defect_rate = atoi(argv[7]);
    sealed_defect_rate = atoi(argv[8]);
    expire_date_defect_rate = atoi(argv[9]);
    correct_label_defect_rate = atoi(argv[10]);
    label_place_defect_rate = atoi(argv[11]);

    min_time_for_inspection = atoi(strtok(argv[12], "-"));
    max_time_for_inspection = atoi(strtok('\0', "-"));

    min_time_for_packaging = atoi(strtok(argv[13], "-"));
    max_time_for_packaging = atoi(strtok('\0', "-"));

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
    initQueue(&created_medicine_queue, sizeof(LiquidMedicine));
    initQueue(&non_defected_medicine_queue, sizeof(LiquidMedicine));

    // create the threads
    pthread_t inspectors[MAX_THREADS];
    pthread_t packagers[MAX_THREADS];

    int inspectors_numbers[MAX_THREADS];
    int packagers_numbers[MAX_THREADS];

    // create inspectors
    for (int i = 0; i < MAX_THREADS; i++)
        inspectors_numbers[i] = i;

    // create packagers
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

    printf("(LINE: %d)Inspectors: %d, Packagers: %d\n", production_line_number, num_inspectors, num_packagers);
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

    // pause();

    // producing medicine
    while(1) {
        LiquidMedicine created_medicine = produce_medicine();

        DrawerMessage drawer_msg;
        drawer_msg.operation_type = PRODUCTION;
        drawer_msg.production_line_number = production_line_number;
        drawer_msg.medicine_type = LIQUID;
        drawer_msg.worker_index = 0;
        memcpy(&drawer_msg.medicine.liquid_medicine, &created_medicine, sizeof(created_medicine));

        if ( msgsnd(drawer_queue_id, &drawer_msg, sizeof(DrawerMessage), 0) == -1 ) {
            perror("Child: msgsend Production Mahmoud");
            pthread_exit( (void*) -1 );
        }

        // pause();

        pthread_mutex_lock(&created_medicine_queue_mutex);

        enqueue(&created_medicine_queue, &created_medicine);
        printf("%d\n", getSize(&created_medicine_queue));
        printf("---------------------------------\n");
        fflush(stdout);

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

                inspectors[num_inspectors] = new_inspector;
                num_inspectors++;
            }

            for (int i = 0; i < msg.num_packagers; i++) {
                pthread_t new_packager;

                pthread_create(&new_packager, NULL, packaging, (void*) &packagers_numbers[num_packagers]);

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

                // delete that thread from memory
                inspectors[random_inspector] = inspectors[num_inspectors-1];
                num_inspectors--;
            }

            for (int i = 0; i < num_sent_packager; i++) {

                if (num_packagers < 2) 
                    break;

                int random_packager = select_from_range(0, num_packagers - 1);

                pthread_cancel(packagers[random_packager]);

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

    freeQueue(&created_medicine_queue);
    freeQueue(&non_defected_medicine_queue);

    return 0;
}