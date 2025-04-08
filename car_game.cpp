#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <iostream>
#include <cmath>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


int winWidth = 500, winHeight = 700;

enum GameState { MENU, PLAYING, GAME_OVER };
GameState gameState = MENU;
GLuint carTexture;


int score = 0;
bool collide = false;

int lanes[3] = { 150, 250, 350 };
int currentLaneIndex = 1;
int vehicleX = lanes[currentLaneIndex], vehicleY = 70;
int ovehicleX[4], ovehicleY[4];
bool obstaclePassed[4];  // NEW: Track if obstacle has been passed
int movd = 0;
char buffer[10];

enum ObstacleType { OBSTACLE_CAR, OBSTACLE_BUSH, OBSTACLE_GUTTER, OBSTACLE_ROCK };
ObstacleType oType[4];

struct BushBlob {
    float offsetX[5];
    float offsetY[5];
    float radius[5];
    float green[5];
};
BushBlob bushBlobs[4];

void *font18 = GLUT_BITMAP_HELVETICA_18;
void *boldFont = GLUT_BITMAP_TIMES_ROMAN_24;

int getTextWidth(const char *text, void *font) {
    int width = 0;
    for (int i = 0; text[i] != '\0'; i++)
        width += glutBitmapWidth(font, text[i]);
    return width;
}

GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* image = stbi_load(filename, &width, &height, &channels, STBI_rgb_alpha);
    if (!image) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        exit(1);
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(image);
    return tex;
}


void drawText(const char *text, int x, int y, void *font, float r, float g, float b) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (int i = 0; i < strlen(text); i++)
        glutBitmapCharacter(font, text[i]);
}

void drawCenteredText(const char *text, int y, void *font, float r, float g, float b) {
    int width = getTextWidth(text, font);
    drawText(text, (winWidth - width) / 2, y, font, r, g, b);
}

void drawButtonCentered(int y, int w, int h, const char *label) {
    int x = (winWidth - w) / 2;
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();

    glColor3f(0, 0, 0);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y); glVertex2f(x + w, y);
    glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();

    int textWidth = getTextWidth(label, boldFont);
    drawText(label, x + (w - textWidth) / 2, y + 12, boldFont, 0, 0, 0);
}

void resetBushBlob(int i) {
    for (int b = 0; b < 5; b++) {
        bushBlobs[i].offsetX[b] = (rand() % 15) - 7;
        bushBlobs[i].offsetY[b] = (rand() % 15) - 7;
        bushBlobs[i].radius[b] = 10 + rand() % 6;
        bushBlobs[i].green[b] = 0.6f + 0.15f * (rand() % 4);
        if (bushBlobs[i].green[b] > 1.0f) bushBlobs[i].green[b] = 1.0f;
    }
}

void resetGame() {
    score = 0;
    collide = false;
    vehicleX = lanes[currentLaneIndex = 1];
    for (int i = 0; i < 4; i++) {
        ovehicleX[i] = lanes[rand() % 3];
        ovehicleY[i] = 1000 - i * 250;
        oType[i] = static_cast<ObstacleType>(rand() % 4);
        obstaclePassed[i] = false; // NEW
        if (oType[i] == OBSTACLE_BUSH) {
            resetBushBlob(i);
        }
    }
    movd = 0;
}

void drawGame() {
    glColor3f(0, 1, 0);
    glBegin(GL_QUADS);
    glVertex2f(0, 0); glVertex2f(0, winHeight);
    glVertex2f(winWidth, winHeight); glVertex2f(winWidth, 0);
    glEnd();

    glColor3f(0.1, 0.1, 0.1);
    glBegin(GL_QUADS);
    glVertex2f(100, 0); glVertex2f(100, winHeight);
    glVertex2f(400, winHeight); glVertex2f(400, 0);
    glEnd();

    glColor3f(1, 1, 1);
    for (int lane = 1; lane < 3; lane++) {
        int laneX = 100 + lane * 100;
        for (int i = 0; i < 20; i++) {
            glBegin(GL_QUADS);
            glVertex2f(laneX - 5, i * 40 + (movd % 40));
            glVertex2f(laneX + 5, i * 40 + (movd % 40));
            glVertex2f(laneX + 5, i * 40 + 20 + (movd % 40));
            glVertex2f(laneX - 5, i * 40 + 20 + (movd % 40));
            glEnd();
        }
    }

    movd -= 5;
    if (movd < -40) movd = 0;

    glColor3f(0, 0, 1);
    glBegin(GL_QUADS);
    glVertex2f(vehicleX - 25, vehicleY - 20);
    glVertex2f(vehicleX + 25, vehicleY - 20);
    glVertex2f(vehicleX + 25, vehicleY + 20);
    glVertex2f(vehicleX - 25, vehicleY + 20);
    glEnd();

    for (int i = 0; i < 4; i++) {
        int x = ovehicleX[i];
        int y = ovehicleY[i];

        switch (oType[i]) {
            case OBSTACLE_CAR:
                glColor3f(1.0, 0.0, 0.0);
                glBegin(GL_QUADS);
                glVertex2f(x - 20, y - 25);
                glVertex2f(x + 20, y - 25);
                glVertex2f(x + 20, y + 25);
                glVertex2f(x - 20, y + 25);
                glEnd();

                glColor3f(0.1, 0.1, 0.1);
                glBegin(GL_QUADS);
                glVertex2f(x - 15, y - 10);
                glVertex2f(x + 15, y - 10);
                glVertex2f(x + 15, y + 10);
                glVertex2f(x - 15, y + 10);
                glEnd();
                break;

            case OBSTACLE_BUSH:
                for (int b = 0; b < 5; b++) {
                    float ox = bushBlobs[i].offsetX[b];
                    float oy = bushBlobs[i].offsetY[b];
                    float r = bushBlobs[i].radius[b];
                    float g = bushBlobs[i].green[b];

                    glColor3f(0.0f, g, 0.0f);
                    glBegin(GL_POLYGON);
                    for (int j = 0; j < 20; ++j) {
                        float theta = j * 2.0f * 3.14159f / 20;
                        float dx = cos(theta) * r;
                        float dy = sin(theta) * r;
                        glVertex2f(x + ox + dx, y + oy + dy);
                    }
                    glEnd();
                }
                break;

            case OBSTACLE_GUTTER:
                glColor3f(0.4f, 0.4f, 0.4f);
                glBegin(GL_QUADS);
                glVertex2f(x - 20, y - 25);
                glVertex2f(x + 20, y - 25);
                glVertex2f(x + 20, y + 25);
                glVertex2f(x - 20, y + 25);
                glEnd();

                glColor3f(1.0f, 1.0f, 0.0f);
                for (int l = -20; l <= 20; l += 15) {
                    glBegin(GL_LINES);
                    glVertex2f(x + l - 10, y - 25);
                    glVertex2f(x + l + 10, y + 25);
                    glEnd();
                }
                break;

            case OBSTACLE_ROCK:
                glColor3f(0.2f, 0.2f, 0.2f);
                glBegin(GL_POLYGON);
                glVertex2f(x - 15, y - 10);
                glVertex2f(x - 5, y - 20);
                glVertex2f(x + 10, y - 10);
                glVertex2f(x + 20, y + 0);
                glVertex2f(x + 5, y + 15);
                glVertex2f(x - 10, y + 10);
                glEnd();
                break;
        }

        if (!collide && ovehicleX[i] == vehicleX &&
            ovehicleY[i] > vehicleY - 40 && ovehicleY[i] < vehicleY + 40)
            collide = true;

        ovehicleY[i] -= 3;

        // NEW: Increment score if obstacle safely passed
        if (!collide && !obstaclePassed[i] && ovehicleY[i] + 25 < vehicleY - 20) {
            score++;
            obstaclePassed[i] = true;
        }

        if (ovehicleY[i] < -50) {
            int newX = lanes[rand() % 3];
            int newY = winHeight;
            bool valid = true;
            for (int j = 0; j < 4; j++) {
                if (j != i && ovehicleX[j] != newX && abs(ovehicleY[j] - newY) < 150) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                ovehicleX[i] = newX;
                ovehicleY[i] = newY;
                oType[i] = static_cast<ObstacleType>(rand() % 4);
                obstaclePassed[i] = false; // NEW
                if (oType[i] == OBSTACLE_BUSH) resetBushBlob(i);
            }
        }
    }

    sprintf(buffer, "%05d", score);
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(10, winHeight - 40); glVertex2f(150, winHeight - 40);
    glVertex2f(150, winHeight - 10); glVertex2f(10, winHeight - 10);
    glEnd();
    drawText("SCORE:", 15, winHeight - 30, boldFont, 1, 0, 0);
    drawText(buffer, 100, winHeight - 30, boldFont, 1, 0, 0);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (gameState == MENU) {
        drawCenteredText("CAR ARCADE GAME", winHeight - 200, boldFont, 1, 0, 0);
        drawCenteredText("BY Kashish & Ananya", winHeight - 230, font18, 1, 1, 1);
        drawButtonCentered(winHeight - 300, 150, 40, "START GAME");
    }
    else if (gameState == PLAYING) {
        drawGame();
        if (collide) gameState = GAME_OVER;
    }
    else if (gameState == GAME_OVER) {
        drawCenteredText("GAME OVER", winHeight / 2 + 40, boldFont, 1, 0, 0);
        char scoreStr[32];
        sprintf(scoreStr, "SCORE:  %05d", score);
        drawCenteredText(scoreStr, winHeight / 2 - 10, boldFont, 1, 1, 1);
        drawButtonCentered(winHeight / 2 - 60, 150, 40, "RESTART");
    }
    glutSwapBuffers();
}

void reshape(int w, int h) {
    winWidth = w;
    winHeight = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void mouseClick(int button, int state, int x, int y) {
    int yflip = winHeight - y;
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (gameState == MENU && x >= (winWidth - 150) / 2 && x <= (winWidth + 150) / 2 && yflip >= winHeight - 300 && yflip <= winHeight - 260) {
            resetGame();
            gameState = PLAYING;
        }
        else if (gameState == GAME_OVER && x >= (winWidth - 150) / 2 && x <= (winWidth + 150) / 2 && yflip >= winHeight / 2 - 60 && yflip <= winHeight / 2 - 20) {
            resetGame();
            gameState = PLAYING;
        }
    }
}

void keyPress(int key, int x, int y) {
    if (gameState != PLAYING) return;
    if (key == GLUT_KEY_LEFT && currentLaneIndex > 0)
        vehicleX = lanes[--currentLaneIndex];
    if (key == GLUT_KEY_RIGHT && currentLaneIndex < 2)
        vehicleX = lanes[++currentLaneIndex];
}

void init() {
    glClearColor(0, 0, 0, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winWidth, 0, winHeight);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
    glutInitWindowSize(winWidth, winHeight);
    glutInitWindowPosition(200, 50);
    glutCreateWindow("Car Arcade Game");
    init();
    glutDisplayFunc(display);
    glutIdleFunc(display);
    glutReshapeFunc(reshape);
    glutSpecialFunc(keyPress);
    glutMouseFunc(mouseClick);
    glutMainLoop();
    return 0;
}
