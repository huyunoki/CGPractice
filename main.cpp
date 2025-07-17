#include "glut.h"
#include <GL/gl.h>
#include <math.h>
#include <cmath>

// Camera variables
float cameraAngleX = 0.0f;
float cameraAngleY = 0.0f;
float lastMouseX, lastMouseY;
bool mouseDragging = false;

// Room dimensions
const float ROOM_SIZE = 10.0f;

void init() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void drawRoom() {
    // Walls
    glBegin(GL_QUADS);
    // Floor
    glColor3f(0.5f, 0.5f, 0.5f);
    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    // Ceiling
    glColor3f(0.8f, 0.8f, 0.8f);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);
    // Walls
    glColor3f(0.7f, 0.7f, 0.7f);
    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    glVertex3f(ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);

    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);

    glVertex3f(ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);
    glVertex3f(ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);

    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, -ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, -ROOM_SIZE, ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, ROOM_SIZE);
    glVertex3f(-ROOM_SIZE, ROOM_SIZE, -ROOM_SIZE);
    glEnd();
}


void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera transformation
    gluLookAt(0.0, 0.0, 0.0,
              sin(cameraAngleY), sin(cameraAngleX), cos(cameraAngleY),
              0.0, 1.0, 0.0);

    drawRoom();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            mouseDragging = false;
        }
    }
}

void motion(int x, int y) {
    if (mouseDragging) {
        float deltaX = (x - lastMouseX) * 0.005f;
        float deltaY = (y - lastMouseY) * 0.005f;

        cameraAngleY += deltaX;
        cameraAngleX += deltaY;

        // Clamp vertical angle
        if (cameraAngleX > 1.5f) cameraAngleX = 1.5f;
        if (cameraAngleX < -1.5f) cameraAngleX = -1.5f;

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Forgotten Scientist's Study");

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}
