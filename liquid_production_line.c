#include "headers.h"
#include "functions.h"

pthread_mutex_t inspection_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t packaging_mutex = PTHREAD_MUTEX_INITIALIZER;

int num_medicine_types;
LiquidSpecs specs[200];
Queue created_medicine_queue;

void* inspection(void* data) {

}

void* packaging(void* data) {

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

        specs[i].remaining_months_for_expiry = select_from_range(6, 24);

        printf(
            "[label: %d]: min_liquid_level: %d, max_liquid_level: %d, remaining_months: %d\n",
             specs[i].label, specs[i].min_liquid_level, specs[i].max_liquid_level, specs[i].remaining_months_for_expiry
        );
        fflush(stdout);
    }
    printf("--------------\n");
    fflush(stdout);
}

LiquidMedicine produce_medicine() {
    LiquidMedicine created_medicine;

    int selected_medicine_type = select_from_range(0, num_medicine_types-1);
    created_medicine.label = specs[selected_medicine_type].label;

    created_medicine.liquid_level = select_from_range(
        specs[selected_medicine_type].min_liquid_level,
        specs[selected_medicine_type].max_liquid_level
    );
    created_medicine.colorR = specs[selected_medicine_type].colorR;
    created_medicine.colorG = specs[selected_medicine_type].colorG;
    created_medicine.colorB = specs[selected_medicine_type].colorB;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    sprintf(created_medicine.production_date, "%d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon+1, tm.tm_mday);

    int months = (tm.tm_mon + 1) + specs[selected_medicine_type].remaining_months_for_expiry;
    int year = tm.tm_year + 1900;

    while (months > 12) {
        year++;
        months -= 12;
    }

    // if (months <= 24 && months > 12) { /* increase one year */
    //     year++;
    //     months -= 12;

    // } else if (months > 24) { /* increase two years */
    //     year += 2;
    //     months -= 24;
    // }

    sprintf(created_medicine.expire_date, "%d-%02d-%02d", year, months, tm.tm_mday);

    printf(
        "[Label: %d], liquid_level: %d, Color(R:%d, G:%d, B:%d) production Date: %s, expire Date: %s\n",
        created_medicine.label, created_medicine.liquid_level, created_medicine.colorR, created_medicine.colorG, created_medicine.colorB,
        created_medicine.production_date, created_medicine.expire_date
    );
    fflush(stdout);

    return created_medicine;
}

void display_queue() {

    
}


int main(int argc, char** argv) {

    if (argc < 5) {
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

    // initialize specs, and produced medicine queue
    initialize_specs();
    initQueue(&created_medicine_queue, sizeof(LiquidMedicine));

    // create the threads
    pthread_t inspectors[num_inspectors];
    pthread_t packagers[num_packagers];

    int inspectors_ids[num_inspectors];
    int packagers_ids[num_packagers];

    // create inspectors
    for (int i = 0; i < num_inspectors; i++)
        inspectors_ids[i] = pthread_create(&inspectors[i], NULL, inspection, (void*)&i);

    // create packagers
    for (int i = 0; i < num_inspectors; i++)
        packagers_ids[i] = pthread_create(&packagers[i], NULL, packaging, (void*)&i);

    // producing medicine
    while(1) {
        LiquidMedicine created_medicine = produce_medicine();

        int rc = pthread_mutex_lock(&inspection_mutex);

        enqueue(&created_medicine_queue, &created_medicine);
        display_queue();
        printf("%d\n", getSize(&created_medicine_queue));
        printf("---------------------------------\n");
        fflush(stdout);

        pthread_mutex_unlock(&inspection_mutex);

        srand(time(NULL) + getpid());

        sleep( select_from_range(min_time_between_each_production, max_time_between_each_production) );
    }

    // wait for threads
    for (int i = 0; i < num_inspectors; i++)
        pthread_join(inspectors[i], NULL);  

    for (int i = 0; i < num_packagers; i++)
        pthread_join(packagers[i], NULL);

    freeQueue(&created_medicine_queue);

    return 0;
}