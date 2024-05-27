#include "headers.h"
#include "functions.h"


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
}