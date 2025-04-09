#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <iostream>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Window dimensions (wider and shorter)
int winWidth = 700, winHeight = 500;
const int baseHeight = 500;  // Base height for speed scaling

// Extended game states to include player selection and registration.
enum GameState { MENU, PLAYER_SELECT, REGISTER, PLAYING, GAME_OVER };
GameState gameState = MENU;

GLuint carTexture;
int score = 0;
bool collide = false;
int lives = 3;

// For player selection
std::vector<std::string> players;      // list of player names
int currentPlayerIndex = 0;            // index of the currently selected player
const int MAX_PLAYERS = 5;

// Buffer for new player input (used in REGISTER state)
std::string newPlayerName = "";

// Game variables
// We'll update lane positions dynamically upon reshape.
int lanes[3] = {150, 250, 350};  // recalculated in reshape
int currentLaneIndex = 1;
int vehicleX = lanes[currentLaneIndex], vehicleY = 70;
int ovehicleX[4], ovehicleY[4];
bool obstaclePassed[4];  // track if obstacle has been passed
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

// Fonts used for text
void *font18 = GLUT_BITMAP_HELVETICA_18;
void *boldFont = GLUT_BITMAP_TIMES_ROMAN_24;

// Global mouse coordinates (for hover effects)
int mouseX = 0, mouseY = 0;

// NEW: Global speed multiplier to adjust game pace.
float speedMultiplier = 1.0f;

void mousePassiveMotion(int x, int y) {
    mouseX = x;
    mouseY = winHeight - y;
}

// Utility: returns the pixel width for a string (using GLUT bitmap fonts)
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

// Draws a scalable, bold title using the stroke font.
void drawBigCenteredTitle(const char *text, float y, float scale, float r, float g, float b) {
    void* font = GLUT_STROKE_MONO_ROMAN;
    float width = 0;
    for (int i = 0; text[i] != '\0'; i++)
        width += glutStrokeWidth(font, text[i]);
    float startX = (winWidth - width * scale) / 2.0f;
    glPushMatrix();
    for (int dx = -2; dx <= 2; dx++) {
        for (int dy = -2; dy <= 2; dy++) {
            glLoadIdentity();
            glTranslatef(startX + dx, y + dy, 0);
            glScalef(scale, scale, 1);
            glColor3f(r, g, b);
            for (int i = 0; text[i] != '\0'; i++)
                glutStrokeCharacter(font, text[i]);
        }
    }
    glPopMatrix();
}

// Fancy button drawing function.
void drawFancyButtonCentered(int y, int w, int h, const char* label) {
    int x = (winWidth - w) / 2;
    bool hovered = (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);
    // Draw shadow
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(x + 3, y - 3);
        glVertex2f(x + w + 3, y - 3);
        glVertex2f(x + w + 3, y + h + 3);
        glVertex2f(x + 3, y + h + 3);
    glEnd();
    
    const char* special1 = "START GAME";
    const char* special2 = "Register New Player";
    float baseR, baseG, baseB;
    if (strcmp(label, special1) == 0 || strcmp(label, special2) == 0) {
        baseR = hovered ? 0.8f : 0.6f;
        baseG = hovered ? 0.8f : 0.6f;
        baseB = 1.0f;
    } else {
        // Force white background for player name buttons.
        baseR = baseG = baseB = 1.0f;
    }
    
    glBegin(GL_QUADS);
        glColor3f(baseR, baseG, baseB);
        glVertex2f(x, y);
        glColor3f(baseR * 0.9f, baseG * 0.9f, baseB * 0.9f);
        glVertex2f(x + w, y);
        glColor3f(baseR * 0.9f, baseG * 0.9f, baseB * 0.9f);
        glVertex2f(x + w, y + h);
        glColor3f(baseR, baseG, baseB);
        glVertex2f(x, y + h);
    glEnd();
    
    // Draw border.
    glColor3f(0, 0, 0);
    glLineWidth(2);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();
    
    // Choose font.
    const void* fontToUse;
    if (strcmp(label, "START GAME") == 0)
        fontToUse = GLUT_BITMAP_HELVETICA_18;
    else
        fontToUse = font18;
    int textWidth = getTextWidth(label, (void*)fontToUse);
    drawText(label, x + (w - textWidth) / 2, y + h / 2 - 6, (void*)fontToUse, 0, 0, 0);
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

void drawDustbin(int bx, int by, int bw, int bh) {
    int dx = bx + bw + 5;
    int iconWidth = 25;
    int iconHeight = bh; 
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_POLYGON);
        glVertex2f(dx, by);
        glVertex2f(dx + iconWidth, by);
        glVertex2f(dx + iconWidth, by + iconHeight);
        glVertex2f(dx, by + iconHeight);
    glEnd();
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_POLYGON);
        glVertex2f(dx - 2, by + iconHeight);
        glVertex2f(dx + iconWidth + 2, by + iconHeight);
        glVertex2f(dx + iconWidth + 2, by + iconHeight + 5);
        glVertex2f(dx - 2, by + iconHeight + 5);
    glEnd();
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINES);
        glVertex2f(dx + iconWidth/2 - 5, by + iconHeight + 5);
        glVertex2f(dx + iconWidth/2 + 5, by + iconHeight + 5);
    glEnd();
}

void resetBushBlob(int i) {
    for (int b = 0; b < 5; b++) {
        bushBlobs[i].offsetX[b] = (rand() % 15) - 7;
        bushBlobs[i].offsetY[b] = (rand() % 15) - 7;
        bushBlobs[i].radius[b] = 10 + rand() % 6;
        bushBlobs[i].green[b] = 0.6f + 0.15f * (rand() % 4);
        if (bushBlobs[i].green[b] > 1.0f)
            bushBlobs[i].green[b] = 1.0f;
    }
}

void resetGame() {
    score = 0;
    collide = false;
    lives = 3;
    vehicleX = lanes[currentLaneIndex = 1];

    // Reset the speed multiplier for a fresh start.
    speedMultiplier = 1.0f;

    for (int i = 0; i < 4; i++) {
        ovehicleX[i] = lanes[rand() % 3];
        ovehicleY[i] = 1000 - i * 250;
        oType[i] = static_cast<ObstacleType>(rand() % 4);
        obstaclePassed[i] = false;
        if (oType[i] == OBSTACLE_BUSH)
            resetBushBlob(i);
    }
    movd = 0;
}

void drawHeart(float x, float y, float size, bool filled) {
    glColor3f(1.0f, 0.0f, 0.0f);
    if (!filled) {
        glLineWidth(2);
        glBegin(GL_LINE_LOOP);
    } else {
        glBegin(GL_POLYGON);
    }
    for (float angle = 0; angle <= 2 * 3.14159f; angle += 0.01f) {
        float px = size * 16 * pow(sin(angle), 3);
        float py = size * (13 * cos(angle) - 5 * cos(2 * angle) - 2 * cos(3 * angle) - cos(4 * angle));
        glVertex2f(x + px, y + py);
    }
    glEnd();
}

void drawGame() {
    // Compute a speed factor based on window height relative to our base height.
    float speedFactor = winHeight / static_cast<float>(baseHeight);
    
    // Set up the road parameters dynamically.
    const int roadWidth = 300;
    int roadLeft = (winWidth - roadWidth) / 2;
    int roadRight = roadLeft + roadWidth;
    
    // Draw the green background.
    glColor3f(0, 1, 0);
    glBegin(GL_QUADS);
        glVertex2f(0, 0);
        glVertex2f(0, winHeight);
        glVertex2f(winWidth, winHeight);
        glVertex2f(winWidth, 0);
    glEnd();
    
    // Draw the road (centered).
    glColor3f(0.1, 0.1, 0.1);
    glBegin(GL_QUADS);
        glVertex2f(roadLeft, 0);
        glVertex2f(roadLeft, winHeight);
        glVertex2f(roadRight, winHeight);
        glVertex2f(roadRight, 0);
    glEnd();
    
    // Draw lane markers.
    glColor3f(1, 1, 1);
    // Lane markers at: roadLeft + 100 and roadLeft + 200.
    for (int lane = 1; lane < 3; lane++) {
        int laneX = roadLeft + lane * 100;
        for (int i = 0; i < 20; i++) {
            glBegin(GL_QUADS);
                glVertex2f(laneX - 5, i * 40 + (movd % static_cast<int>(40 * speedFactor * speedMultiplier)));
                glVertex2f(laneX + 5, i * 40 + (movd % static_cast<int>(40 * speedFactor * speedMultiplier)));
                glVertex2f(laneX + 5, i * 40 + 20 + (movd % static_cast<int>(40 * speedFactor * speedMultiplier)));
                glVertex2f(laneX - 5, i * 40 + 20 + (movd % static_cast<int>(40 * speedFactor * speedMultiplier)));
            glEnd();
        }
    }
    // Update scrolling with speed scaling.
    movd -= static_cast<int>(5 * speedFactor * speedMultiplier);
    if (movd < -static_cast<int>(40 * speedFactor * speedMultiplier))
        movd = 0;
    
    // Draw the player's vehicle.
    glColor3f(0, 0, 1);
    glBegin(GL_QUADS);
        glVertex2f(vehicleX - 25, vehicleY - 20);
        glVertex2f(vehicleX + 25, vehicleY - 20);
        glVertex2f(vehicleX + 25, vehicleY + 20);
        glVertex2f(vehicleX - 25, vehicleY + 20);
    glEnd();
    
    // Draw obstacles.
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
                    glVertex2f(x + 20, y);
                    glVertex2f(x + 5, y + 15);
                    glVertex2f(x - 10, y + 10);
                glEnd();
                break;
        }
        // Collision detection.
        if (!collide && ovehicleX[i] == vehicleX &&
            ovehicleY[i] > vehicleY - 40 && ovehicleY[i] < vehicleY + 40) {
            lives--;
            if (lives <= 0) {
                collide = true;
                gameState = GAME_OVER;
            } else {
                vehicleX = lanes[currentLaneIndex = 1];
            }
        }
        // Move obstacles with speed scaling.
        ovehicleY[i] -= static_cast<int>(3 * speedFactor * speedMultiplier);
        if (!collide && !obstaclePassed[i] && ovehicleY[i] + 25 < vehicleY - 20) {
            score++;
            obstaclePassed[i] = true;
        }
        // Reset obstacles when they leave the screen.
        if (ovehicleY[i] < -static_cast<int>(50 * speedFactor)) {
            int newX = lanes[rand() % 3];
            int newY = winHeight;
            bool valid = true;
            for (int j = 0; j < 4; j++) {
                if (j != i && ovehicleX[j] != newX && abs(ovehicleY[j] - newY) < 150)
                {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                ovehicleX[i] = newX;
                ovehicleY[i] = newY;
                oType[i] = static_cast<ObstacleType>(rand() % 4);
                obstaclePassed[i] = false;
                if (oType[i] == OBSTACLE_BUSH)
                    resetBushBlob(i);
            }
        }
    }
    
    sprintf(buffer, "%05d", score);
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(10, winHeight - 40); 
        glVertex2f(150, winHeight - 40);
        glVertex2f(150, winHeight - 10); 
        glVertex2f(10, winHeight - 10);
    glEnd();
    drawText("SCORE:", 15, winHeight - 30, boldFont, 1, 0, 0);
    drawText(buffer, 100, winHeight - 30, boldFont, 1, 0, 0);
    for (int i = 0; i < 3; i++) {
        float heartX = 30 + i * 50;
        float heartY = winHeight - 80;
        drawHeart(heartX, heartY, 1.5f, i < lives);
    }
    
    // Gradually increase the speed multiplier to harden the game over time.
    speedMultiplier += 0.0005f;
}

bool isInside(int x, int y, int bx, int by, int bw, int bh) {
    return (x >= bx && x <= bx + bw && y >= by && y <= by + bh);
}

// Updated player selection interface.
void drawPlayerSelection() {
    drawCenteredText("SELECT PLAYER", winHeight - 60, boldFont, 1, 1, 1);
    
    int buttonWidth = 200;
    int buttonHeight = 40;
    int gap = 20;
    
    int numItems = players.size();
    bool canRegister = (players.size() < MAX_PLAYERS);
    if (canRegister)
        numItems++;
    
    int headerHeight = 60;
    int bottomMargin = 20;
    int availableArea = winHeight - headerHeight - bottomMargin;
    int totalHeight = numItems * buttonHeight + (numItems - 1) * gap;
    int availableTop = bottomMargin + availableArea;
    int startY = availableTop - (availableArea - totalHeight) / 2;
    
    // Draw each player button with a dustbin for removal.
    int bx = (winWidth - buttonWidth) / 2;
    for (int i = 0; i < players.size(); i++) {
        int by = startY - i * (buttonHeight + gap);
        drawFancyButtonCentered(by, buttonWidth, buttonHeight, players[i].c_str());
        drawDustbin(bx, by, buttonWidth, buttonHeight);
    }
    int by = startY - players.size() * (buttonHeight + gap);
    if (canRegister) {
        drawFancyButtonCentered(by, buttonWidth, buttonHeight, "Register New Player");
    } else {
        drawCenteredText("Remove a player to register", by + buttonHeight/2 - 6, boldFont, 1, 0, 0);
    }
}

// Updated registration screen.
void drawRegistration() {
    drawCenteredText("REGISTER NEW PLAYER", winHeight - 60, boldFont, 1, 1, 1);
    
    int headerHeight = 60;
    int bottomMargin = 20;
    int availableArea = winHeight - headerHeight - bottomMargin;
    int numLines = 3;
    int lineHeight = 40;
    int gap = 20;
    int totalHeight = numLines * lineHeight + (numLines - 1) * gap;
    int availableTop = bottomMargin + availableArea;
    int startY = availableTop - (availableArea - totalHeight) / 2;
    
    drawCenteredText("Enter player name:", startY, boldFont, 1, 1, 1);
    drawCenteredText(newPlayerName.c_str(), startY - (lineHeight + gap), boldFont, 1, 1, 0);
    drawCenteredText("Press Enter to submit", startY - 2*(lineHeight + gap), boldFont, 1, 1, 1);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (gameState == MENU) {
        const char* titleText = "CAR ARCADE GAME";
        void* strokeFont = GLUT_STROKE_MONO_ROMAN;
        float textWidth = 0;
        for (int i = 0; titleText[i] != '\0'; i++) {
            textWidth += glutStrokeWidth(strokeFont, titleText[i]);
        }
        float titleScale = (winWidth * 0.9f) / textWidth;
        if (titleScale > 0.4f)
            titleScale = 0.4f;
        int titleY = static_cast<int>(winHeight * 0.65);
        int subtitleY = static_cast<int>(winHeight * 0.58);
        int buttonY = static_cast<int>(winHeight * 0.35);
        const int buttonWidth = 150;
        const int buttonHeight = 50;
        drawBigCenteredTitle(titleText, titleY, titleScale, 1, 0, 0);
        drawCenteredText("BY Kashish & Ananya", subtitleY, font18, 1, 1, 1);
        drawFancyButtonCentered(buttonY, buttonWidth, buttonHeight, "START GAME");
    }
    else if (gameState == PLAYER_SELECT) {
        drawPlayerSelection();
    }
    else if (gameState == REGISTER) {
        drawRegistration();
    }
    else if (gameState == PLAYING) {
        drawGame();
    }
    else if (gameState == GAME_OVER) {
        drawCenteredText("GAME OVER", winHeight / 2 + 40, boldFont, 1, 0, 0);
        char scoreStr[32];
        sprintf(scoreStr, "SCORE:  %05d", score);
        drawCenteredText(scoreStr, winHeight / 2 - 10, boldFont, 1, 1, 1);
        drawFancyButtonCentered(winHeight / 2 - 60, 150, 40, "RESTART");
        drawFancyButtonCentered(winHeight / 2 - 110, 150, 40, "CHANGE PLAYER");
    }
    glutSwapBuffers();
}

void mouseClick(int button, int state, int x, int y) {
    int yflip = winHeight - y;
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (gameState == MENU) {
            if (x >= (winWidth - 150) / 2 && x <= (winWidth + 150) / 2 &&
                yflip >= winHeight / 2 - 60 && yflip <= winHeight / 2 - 20) {
                gameState = PLAYER_SELECT;
            }
        }
        else if (gameState == PLAYER_SELECT) {
            int buttonWidth = 200;
            int buttonHeight = 40;
            int gap = 20;
            int headerHeight = 60, bottomMargin = 20;
            int availableArea = winHeight - headerHeight - bottomMargin;
            int numItems = players.size();
            bool canRegister = (players.size() < MAX_PLAYERS);
            if (canRegister)
                numItems++;
            int totalHeight = numItems * buttonHeight + (numItems - 1) * gap;
            int availableTop = bottomMargin + availableArea;
            int startY = availableTop - (availableArea - totalHeight) / 2;
            int bx = (winWidth - buttonWidth) / 2;
            // Check dustbin clicks for removal.
            for (int i = 0; i < players.size(); i++) {
                int by = startY - i * (buttonHeight + gap);
                int dustbinX = bx + buttonWidth + 5;
                if (isInside(x, yflip, dustbinX, by, 25, buttonHeight)) {
                    std::cout << "Removed player: " << players[i] << std::endl;
                    players.erase(players.begin() + i);
                    if (currentPlayerIndex >= players.size())
                        currentPlayerIndex = 0;
                    return;
                }
            }
            // Check existing player buttons.
            for (int i = 0; i < players.size(); i++) {
                int by = startY - i * (buttonHeight + gap);
                if (isInside(x, yflip, bx, by, buttonWidth, buttonHeight)) {
                    currentPlayerIndex = i;
                    std::cout << "Selected player: " << players[i] << std::endl;
                    gameState = PLAYING;
                    return;
                }
            }
            // Check for "Register New Player" if available.
            if (canRegister) {
                int bxRegister = (winWidth - 200) / 2;
                int by = startY - players.size() * (buttonHeight + gap);
                if (isInside(x, yflip, bxRegister, by, 200, buttonHeight)) {
                    newPlayerName = "";
                    gameState = REGISTER;
                    return;
                }
            }
        }
        else if (gameState == GAME_OVER) {
            int buttonWidth = 150;
            int buttonHeight = 40;
            int restartButtonX = (winWidth - buttonWidth) / 2;
            int restartButtonY = winHeight / 2 - 60;
            int changeButtonX = restartButtonX;
            int changeButtonY = winHeight / 2 - 110;
            if (isInside(x, yflip, restartButtonX, restartButtonY, buttonWidth, buttonHeight)) {
                resetGame();
                gameState = PLAYING;
                return;
            }
            if (isInside(x, yflip, changeButtonX, changeButtonY, buttonWidth, buttonHeight)) {
                gameState = PLAYER_SELECT;
                return;
            }
        }
    }
}

void keyboard(unsigned char key, int x, int y) {
    if (gameState == REGISTER) {
        if (key == 13) { // Enter key
            if (!newPlayerName.empty()) {
                players.push_back(newPlayerName);
                std::cout << "Registered new player: " << newPlayerName << std::endl;
            }
            gameState = PLAYER_SELECT;
        }
        else if (key == 8) { // Backspace key
            if (!newPlayerName.empty())
                newPlayerName.pop_back();
        }
        else if (key >= 32 && key <= 126 && newPlayerName.size() < 20) {
            newPlayerName.push_back(key);
        }
    }
}

void keyPress(int key, int x, int y) {
    if (gameState != PLAYING)
        return;
    if (key == GLUT_KEY_LEFT && currentLaneIndex > 0)
        vehicleX = lanes[--currentLaneIndex];
    if (key == GLUT_KEY_RIGHT && currentLaneIndex < 2)
        vehicleX = lanes[++currentLaneIndex];
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

    // Recalculate lane positions based on the centered road.
    const int roadWidth = 300;
    int roadLeft = (w - roadWidth) / 2;
    lanes[0] = roadLeft + 50;
    lanes[1] = roadLeft + 150;
    lanes[2] = roadLeft + 250;
    
    // Update vehicle position to align with new lane centers.
    vehicleX = lanes[currentLaneIndex];
    
    // Update obstacles to move them to the nearest lane.
    for (int i = 0; i < 4; i++) {
        float diff0 = fabs(ovehicleX[i] - lanes[0]);
        float diff1 = fabs(ovehicleX[i] - lanes[1]);
        float diff2 = fabs(ovehicleX[i] - lanes[2]);
        if(diff1 < diff0 && diff1 < diff2) {
            ovehicleX[i] = lanes[1];
        } else if(diff2 < diff0 && diff2 < diff1) {
            ovehicleX[i] = lanes[2];
        } else {
            ovehicleX[i] = lanes[0];
        }
    }
}

void init() {
    glClearColor(0, 0, 0, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winWidth, 0, winHeight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv) {
    // Initialize default players.
    players.push_back("kashish");
    players.push_back("dananya");

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
    glutKeyboardFunc(keyboard);
    glutPassiveMotionFunc(mousePassiveMotion);
    glutMainLoop();
    return 0;
}
