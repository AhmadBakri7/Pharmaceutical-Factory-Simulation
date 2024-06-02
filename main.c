#include "headers.h"
#include "functions.h"

#define DEFAULT_SETTINGS "settings.txt"

void readFile(char* filename);
void time_limit(int sig);
void end_program();
bool check_thresholds(int* liquid_counter, int* pill_counter, int pill_out_of_spec, int liquid_out_of_spec);

static char INSPECTORS_PER_PRODUCTION_LINE[20];
static char PACKAGERS_PER_PRODUCTION_LINE[20];
static int LIQUID_PRODUCTION_LINES;
static int PILL_PRODUCTION_LINES;
static int LIQUID_MEDICINE_TYPES;
static int PILL_MEDICINE_TYPES;
static char TIME_BETWEEN_EACH_PRODUCTION[20];
static char TIME_FOR_INSPECTION[20];
static char TIME_FOR_PACKAGING[20];
static char MONTHS_BEFORE_EXPIRY[20];

static char NUM_PILLS_PER_CONTAINER[20];
static char NUM_CONTAINERS_PER_PILL_MEDICINE[20];
static int PILL_MISSING_DEFECT_RATE;
static int PILL_COLOR_DEFECT_RATE;
static int PILL_SIZE_DEFECT_RATE;
static int PILL_EXPIRE_DATE_DEFECT_RATE;

static int LIQUID_MEDICINE_LIQUID_LEVEL_DEFECT_RATE;
static int LIQUID_MEDICINE_COLOR_DEFECT_RATE;
static int LIQUID_MEDICINE_SEALED_DEFECT_RATE;
static int LIQUID_MEDICINE_EXPIRE_DATE_DEFECT_RATE;
static int LIQUID_MEDICINE_CORRECT_LABEL_DEFECT_RATE;
static int LIQUID_MEDICINE_LABEL_PLACE_DEFECT_RATE;

static int LIQUID_MEDICINE_OUT_OF_SPEC_THRESHOLD;
static int PILL_MEDICINE_OUT_OF_SPEC_THRESHOLD;
static int LIQUID_MEDICINE_TYPE_PRODUCE_THRESHOLD;
static int PILL_MEDICINE_TYPE_PRODUCE_THRESHOLD;
static float SPEED_THRESHOLD;
static int TIME_LIMIT;

pid_t* liquid_production_lines;
pid_t* pills_production_lines;

int feedback_queue_id, emp_transfer_queue_id;
int speed_shmem_id;
int speed_sem_id;

int main(int argc, char** argv) {

    if (argc < 2) {
        readFile(DEFAULT_SETTINGS);
    } else {
        readFile(argv[1]);
    }

    int produced_liquid_medicine_counter[LIQUID_MEDICINE_TYPES];
    int produced_pill_medicine_counter[PILL_MEDICINE_TYPES];

    int liquid_medicine_defected_counter = 0, 
        pill_medicine_defected_counter = 0;

    key_t feedback_queue_key = ftok(".", 'Q');
    key_t emp_transfer_queue_key = ftok(".", 'E');
    key_t production_line_speed_shmem_key = ftok(".", 'M');
    key_t production_line_speed_sem_key = ftok(".", 'S');

    if ( (feedback_queue_id = msgget(feedback_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    if ( (emp_transfer_queue_id = msgget(emp_transfer_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    // create or retrieve the semaphore
    if ( (speed_sem_id = semget(production_line_speed_sem_key, 1, IPC_CREAT | 0660)) == -1 ) {
        perror("semget: IPC_CREAT | 0660");
        end_program();
    }

    // initialize semaphore
    union semun speed_su;
    speed_su.val = 1;
    if (semctl(speed_sem_id, 0, SETVAL, speed_su) < 0) {
        perror("semctl");
        end_program();
    }

    // Create or retrieve the shared memory segment
    if ((speed_shmem_id = shmget(
        production_line_speed_shmem_key,
        sizeof(MemoryCell) * (LIQUID_PRODUCTION_LINES + PILL_PRODUCTION_LINES),
        IPC_CREAT | 0666)) < 0)
    {
        perror("shmget");
        end_program();
    }

    if ( signal(SIGINT, time_limit) == SIG_ERR ) {
        perror("SIGINT Error in main");
        exit(SIGQUIT);
    }

    if ( signal(SIGTSTP, time_limit) == SIG_ERR ) {
        perror("SIGTSTP Error in main");
        exit(SIGQUIT);
    }

    if ( signal(SIGALRM, time_limit) == SIG_ERR ) {
        perror("SIGALRM Error in main");
        exit(SIGQUIT);
    }

    liquid_production_lines = (pid_t*) malloc(sizeof(pid_t) * LIQUID_PRODUCTION_LINES);
    pills_production_lines = (pid_t*) malloc(sizeof(pid_t) * PILL_PRODUCTION_LINES);

    for (int i = 0; i < LIQUID_PRODUCTION_LINES; i++) {
        liquid_production_lines[i] = fork();

        if (liquid_production_lines[i] == -1) {
            perror("Forking Error");
            exit(-1);

        } else if (liquid_production_lines[i] == 0) {
            char med_types[20];
            char liquid_defect[20];
            char color_defect[20], sealed_defect[20], expire_defect[20], label_defect[20],
                 label_place_defect[20];
            char msg_queue[20];

            sprintf(med_types, "%d", LIQUID_MEDICINE_TYPES);
            sprintf(liquid_defect, "%d", LIQUID_MEDICINE_LIQUID_LEVEL_DEFECT_RATE);
            sprintf(color_defect, "%d", LIQUID_MEDICINE_COLOR_DEFECT_RATE);
            sprintf(sealed_defect, "%d", LIQUID_MEDICINE_SEALED_DEFECT_RATE);
            sprintf(expire_defect, "%d", LIQUID_MEDICINE_EXPIRE_DATE_DEFECT_RATE);
            sprintf(label_defect, "%d", LIQUID_MEDICINE_CORRECT_LABEL_DEFECT_RATE);
            sprintf(label_place_defect, "%d", LIQUID_MEDICINE_LABEL_PLACE_DEFECT_RATE);
            sprintf(msg_queue, "%d", feedback_queue_id);
            
            execlp(
                "./liquid_production_line", "liquid_production_line",
                INSPECTORS_PER_PRODUCTION_LINE, PACKAGERS_PER_PRODUCTION_LINE, med_types, 
                TIME_BETWEEN_EACH_PRODUCTION, MONTHS_BEFORE_EXPIRY, liquid_defect, color_defect,
                sealed_defect, expire_defect, label_defect, label_place_defect, 
                TIME_FOR_INSPECTION, TIME_FOR_PACKAGING, msg_queue, NULL
            );
            perror("Exec liquid_production_line Failed");
            exit(-1);
        }
    }

    for (int i = 0; i < PILL_PRODUCTION_LINES; i++) {
        pills_production_lines[i] = fork();

        if (pills_production_lines[i] == -1) {
            perror("Forking Error");
            exit(-1);

        } else if (pills_production_lines[i] == 0) {
            char med_types[20];
            char missing_defect[20], expire_defect[20], pill_color_defect[20],
                 pill_size_defect[20];
            char my_number[20], num_prod_lines[20], speed_threshold[20];

            sprintf(med_types, "%d", PILL_MEDICINE_TYPES);
            sprintf(missing_defect, "%d", PILL_MISSING_DEFECT_RATE);
            sprintf(expire_defect, "%d", PILL_EXPIRE_DATE_DEFECT_RATE);
            sprintf(pill_color_defect, "%d", PILL_COLOR_DEFECT_RATE);
            sprintf(pill_size_defect, "%d", PILL_SIZE_DEFECT_RATE);

            sprintf(my_number, "%d", i);
            sprintf(num_prod_lines, "%d", PILL_PRODUCTION_LINES);
            sprintf(speed_threshold, "%f", SPEED_THRESHOLD);
            
            execlp(
                "./pill_production_line", "pill_production_line",
                INSPECTORS_PER_PRODUCTION_LINE, PACKAGERS_PER_PRODUCTION_LINE, med_types,
                TIME_BETWEEN_EACH_PRODUCTION, MONTHS_BEFORE_EXPIRY, missing_defect, pill_color_defect,
                pill_size_defect, expire_defect, TIME_FOR_INSPECTION,
                TIME_FOR_PACKAGING, NUM_PILLS_PER_CONTAINER, NUM_CONTAINERS_PER_PILL_MEDICINE,
                my_number, num_prod_lines, speed_threshold, NULL
            );
            perror("Exec pill_production_line Failed");
            exit(-1);
        }
    }

    for (int i = 0; i < LIQUID_MEDICINE_TYPES; i++)
        produced_liquid_medicine_counter[i] = 0;

    for (int i = 0; i < PILL_MEDICINE_TYPES; i++)
        produced_pill_medicine_counter[i] = 0;

    alarm(TIME_LIMIT);

    while (1) {
        FeedBackMessage message;

        if ( msgrcv(feedback_queue_id, &message, sizeof(message), 0, 0) == -1 ) {
            perror("msgrcv Parent (News queue)");
            break;
        }

        printf("(MAIN) received MSG-Type: %ld, Medicine-Type: %d\n", message.message_type, message.medicine_type);

        switch (message.message_type)
        {
        case PRODUCED_LIQUID_MEDICINE:
            produced_liquid_medicine_counter[message.medicine_type]++;
            break;
        
        case PRODUCED_PILL_MEDICINE:
            produced_pill_medicine_counter[message.medicine_type]++;
            break;

        case DEFECTED_LIQUID_MEDICINE:
            liquid_medicine_defected_counter++;
            break;

        case DEFECTED_PILL_MEDICINE:
            pill_medicine_defected_counter++;
            break;
        }

        bool stop_program = check_thresholds(
            produced_liquid_medicine_counter,
            produced_pill_medicine_counter,
            liquid_medicine_defected_counter,
            pill_medicine_defected_counter
        );

        if (stop_program)
            break;
    }

    end_program();

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
            LIQUID_PRODUCTION_LINES = atoi(str);

        } else if (strcmp(label, "PILL_PRODUCTION_LINES") == 0){
            PILL_PRODUCTION_LINES = atoi(str); 

        } else if (strcmp(label, "PACKAGERS_PER_PRODUCTION_LINE") == 0){
            strcpy(PACKAGERS_PER_PRODUCTION_LINE, str);

        } else if (strcmp(label, "LIQUID_MEDICINE_TYPES") == 0){
            LIQUID_MEDICINE_TYPES = atoi(str);

        } else if (strcmp(label, "PILL_MEDICINE_TYPES") == 0){
            PILL_MEDICINE_TYPES = atoi(str);

        } else if (strcmp(label, "TIME_BETWEEN_EACH_PRODUCTION") == 0){
            strcpy(TIME_BETWEEN_EACH_PRODUCTION, str);

        } else if (strcmp(label, "TIME_FOR_INSPECTION") == 0){
            strcpy(TIME_FOR_INSPECTION, str);

        } else if (strcmp(label, "TIME_FOR_PACKAGING") == 0){
            strcpy(TIME_FOR_PACKAGING, str);

        } else if (strcmp(label, "MONTHS_BEFORE_EXPIRY") == 0){
            strcpy(MONTHS_BEFORE_EXPIRY, str);

        } else if (strcmp(label, "LIQUID_MEDICINE_LIQUID_LEVEL_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_LIQUID_LEVEL_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_COLOR_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_COLOR_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_SEALED_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_SEALED_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_EXPIRE_DATE_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_EXPIRE_DATE_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_CORRECT_LABEL_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_CORRECT_LABEL_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_LABEL_PLACE_DEFECT_RATE") == 0){
            LIQUID_MEDICINE_LABEL_PLACE_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_OUT_OF_SPEC_THRESHOLD") == 0){
            LIQUID_MEDICINE_OUT_OF_SPEC_THRESHOLD = atoi(str);

        } else if (strcmp(label, "PILL_MEDICINE_OUT_OF_SPEC_THRESHOLD") == 0){
            PILL_MEDICINE_OUT_OF_SPEC_THRESHOLD = atoi(str);

        } else if (strcmp(label, "LIQUID_MEDICINE_TYPE_PRODUCE_THRESHOLD") == 0){
            LIQUID_MEDICINE_TYPE_PRODUCE_THRESHOLD = atoi(str);

        } else if (strcmp(label, "PILL_MEDICINE_TYPE_PRODUCE_THRESHOLD") == 0){
            PILL_MEDICINE_TYPE_PRODUCE_THRESHOLD = atoi(str);
            
        } else if (strcmp(label, "TIME_LIMIT") == 0){
            TIME_LIMIT = atoi(str);

        } else if (strcmp(label, "PILL_MISSING_DEFECT_RATE") == 0){
            PILL_MISSING_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "PILL_COLOR_DEFECT_RATE") == 0){
            PILL_COLOR_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "PILL_SIZE_DEFECT_RATE") == 0){
            PILL_SIZE_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "PILL_EXPIRE_DATE_DEFECT_RATE") == 0){
            PILL_EXPIRE_DATE_DEFECT_RATE = atoi(str);

        } else if (strcmp(label, "NUM_PILLS_PER_CONTAINER") == 0){
            strcpy(NUM_PILLS_PER_CONTAINER, str);
        }
        else if (strcmp(label, "NUM_CONTAINERS_PER_PILL_MEDICINE") == 0){
            strcpy(NUM_CONTAINERS_PER_PILL_MEDICINE, str);

        } else if (strcmp(label, "SPEED_THRESHOLD") == 0){
            SPEED_THRESHOLD = atof(str);
        }
        //  else if (strcmp(label, "PLANES_DESTROYED_THRESHOLD") == 0){
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

bool check_thresholds(int* liquid_counter, int* pill_counter, int liquid_out_of_spec, int pill_out_of_spec) {

    for (int i = 0; i < LIQUID_MEDICINE_TYPES; i++) {
        if (liquid_counter[i] >= LIQUID_MEDICINE_TYPE_PRODUCE_THRESHOLD)
            return true;
    }

    for (int i = 0; i < PILL_MEDICINE_TYPES; i++) {
        if (pill_counter[i] == PILL_MEDICINE_TYPE_PRODUCE_THRESHOLD)
            return true;
    }

    if (pill_out_of_spec == PILL_MEDICINE_OUT_OF_SPEC_THRESHOLD || liquid_out_of_spec == LIQUID_MEDICINE_OUT_OF_SPEC_THRESHOLD)
        return true;

    return false;
}

void end_program() {
    for (int i = 0; i < LIQUID_PRODUCTION_LINES; i++)
        kill(liquid_production_lines[i], SIGINT);

    for (int i = 0; i < PILL_PRODUCTION_LINES; i++)
        kill(pills_production_lines[i], SIGINT);

    while ( wait(NULL) > 0 );

    if (msgctl(feedback_queue_id, IPC_RMID, (struct msqid_ds *) 0) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    if (msgctl(emp_transfer_queue_id, IPC_RMID, (struct msqid_ds *) 0) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }

    if ( semctl(speed_sem_id, IPC_RMID, 0) == -1 ) {
        perror("semctl: IPC_RMID");	/* remove semaphore */
        exit(5);
    }

    if ( shmctl(speed_shmem_id, IPC_RMID, (struct shmid_ds *) 0) == -1 ) {
        perror("shmid: IPC_RMID");	/* remove shared memory */
        exit(5);
    }

    free(liquid_production_lines);
    free(pills_production_lines);

    exit(0);
}

void time_limit(int sig) {
    end_program();
}