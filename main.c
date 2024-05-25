#include "headers.h"

#define DEFAULT_SETTINGS "settings.txt"

static char INSPECTORS_PER_PRODUCTION_LINE[20];
static char PACKAGERS_PER_PRODUCTION_LINE[20];
static char LIQUID_PRODUCTION_LINES[20];
static char PILL_PRODUCTION_LINES[20];

static int MEDICINE_TYPES;

int main(int argc, char** argv) {

    // if (argc < 2) {
    //     readFile(DEFAULT_SETTINGS);
    // } else {
    //     readFile(argv[1]);
    // }

    // pid_t liquid_production_lines[LIQUID_PRODUCTION_LINES];
    // pid_t pills_production_lines[PILL_PRODUCTION_LINES];

    // for (int i = 0; i < LIQUID_PRODUCTION_LINES; i++) {
    //     liquid_production_lines[i] = fork();

    //     if (liquid_production_lines[i] == -1) {
    //         perror("Forking Error");
    //         exit(-1);

    //     } else if (liquid_production_lines[i] == 0) {
    //         char med_types[20];

    //         sprintf(med_types, "%d", MEDICINE_TYPES);
            
    //         execlp(
    //             "./liquid_production_line", "liquid_production_line",
    //             INSPECTORS_PER_PRODUCTION_LINE, PACKAGERS_PER_PRODUCTION_LINE, med_types, NULL
    //         );
    //         perror("Exec liquid_production_line Failed");
    //         exit(-1);
    //     }
    // }

    return 0;
}

void readFile(char* filename) {
    char line[200];
    char label[50];

    FILE *file;
    file = fopen(filename, "r");

    if (file == NULL) {
        perror("The file not exist\n");
        exit(-2);
    }

    char separator[] = "=";

    while(fgets(line, sizeof(line), file) != NULL){

        char* str = strtok(line, separator);
        strncpy(label, str, sizeof(label));
        str = strtok(NULL, separator);

        if (strcmp(label, "INSPECTORS_PER_PRODUCTION_LINE") == 0){
            strcpy(INSPECTORS_PER_PRODUCTION_LINE, str);
        } else if (strcmp(label, "LIQUID_PRODUCTION_LINES") == 0){
            strcpy(LIQUID_PRODUCTION_LINES, str);
        } else if (strcmp(label, "PILL_PRODUCTION_LINES") == 0){
            strcpy(PILL_PRODUCTION_LINES, str); 
        } else if (strcmp(label, "PACKAGERS_PER_PRODUCTION_LINE") == 0){
            strcpy(PACKAGERS_PER_PRODUCTION_LINE, str);
        } else if (strcmp(label, "MEDICINE_TYPES") == 0){
            MEDICINE_TYPES = atoi(str);
        }
        //  else if (strcmp(label, "WEIGHT_PER_CONTAINER") == 0){
        //     strcpy(WEIGHT_PER_CONTAINER, str);

        // } else if (strcmp(label, "DROP_PERIOD") == 0){
        //     DROP_PERIOD = atoi(str);

        // } else if (strcmp(label, "PLANE_SAFE_DISTANCE") == 0){
        //     PLANE_SAFE_DISTANCE = atoi(str);

        // } else if (strcmp(label, "DROP_LOST_THRESHOLD") == 0){
        //     DROP_LOST_THRESHOLD = atoi(str);

        // } else if (strcmp(label, "REFILL_RANGE") == 0){
        //     strcpy(REFILL_RANGE, str);

        // } else if (strcmp(label, "AMPLITUDE_RANGE") == 0){
        //     strcpy(AMPLITUDE_RANGE, str);

        // } else if (strcmp(label, "WORKERS_ENERGY_DECAY") == 0){
        //     strcpy(WORKERS_ENERGY_DECAY, str);

        // } else if (strcmp(label, "OCCUPATION_BRUTALITY") == 0){
        //     OCCUPATION_BRUTALITY = atoi(str);

        // } else if (strcmp(label, "WORKERS_START_ENERGY") == 0){
        //     strcpy(WORKERS_START_ENERGY, str);

        // } else if (strcmp(label, "DISTRIBUTOR_BAGS_TRIP") == 0){
        //     DISTRIBUTOR_BAGS_TRIP = atoi(str);

        // } else if (strcmp(label, "DISTRIBUTOR_DEAD_BEFORE_SWAP_THRESHOLD") == 0){
        //     DISTRIBUTOR_DEAD_BEFORE_SWAP_THRESHOLD = atoi(str);

        // } else if (strcmp(label, "NUM_DISTRIBUTORS") == 0){
        //     NUM_DISTRIBUTORS = atoi(str);

        // } else if (strcmp(label, "FAMILIES_STARVATION_RATE_RANGE") == 0){
        //     strcpy(FAMILIES_STARVATION_RATE_RANGE, str);

        // } else if (strcmp(label, "FAMILIES_STARVATION_RATE_INCREASE") == 0){
        //     FAMILIES_STARVATION_RATE_INCREASE = atoi(str);
            
        // } else if (strcmp(label, "FAMILIES_STARVATION_RATE_DECREASE") == 0){
        //     FAMILIES_STARVATION_RATE_DECREASE = atoi(str);

        // } else if (strcmp(label, "FAMILIES_INCREASE_ALARM") == 0){
        //     FAMILIES_INCREASE_ALARM = atoi(str);

        // } else if (strcmp(label, "FAMILIES__STARVATION_SURVIVAL_THRESHOLD") == 0){
        //     FAMILIES__STARVATION_SURVIVAL_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "SORTER_REQUIRED_STARVE_RATE_DECREASE_PERCENTAGE") == 0){
        //     SORTER_REQUIRED_STARVE_RATE_DECREASE_PERCENTAGE = atoi(str);

        // /* program end thresholds */
        // } else if (strcmp(label, "COLLECTORS_MARTYRED_THRESHOLD") == 0){
        //     COLLECTORS_MARTYRED_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "DISTRIBUTORS_MARTYRED_THRESHOLD") == 0){
        //     DISTRIBUTORS_MARTYRED_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "PLANES_DESTROYED_THRESHOLD") == 0){
        //     PLANES_DESTROYED_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "PACKAGES_DESTROYED_THRESHOLD") == 0){
        //     PACKAGES_DESTROYED_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "FAMILIES_DEATHRATE_THRESHOLD") == 0){
        //     FAMILIES_DEATHRATE_THRESHOLD = atoi(str);
        // } else if (strcmp(label, "SIMULATION_TIME") == 0){
        //     SIMULATION_TIME = atoi(str);
        // }
    }

    fclose(file);
}