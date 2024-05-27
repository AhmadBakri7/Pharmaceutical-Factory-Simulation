#include "headers.h"
#include "functions.h"

pthread_mutex_t created_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t non_defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packaged_medicines_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty_created_medicine_queue_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_non_defected_queue_cv = PTHREAD_COND_INITIALIZER;

int num_medicine_types;
LiquidSpecs specs[200];

Queue created_medicine_queue;
Queue non_defected_medicine_queue;
Queue defected_medicine_queue;
Queue packaged_medicines;

int max_months_before_expiry;
int min_months_before_expiry;

int min_time_for_inspection;
int max_time_for_inspection;

int min_time_for_packaging;
int max_time_for_packaging;

int serial_counter = 0;

int liquid_level_defect_rate, color_defect_rate, sealed_defect_rate,
    expire_date_defect_rate, correct_label_defect_rate, label_place_defect_rate;

int message_queue_id;
time_t start_time;
float speed = 1.0;

void* inspection(void* data) {
    LiquidMedicine medicine;

    int my_number = *((int*) data);

    while (1) {
        pthread_mutex_lock(&created_medicine_queue_mutex);

        if (isEmpty(&created_medicine_queue))
            pthread_cond_wait(&empty_created_medicine_queue_cv, &created_medicine_queue_mutex);
        
        if (dequeue(&created_medicine_queue, &medicine) == -1) {
            pthread_mutex_unlock(&created_medicine_queue_mutex);
            continue;
        }

        pthread_mutex_unlock(&created_medicine_queue_mutex);

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
            pthread_mutex_lock(&defected_medicine_queue_mutex);
            enqueue(&defected_medicine_queue, &medicine);

            printf("An (INSPECTOR): %d, put a medicine [%d] in Trash (%d)\n", my_number, medicine.serial_number, getSize(&defected_medicine_queue));
            fflush(stdout);

            pthread_mutex_unlock(&defected_medicine_queue_mutex);

            // send to main.c (parent process) that a medicine is defected, using a message queue.
            Message msg;
            msg.message_type = DEFECTED_LIQUID_MEDICINE;

            if (msgsnd(message_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }
        } else {
            pthread_mutex_lock(&non_defected_medicine_queue_mutex);
            enqueue(&non_defected_medicine_queue, &medicine);

            printf("An (INSPECTOR): %d, put a medicine [%d] in Non Defected\n", my_number, medicine.serial_number);
            fflush(stdout);

            pthread_cond_signal(&empty_non_defected_queue_cv);

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }
    }
}

void* packaging(void* data) {
    LiquidMedicine medicine;
    LiquidPackage package;

    int my_number = *((int*) data);

    while (1) {
        pthread_mutex_lock(&non_defected_medicine_queue_mutex);

        if (isEmpty(&non_defected_medicine_queue))
            pthread_cond_wait(&empty_non_defected_queue_cv, &non_defected_medicine_queue_mutex);

        if (dequeue(&non_defected_medicine_queue, &medicine) != -1) {

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);

            sleep( select_from_range(min_time_for_packaging, max_time_for_packaging) );

            memcpy(&package.medicine, &medicine, sizeof(medicine));
            strcpy(package.prescription, "Please Follow Your Doctor's Instructions");

            printf("A (PACKAGER): %d, put medicine [%d] in Packaged Queue (%d)\n", my_number, medicine.serial_number, getSize(&packaged_medicines));
            fflush(stdout);
            
            pthread_mutex_lock(&packaged_medicines_mutex);
            enqueue(&packaged_medicines, &package);

            if (getSize(&packaged_medicines) == 1) {
                start_time = time(NULL);
            }

            pthread_mutex_unlock(&packaged_medicines_mutex);

            Message msg;
            msg.message_type = PRODUCED_LIQUID_MEDICINE;
            msg.medicine_type = medicine.type;

            if (msgsnd(message_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }

        } else {
            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }

        if (start_time != time(NULL))
            speed = getSize(&packaged_medicines) / (time(NULL) - start_time);
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
        "(PRODUCED) [Serial Number: %d, Type: %d] Label: %d, liquid_level: %d, Color(R:%d, G:%d, B:%d) sealed: %d, production Date: %s, expire Date: %s\n",
        created_medicine.serial_number, created_medicine.type, created_medicine.label, created_medicine.liquid_level, 
        created_medicine.colorR, created_medicine.colorG, created_medicine.colorB,
        created_medicine.isSealed, created_medicine.production_date, created_medicine.expire_date
    );
    fflush(stdout);

    printf("Defected: %d\n", flag);
    fflush(stdout);

    return created_medicine;
}

int main(int argc, char** argv) {

    if (argc < 15) {
        perror("Not enough args");
        exit(-1);
    }

    int min_inspectors = atoi(strtok(argv[1], "-"));
    int max_inspectors = atoi(strtok('\0', "-"));

    int min_packagers = atoi(strtok(argv[2], "-"));
    int max_packagers = atoi(strtok('\0', "-"));

    int num_inspectors = select_from_range(min_inspectors, max_inspectors); /* Number of threads */
    int num_packagers = select_from_range(min_packagers, max_packagers);   /* Number of threads */

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

    message_queue_id = atoi(argv[14]);

    // initialize specs, and produced medicine queue
    initialize_specs();
    initQueue(&created_medicine_queue, sizeof(LiquidMedicine));
    initQueue(&non_defected_medicine_queue, sizeof(LiquidMedicine));
    initQueue(&defected_medicine_queue, sizeof(LiquidMedicine));
    initQueue(&packaged_medicines, sizeof(LiquidPackage));

    // create the threads
    pthread_t inspectors[num_inspectors];
    pthread_t packagers[num_packagers];

    int inspectors_ids[num_inspectors];
    int packagers_ids[num_packagers];

    // create inspectors
    for (int i = 0; i < num_inspectors; i++)
        inspectors_ids[i] = pthread_create(&inspectors[i], NULL, inspection, (void*)&i);

    // create packagers
    for (int i = 0; i < num_packagers; i++)
        packagers_ids[i] = pthread_create(&packagers[i], NULL, packaging, (void*)&i);

    // producing medicine
    while(1) {
        LiquidMedicine created_medicine = produce_medicine();

        pthread_mutex_lock(&created_medicine_queue_mutex);

        enqueue(&created_medicine_queue, &created_medicine);
        printf("%d\n", getSize(&created_medicine_queue));
        printf("---------------------------------\n");
        fflush(stdout);

        pthread_cond_signal(&empty_created_medicine_queue_cv);

        pthread_mutex_unlock(&created_medicine_queue_mutex);

        srand(time(NULL) + getpid());

        sleep( select_from_range(min_time_between_each_production, max_time_between_each_production) );
    }

    // wait for threads
    for (int i = 0; i < num_inspectors; i++)
        pthread_join(inspectors[i], NULL);  

    for (int i = 0; i < num_packagers; i++)
        pthread_join(packagers[i], NULL);

    freeQueue(&created_medicine_queue);
    freeQueue(&defected_medicine_queue);
    freeQueue(&non_defected_medicine_queue);
    freeQueue(&packaged_medicines);

    return 0;
}