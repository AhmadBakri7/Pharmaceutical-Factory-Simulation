#include "headers.h"
// Function to initialize OpenGL settings
void drawPlayerWithCane(float x, float y);
void drawText(float x, float y, const char *string);

void initialize() {
    glClearColor(0.0, 0.0, 0.0, 1.0); // Set background color to black
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
    drawRectangle(x + width / 6.0f, y + height / 2.0f + height / 16.0f, width * 2.0f / 3.0f, height / 8.0f);
    char buff[1000];
    glColor3f(0, 0, 0);
    sprintf(buff, "%d", number);
    drawText(x + width / 6.0f, y + height / 2.0f + height / 16.0f, buff);
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
void drawWorker(float x, float y) {
    // Set player color
    glColor3f(1.0f, 0.0f, 0.0f); // Set color to red

    // Draw head
    drawFilledCircle(x, y + 0.08f, 0.03f, 20);

    glColor3f(1.0f, 1.0f, 1.0f); // Set color to white
    //draw eyes
    drawFilledCircle(x-0.015, y + 0.09f, 0.006f, 10);//left eye
    drawFilledCircle(x+0.015, y + 0.09f, 0.006f, 10);//right eye
    //draw mouth
    drawRectangle(x-0.021, y+0.065, 0.04, 0.003);

    glColor3f(1.0f, 0.0f, 0.0f); // Set color to green
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

void drawText(float x, float y, const char *string) {
    glRasterPos2f(x, y);

    // Loop through each character of the string
    while (*string) {
        glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, *string);
        string++;
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
    glColor3f(1.0, 1.0, 1.0); // Set color to white
    drawRectangle(-2.0f, 0.3f, 4.0f, 0.02f);

    // the second line for liquid 
    glColor3f(1.0, 1.0, 1.0); // Set color to white
    drawRectangle(-2.0f, 0.6f, 4.0f, 0.02f);

    // Draw a bottled medicine
    int number=2;
    int num_of_bottled=3;
      for (int i = 0; i < num_of_bottled; i++) {
        float x_offset = i * (1.0f / (num_of_bottled - 1)); // Calculate the x offset
        drawBottledMedicine(-0.2f+ x_offset, 0.4f, 0.1f, 0.2f,number);
    }

    // the first line for pill 
    glColor3f(1.0, 1.0, 1.0); // Set color to white
    drawRectangle(-2.0f, -0.1f, 4.0f, 0.02f);

    // the second line for pill 
    glColor3f(1.0, 1.0, 1.0); // Set color to white
    drawRectangle(-2.0f, -0.4f, 4.0f, 0.02f);
    int Number_Of_Pills=6;
    float pillSize=0.05f; 
      int numofplastic=3;
      for (int i = 0; i < numofplastic; i++) {
        float x_offset = i * (1.0f / (numofplastic - 1)); // Calculate the x offset
    drawPlasticContainer(-0.2f+ x_offset, -0.3f, 0.2f, 0.2f,Number_Of_Pills,pillSize );
    }
    int num_of_worker=5;
    for (int i = 0; i < num_of_worker; i++) {
        float x_offset = i * (1.0f / (num_of_worker - 1)); // Calculate the x offset
        drawWorker(-0.2f+x_offset,-0.5f); // Adjust the coordinates as needed
    }
    glutSwapBuffers(); // Swap the front and back buffers
}

// Main function
int main(int argc, char** argv) {
    glutInit(&argc, argv); // Initialize GLUT
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // Enable double buffering, RGB colors, and depth buffer
    glutInitWindowSize(950, 700); // Set the window size
    glutCreateWindow("Pharmaceutical Factory Simulation"); // Create the window with a title

    initialize(); // Call initialization routine
    glutDisplayFunc(display); // Register the display function

    glutMainLoop(); // Enter the GLUT event processing loop
    
    return 0;
}


