// Simple Tetris in C++ with Raylib
// Controls: Left/Right move, Up rotates, Down soft-drops, Space hard-drops
//           R restarts, P pauses, ESC quits

#include "include/raylib.h"
#include <array>
#include <cstdlib>
#include <ctime>

// --- Constants ---
constexpr int CELL      = 30;          // pixels per block
constexpr int COLS      = 10;          // board width in cells
constexpr int ROWS      = 20;          // board height in cells
constexpr int PANEL_W   = 6 * CELL;   // right info panel width
constexpr int SCR_W     = COLS * CELL + PANEL_W;
constexpr int SCR_H     = ROWS * CELL;
constexpr float DROP    = 0.6f;        // seconds between auto-drops
constexpr float SOFT    = 0.04f;       // seconds per row on soft drop
constexpr float LOCK    = 0.5f;        // lock delay after piece lands
constexpr float DAS_INI = 0.167f;      // auto-repeat initial delay
constexpr float DAS_REP = 0.033f;      // auto-repeat rate (~30/s)

// === Tetromino shapes: 7 types x 4 rotations x 4 rows x 4 cols ===
// Standard Super Rotation System (SRS)
const int SHAPES[7][4][4][4] = {
    // I (cyan)
    {{{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
     {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,1,1},{0,0,0,0}},
     {{0,1,0,0},{0,1,0,0},{0,1,0,0},{0,1,0,0}}},
    // O (yellow)
    {{{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,1,0},{0,0,0,0}}},
    // T (purple)
    {{{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,1,0,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,1,0},{0,1,0,0}},
     {{0,0,0,0},{0,1,0,0},{1,1,0,0},{0,1,0,0}}},
    // S (green)
    {{{0,0,0,0},{0,1,1,0},{1,1,0,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0}},
     {{0,0,0,0},{0,0,0,0},{0,1,1,0},{1,1,0,0}},
     {{0,0,0,0},{1,0,0,0},{1,1,0,0},{0,1,0,0}}},
    // Z (red)
    {{{0,0,0,0},{1,1,0,0},{0,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,0,1,0},{0,1,1,0},{0,1,0,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,0,0},{0,1,1,0}},
     {{0,0,0,0},{0,1,0,0},{1,1,0,0},{1,0,0,0}}},
    // J (blue)
    {{{0,0,0,0},{1,0,0,0},{1,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,1,0},{0,1,0,0},{0,1,0,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,1,0},{0,0,1,0}},
     {{0,0,0,0},{0,1,0,0},{0,1,0,0},{1,1,0,0}}},
    // L (orange)
    {{{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}},
     {{0,0,0,0},{0,1,0,0},{0,1,0,0},{0,1,1,0}},
     {{0,0,0,0},{0,0,0,0},{1,1,1,0},{1,0,0,0}},
     {{0,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,0,0}}}
};

// Piece colors: index 0 = empty background, 1-7 = piece type + 1
const Color COLORS[8] = {
    DARKGRAY,   // 0
    SKYBLUE,    // 1: I
    YELLOW,     // 2: O
    PURPLE,     // 3: T
    GREEN,      // 4: S
    RED,        // 5: Z
    BLUE,       // 6: J
    ORANGE      // 7: L
};

// --- Game state ---
std::array<std::array<int, COLS>, ROWS> board{}; // 0=empty, 1-7=locked piece color
int  curType, curRot, curX, curY;  // current falling piece
int  nextType;                      // next piece (shown in panel)
int  score, linesCleared;           // player stats
float dropTimer, lockTimer;         // timers
bool gameOver, paused;

// --- Random piece 0..6 ---
int randPiece() { return rand() % 7; }

// --- Collision check: would the current piece collide at (curX+dx, curY+dy)? ---
bool hits(int dx, int dy) {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (SHAPES[curType][curRot][r][c]) {
                int bx = curX + c + dx, by = curY + r + dy;
                if (bx < 0 || bx >= COLS || by >= ROWS) return true;
                if (by < 0) continue;          // allow above-board spawn
                if (board[by][bx]) return true; // hit locked block
            }
    return false;
}

// --- Lock the current piece onto the board ---
void lockPiece() {
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (SHAPES[curType][curRot][r][c]) {
                int bx = curX + c, by = curY + r;
                if (by >= 0 && by < ROWS)
                    board[by][bx] = curType + 1;
            }
}

// --- Clear completed rows and update score ---
void clearLines() {
    int n = 0;
    for (int r = ROWS - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < COLS; c++)
            if (!board[r][c]) { full = false; break; }
        if (!full) continue;
        n++;
        // shift everything above down by one
        for (int rr = r; rr > 0; rr--)
            for (int c = 0; c < COLS; c++)
                board[rr][c] = board[rr - 1][c];
        for (int c = 0; c < COLS; c++) board[0][c] = 0;
        r++; // re-check this row (it was shifted down)
    }
    if (n == 1) score += 100;
    else if (n == 2) score += 300;
    else if (n == 3) score += 500;
    else if (n == 4) score += 800;       // Tetris!
    linesCleared += n;
}

// --- Spawn a new piece at the top center ---
void spawn() {
    curType = nextType;
    nextType = randPiece();
    curRot  = 0;
    curX    = COLS / 2 - 2;   // center 4-wide piece on 10-wide board
    curY    = -1;              // start just above visible area
    dropTimer = 0;
    lockTimer = 0;
    if (hits(0, 0)) gameOver = true;
}

// --- Rotate clockwise with basic wall kicks ---
void rotateCW() {
    int old = curRot;
    curRot = (curRot + 1) % 4;
    if (hits(0, 0)) {
        curX++;                     // try shift right
        if (hits(0, 0)) {
            curX -= 2;              // try shift left
            if (hits(0, 0)) {
                curX++;             // restore X
                curRot = old;       // revert rotation
            }
        }
    }
}

// --- Try moving piece by (dx, dy); returns success ---
bool move(int dx, int dy) {
    if (!hits(dx, dy)) { curX += dx; curY += dy; return true; }
    return false;
}

// --- Hard drop: piece falls instantly, then lock ---
void hardDrop() {
    int dist = 0;
    while (!hits(0, dist + 1)) dist++;
    curY += dist;
    score += dist * 2;          // 2 pts bonus per row dropped
    lockPiece();
    clearLines();
    spawn();
}

// --- Reset the game ---
void reset() {
    for (auto &r : board) r.fill(0);
    score = linesCleared = 0;
    gameOver = paused = false;
    srand(time(nullptr));
    nextType = randPiece();
    spawn();
}

// --- Draw a single cell at (px, py) ---
void drawCell(int px, int py, Color c) {
    DrawRectangle(px + 1, py + 1, CELL - 2, CELL - 2, c);
    Color hi = Fade(WHITE, 0.25f);
    Color lo = Fade(BLACK, 0.25f);
    DrawLine(px + 1, py + 1, px + CELL - 3, py + 1, hi);
    DrawLine(px + 1, py + 1, px + 1, py + CELL - 3, hi);
    DrawLine(px + 1, py + CELL - 3, px + CELL - 3, py + CELL - 3, lo);
    DrawLine(px + CELL - 3, py + 1, px + CELL - 3, py + CELL - 3, lo);
}

// --- Draw the board: grid, locked blocks, border ---
void drawBoard() {
    // dark background
    DrawRectangle(0, 0, COLS * CELL, ROWS * CELL, Color{18, 18, 18, 255});
    // grid lines
    auto gridColor = Color{40, 40, 40, 255};
    for (int r = 0; r <= ROWS; r++)
        DrawLine(0, r * CELL, COLS * CELL, r * CELL, gridColor);
    for (int c = 0; c <= COLS; c++)
        DrawLine(c * CELL, 0, c * CELL, ROWS * CELL, gridColor);
    // locked cells
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (board[r][c])
                drawCell(c * CELL, r * CELL, COLORS[board[r][c]]);
    // border
    DrawRectangleLines(0, 0, COLS * CELL, ROWS * CELL, GRAY);
}

// --- Draw the current piece ---
void drawPiece() {
    Color pieceColor = COLORS[curType + 1];
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (SHAPES[curType][curRot][r][c]) {
                int by = curY + r;
                if (by >= 0 && by < ROWS)
                    drawCell(curX * CELL + c * CELL, by * CELL, pieceColor);
            }
}

// --- Draw the right-side info panel ---
void drawPanel() {
    int x = COLS * CELL + 15, y = 20;

    DrawText("TETRIS", x, y, 28, WHITE);  y += 50;

    // next piece preview
    DrawText("NEXT", x, y, 16, GRAY);  y += 22;
    DrawRectangleLines(x, y, 4 * 16, 4 * 16, DARKGRAY);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            if (SHAPES[nextType][0][r][c])
                DrawRectangle(x + c * 16, y + r * 16, 16, 16, COLORS[nextType + 1]);
    y += 4 * 16 + 32;

    DrawText("SCORE", x, y, 16, GRAY);  y += 22;
    DrawText(TextFormat("%d", score), x, y, 22, WHITE);  y += 45;

    DrawText("LINES", x, y, 16, GRAY);  y += 22;
    DrawText(TextFormat("%d", linesCleared), x, y, 22, WHITE);  y += 45;

    DrawText("KEYS", x, y, 14, GRAY);  y += 20;
    DrawText("Move:  Left/Right", x, y, 14, GRAY);  y += 16;
    DrawText("Rotate: Up", x, y, 14, GRAY);           y += 16;
    DrawText("Soft:  Down", x, y, 14, GRAY);          y += 16;
    DrawText("Hard:  Space", x, y, 14, GRAY);         y += 16;
    DrawText("R: Restart", x, y, 14, GRAY);           y += 16;
    DrawText("P: Pause", x, y, 14, GRAY);             y += 16;
    DrawText("ESC: Quit", x, y, 14, GRAY);
}

// --- Draw overlay text (game over / pause) ---
void drawOverlay(const char *title, const char *sub, Color tc) {
    int h = 90;
    DrawRectangle(0, SCR_H / 2 - h / 2, SCR_W, h, Fade(BLACK, 0.75f));
    int tw = MeasureText(title, 36);
    DrawText(title, SCR_W / 2 - tw / 2, SCR_H / 2 - 30, 36, tc);
    tw = MeasureText(sub, 18);
    DrawText(sub, SCR_W / 2 - tw / 2, SCR_H / 2 + 6, 18, WHITE);
}

// --- Per-frame update ---
void update() {
    if (gameOver || paused) return;

    float dt = GetFrameTime();

    // === Horizontal movement: instant on press, auto-repeat on hold (DAS) ===
    static int   moveDir   = 0;
    static float moveTimer = 0;
    static bool  dasStarted = false;

    // immediate move on first keypress
    if (IsKeyPressed(KEY_LEFT))  { move(-1, 0); }
    if (IsKeyPressed(KEY_RIGHT)) { move(1, 0);  }

    int dir = 0;
    if (IsKeyDown(KEY_LEFT))  dir = -1;
    if (IsKeyDown(KEY_RIGHT)) dir = 1;

    if (dir != moveDir) { moveDir = dir; moveTimer = 0; dasStarted = false; }

    if (dir != 0) {
        moveTimer += dt;
        float delay = dasStarted ? DAS_REP : DAS_INI;
        while (moveTimer >= delay) {
            moveTimer -= delay;
            move(dir, 0);
            dasStarted = true;
        }
    }

    // === Rotation (single press only) ===
    if (IsKeyPressed(KEY_UP)) rotateCW();

    // === Hard drop ===
    if (IsKeyPressed(KEY_SPACE)) hardDrop();

    // === Gravity + soft-drop + lock delay ===
    float speed = IsKeyDown(KEY_DOWN) ? SOFT : DROP;
    dropTimer += dt;

    while (dropTimer >= speed) {
        dropTimer -= speed;
        if (move(0, 1)) {
            if (IsKeyDown(KEY_DOWN)) score++;  // soft-drop bonus
            lockTimer = 0;
        } else {
            lockTimer += speed;
            if (lockTimer >= LOCK) {
                lockPiece();
                clearLines();
                spawn();
                break;
            }
        }
    }
}

// === Entry point ===
int main() {
    InitWindow(SCR_W, SCR_H, "Tetris");
    SetTargetFPS(60);
    reset();

    while (!WindowShouldClose()) {
        // global keys (work even when paused)
        if (IsKeyPressed(KEY_R)) reset();
        if (IsKeyPressed(KEY_P) && !gameOver) paused = !paused;

        if (!paused) update();

        BeginDrawing();
        ClearBackground(BLACK);
        drawBoard();
        if (!gameOver) drawPiece();
        drawPanel();
        if (gameOver)      drawOverlay("GAME OVER", "Press R to restart", RED);
        else if (paused)   drawOverlay("PAUSED", "Press P to resume", YELLOW);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
