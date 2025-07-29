#include "glut.h"
#include <GL/gl.h>
#define _USE_MATH_DEFINES // For M_PI on Windows
#include <math.h>
#include <cmath>
#include <vector>
#include <iostream> // For debugging, if needed
#include <random>   // For random number generation
#include <chrono>   // For seeding random number generator

// Camera variables
float cameraAngleX = 20.0f; // Initial downward angle (pitch)
// cameraAngleY is now effectively managed by humanRotationY for camera yaw
// float cameraAngleY = 0.0f; 
float cameraDistance = 8.0f; // Increased distance for third-person view
float lastMouseX, lastMouseY;
bool mouseDragging = false;

// Human model's state
float humanX = 0.0f;
float humanY = 0.0f;
float humanZ = 0.0f;
float humanRotationY = 0.0f; // Human's orientation (yaw), controlled by mouse X

// Keyboard state array
bool keyStates[256] = { false }; // For standard keys
bool specialKeyStates[256] = { false }; // For special keys (GLUT_KEY_UP, etc.)

// Animation state
float walkCycle = 0.0f;

// Random number generator setup
std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
std::uniform_real_distribution<float> dist_pos(-50.0f, 50.0f); // For initial X, Z positions (used only for initial spawn)
std::uniform_real_distribution<float> dist_vel_y(0.01f, 0.03f); // For upward velocity
std::uniform_real_distribution<float> dist_vel_xz(-0.005f, 0.005f); // For horizontal drift
std::uniform_real_distribution<float> dist_flame_offset(0.0f, 100.0f); // For unique flame animation offsets
// Initial Y positions for lanterns, can start below or above the human
std::uniform_real_distribution<float> dist_starting_y(-10.0f, 50.0f);

struct Object {
    float x, y, z; // Position
    float r, g, b; // Color
    float radius;  // Size
};

std::vector<Object> objects; // Static objects in the scene (currently empty)

// Lantern state - now for individual lanterns
struct KomLoyLantern {
    float x, y, z; // Position
    float velX, velY, velZ; // Velocity for movement
    float currentFlameAnimation; // Individual flame animation time
    float corePulsation; // Individual core pulsation value
};

std::vector<KomLoyLantern> lanterns; // Vector to hold multiple lanterns
const int NUM_LANTERNS = 1500; // Number of lanterns to generate

// Display list for the static parts of the lantern
GLuint lanternDisplayList;

struct FlamePolygon {
    float scale_x, scale_y;
    float rotation;
    float alpha;
    float animation_offset; // Unique offset for each polygon's animation
};

std::vector<FlamePolygon> flamePolygons; // These define the general flame shape/behavior

// Function prototypes (declarations)
void drawFlame(float flameAnimTime, float corePulse);
void drawHook();
void drawLanternFrame();
void drawLanternCover();
void drawLanternRoof();
void drawSingleLantern(const KomLoyLantern& l);
void drawHuman();


// Function to draw the flame (now takes individual lantern's animation time)
void drawFlame(float flameAnimTime, float corePulse) {
    // Save current lighting state
    GLboolean wasLightingEnabled;
    glGetBooleanv(GL_LIGHTING, &wasLightingEnabled);

    // --- Draw the Flame Core ---
    glEnable(GL_LIGHTING); // Enable lighting for the core's emission
    glDepthMask(GL_TRUE); // Ensure core writes to depth buffer

    glPushMatrix();
    // Position the core slightly below the lantern's base, as per the image
    glTranslatef(0.0f, -0.65f, 0.0f);

    // Animate the core's size and color based on individual corePulse
    float corePulseScale = 0.1f + corePulse * 0.05f; // Small pulsation in size
    float coreR = 1.0f;
    float coreG = 0.4f + corePulse * 0.6f; // Brighter yellow with pulsation
    float coreB = 0.1f;

    glColor4f(coreR, coreG, coreB, 1.0f); // Opaque core color
    glScalef(corePulseScale, corePulseScale * 1.5f, corePulseScale); // Elongated shape, now with pulsation

    // Set emission property for the core to make it glow
    GLfloat emission[] = { coreR, coreG * 0.7f, coreB * 0.5f, 1.0f }; // Warm yellow glow, linked to core color
    glMaterialfv(GL_FRONT, GL_EMISSION, emission);

    glutSolidSphere(0.5, 10, 10); // Draw the core as a sphere

    // Reset emission so other objects don't glow
    GLfloat no_emission[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
    glPopMatrix();

    // --- Draw the Oscillating Flame Polygons ---
    glDisable(GL_LIGHTING); // Disable lighting for the flame polygons (they are self-illuminating)
    glDepthMask(GL_FALSE);    // Disable depth writing for proper blending (prevents polygons from obscuring each other)
    // Use additive blending for a glowing effect. This makes overlapping transparent parts brighter.
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& p : flamePolygons) {
        glPushMatrix();
        // Position the flame polygons so their base (local Y=0) aligns with the core's center
        // This ensures the flame does not extend below the core.
        glTranslatef(0.0f, -0.65f, 0.0f); // Flame base starts at core's center Y
        glRotatef(p.rotation, 0.0f, 0.0f, 1.0f); // Rotate for flickering effect
        // Adjust scale for flame to be contained within the lantern
        glScalef(p.scale_x * 0.8f, p.scale_y * 0.8f, 1.0f); // Reduced overall flame size

        // Draw multiple layers for 3D effect and smoother appearance
        for (int i = 0; i < 10; ++i) {
            glPushMatrix();
            // Rotate around Y-axis with some random wobble for a more organic look
            glRotatef(i * 18.0f + sin(p.animation_offset * 3.0f) * 5.0f, 0.0f, 1.0f, 0.0f);

            glBegin(GL_TRIANGLES);
            // Tip of the flame (brighter, more yellow, fading out with layers)
            // Adjusted Y coordinates so the base is at local Y=0
            glColor4f(1.0f, 1.0f, 0.5f, p.alpha * (0.8f - 0.5f * (float)i / 9.0f));
            glVertex3f(0.0f, 0.75f, 0.0f); // Tip (original 0.5 + 0.25 offset)

            // Base of the flame (reddish-orange, less transparent, brighter with core pulsation)
            // Adjusted Y coordinates so the base is at local Y=0
            glColor4f(1.0f, 0.4f + corePulse * 0.3f, 0.1f, p.alpha * (0.2f + 0.3f * (float)i / 9.0f));
            glVertex3f(-0.2f, 0.0f, 0.0f); // Bottom-left (original -0.25 + 0.25 offset)
            glVertex3f(0.2f, 0.0f, 0.0f);  // Bottom-right (original -0.25 + 0.25 offset)
            glEnd();
            glPopMatrix();
        }

        glPopMatrix();
    }

    glDepthMask(GL_TRUE);    // Re-enable depth writing after drawing transparent parts

    // Restore lighting state
    if (wasLightingEnabled) {
        glEnable(GL_LIGHTING);
    }
    else {
        glDisable(GL_LIGHTING);
    }
    // Restore default blending function to avoid affecting other transparent objects
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

// Function to draw the hook above the lantern (adjusted for Kom Loy style)
void drawHook() {
    glDisable(GL_LIGHTING); // Hook should not be affected by scene lighting
    glColor3f(0.3f, 0.3f, 0.3f); // Dark gray for metal hook

    float hookHeight = 0.1f; // Shorter hook
    float hookRadius = 0.02f; // Thinner hook
    int segments = 16; // Number of segments for the cylinder approximation

    glPushMatrix();
    // Position above the lantern, adjusted for the new lantern shape
    glTranslatef(0.0f, 0.5f + hookHeight / 2.0f + 0.1f, 0.0f);

    // Draw top circle
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, hookHeight / 2.0f, 0.0f); // Center of top circle
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    // Draw bottom circle
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, -hookHeight / 2.0f, 0.0f); // Center of bottom circle
    for (int i = segments; i >= 0; --i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), -hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    // Draw cylinder sides
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(hookRadius * cos(angle), hookHeight / 2.0f, hookRadius * sin(angle));
        glVertex3f(hookRadius * cos(angle), -hookHeight / 2.0f, hookRadius * sin(angle));
    }
    glEnd();

    glPopMatrix();

    glEnable(GL_LIGHTING); // Re-enable lighting
}

// Function to draw the lantern frame (updated for Kom Loy style - minimal internal frame)
void drawLanternFrame() {
    glDisable(GL_LIGHTING); // Frame should not be affected by scene lighting
    glColor3f(0.4f, 0.2f, 0.0f); // Dark brown for bamboo (very thin)

    float baseRadius = 0.35f; // Wider base
    float frameThickness = 0.015f; // Slightly thicker for the base ring/fence

    glPushMatrix();
    // Position this ring at the very bottom of the lantern's paper body
    glTranslatef(0.0f, -0.6f, 0.0f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= 30; ++i) { // More segments for a smoother circle
        float angle = 2.0f * M_PI * (float)i / 30.0f;
        glVertex3f(baseRadius * cos(angle), 0.0f, baseRadius * sin(angle));
        glVertex3f(baseRadius * cos(angle), frameThickness, baseRadius * sin(angle));
    }
    glEnd();
    glPopMatrix();

    // Draw the burner base (a small square at the bottom, attached to the ring)
    glColor3f(0.6f, 0.4f, 0.0f); // Slightly lighter brown for the burner
    glPushMatrix();
    glTranslatef(0.0f, -0.6f - 0.1f, 0.0f); // Below the bottom ring
    glScalef(baseRadius * 0.8f, frameThickness * 3.0f, baseRadius * 0.8f); // Flat square
    glutSolidCube(1.0);
    glPopMatrix();

    // No other vertical pillars or top rings as per user's request.

    glEnable(GL_LIGHTING); // Re-enable lighting
}

// Function to draw the lantern cover (updated for Kom Loy style - tapered cylinder)
void drawLanternCover() {
    glDisable(GL_LIGHTING); // Cover should not be affected by scene lighting
    glDepthMask(GL_FALSE);    // Disable depth writing for proper blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Use normal blending for the cover

    // Adjusted alpha to make it barely transparent
    glColor4f(1.0f, 0.9f, 0.7f, 0.95f); // Semi-transparent paper color (off-white/cream), much less transparent

    float baseRadius = 0.34f; // Slightly smaller than frame base
    // topRadius is now very close to baseRadius for a cylindrical shape
    float topRadius = 0.33f;  // Very slight taper, almost a cylinder
    float coverHeight = 1.2f; // Taller to match the image
    int segments = 40; // More segments for smoother curve
    int stacks = 20;   // More stacks for vertical smoothness

    // Draw a tapered cylinder/conical frustum for the main paper body
    glPushMatrix();
    // Position the cover so its bottom aligns with the frame's bottom ring
    glTranslatef(0.0f, -coverHeight / 2.0f + (0.6f - coverHeight / 2.0f), 0.0f);

    for (int j = 0; j < stacks; ++j) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * M_PI * (float)i / (float)segments;

            float r1 = baseRadius + (topRadius - baseRadius) * (float)j / (float)stacks;
            float r2 = baseRadius + (topRadius - baseRadius) * (float)(j + 1) / (float)stacks;

            float y1 = (float)j / (float)stacks * coverHeight;
            float y2 = (float)(j + 1) / (float)stacks * coverHeight;

            // Vertices for the current strip segment
            glVertex3f(r1 * cos(angle), y1, r1 * sin(angle));
            glVertex3f(r2 * cos(angle), y2, r2 * sin(angle));
        }
        glEnd();
    }
    glPopMatrix();

    glDepthMask(GL_TRUE);    // Re-enable depth writing
    glEnable(GL_LIGHTING);  // Re-enable lighting
}

// Function to draw the lantern roof (flat)
void drawLanternRoof() {
    glDisable(GL_LIGHTING); // Roof should not be affected by scene lighting
    // Use the same color as the lantern cover
    glColor4f(1.0f, 0.9f, 0.7f, 0.95f);

    float roofRadius = 0.35f; // Slightly larger than the lantern cover's top
    float roofThickness = 0.02f; // Very thin for a flat roof
    int segments = 30; // For a smooth circular shape

    glPushMatrix();
    // Position the roof directly on top of the lantern cover.
    // The lantern cover's top is at Y = 0.6f. So, the roof's bottom should be at Y = 0.6f.
    glTranslatef(0.0f, 0.6f, 0.0f);

    // Draw the top face of the flat roof (a circle)
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, roofThickness, 0.0f); // Center of the top face
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), roofThickness, roofRadius * sin(angle));
    }
    glEnd();

    // Draw the bottom face of the flat roof (a circle)
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, 0.0f); // Center of the bottom face
    for (int i = segments; i >= 0; --i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), 0.0f, roofRadius * sin(angle));
    }
    glEnd();

    // Draw the side of the flat roof (a thin cylinder wall)
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * M_PI * (float)i / (float)segments;
        glVertex3f(roofRadius * cos(angle), roofThickness, roofRadius * sin(angle));
        glVertex3f(roofRadius * cos(angle), 0.0f, roofRadius * sin(angle));
    }
    glEnd();

    glPopMatrix();
    glEnable(GL_LIGHTING); // Re-enable lighting
}


// Function to draw a single lantern (now takes a KomLoyLantern object)
void drawSingleLantern(const KomLoyLantern& l) {
    glPushMatrix();
    glTranslatef(l.x, l.y, l.z); // Position the lantern based on its properties

    // Call the display list for the static parts of the lantern
    glCallList(lanternDisplayList);

    // Draw the flame separately as it's dynamic per lantern
    drawFlame(l.currentFlameAnimation, l.corePulsation);

    glPopMatrix();
}

// Function to draw the human model (unchanged)
void drawHuman() {
    glPushMatrix();

    // Position and orient the human model
    glTranslatef(humanX, humanY, humanZ);
    glRotatef(humanRotationY, 0.0f, 1.0f, 0.0f); // Human's rotation applied here

    // Animation parameters for walking
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

// Initialization function
void init() {
    glClearColor(0.0f, 0.0f, 0.1f, 1.0f); // Dark blue background for night sky
    glEnable(GL_DEPTH_TEST); // Enable depth testing for correct 3D rendering
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, 800.0 / 600.0, 0.1, 500.0); // Increased far clipping plane for distant lanterns
    glMatrixMode(GL_MODELVIEW);

    // Enable blending for transparency effects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Default blending for transparent objects

    // Enable lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0); // Enable light source 0
    glEnable(GL_COLOR_MATERIAL); // Allow glColor3f to set material properties

    // Define light properties (a subtle ambient light for overall scene)
    GLfloat light_position[] = { 0.0f, 100.0f, 0.0f, 0.0f }; // Directional light from above
    GLfloat light_ambient[] = { 0.1f, 0.1f, 0.15f, 1.0f }; // Very dim blue ambient light
    GLfloat light_diffuse[] = { 0.3f, 0.3f, 0.4f, 1.0f }; // Dim diffuse light
    GLfloat light_specular[] = { 0.0f, 0.0f, 0.0f, 1.0f }; // No specular light from global source

    glLightfv(GL_LIGHT0, GL_POSITION, light_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    // Initialize flame polygons with unique animation offsets
    for (int i = 0; i < 5; ++i) {
        flamePolygons.push_back({ 1.0f, 1.0f, 0.0f, 1.0f, dist_flame_offset(rng) }); // Use random offset
    }

    // --- Create the display list for the static lantern parts ---
    lanternDisplayList = glGenLists(1); // Generate a unique ID for the display list
    glNewList(lanternDisplayList, GL_COMPILE); // Start compiling commands into the list
    // Draw the static components of a single lantern at its local origin (0,0,0)
    drawLanternFrame();
    drawLanternCover();
    drawLanternRoof();
    drawHook();
    glEndList(); // End compiling the display list

    // Initialize lanterns
    for (int i = 0; i < NUM_LANTERNS; ++i) {
        KomLoyLantern newLantern;
        newLantern.x = dist_pos(rng);
        newLantern.y = dist_starting_y(rng); // Use new distribution for random starting Y
        newLantern.z = dist_pos(rng);
        newLantern.velX = dist_vel_xz(rng);
        newLantern.velY = dist_vel_y(rng);
        newLantern.velZ = dist_vel_xz(rng);
        newLantern.currentFlameAnimation = dist_flame_offset(rng); // Each lantern has its own flame animation time
        newLantern.corePulsation = 0.0f; // Will be calculated per frame

        lanterns.push_back(newLantern);
    }
}

// Display callback function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear color and depth buffers
    glLoadIdentity(); // Reset the modelview matrix

    // Set up the third-person camera looking at the human
    // Calculate camera position behind and slightly above the human,
    // relative to human's facing direction (humanRotationY)
    float cameraYawRad = humanRotationY * M_PI / 180.0f; // Camera yaw aligns with human's yaw
    float camX = humanX - sin(cameraYawRad) * cameraDistance;
    float camY = humanY + 1.5f; // Camera height
    float camZ = humanZ + cos(cameraYawRad) * cameraDistance;

    // Apply camera pitch (cameraAngleX) to the camera's vertical position relative to the human
    // This is a simplified way to apply pitch with gluLookAt
    float pitchOffset = cameraDistance * sin(cameraAngleX * M_PI / 180.0f);
    camY += pitchOffset;

    // Use gluLookAt to set the camera
    gluLookAt(camX, camY, camZ,       // Camera position
        humanX, humanY + 0.5f, humanZ, // Look at human's center
        0.0f, 1.0f, 0.0f);      // Up vector

    // Draw the human model
    drawHuman();

    // Draw all lanterns
    for (const auto& l : lanterns) {
        drawSingleLantern(l);
    }

    glutSwapBuffers(); // Swap the front and back buffers
}

// Reshape callback function
void reshape(int w, int h) {
    glViewport(0, 0, w, h); // Set the viewport to the new window size
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 0.1, 500.0); // Update perspective projection
    glMatrixMode(GL_MODELVIEW);
}

// Mouse callback function
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

// Mouse motion callback function
void motion(int x, int y) {
    if (mouseDragging) {
        float deltaX = (float)(x - lastMouseX); // Explicit cast to float
        float deltaY = (float)(y - lastMouseY); // Explicit cast to float

        humanRotationY += deltaX * 0.5f; // Mouse X now rotates the human directly
        cameraAngleX += deltaY * 0.5f; // Mouse Y still controls camera pitch

        // Clamp cameraAngleX to prevent flipping
        if (cameraAngleX > 89.0f) cameraAngleX = 89.0f;
        if (cameraAngleX < -89.0f) cameraAngleX = -89.0f;

        lastMouseX = x;
        lastMouseY = y;

        glutPostRedisplay(); // Request a redraw
    }
}

// Keyboard key down callback function (for standard keys)
void keyboard(unsigned char key, int x, int y) {
    // 'l' or 'L' no longer releases a single lantern, as all are released at start
    keyStates[key] = true; // Set key state to true when pressed
}

// Keyboard key up callback function (for standard keys)
void keyboardUp(unsigned char key, int x, int y) {
    keyStates[key] = false; // Set key state to false when released
}

// Special keyboard key down callback function (for arrow keys)
void specialKeyboard(int key, int x, int y) {
    specialKeyStates[key] = true; // Set special key state to true when pressed
}

// Special keyboard key up callback function (for arrow keys)
void specialKeyboardUp(int key, int x, int y) {
    specialKeyStates[key] = false; // Set special key state to false when released
}

// Timer function for animation updates
void timer(int value) {
    float moveSpeed = 2.0f;
    bool isMoving = false;

    // Calculate movement direction based on human's Y-axis rotation (facing direction)
    float humanFacingRad = humanRotationY * M_PI / 180.0; // Use human's own rotation for movement direction

    float deltaMoveX = 0.0f;
    float deltaMoveZ = 0.0f;

    // Forward/Backward movement
    if (specialKeyStates[GLUT_KEY_UP]) {
        deltaMoveX += sin(humanFacingRad) * moveSpeed;
        deltaMoveZ -= cos(humanFacingRad) * moveSpeed;
        isMoving = true;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        deltaMoveX -= sin(humanFacingRad) * moveSpeed;
        deltaMoveZ += cos(humanFacingRad) * moveSpeed;
        isMoving = true;
    }
    // Strafe Left/Right movement (relative to human's facing)
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        // Strafe left is 90 degrees counter-clockwise from facing direction
        deltaMoveX -= cos(humanFacingRad) * moveSpeed;
        deltaMoveZ -= sin(humanFacingRad) * moveSpeed;
        isMoving = true;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        // Strafe right is 90 degrees clockwise from facing direction
        deltaMoveX += cos(humanFacingRad) * moveSpeed;
        deltaMoveZ += sin(humanFacingRad) * moveSpeed;
        isMoving = true;
    }

    humanX += deltaMoveX;
    humanZ += deltaMoveZ;

    // Update walk animation cycle
    if (isMoving) {
        walkCycle += 0.1f; // Adjust animation speed
    }
    else {
        walkCycle = 0; // Reset to standing pose when not moving
    }

    // Define a culling box around the human for lantern management
    // Lanterns below the human's feet will disappear quickly
    float cull_min_y = humanY - 1.0f; // 1 unit below human (effectively at human's feet)
    // ランタンが消える高さ (現在の 75.0f の半分)
    float cull_max_y = humanY + 37.5f; // 75.0f / 2 = 37.5f
    // Lanterns too far horizontally will disappear
    float cull_horizontal_radius = 50.0f; // Increased for a wider, more consistent distribution

    // Distributions for respawning lanterns relative to the human
    // New lanterns will appear from human's height up to a certain height above
    // 再出現する高さの範囲も半分に調整 (70.0f / 2 = 35.0f)
    std::uniform_real_distribution<float> respawn_y_offset_dist(0.0f, 35.0f); // キャラクターの目線あたりから上空35mまでで再出現
    // New lanterns will appear within a closer horizontal range, matching cull_horizontal_radius
    std::uniform_real_distribution<float> respawn_xz_offset_dist(-50.0f, 50.0f); // Offset from humanX/Z (matches cull radius)

    // Update all lanterns
    for (auto& l : lanterns) {
        l.x += l.velX;
        l.y += l.velY;
        l.z += l.velZ;

        // Animate each lantern's flame independently
        l.currentFlameAnimation += 0.05f; // Slower overall animation
        l.corePulsation = (sin(l.currentFlameAnimation * 1.0f) + 1.0f) * 0.5f;

        // Reset lantern if it goes outside the culling box
        // This logic ensures continuous respawn even when stationary
        if (l.y < cull_min_y || l.y > cull_max_y ||
            std::abs(l.x - humanX) > cull_horizontal_radius ||
            std::abs(l.z - humanZ) > cull_horizontal_radius)
        {
            // Respawn lantern at a random position within the desired viewable range
            // This ensures lanterns appear around the character and can start high up.
            l.y = humanY + respawn_y_offset_dist(rng); // Respawn from human's height upwards
            l.x = humanX + respawn_xz_offset_dist(rng);
            l.z = humanZ + respawn_xz_offset_dist(rng);

            l.velX = dist_vel_xz(rng); // Re-randomize horizontal velocity
            l.velY = dist_vel_y(rng);  // Re-randomize upward velocity
            l.velZ = dist_vel_xz(rng);
        }
    }

    glutPostRedisplay(); // Request a redraw
    glutTimerFunc(16, timer, 0); // Call timer again after 16ms (approx 60 FPS)
}

int main(int argc, char** argv) {
    glutInit(&argc, argv); // Initialize GLUT
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // Double buffer, RGB color, Depth buffer
    glutInitWindowSize(800, 600); // Set initial window size
    glutCreateWindow("Kom Loy Festival Simulation"); // Create window with title

    init(); // Call custom initialization function

    // Register callback functions
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard); // Register standard keyboard callback
    glutKeyboardUpFunc(keyboardUp); // Register standard keyboard up callback
    glutSpecialFunc(specialKeyboard); // Register special keyboard callback
    glutSpecialUpFunc(specialKeyboardUp); // Register special keyboard up callback
    glutTimerFunc(0, timer, 0); // Start the timer immediately

    glutMainLoop(); // Enter the GLUT event processing loop
    return 0;
}
