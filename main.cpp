#include "glut.h"
#include <GL/gl.h>
#include <math.h>
#include <cmath>
#include <vector>

// Camera variables
float cameraAngleX = 20.0f; // Initial downward angle
float cameraAngleY = 0.0f;
float cameraDistance = 8.0f; // Increased distance for third-person view
float lastMouseX, lastMouseY;
bool mouseDragging = false;

// Human model's state
float humanX = 0.0f;
float humanY = 0.0f;
float humanZ = 0.0f;
float humanRotationY = 0.0f; // Human's orientation

// Keyboard state array
bool keyStates[256] = {false};

// Animation state
float walkCycle = 0.0f;

struct Object {
    float x, y, z; // Position
    float r, g, b; // Color
    float radius;  // Size
};

std::vector<Object> objects;

void drawHuman() {
    glPushMatrix();

    // Position and orient the human model
    glTranslatef(humanX, humanY, humanZ);
    glRotatef(humanRotationY, 0.0f, 1.0f, 0.0f);

    // Animation parameters
    float armAngle = sin(walkCycle) * 45.0f;
    float legAngle = sin(walkCycle) * 45.0f;

    // Torso
    glColor3f(0.0f, 0.0f, 1.0f); // Blue torso
    glPushMatrix();
    glTranslatef(0.0f, 0.5f, 0.0f);
    glScalef(0.6f, 1.0f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Head
    glColor3f(1.0f, 0.8f, 0.6f); // Skin color
    glPushMatrix();
    glTranslatef(0.0f, 1.25f, 0.0f);
    glutSolidSphere(0.3, 20, 20);
    glPopMatrix();

    // Right Arm
    glPushMatrix();
    glTranslatef(0.4f, 0.9f, 0.0f);
    glRotatef(armAngle, 1.0f, 0.0f, 0.0f); // Animate arm swing
    glTranslatef(0.0f, -0.4f, 0.0f);
    glScalef(0.2f, 0.8f, 0.2f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Left Arm
    glPushMatrix();
    glTranslatef(-0.4f, 0.9f, 0.0f);
    glRotatef(-armAngle, 1.0f, 0.0f, 0.0f); // Animate arm swing (opposite)
    glTranslatef(0.0f, -0.4f, 0.0f);
    glScalef(0.2f, 0.8f, 0.2f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Right Leg
    glColor3f(0.2f, 0.2f, 0.2f); // Dark pants
    glPushMatrix();
    glTranslatef(0.2f, 0.0f, 0.0f);
    glRotatef(-legAngle, 1.0f, 0.0f, 0.0f); // Animate leg swing
    glTranslatef(0.0f, -0.5f, 0.0f);
    glScalef(0.25f, 1.0f, 0.25f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Left Leg
    glPushMatrix();
    glTranslatef(-0.2f, 0.0f, 0.0f);
    glRotatef(legAngle, 1.0f, 0.0f, 0.0f); // Animate leg swing (opposite)
    glTranslatef(0.0f, -0.5f, 0.0f);
    glScalef(0.25f, 1.0f, 0.25f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPopMatrix();
}


void init() {
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f); // Gray background
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);

    // Enable lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    // Define light properties
    GLfloat light_position[] = { 5.0f, 5.0f, 5.0f, 0.0f };
    GLfloat light_ambient[] = { 0.2f, 0.2f, 0.2f, 1.0f };
    GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Set up the third-person camera
    glTranslatef(0.0f, -1.0f, -cameraDistance); // Move camera back and slightly down
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);   // Rotate view vertically (pitch)
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);   // Rotate view horizontally (yaw)
    glTranslatef(-humanX, -humanY, -humanZ);     // Center the view on the human

    // Draw the ground
    glColor3f(0.6f, 0.6f, 0.6f);
    glBegin(GL_QUADS);
        glVertex3f(-100.0f, -1.0f, -100.0f);
        glVertex3f(-100.0f, -1.0f,  100.0f);
        glVertex3f( 100.0f, -1.0f,  100.0f);
        glVertex3f( 100.0f, -1.0f, -100.0f);
    glEnd();

    // Draw the static objects
    for (const auto& obj : objects) {
        glPushMatrix();
        glTranslatef(obj.x, obj.y, obj.z);
        glColor3f(obj.r, obj.g, obj.b);
        glutSolidSphere(obj.radius, 20, 20);
        glPopMatrix();
    }

    // Draw the human
    drawHuman();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        }
        else {
            mouseDragging = false;
        }
    }
}

void motion(int x, int y) {
    if (mouseDragging) {
        float deltaX = (x - lastMouseX);
        float deltaY = (y - lastMouseY);

        cameraAngleY += deltaX * 0.5f;
        cameraAngleX += deltaY * 0.5f;

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay();
    }
}

void specialKeyboard(int key, int x, int y) {
    keyStates[key] = true;
}

void specialKeyboardUp(int key, int x, int y) {
    keyStates[key] = false;
}

void idle() {
    float moveSpeed = 0.001f;
    bool isMoving = false;

    // Calculate movement direction based on camera's Y-axis rotation
    float moveAngle = cameraAngleY * 3.14159 / 180.0;

    if (keyStates[GLUT_KEY_UP]) {
        humanX += sin(moveAngle) * moveSpeed;
        humanZ -= cos(moveAngle) * moveSpeed;
        humanRotationY = cameraAngleY; // Align human rotation with camera
        isMoving = true;
    }
    if (keyStates[GLUT_KEY_DOWN]) {
        humanX -= sin(moveAngle) * moveSpeed;
        humanZ += cos(moveAngle) * moveSpeed;
        humanRotationY = cameraAngleY + 180.0f; // Turn around
        isMoving = true;
    }
    if (keyStates[GLUT_KEY_LEFT]) {
        humanX -= cos(moveAngle) * moveSpeed;
        humanZ -= sin(moveAngle) * moveSpeed;
        humanRotationY = cameraAngleY - 90.0f; // Strafe left
        isMoving = true;
    }
    if (keyStates[GLUT_KEY_RIGHT]) {
        humanX += cos(moveAngle) * moveSpeed;
        humanZ += sin(moveAngle) * moveSpeed;
        humanRotationY = cameraAngleY + 90.0f; // Strafe right
        isMoving = true;
    }

    // Update walk animation cycle
    if (isMoving) {
        walkCycle += 0.001f; // Adjust animation speed
    } else {
        walkCycle = 0; // Reset to standing pose
    }

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Human walking");

    init();

    // Create some objects
    objects.push_back({-5.0f, 0.0f, -5.0f, 1.0f, 0.0f, 0.0f, 0.5f});
    objects.push_back({5.0f, 0.0f, -2.0f, 0.0f, 1.0f, 0.0f, 0.3f});
    objects.push_back({-2.0f, 0.5f, 5.0f, 0.0f, 0.0f, 1.0f, 0.4f});

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutSpecialFunc(specialKeyboard);
    glutSpecialUpFunc(specialKeyboardUp);
    glutIdleFunc(idle);

    glutMainLoop();
    return 0;
}