#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS    5
#define TFT_DC    21
#define TFT_RST   4
#define BTN_1     27
#define BTN_2     13

#define C_BLACK    0x0000
#define C_WHITE    0xFFFF
#define C_SKY      0x4D19
#define C_PIPE     0x07E0
#define C_PIPE_D   0x03E0
#define C_PIPE_CAP 0x2FE6
#define C_BG_DUNK  0x10A2
#define C_HOOP     0xF800
#define C_HOOP_D   0xA000
#define C_NET      0xBDF7
#define C_BALL     0xFA00
#define C_BALL_D   0xC180
#define C_WING     0xFFFF
#define C_GRAY     0x8410
#define C_GRASS    0x2580
#define C_GRASS_D  0x1C00
#define C_BIRD_Y   0xFFE0
#define C_BEAK     0xF800
#define C_BOARD    0x39E7

#define FLAPPY_GRAVITY    0.32
#define FLAPPY_JUMP      -3.0
#define FLAPPY_MAX_FALL   4.0
#define DUNK_GRAVITY      0.28
#define DUNK_JUMP        -3.5
#define DUNK_MAX_FALL     4.5
#define DUNK_AIR_DRAG     0.995
#define BALL_RADIUS       5
#define BALL_BOUNCE       0.55
#define RIM_BOUNCE_VX     1.2
#define FRAME_DELAY 25

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

enum GameState { MENU, FLAPPY, DUNK, GAMEOVER };
GameState currentState = MENU;
int selectedGame = 0;

float birdY = 64.0;
float birdVelocity = 0.0;
float birdX = 20.0;
float ballVX = 0;
float ballSpin = 0;

bool b1Press = false;
bool b1Short = false;
bool b1Long = false;
bool b2Press = false;
unsigned long btn1PressTime = 0;
bool btn1IsDown = false;
bool btn1WasDown = false;
bool btn1LongFired = false;
bool btn2WasDown = false;

int gameSpeed = 2;
int score = 0;
bool scored[2];
int obstacleX[2];
int obstacleY[2];
int prevBirdX, prevBirdY;
int prevObsX[2], prevObsY[2];
bool firstFrame = true;

unsigned long lastFrameTime = 0;
volatile bool flapPending = false;

void drawScore(int s, uint16_t bg) {
  tft.fillRect(60, 4, 40, 14, bg);
  tft.setTextColor(C_WHITE, bg);
  tft.setTextSize(1);
  tft.setCursor(65, 6);
  tft.print("S:");
  tft.print(s);
}

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);

  pinMode(BTN_1, INPUT_PULLDOWN);
  pinMode(BTN_2, INPUT_PULLDOWN);

  drawMenu();
}

void handleButtons() {
  bool b1Now = digitalRead(BTN_1);
  bool b2Now = digitalRead(BTN_2);

  // Reset all one-shot flags each call
  b1Press = false;
  b1Short = false;
  b1Long = false;
  b2Press = false;

  // --- BUTTON 1 ---
  if (b1Now && !btn1WasDown) {
    // JUST PRESSED - instant detection
    b1Press = true;
    btn1PressTime = millis();
    btn1IsDown = true;
    btn1LongFired = false;
  }

  if (!b1Now && btn1WasDown) {
    // JUST RELEASED
    if (!btn1LongFired && (millis() - btn1PressTime < 400)) {
      b1Short = true;
    }
    btn1IsDown = false;
  }

  if (b1Now && btn1IsDown && !btn1LongFired && (millis() - btn1PressTime >= 400)) {
    b1Long = true;
    btn1LongFired = true;
  }

  btn1WasDown = b1Now;

  // --- BUTTON 2 - instant press ---
  if (b2Now && !btn2WasDown) {
    b2Press = true;
  }
  btn2WasDown = b2Now;
}

void eraseRect(int x, int y, int w, int h, uint16_t bg) {
  int x1 = max(0, x);
  int y1 = max(0, y);
  int x2 = min(159, x + w - 1);
  int y2 = min(127, y + h - 1);
  if (x1 > x2 || y1 > y2) return;
  tft.fillRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1, bg);
}

void loop() {
  handleButtons();

  switch (currentState) {
    case MENU:    runMenuLoop();   break;
    case FLAPPY:  runFlappyLoop(); break;
    case DUNK:    runDunkLoop();   break;
    case GAMEOVER: break;
  }
}
  // Navigate with short press or instant press
  if (b1Short) {
    selectedGame = (selectedGame + 1) % 2;
    drawMenuSelection();
  }
  if (b2Press) {
    selectedGame = (selectedGame + 1) % 2;
    drawMenuSelection();
  }

  // Start game with long press
  if (b1Long) {
    resetGame();
    if (selectedGame == 0) {
      tft.fillScreen(C_SKY);
      drawGround();
      currentState = FLAPPY;
    } else {
      tft.fillScreen(C_BG_DUNK);
      drawCourt();
      currentState = DUNK;
    }
    delay(300);
  }
}

void drawMenu() {
  tft.fillScreen(C_BLACK);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(14, 8);
  tft.print("SELECT GAME");

  tft.drawFastHLine(10, 28, 140, C_GRAY);

  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(8, 100);
  tft.print("Btn1: Navigate/Select");
  tft.setCursor(8, 112);
  tft.print("Hold Btn1: Start Game");

  drawMenuSelection();
}

void drawMenuSelection() {
  tft.fillRect(8, 36, 70, 50, C_BLACK);
  tft.fillRect(82, 36, 70, 50, C_BLACK);

  uint16_t border1 = (selectedGame == 0) ? C_WHITE : C_GRAY;
  tft.drawRoundRect(8, 36, 70, 50, 4, border1);
  if (selectedGame == 0) tft.drawRoundRect(9, 37, 68, 48, 3, border1);
  tft.fillRoundRect(11, 39, 64, 44, 2, C_SKY);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(22, 50);
  tft.print("FLAPPY");
  tft.setCursor(24, 62);
  tft.print("BIRD");
  tft.fillRect(38, 42, 6, 4, C_BIRD_Y);

  uint16_t border2 = (selectedGame == 1) ? C_WHITE : C_GRAY;
  tft.drawRoundRect(82, 36, 70, 50, 4, border2);
  if (selectedGame == 1) tft.drawRoundRect(83, 37, 68, 48, 3, border2);
  tft.fillRoundRect(85, 39, 64, 44, 2, C_BG_DUNK);
  tft.setTextColor(C_WHITE);
  tft.setCursor(96, 50);
  tft.print("FLAPPY");
  tft.setCursor(98, 62);
  tft.print("HOOP");
  tft.fillCircle(112, 44, 3, C_BALL);
}

// ==========================================
// DRAWING HELPERS
// ==========================================

void drawGround() {
  tft.fillRect(0, 120, 160, 8, C_GRASS);
  tft.drawFastHLine(0, 120, 160, C_GRASS_D);
  for (int x = 0; x < 160; x += 8) {
    tft.drawPixel(x, 119, C_GRASS);
    tft.drawPixel(x + 3, 118, C_GRASS);
  }
}

void drawCourt() {
  tft.fillRect(0, 122, 160, 6, C_BOARD);
  tft.drawFastHLine(0, 122, 160, C_GRAY);
}

void eraseRect(int x, int y, int w, int h, uint16_t bg) {
  int x1 = max(0, x);
  int y1 = max(0, y);
  int x2 = min(159, x + w - 1);
  int y2 = min(127, y + h - 1);
  if (x1 > x2 || y1 > y2) return;
  tft.fillRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1, bg);
}

void drawBird(int x, int y) {
  tft.fillRoundRect(x, y, 12, 10, 2, C_BIRD_Y);
  tft.fillRect(x + 8, y + 2, 2, 2, C_WHITE);
  tft.drawPixel(x + 9, y + 2, C_BLACK);
  tft.fillRect(x + 11, y + 4, 4, 3, C_BEAK);
  tft.fillRect(x + 11, y + 6, 4, 1, 0xC000);
  if (birdVelocity < 0) {
    tft.fillTriangle(x + 2, y + 3, x + 7, y + 3, x + 4, y - 2, C_WHITE);
  } else {
    tft.fillRect(x + 2, y + 4, 6, 3, C_WHITE);
  }
  tft.fillRect(x - 1, y + 2, 2, 3, 0xFCC0);
}

void eraseBird(int x, int y, uint16_t bg) {
  eraseRect(x - 2, y - 3, 18, 16, bg);
}

void drawWingedBall(int x, int y) {
  int cx = x + BALL_RADIUS;
  int cy = y + BALL_RADIUS;

  if (birdVelocity < -0.5) {
    tft.fillTriangle(x - 4, cy - 1, x, cy + 2, x - 2, cy - 6, C_WING);
    tft.fillTriangle(x + BALL_RADIUS * 2 + 4, cy - 1, x + BALL_RADIUS * 2, cy + 2, x + BALL_RADIUS * 2 + 2, cy - 6, C_WING);
  } else if (birdVelocity > 1.0) {
    tft.fillTriangle(x - 4, cy, x, cy + 1, x - 2, cy + 5, C_WING);
    tft.fillTriangle(x + BALL_RADIUS * 2 + 4, cy, x + BALL_RADIUS * 2, cy + 1, x + BALL_RADIUS * 2 + 2, cy + 5, C_WING);
  } else {
    tft.fillTriangle(x - 5, cy, x, cy + 2, x - 5, cy - 3, C_WING);
    tft.fillTriangle(x + BALL_RADIUS * 2 + 5, cy, x + BALL_RADIUS * 2, cy + 2, x + BALL_RADIUS * 2 + 5, cy - 3, C_WING);
  }

  tft.fillCircle(cx, cy, BALL_RADIUS, C_BALL);
  tft.drawCircle(cx, cy, BALL_RADIUS, C_BALL_D);
  tft.drawFastHLine(cx - 4, cy, 9, C_BALL_D);
  tft.drawFastVLine(cx, cy - 4, 9, C_BALL_D);
  tft.drawPixel(cx - 3, cy - 2, C_BALL_D);
  tft.drawPixel(cx - 3, cy + 2, C_BALL_D);
  tft.drawPixel(cx + 3, cy - 2, C_BALL_D);
  tft.drawPixel(cx + 3, cy + 2, C_BALL_D);
}

void eraseWingedBall(int x, int y, uint16_t bg) {
  eraseRect(x - 7, y - 7, BALL_RADIUS * 2 + 14, BALL_RADIUS * 2 + 14, bg);
}

void drawHoop(int x, int centerY) {
  int hoopGap = 30;
  int rimY_top = centerY - (hoopGap / 2);
  int rimY_bot = centerY + (hoopGap / 2);
  int rimThick = 3;
  int hoopWidth = 20;

  tft.fillRect(x + hoopWidth - 2, rimY_top - 10, 4, hoopGap + 20, C_BOARD);
  tft.drawRect(x + hoopWidth - 2, rimY_top - 10, 4, hoopGap + 20, C_GRAY);

  tft.fillRect(x, rimY_top - rimThick, hoopWidth, rimThick, C_HOOP);
  tft.drawRect(x, rimY_top - rimThick, hoopWidth, rimThick, C_HOOP_D);
  tft.fillCircle(x + 1, rimY_top - rimThick / 2, 2, C_HOOP);

  tft.fillRect(x, rimY_bot, hoopWidth, rimThick, C_HOOP);
  tft.drawRect(x, rimY_bot, hoopWidth, rimThick, C_HOOP_D);
  tft.fillCircle(x + 1, rimY_bot + rimThick / 2, 2, C_HOOP);

  for (int i = 0; i < hoopWidth; i += 4) {
    tft.drawLine(x + i, rimY_top, x + i + 2, rimY_bot, C_NET);
    tft.drawLine(x + i + 2, rimY_top, x + i, rimY_bot, C_NET);
  }
  int netMid = (rimY_top + rimY_bot) / 2;
  tft.drawFastHLine(x + 2, netMid, hoopWidth - 4, C_NET);
  tft.drawFastHLine(x + 1, netMid - 5, hoopWidth - 2, C_NET);
  tft.drawFastHLine(x + 1, netMid + 5, hoopWidth - 2, C_NET);
}

void eraseHoop(int x, int centerY, uint16_t bg) {
  int hoopGap = 30;
  int rimY_top = centerY - (hoopGap / 2);
  eraseRect(x - 2, rimY_top - 12, 28, hoopGap + 24, bg);
}

void drawPipe(int x, int gapCenter) {
  int gapSize = 38;
  int topH = gapCenter - (gapSize / 2);
  int botY = gapCenter + (gapSize / 2);
  int pipeW = 22;
  int capW = 26;
  int capH = 6;

  if (x < -capW || x > 160) return;

  if (topH > 0) {
    tft.fillRect(x + 2, 0, pipeW - 4, topH - capH, C_PIPE);
    tft.drawRect(x + 2, 0, pipeW - 4, topH - capH, C_PIPE_D);
    tft.drawFastVLine(x + 4, 0, topH - capH, C_PIPE_CAP);
  }
  tft.fillRect(x, topH - capH, capW, capH, C_PIPE);
  tft.drawRect(x, topH - capH, capW, capH, C_PIPE_D);
  tft.drawFastVLine(x + 2, topH - capH, capH, C_PIPE_CAP);

  int botBodyStart = botY + capH;
  if (botBodyStart < 120) {
    tft.fillRect(x + 2, botBodyStart, pipeW - 4, 120 - botBodyStart, C_PIPE);
    tft.drawRect(x + 2, botBodyStart, pipeW - 4, 120 - botBodyStart, C_PIPE_D);
    tft.drawFastVLine(x + 4, botBodyStart, 120 - botBodyStart, C_PIPE_CAP);
  }
  tft.fillRect(x, botY, capW, capH, C_PIPE);
  tft.drawRect(x, botY, capW, capH, C_PIPE_D);
  tft.drawFastVLine(x + 2, botY, capH, C_PIPE_CAP);
}

void drawScore(int s, uint16_t bg) {
  tft.fillRect(60, 4, 40, 14, bg);
  tft.setTextColor(C_WHITE, bg);
  tft.setTextSize(1);
  tft.setCursor(65, 6);
  tft.print("S:");
  tft.print(s);
}

// ==========================================
// GAME 1: FLAPPY BIRD
// ==========================================

// Latched flap flag - set by button handler, consumed by game loop
volatile bool flapPending = false;

void runFlappyLoop() {
  // Check for flap BEFORE frame gate - latch it so it's never missed
  if (b1Press) {
    flapPending = true;
  }

  // Frame timing
  unsigned long now = millis();
  if (now - lastFrameTime < FRAME_DELAY) return;
  lastFrameTime = now;

  // Pause
  if (b2Press) {
    tft.fillRoundRect(40, 50, 80, 28, 4, C_BLACK);
    tft.drawRoundRect(40, 50, 80, 28, 4, C_WHITE);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(48, 54);
    tft.print("PAUSE");
    delay(300);
    while (true) {
      handleButtons();
      if (b2Press) break;
      delay(10);
    }
    tft.fillScreen(C_SKY);
    drawGround();
    firstFrame = true;
    flapPending = false;
    return;
  }

  // Consume latched flap
  if (flapPending) {
    birdVelocity = FLAPPY_JUMP;
    flapPending = false;
  }

  // Physics
  birdVelocity += FLAPPY_GRAVITY;
  if (birdVelocity > FLAPPY_MAX_FALL) birdVelocity = FLAPPY_MAX_FALL;

  birdY += birdVelocity;
  int newBirdY = (int)birdY;

  // Erase old frame
  if (!firstFrame) {
    eraseBird((int)birdX, prevBirdY, C_SKY);
    for (int i = 0; i < 2; i++) {
      if (prevObsX[i] != obstacleX[i]) {
        int eraseX = obstacleX[i] + 26;
        if (eraseX >= 0 && eraseX < 160) {
          tft.drawFastVLine(eraseX, 0, 120, C_SKY);
          tft.drawFastVLine(eraseX + 1, 0, 120, C_SKY);
        }
        int oldRight = prevObsX[i] + 26;
        if (oldRight >= 0 && oldRight < 160) {
          tft.drawFastVLine(oldRight, 0, 120, C_SKY);
          tft.drawFastVLine(oldRight + 1, 0, 120, C_SKY);
        }
      }
    }
  }

  prevBirdY = newBirdY;
  for (int i = 0; i < 2; i++) {
    prevObsX[i] = obstacleX[i];
    prevObsY[i] = obstacleY[i];
  }

  // Update obstacles
  for (int i = 0; i < 2; i++) {
    obstacleX[i] -= gameSpeed;
    if (obstacleX[i] < -30) {
      obstacleX[i] = 160 + random(0, 20);
      obstacleY[i] = random(30, 90);
      scored[i] = false;
    }
    if (!scored[i] && obstacleX[i] + 22 < (int)birdX) {
      score++;
      scored[i] = true;
    }
  }

  // Draw obstacles
  for (int i = 0; i < 2; i++) {
    if (obstacleX[i] > -30 && obstacleX[i] < 160) {
      drawPipe(obstacleX[i], obstacleY[i]);
    }
  }

  // Draw bird
  drawBird((int)birdX, newBirdY);

  // Collision
  if (birdY > 108 || birdY < -5) {
    flapPending = false;
    gameOver();
    return;
  }

  int bx1 = (int)birdX;
  int by1 = newBirdY;
  int bx2 = bx1 + 12;
  int by2 = by1 + 10;

  for (int i = 0; i < 2; i++) {
    int gapSize = 38;
    int topH = obstacleY[i] - (gapSize / 2);
    int botY = obstacleY[i] + (gapSize / 2);
    int px1 = obstacleX[i];
    int px2 = obstacleX[i] + 22;

    if (bx2 > px1 && bx1 < px2) {
      if (by1 < topH || by2 > botY) {
        flapPending = false;
        gameOver();
        return;
      }
    }
  }

  drawGround();
  drawScore(score, C_SKY);
  firstFrame = false;
}

// ==========================================
// GAME 2: FLAPPY HOOP
// ==========================================

int checkHoopCollision(float bx, float by, int hoopX, int hoopCenterY) {
  int hoopGap = 30;
  int rimThick = 3;
  int hoopWidth = 20;

  float bcx = bx + BALL_RADIUS;
  float bcy = by + BALL_RADIUS;

  int rimTopY1 = hoopCenterY - (hoopGap / 2) - rimThick;
  int rimTopY2 = hoopCenterY - (hoopGap / 2);
  int rimBotY1 = hoopCenterY + (hoopGap / 2);
  int rimBotY2 = hoopCenterY + (hoopGap / 2) + rimThick;

  int rimX1 = hoopX;
  int rimX2 = hoopX + hoopWidth;

  if (bcx + BALL_RADIUS > rimX1 && bcx - BALL_RADIUS < rimX2) {
    if (bcy - BALL_RADIUS < rimTopY2 && bcy + BALL_RADIUS > rimTopY1) {
      return 1;
    }
    if (bcy - BALL_RADIUS < rimBotY2 && bcy + BALL_RADIUS > rimBotY1) {
      return 2;
    }
    if (bcy > rimTopY2 && bcy < rimBotY1) {
      return 3;
    }
    if (bcy - BALL_RADIUS < rimTopY1) return 0;
    if (bcy + BALL_RADIUS > rimBotY2) return 0;
  }

  int boardX = hoopX + hoopWidth - 2;
  if (bcx + BALL_RADIUS > boardX && bcx - BALL_RADIUS < boardX + 4) {
    int boardTop = hoopCenterY - (hoopGap / 2) - 10;
    int boardBot = hoopCenterY + (hoopGap / 2) + 10;
    if (bcy > boardTop && bcy < boardBot) {
      return 4;
    }
  }

  return 0;
}

void handleRimBounce(int collisionType, int hoopX, int hoopCenterY) {
  int hoopGap = 30;
  int rimThick = 3;

  float bcx = birdX + BALL_RADIUS;

  switch (collisionType) {
    case 1: {
      int rimBottom = hoopCenterY - (hoopGap / 2);
      if (birdVelocity > 0) {
        birdVelocity = -birdVelocity * BALL_BOUNCE;
        birdY = rimBottom - BALL_RADIUS * 2 - 2;
      } else {
        birdVelocity = abs(birdVelocity) * BALL_BOUNCE;
        birdY = rimBottom + 1;
      }
      if (bcx < hoopX + 10) ballVX -= RIM_BOUNCE_VX;
      else ballVX += RIM_BOUNCE_VX;
      break;
    }
    case 2: {
      int rimTop = hoopCenterY + (hoopGap / 2);
      if (birdVelocity > 0) {
        birdVelocity = -birdVelocity * BALL_BOUNCE;
        birdY = rimTop - BALL_RADIUS * 2 - 1;
      } else {
        birdVelocity = abs(birdVelocity) * BALL_BOUNCE;
        birdY = rimTop + rimThick + 1;
      }
      if (bcx < hoopX + 10) ballVX -= RIM_BOUNCE_VX;
      else ballVX += RIM_BOUNCE_VX;
      break;
    }
    case 4: {
      ballVX = -abs(ballVX) - 1.0;
      birdX = hoopX + 14;
      birdVelocity *= 0.8;
      break;
    }
  }
}

void runDunkLoop() {
  // Latch flap BEFORE frame gate
  if (b1Press) {
    flapPending = true;
  }

  // Frame timing
  unsigned long now = millis();
  if (now - lastFrameTime < FRAME_DELAY) return;
  lastFrameTime = now;

  // Pause
  if (b2Press) {
    tft.fillRoundRect(40, 50, 80, 28, 4, C_BLACK);
    tft.drawRoundRect(40, 50, 80, 28, 4, C_WHITE);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(48, 54);
    tft.print("PAUSE");
    delay(300);
    while (true) {
      handleButtons();
      if (b2Press) break;
      delay(10);
    }
    tft.fillScreen(C_BG_DUNK);
    drawCourt();
    firstFrame = true;
    flapPending = false;
    return;
  }

  // Consume latched flap
  if (flapPending) {
    birdVelocity = DUNK_JUMP;
    ballVX += 0.3;
    flapPending = false;
  }

  // Physics
  birdVelocity += DUNK_GRAVITY;
  if (birdVelocity > DUNK_MAX_FALL) birdVelocity = DUNK_MAX_FALL;
  ballVX *= DUNK_AIR_DRAG;
  if (abs(ballVX) < 0.05) ballVX = 0;
  if (ballVX > 3.0) ballVX = 3.0;
  if (ballVX < -3.0) ballVX = -3.0;

  birdY += birdVelocity;
  birdX += ballVX;

  float targetX = 20.0;
  birdX += (targetX - birdX) * 0.02;
  if (birdX < 2) { birdX = 2; ballVX = abs(ballVX) * 0.3; }
  if (birdX > 50) { birdX = 50; ballVX = -abs(ballVX) * 0.3; }

  int newBirdY = (int)birdY;
  int newBirdX = (int)birdX;

  // Floor bounce
  if (birdY > 108) {
    birdY = 108;
    if (abs(birdVelocity) > 1.5) {
      birdVelocity = -birdVelocity * 0.4;
    } else {
      flapPending = false;
      gameOver();
      return;
    }
  }
  if (birdY < -15) {
    birdY = -15;
    birdVelocity = abs(birdVelocity) * 0.3;
  }

  // Erase old frame
  if (!firstFrame) {
    eraseWingedBall(prevBirdX, prevBirdY, C_BG_DUNK);
    for (int i = 0; i < 2; i++) {
      if (prevObsX[i] != obstacleX[i]) {
        int eraseStart = obstacleX[i] + 24;
        for (int dx = 0; dx < gameSpeed + 2; dx++) {
          int ex = eraseStart + dx;
          if (ex >= 0 && ex < 160) {
            tft.drawFastVLine(ex, 0, 122, C_BG_DUNK);
          }
        }
      }
    }
  }

  prevBirdY = newBirdY;
  prevBirdX = newBirdX;
  for (int i = 0; i < 2; i++) {
    prevObsX[i] = obstacleX[i];
    prevObsY[i] = obstacleY[i];
  }

  // Update obstacles
  for (int i = 0; i < 2; i++) {
    obstacleX[i] -= gameSpeed;
    if (obstacleX[i] < -30) {
      obstacleX[i] = 160 + random(20, 60);
      obstacleY[i] = random(35, 95);
      scored[i] = false;
    }
  }

  // Hoop collision
  for (int i = 0; i < 2; i++) {
    if (obstacleX[i] > -30 && obstacleX[i] < 160) {
      int col = checkHoopCollision(birdX, birdY, obstacleX[i], obstacleY[i]);
      if (col == 1 || col == 2 || col == 4) {
        handleRimBounce(col, obstacleX[i], obstacleY[i]);
      }
      if (col == 3 && !scored[i]) {
        score++;
        scored[i] = true;
        tft.drawCircle((int)birdX + BALL_RADIUS, (int)birdY + BALL_RADIUS, BALL_RADIUS + 3, C_WHITE);
      }
    }
  }

  // Draw hoops
  for (int i = 0; i < 2; i++) {
    if (obstacleX[i] > -28 && obstacleX[i] < 160) {
      drawHoop(obstacleX[i], obstacleY[i]);
    }
  }

  // Draw ball
  drawWingedBall(newBirdX, newBirdY);

  drawCourt();
  drawScore(score, C_BG_DUNK);
  firstFrame = false;
}

// ==========================================
// GAME RESET & GAME OVER
// ==========================================
void resetGame() {
  birdY = 64;
  birdVelocity = 0;
  birdX = 20;
  ballVX = 0;
  ballSpin = 0;
  score = 0;
  firstFrame = true;
  flapPending = false;

  obstacleX[0] = 160;
  obstacleY[0] = 64;
  scored[0] = false;

  obstacleX[1] = 250;
  obstacleY[1] = 70;
  scored[1] = false;

  prevBirdX = 20;
  prevBirdY = 64;
  prevObsX[0] = 160;
  prevObsX[1] = 250;
  prevObsY[0] = 64;
  prevObsY[1] = 70;

  lastFrameTime = millis();
}

void gameOver() {
  currentState = GAMEOVER;
  flapPending = false;

  delay(200);
  tft.fillScreen(C_BLACK);

  tft.drawRect(0, 0, 160, 128, C_HOOP);
  tft.drawRect(1, 1, 158, 126, C_HOOP);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(22, 25);
  tft.print("GAME OVER");

  tft.drawFastHLine(20, 45, 120, C_GRAY);

  tft.setTextSize(2);
  tft.setCursor(40, 55);
  tft.setTextColor(C_BALL);
  tft.print(score);

  tft.setTextSize(1);
  tft.setTextColor(C_GRAY);
  tft.setCursor(68, 60);
  tft.print("pts");

  tft.setTextColor(C_WHITE);
  tft.setCursor(18, 95);
  tft.print("Hold Btn1 for Menu");

  delay(800);

  while (true) {
    handleButtons();
    if (b1Long) {
      currentState = MENU;
      drawMenu();
      delay(300);
      return;
    }
    delay(10);
  }
}