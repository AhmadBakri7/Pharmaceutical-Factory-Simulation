#include "headers.h"
// Function to initialize OpenGL settings

int drawer_queue_id;
int num_liquid_lines, num_pill_lines;

float array_of_positions[100][100];
float array_of_y_positions[100];
bool flag = true;

// InspectorGUI inspectors[100];
// PackagerGUI packagers[100];

int num_inspectors[100];
int num_packagers[100];

PlasticContainerGUI plastic_containers[100][100];
LiquidMedicineGUI liquid_medicines[100][100];

float speeds[100];

int produced_medicine_counter[100];
int liquid_medicine_counter = 0;
int pill_medicine_counter = 0;
int trash_counter[100];
int non_defected_counter[100];
int packaged_medicines_counter[100];

void drawText(float x, float y, const char *string);

void initialize() {
    glClearColor(1.0, 1.0, 1.0, 1.0); // Set background color to black
    glEnable(GL_DEPTH_TEST); // Enable depth test for 3D rendering
    glDepthFunc(GL_LEQUAL); // Specify the depth test function
}

// Function to draw a circle
void drawFilledCircle(float cx, float cy, float r, int num_segments) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy); // Center of circle
    for(int i = 0; i <= num_segments; i++) {
        float theta = 2.0f * 3.1415926f * (float)i / (float)num_segments; // Get the current angle
        float x = r * cosf(theta); // Calculate the x component
        float y = r * sinf(theta); // Calculate the y component
        glVertex2f(cx + x, cy + y); // Output vertex
    }
    glEnd();
}

// Function to draw a rectangle
void drawRectangle(float x, float y, float width, float height) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

// Function to draw a bottled medicine
void drawBottledMedicine(float x, float y, float width, float height,int number) {
    // Draw the body of the bottle
    glColor3f(0.0f, 0.0f, 0.5f); // Set color to blue
    drawRectangle(x, y, width, height);

    // Draw the rounded top part of the bottle
    glColor3f(0.0f, 0.0f, 0.5f); // Set color to blue
    drawRectangle(x + width/3.8, y+height/5 , width / 2.0f, height);
    // Draw the cap of the bottle
    glColor3f(1.0f, 0.6f, 0.0f); // Set color to orange
    drawRectangle(x + width/3.8, y+height*1.2 , width / 2.0f, height/5.0f);

    // Draw the cap ridges
    glColor3f(1.0f, 0.4f, 0.0f); // Set color to darker orange
    for (float i = x + width/4.5; i < x + width/1.29; i += (width / 2.0f) / 10.0f) {
    drawRectangle(i, y + height * 1.2, (width / 2.0f) / 10.0f, height / 5.0f);
     }

    // Draw the label on the bottle
    glColor3f(1.0f, 0.8f, 0.0f); // Set color to yellow
    drawRectangle(x, y + height / 2.0f, width, height / 4.0f);

    // Draw the white part of the label
    glColor3f(1.0f, 1.0f, 1.0f); // Set color to white
    drawRectangle(x + width / 6.0f, y + height / 2.02f + height / 16.0f, width * 2.0f / 3.0f, height / 6.0f);
    char buff[1000];
    glColor3f(0, 0, 0);
    sprintf(buff, "%d", number);
    drawText(x + width / 3.0f, y + height / 2.0f + height / 16.0f, buff);
}
void drawPlasticContainer(float x, float y, float width, float height, int numPills, float pillSize ) {
    // Draw the body of the plastic container
    glColor3f(0.5f, 0.5f, 0.5f); // Set color to gray for the container
    drawRectangle(x, y, width, height);

    // Calculate the gap between rows and the size of each pill
    float rowGap = 0.005f;
    float pillGap = 0.005f;
    pillSize = (height - (numPills/2 + 1) * rowGap) / numPills*2;

    // Draw the pills inside the container
    glColor3f(1.0f, 0.0f, 0.0f); // Set color to red for the pills
    for (int i = 0; i < numPills; ++i) {
        // Calculate row index and position within the row
        int rowIndex = i / 2;
        float pillX = x + (i % 2) * (width / 2) + pillGap;
        float pillY = y + (rowIndex + 1) * rowGap + rowIndex * pillSize;

        // Draw the pill
        drawFilledCircle(pillX + width / 4, pillY + pillSize / 2, pillSize / 2, 100);
    }
}

void drawTrashCan(float x, float y, float width, float height) {
    // Color for the main body
    glColor3f(0.5f, 0.5f, 0.5f);
    // Draw the main body of the trash can
    drawRectangle(x, y, width, height);

    // Draw the top lid
    glColor3f(0.3f, 0.3f, 0.3f);  // Darker shade for the lid
    float lidHeight = height * 0.1;  // Lid is 10% of the can's height
    drawRectangle(x, y + height - lidHeight/2, width, lidHeight);

    // Draw the lid handle
    //glColor3f(0.2f, 0.2f, 0.2f);  // Even darker for the handle
    float handleHeight = lidHeight * 2;  // Handle is 50% of the lid's height
    float handleWidth = width * 0.5;  // Center the handle
    drawRectangle(x + (width/4), y + height + 0.01, handleWidth, handleHeight/2);

    // Draw vertical stripes
    glColor3f(0.4f, 0.4f, 0.4f);  // Slightly darker gray for the stripes
    int numStripes = 3;
    float stripeWidth = width * 0.1;  // Each stripe takes up 10% of the width
    float spaceBetween = (width - numStripes * stripeWidth) / (numStripes + 1);
    for (int i = 0; i < numStripes; i++) {
        float stripeX = x + spaceBetween + i * (stripeWidth + spaceBetween);
        drawRectangle(stripeX, y, stripeWidth, height - lidHeight);
    }
}

void drawWorker(float x, float y, bool type) {
    if(type){
        // Set player color
        glColor3f(1.0f, 0.0f, 0.0f); // Set color to red
    }
    else{
        glColor3f(0.0f, 1.0f, 0.0f); // Set color to red
    }
    // Draw head
    drawFilledCircle(x, y + 0.08f, 0.03f, 20);

    glColor3f(1.0f, 1.0f, 1.0f); // Set color to white
    //draw eyes
    drawFilledCircle(x-0.015, y + 0.09f, 0.006f, 10);//left eye
    drawFilledCircle(x+0.015, y + 0.09f, 0.006f, 10);//right eye
    //draw mouth
    drawRectangle(x-0.021, y+0.065, 0.04, 0.003);

    if(type){
        // Set player color
        glColor3f(1.0f, 0.0f, 0.0f); // Set color to red
    }
    else{
        glColor3f(0.0f, 1.0f, 0.0f); // Set color to red
    }
    // Draw body
    glBegin(GL_LINES);
    glVertex2f(x, y + 0.05); // Body's upper point
    glVertex2f(x, y - 0.1f); // Body's lower point
    glEnd();

    // Draw arms
    glBegin(GL_LINES);
    glVertex2f(x, y + 0.01f); // Arms' upper point
    glVertex2f(x - 0.05f, y - 0.03f); // Left arm's lower point

    glEnd();
    glBegin(GL_LINES);
    glVertex2f(x, y + 0.01f); // Arms' upper point
    glVertex2f(x + 0.05f, y - 0.03f); // Right arm's lower point
    glEnd();

    // Draw legs
    glBegin(GL_LINES);
    glVertex2f(x, y - 0.1f); // Legs' upper point
    glVertex2f(x - 0.03f, y - 0.15f); // Left leg's lower point
    glEnd();
    glBegin(GL_LINES);
    glVertex2f(x, y - 0.1f); // Legs' upper point
    glVertex2f(x + 0.03f, y - 0.15f); // Right leg's lower point
    glEnd();
}   

void drawCircle(float cx, float cy, float r, int num_segments) {
    glBegin(GL_POLYGON);
    for(int ii = 0; ii < num_segments; ii++) {
        float theta = 2.0f * 3.1415926f * ((float)ii / (float)num_segments); // Ensure proper casting
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

void drawPackage(float x, float y, float width, float height) {
    // Color for the main body of the box
    glColor3f(0.76f, 0.6f, 0.42f);
    drawRectangle(x, y, width, height);  // Drawing the main rectangular body

    // Drawing the flaps of the box
    glColor3f(0.8f, 0.65f, 0.48f);  // Lighter shade for the flaps

    // Left flap as a trapezoid
    glBegin(GL_QUADS);
    glVertex2f(x, y + height);  // Bottom left
    glVertex2f(x + width * 0.5f, y + height);  // Bottom right
    glVertex2f(x + width * 0.4f, y + height + height * 0.2f);  // Top right, slightly inward
    glVertex2f(x, y + height + height * 0.2f);  // Top left
    glEnd();

    // Right flap as a trapezoid
    glBegin(GL_QUADS);
    glVertex2f(x + width * 0.5f, y + height);  // Bottom left
    glVertex2f(x + width, y + height);  // Bottom right
    glVertex2f(x + width, y + height + height * 0.2f);  // Top right
    glVertex2f(x + width * 0.6f, y + height + height * 0.2f);  // Top left, slightly inward
    glEnd();
}


void drawText(float x, float y, const char *string) {
    glRasterPos2f(x, y);

    // Loop through each character of the string
    while (*string) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *string);
        string++;
    }
}

int find_liquid_medicine(int production_line_number, int serial_number) {

    for (int i = 0; i < liquid_medicine_counter; i++) {

        if (liquid_medicines[production_line_number][i].medicine.serial_number == serial_number)
            return i;
    }
    return -1;
}

int find_pill_medicine(int production_line_number, int serial_number) {

    for (int i = 0; i < pill_medicine_counter; i++) {

        if (plastic_containers[production_line_number][i].plastic_container.serial_number == serial_number)
            return i;
    }
    return -1;
}

void read_from_queue() {

    struct msqid_ds buf;
    msgctl(drawer_queue_id, IPC_STAT, &buf);

    for (int i = 0; i < buf.msg_qnum; i++) {
        DrawerMessage msg;

        if (msgrcv(drawer_queue_id, &msg, sizeof(msg), 0, IPC_NOWAIT) == -1) {
            perror("msg error");
            exit(-1);
        }

        int n = msg.production_line_number;

        switch (msg.operation_type)
        {
        case PRODUCTION:
            produced_medicine_counter[n]++;

            if (msg.medicine_type == LIQUID) {
                memcpy(&liquid_medicines[n][liquid_medicine_counter].medicine, &msg.medicine.liquid_medicine, sizeof(LiquidMedicine));

                liquid_medicines[n][liquid_medicine_counter].x = array_of_positions[n][0];
                liquid_medicines[n][liquid_medicine_counter].y = array_of_y_positions[n];
                liquid_medicines[n][liquid_medicine_counter].draw_this = true;
                liquid_medicine_counter++;

            } else {
                plastic_containers[n][pill_medicine_counter].x = array_of_positions[n][0];
                pill_medicine_counter++;
            }
            break;

        case INSPECTION_START:

            if (msg.medicine_type == LIQUID) {

                int index = find_liquid_medicine(n, msg.medicine.liquid_medicine.serial_number);
                liquid_medicines[n][index].x = array_of_positions[n][msg.worker_index + 1];
                liquid_medicines[n][index].y = array_of_y_positions[n];

            } else {
                int index = find_pill_medicine(n, msg.medicine.plastic_container.serial_number);
                plastic_containers[n][index].x = array_of_positions[msg.production_line_number][msg.worker_index + 1];
                plastic_containers[n][index].y = array_of_y_positions[msg.production_line_number];
            }
            break;

        case INSPECTION_FAILED:

            if (msg.medicine_type == LIQUID) {
                int index = find_liquid_medicine(n, msg.medicine.liquid_medicine.serial_number);

                memcpy(&liquid_medicines[n][index], &liquid_medicines[n][liquid_medicine_counter-1], sizeof(LiquidMedicineGUI));
                liquid_medicine_counter--;

            } else {
                int index = find_pill_medicine(n, msg.medicine.liquid_medicine.serial_number);

                free(plastic_containers[n][index].plastic_container.pills); /* Erase the Pills from memory */

                memcpy(&plastic_containers[n][index], &plastic_containers[n][pill_medicine_counter-1], sizeof(PlasticContainerGUI));
                pill_medicine_counter--;
            }
            trash_counter[msg.production_line_number]++;
            break;

        case INSPECTION_SUCCESSFUL:

            if (msg.medicine_type == LIQUID) {
                int index = find_liquid_medicine(n, msg.medicine.liquid_medicine.serial_number);
                liquid_medicines[n][index].draw_this = false;

            } else {
                int index = find_pill_medicine(n, msg.medicine.liquid_medicine.serial_number);
                plastic_containers[n][index].draw_this = false;
            }
            non_defected_counter[msg.production_line_number]++;
            break;

        case PACKAGING_START:
            if (msg.medicine_type == LIQUID) {
                int index = find_liquid_medicine(n, msg.medicine.liquid_medicine.serial_number);
                
                liquid_medicines[n][index].x = array_of_positions[msg.production_line_number][msg.worker_index + 1 + num_inspectors[msg.production_line_number]];
                liquid_medicines[n][index].y = array_of_y_positions[msg.production_line_number];
                liquid_medicines[n][index].draw_this = true;

            } else {
                int index = find_pill_medicine(n, msg.medicine.plastic_container.serial_number);
                plastic_containers[n][index].x = array_of_positions[msg.production_line_number][msg.worker_index + 1 + num_inspectors[msg.production_line_number]];
                plastic_containers[n][index].y = array_of_y_positions[msg.production_line_number];
                plastic_containers[n][index].draw_this = true;
            }
            non_defected_counter[msg.production_line_number]--;
            break;
        
        case PACKAGING_END:
            if (msg.medicine_type == LIQUID) {
                int index = find_liquid_medicine(n, msg.medicine.liquid_medicine.serial_number);

                memcpy(&liquid_medicines[n][index], &liquid_medicines[n][liquid_medicine_counter-1], sizeof(LiquidMedicineGUI));
                liquid_medicine_counter--;

            } else {
                int index = find_pill_medicine(n, msg.medicine.plastic_container.serial_number);

                free(plastic_containers[n][index].plastic_container.pills); /* Erase the Pills from memory */
                memcpy(&plastic_containers[n][index], &plastic_containers[pill_medicine_counter-1], sizeof(PlasticContainerGUI));
                pill_medicine_counter--;
            }
            packaged_medicines_counter[msg.production_line_number]++;
            break;
        }
    }
}

// Function to display the scene
void display() {

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the color and depth buffers
    glLoadIdentity(); // Reset the current modelview matrix

    // Set up the view
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // the first line for liquid
    glColor3f(0.0, 0.0, 0.0); // Set color to white
    drawRectangle(-2.0f, 0.65f, 4.0f, 0.02f);

    // the second line for liquid
    glColor3f(0.0, 0.0, 0.0); // Set color to white
    drawRectangle(-2.0f, 0.89f, 4.0f, 0.02f);

    drawTrashCan(0.65f, 0.39, 0.15f, 0.2f);
    drawPackage(0.83f, 0.39, 0.15f, 0.2f);  // Adjust position and size as needed
    char buff[1000];
    
    // write the trash counter
    glColor3f(0, 0, 0);
    sprintf(buff, "%d", trash_counter[0]);
    drawText(0.65f, 0.35, buff);

    // write the packaged counter
    glColor3f(0, 0, 0);
    sprintf(buff, "%d", packaged_medicines_counter[0]);
    drawText(0.83f, 0.35, buff);

    for (int i = 0; i < num_inspectors[0]; i++) {//liquid medicine
        float x_offset = i * 0.3;
        drawWorker(-0.75f + x_offset/1.5, 0.55f, true); // inspectors

        glColor3f(0, 0, 0);
        sprintf(buff, "%d", i);
        drawText(-0.76f + x_offset/1.5, 0.5, buff);
        // drawText(array_of_positions[0][i+1], 0.5, buff);

        if (flag) {
            array_of_positions[0][i+1] = -0.8f + x_offset/1.5; //the first dimension(0) should be the index of the prod.line
        }
    }
    for (int i = 0; i < num_packagers[0]; i++) {//liquid medicine
        float x_offset = i * 0.3;
        drawWorker(0.55f - x_offset/1.5, 0.55f, false); // packagers

        glColor3f(0, 0, 0);
        sprintf(buff, "%d", i);
        drawText(0.54f - x_offset/1.5, 0.5, buff);
        // drawText(array_of_positions[0][i+1], 0.5, buff);
        
        if (flag) {
            array_of_positions[0][ i + 1 + num_inspectors[0] ] = 0.5f - x_offset/1.5; //i should be (i+num_of_inspectors)
            array_of_positions[0][0] = -0.95; //production
            array_of_y_positions[0] = 0.70f; //production
            flag = false;
        }
    }

    // glColor3f(0, 0, 0);
    // sprintf(buff, "%d", );
    // drawText(0.54f - x_offset/1.5, 0.5, buff);

    // // the second line for pill 
    // glColor3f(0.0, 0.0, 0.0); // Set color to white
    // drawRectangle(-2.0f, 0.27f, 4.0f, 0.02f);

    // glColor3f(0.0, 0.0, 0.0); // Set color to white
    // drawRectangle(-2.0f, 0.02f, 4.0f, 0.02f);

    // drawTrashCan(0.65f, -0.24, 0.15f, 0.2f);
    // drawPackage(0.83f, -0.24, 0.15f, 0.2f);  // Adjust position and size as needed

    // // write the trash counter
    // glColor3f(0, 0, 0);
    // sprintf(buff, "%d", trash_counter[1]);
    // drawText(0.65f, -0.24, buff);

    // // write the packaged counter
    // glColor3f(0, 0, 0);
    // sprintf(buff, "%d", packaged_medicines_counter[1]);
    // drawText(0.83f, -0.24, buff);

    // for (int i = 0; i < num_inspectors[1]; i++) {
    //     float x_offset = i * 0.3;
    //     drawWorker(-0.75f + x_offset/1.5, -0.08f, true); // inspectors

    //     if (flag){
    //         array_of_positions[1][i+1] = -0.83f + x_offset/1.5; //the first dimension(1) should be the index of the prod.line
    //     }
    // }
    // for (int i = 0; i < num_packagers[1]; i++) {
    //     float x_offset = i * 0.3;
    //     drawWorker(0.55f - x_offset/1.5, -0.08f, false); // packagers

    //     if (flag){
    //         array_of_positions[1][i+1] = 0.47f - x_offset/1.5; //i should be (i+num_of_inspectors)
    //         array_of_positions[1][0] = -0.99; //production
    //     }
    // }

    // // the third line for liquid
    // glColor3f(0.0, 0.0, 0.0); // Set color to white
    // drawRectangle(-2.0f, -0.63f, 4.0f, 0.02f);

    // // the second line for liquid 
    // glColor3f(0.0, 0.0, 0.0); // Set color to white
    // drawRectangle(-2.0f, -0.39f, 4.0f, 0.02f);

    // // for (int i = 0; i < num_of_bottled; i++) {
    // //     float x_offset = i * (1.0f / (num_of_bottled - 1)); // Calculate the x offset
    // //     drawBottledMedicine(-0.97f+ x_offset/2.99, -0.6f, 0.1f, 0.2f,number);
    // // }

    // drawTrashCan(0.65f, -0.88, 0.15f, 0.2f);
    // drawPackage(0.83f, -0.88, 0.15f, 0.2f);  // Adjust position and size as needed
    // // write the trash counter
    // glColor3f(0, 0, 0);
    // sprintf(buff, "%d", trash_counter[2]);
    // drawText(0.65f, -0.88, buff);

    // // write the packaged counter
    // glColor3f(0, 0, 0);
    // sprintf(buff, "%d", packaged_medicines_counter[2]);
    // drawText(0.83f, -0.88, buff);

    // for (int i = 0; i < num_inspectors[2]; i++) {//liquid medicine
    //     float x_offset = i * 0.3;
    //     drawWorker(-0.75f+x_offset/1.5,-0.73f, true); // inspectors

    //     if (flag) {
    //         array_of_positions[2][i+1] = -0.8f+x_offset/1.5;//the first dimension(2) should be the index of the prod.line
    //     }
    // }
    // for (int i = 0; i < num_packagers[2]; i++) {//liquid medicine
    //     float x_offset = i * 0.3;
    //     drawWorker(0.55f-x_offset/1.5,-0.73f, false); // packagers

    //     if (flag) {
    //         array_of_positions[2][i+1] = 0.5f-x_offset/1.5;//i should be (i+num_of_inspectors)
    //         array_of_positions[0][0] = -0.95;//production
    //         flag = false;
    //     }
    // }

    read_from_queue();

    for (int i = 0; i < liquid_medicine_counter; i++) {
        //drawBottledMedicine(-0.8f+0.6/1.5, 0.68f, 0.1f, 0.2f,number);

        if (liquid_medicines[0][i].draw_this)
            drawBottledMedicine(liquid_medicines[0][i].x, liquid_medicines[0][i].y, 0.1f, 0.2f, liquid_medicines[0][i].medicine.serial_number);
    }

    // for (int i = 0; i < numofplastic; i++) {
    //     float x_offset = i * 0.3; // Calculate the x offset
    //     drawPlasticContainer(0.47f - x_offset/1.5, 0.057f, 0.15f, 0.2f,Number_Of_Pills,pillSize );
    // }

    glutSwapBuffers(); // Swap the front and back buffers
}

// Timer function to animate
void timer(int value) {
    glutPostRedisplay(); // Redraw the scene
    glutTimerFunc(16, timer, 0); // Re-register timer callback
}

// Main function
int main(int argc, char** argv) {

    if (argc < 3) {
        perror("Not Enough Args");
        exit(-1);
    }

    key_t drawer_queue_key = ftok(".", 'D');

    if ( (drawer_queue_id = msgget(drawer_queue_key, IPC_CREAT | 0770)) == -1 ) {
        perror("Queue create");
        exit(1);
    }

    num_liquid_lines = atoi(argv[1]);
    num_pill_lines = atoi(argv[2]);

    for (int i = 0; i < 100; i++) {
        trash_counter[i] = 0;
        non_defected_counter[i] = 0;
        produced_medicine_counter[i] = 0;
    }

    for (int i = 0; i < num_liquid_lines + num_pill_lines; i++) {

        InitialMessage init_msg;

        if (msgrcv(drawer_queue_id, &init_msg, sizeof(init_msg), INITIAL, 0) == -1) {
            perror("msg error");
            exit(-1);
        }
        num_inspectors[i] = init_msg.num_inspectors;
        num_packagers[i] = init_msg.num_packagers;
    }

    for (int i = 0; i < num_inspectors[0]; i++) {
        array_of_positions[0][i+1 ] = -0.8f + ((i*0.3) / 1.5);
    }

    for (int i = 0; i < num_packagers[0]; i++) {
        array_of_positions[0][i + num_inspectors[0] + 1 ] = 0.5f - ((0.3*i) / 1.5);
    }
    

    glutInit(&argc, argv); // Initialize GLUT
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // Enable double buffering, RGB colors, and depth buffer
    glutInitWindowSize(1500, 980); // Set the window size
    glutCreateWindow("Pharmaceutical Factory Simulation"); // Create the window with a title

    initialize(); // Call initialization routine
    glutDisplayFunc(display); // Register the display function
    glutTimerFunc(0, timer, 0);

    glutMainLoop(); // Enter the GLUT event processing loop

    return 0;
}