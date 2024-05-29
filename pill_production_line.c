#include "headers.h"
#include "functions.h"

pthread_mutex_t created_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t non_defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t defected_medicine_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packaged_medicines_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t empty_created_medicine_queue_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t empty_non_defected_queue_cv = PTHREAD_COND_INITIALIZER;

int num_medicine_types;
PillMedicineSpecs specs[200];
Pill pill_specs[200];

Queue created_medicine_queue;
Queue non_defected_medicine_queue[100];
Queue defected_medicine_queue;
Queue packaged_medicines;

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

int message_queue_id;
time_t start_time;
float speed = 1.0;

void* inspection(void* data) {
    PlasticContainer medicine;

    int my_number = *((int*) data);

    while (1) {

        bool defect = false;

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
        if (medicine.num_pills != specs[medicine.type].num_pills
            || production_year != printed_year || production_month != printed_month)
        {
            defect = true;
        }

        for (int i = 0; i < medicine.num_pills; i++) { /* check each pill */

            if (medicine.pills[i].colorR != specs[medicine.type].pill_colorR
                || medicine.pills[i].colorG != specs[medicine.type].pill_colorG
                || medicine.pills[i].colorB != specs[medicine.type].pill_colorB
                || medicine.pills[i].size != specs[medicine.type].pill_size)
            {
                defect = true;
                break;
            }
        }

        if (defect) {
            pthread_mutex_lock(&defected_medicine_queue_mutex);
            enqueue(&defected_medicine_queue, &medicine);
            // enqueue(&defected_medicine_queue[medicine.type], &medicine);

            printf("An (INSPECTOR): %d, put a medicine [%d] in Trash (%d)\n", my_number, medicine.serial_number, getSize(&defected_medicine_queue));
            fflush(stdout);

            pthread_mutex_unlock(&defected_medicine_queue_mutex);

            // send to main.c (parent process) that a medicine is defected, using a message queue.
            Message msg;
            msg.message_type = DEFECTED_PILL_MEDICINE;

            if ( msgsnd(message_queue_id, &msg, sizeof(msg), 0) == -1 ) {
                perror("Child: msgsend");
                pthread_exit( (void*) -1 );
            }
        } else {
            pthread_mutex_lock(&non_defected_medicine_queue_mutex);
            enqueue(&non_defected_medicine_queue[medicine.type], &medicine);

            printf("An (INSPECTOR): %d, put a medicine of type %d in Non Defected array\n", my_number, medicine.type);
            fflush(stdout);

            pthread_cond_signal(&empty_non_defected_queue_cv);

            pthread_mutex_unlock(&non_defected_medicine_queue_mutex);
        }
    }
}

void* packaging(void* data) {
    PlasticContainer medicine;
    PillPackage package;

    int my_number = *((int*) data);

    printf("Hello from: %d\n", my_number);

    while (1) {
        pthread_mutex_lock(&non_defected_medicine_queue_mutex);

        bool no_medicine_to_package = true, medicine_was_packages = false;

        for (int i = 0; i < num_medicine_types; i++) {
            if (non_defected_medicine_queue[i].size >= specs[i].num_plastic_containers)
                no_medicine_to_package = false;
        }

        if (no_medicine_to_package)
            pthread_cond_wait(&empty_non_defected_queue_cv, &non_defected_medicine_queue_mutex);

        for (int i = 0; i < num_medicine_types; i++) {

            if (non_defected_medicine_queue[i].size >= specs[i].num_plastic_containers) {

                medicine_was_packages = true;

                for (int j = 0; j < specs[i].num_plastic_containers; j++) {

                    if (dequeue(&non_defected_medicine_queue[i], &medicine) != -1) {
                        memcpy(&package.containers[j], &medicine, sizeof(medicine));

                        printf("(PACKAGER) %d put medicine [%d] of type %d in a package\n", my_number, medicine.serial_number, specs[i].type);
                        fflush(stdout);
                    }
                }
                break;
            }
        }
        pthread_mutex_unlock(&non_defected_medicine_queue_mutex);

        if (!medicine_was_packages)
            continue;

        strcpy(package.prescription, "Please Follow Your Doctor's Instructions");

        sleep( select_from_range(min_time_for_packaging, max_time_for_packaging) );

        pthread_mutex_lock(&packaged_medicines_mutex);
        enqueue(&packaged_medicines, &package);
        pthread_mutex_unlock(&packaged_medicines_mutex);

        if (getSize(&packaged_medicines) == 1) {
            start_time = time(NULL);
        }

        Message msg;
        msg.message_type = PRODUCED_PILL_MEDICINE;
        msg.medicine_type = medicine.type;

        if (msgsnd(message_queue_id, &msg, sizeof(msg), 0) == -1 ) {
            perror("Child: msgsend");
            pthread_exit( (void*) -1 );
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
        "(PRODUCED) [Serial Number: %d, Type: %d] num_pills: %d, production Date: %s, expire Date: %s\n",
        created_medicine.serial_number, created_medicine.type, created_medicine.num_pills,
        created_medicine.production_date, created_medicine.expire_date
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

int main(int argc, char** argv) {

    if (argc < 15) {
        perror("Not enough args Pill Production Line");
        exit(-1);
    }

    int min_inspectors = atoi(strtok(argv[1], "-"));
    int max_inspectors = atoi(strtok('\0', "-"));

    int min_packagers = atoi(strtok(argv[2], "-"));
    int max_packagers = atoi(strtok('\0', "-"));

    int num_inspectors = select_from_range(min_inspectors, max_inspectors); /* Number of threads */
    int num_packagers = select_from_range(min_packagers, max_packagers);    /* Number of threads */

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

    message_queue_id = atoi(argv[12]);

    min_num_pills_per_container = atoi(strtok(argv[13], "-"));
    max_num_pills_per_container = atoi(strtok('\0', "-"));

    min_num_plastic_containers_per_pill_medicine = atoi(strtok(argv[14], "-"));
    max_num_plastic_containers_per_pill_medicine = atoi(strtok('\0', "-"));
    
    printf("HEllo world!!!\n");

    // initialize specs, and produced medicine queue
    initialize_specs();
    initQueue(&created_medicine_queue, sizeof(PlasticContainer));
    initQueue(&defected_medicine_queue, sizeof(PlasticContainer));
    initQueue(&packaged_medicines, sizeof(PillPackage));

    for (int i = 0; i < num_medicine_types; i++){
        initQueue(&non_defected_medicine_queue[i], sizeof(PlasticContainer));
    }

    // create the threads
    pthread_t inspectors[num_inspectors];
    pthread_t packagers[num_packagers];

    int inspectors_numbers[num_inspectors];
    int packagers_numbers[num_packagers];

    // create inspectors
    for (int i = 0; i < num_inspectors; i++)
        inspectors_numbers[i] = i;

    // create packagers
    for (int i = 0; i < num_packagers; i++)
        packagers_numbers[i] = i;

    // create inspectors
    for (int i = 0; i < num_inspectors; i++)
        pthread_create(&inspectors[i], NULL, inspection, (void*) &inspectors_numbers[i]);

    // create packagers
    for (int i = 0; i < num_packagers; i++)
        pthread_create(&packagers[i], NULL, packaging, (void*) &packagers_numbers[i]);

    // producing medicine
    while(1) {
        PlasticContainer created_plastic_container = produce_medicine();

        pthread_mutex_lock(&created_medicine_queue_mutex);

        enqueue(&created_medicine_queue, &created_plastic_container);
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
    freeQueue(&packaged_medicines);

    for (int i = 0; i< num_medicine_types; i++){
        freeQueue(&non_defected_medicine_queue[i]);
    }

    return 0;
}
