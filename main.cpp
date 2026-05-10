// Tetris — C++17, Raylib, Object-Oriented
// Controls: Left/Right move, Up rotates, Down soft-drops, Space hard-drops
//           R restarts, P pauses, ESC quits

#include "include/raylib.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <random>

class Tetris {
public:
    // --- Public interface (called from main) ---
    void reset();
    void update();
    void draw() const;

    bool isPaused() const { return paused_; }

    static constexpr int screenW() { return SCR_W; }
    static constexpr int screenH() { return SCR_H; }

private:
    // --- Layout constants ---
    static constexpr int CELL    = 30;
    static constexpr int COLS    = 10;
    static constexpr int ROWS    = 20;
    static constexpr int PANEL_W = 6 * CELL;
    static constexpr int SCR_W   = COLS * CELL + PANEL_W;
    static constexpr int SCR_H   = ROWS * CELL;

    // --- Timing constants ---
    static constexpr float DROP    = 0.6f;
    static constexpr float SOFT    = 0.04f;
    static constexpr float LOCK    = 0.5f;
    static constexpr float DAS_INI = 0.167f;
    static constexpr float DAS_REP = 0.033f;

    // --- Tetromino shapes: 7 types × 4 rotations × 4 rows × 4 cols (SRS) ---
    static inline constexpr int SHAPES[7][4][4][4] = {
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

    // --- Piece colors: index 0 = empty background, 1-7 = piece type + 1 ---
    static inline const Color COLORS[8] = {
        DARKGRAY, SKYBLUE, YELLOW, PURPLE, GREEN, RED, BLUE, ORANGE
    };

    // --- Board ---
    using Row = std::array<int, COLS>;
    using Grid = std::array<Row, ROWS>;
    Grid board_{};

    // --- Active piece ---
    int curType_{}, curRot_{}, curX_{}, curY_{};
    int nextType_{};

    // --- Stats & state ---
    int   score_{}, linesCleared_{};
    float dropTimer_{}, lockTimer_{};
    bool  gameOver_{false}, paused_{false};

    // --- DAS (Delayed Auto Shift) ---
    int   moveDir_{};
    float moveTimer_{};
    bool  dasStarted_{false};

    // --- Random number generation ---
    std::mt19937                     rng_{};
    std::uniform_int_distribution<int> pieceDist_{0, 6};

    // === Private methods =============================================

    int randPiece() { return pieceDist_(rng_); }

    // --- Collision check ---
    bool hits(int dx, int dy) const {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (SHAPES[curType_][curRot_][r][c]) {
                    int bx = curX_ + c + dx, by = curY_ + r + dy;
                    if (bx < 0 || bx >= COLS || by >= ROWS) return true;
                    if (by < 0) continue;
                    if (board_[by][bx]) return true;
                }
        return false;
    }

    // --- Lock piece onto board ---
    void lockPiece() {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (SHAPES[curType_][curRot_][r][c]) {
                    int bx = curX_ + c, by = curY_ + r;
                    if (by >= 0 && by < ROWS)
                        board_[by][bx] = curType_ + 1;
                }
    }

    // --- Clear completed rows ---
    void clearLines() {
        int n = 0;
        for (int r = ROWS - 1; r >= 0; r--) {
            bool full = true;
            for (int c = 0; c < COLS; c++)
                if (!board_[r][c]) { full = false; break; }
            if (!full) continue;
            n++;
            for (int rr = r; rr > 0; rr--)
                for (int c = 0; c < COLS; c++)
                    board_[rr][c] = board_[rr - 1][c];
            for (int c = 0; c < COLS; c++) board_[0][c] = 0;
            r++;
        }
        if      (n == 1) score_ += 100;
        else if (n == 2) score_ += 300;
        else if (n == 3) score_ += 500;
        else if (n == 4) score_ += 800;
        linesCleared_ += n;
    }

    // --- Spawn next piece ---
    void spawn() {
        curType_  = nextType_;
        nextType_ = randPiece();
        curRot_   = 0;
        curX_     = COLS / 2 - 2;
        curY_     = -1;
        dropTimer_ = 0;
        lockTimer_ = 0;
        if (hits(0, 0)) gameOver_ = true;
    }

    // --- Rotate clockwise with wall kicks ---
    void rotateCW() {
        int old = curRot_;
        curRot_ = (curRot_ + 1) % 4;
        if (hits(0, 0)) {
            curX_++;
            if (hits(0, 0)) {
                curX_ -= 2;
                if (hits(0, 0)) {
                    curX_++;
                    curRot_ = old;
                }
            }
        }
    }

    // --- Try moving piece by (dx, dy) ---
    bool move(int dx, int dy) {
        if (!hits(dx, dy)) { curX_ += dx; curY_ += dy; return true; }
        return false;
    }

    // --- Hard drop ---
    void hardDrop() {
        int dist = 0;
        while (!hits(0, dist + 1)) dist++;
        curY_ += dist;
        score_ += dist * 2;
        lockPiece();
        clearLines();
        spawn();
    }

    // --- Drawing helpers ---
    static void drawCell(int px, int py, Color c) {
        DrawRectangle(px + 1, py + 1, CELL - 2, CELL - 2, c);
        Color hi = Fade(WHITE, 0.25f);
        Color lo = Fade(BLACK, 0.25f);
        DrawLine(px + 1, py + 1, px + CELL - 3, py + 1, hi);
        DrawLine(px + 1, py + 1, px + 1, py + CELL - 3, hi);
        DrawLine(px + 1, py + CELL - 3, px + CELL - 3, py + CELL - 3, lo);
        DrawLine(px + CELL - 3, py + 1, px + CELL - 3, py + CELL - 3, lo);
    }

    void drawBoard() const {
        DrawRectangle(0, 0, COLS * CELL, ROWS * CELL, Color{18, 18, 18, 255});
        Color grid = Color{40, 40, 40, 255};
        for (int r = 0; r <= ROWS; r++)
            DrawLine(0, r * CELL, COLS * CELL, r * CELL, grid);
        for (int c = 0; c <= COLS; c++)
            DrawLine(c * CELL, 0, c * CELL, ROWS * CELL, grid);
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if (board_[r][c])
                    drawCell(c * CELL, r * CELL, COLORS[board_[r][c]]);
        DrawRectangleLines(0, 0, COLS * CELL, ROWS * CELL, GRAY);
    }

    void drawPiece() const {
        Color c = COLORS[curType_ + 1];
        for (int r = 0; r < 4; r++)
            for (int cc = 0; cc < 4; cc++)
                if (SHAPES[curType_][curRot_][r][cc]) {
                    int by = curY_ + r;
                    if (by >= 0 && by < ROWS)
                        drawCell(curX_ * CELL + cc * CELL, by * CELL, c);
                }
    }

    void drawPanel() const {
        int x = COLS * CELL + 15, y = 20;

        DrawText("TETRIS", x, y, 28, WHITE);  y += 50;

        DrawText("NEXT", x, y, 16, GRAY);  y += 22;
        DrawRectangleLines(x, y, 4 * 16, 4 * 16, DARKGRAY);
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (SHAPES[nextType_][0][r][c])
                    DrawRectangle(x + c * 16, y + r * 16, 16, 16, COLORS[nextType_ + 1]);
        y += 4 * 16 + 32;

        DrawText("SCORE", x, y, 16, GRAY);  y += 22;
        DrawText(TextFormat("%d", score_), x, y, 22, WHITE);  y += 45;

        DrawText("LINES", x, y, 16, GRAY);  y += 22;
        DrawText(TextFormat("%d", linesCleared_), x, y, 22, WHITE);  y += 45;

        DrawText("KEYS", x, y, 14, GRAY);  y += 20;
        DrawText("Move:  Left/Right", x, y, 14, GRAY);  y += 16;
        DrawText("Rotate: Up",    x, y, 14, GRAY);  y += 16;
        DrawText("Soft:  Down",   x, y, 14, GRAY);  y += 16;
        DrawText("Hard:  Space",  x, y, 14, GRAY);  y += 16;
        DrawText("R: Restart",    x, y, 14, GRAY);  y += 16;
        DrawText("P: Pause",      x, y, 14, GRAY);  y += 16;
        DrawText("ESC: Quit",     x, y, 14, GRAY);
    }

    void drawOverlay(const char* title, const char* sub, Color tc) const {
        int h = 90;
        DrawRectangle(0, SCR_H / 2 - h / 2, SCR_W, h, Fade(BLACK, 0.75f));
        int tw = MeasureText(title, 36);
        DrawText(title, SCR_W / 2 - tw / 2, SCR_H / 2 - 30, 36, tc);
        tw = MeasureText(sub, 18);
        DrawText(sub, SCR_W / 2 - tw / 2, SCR_H / 2 + 6, 18, WHITE);
    }
};

// === Public method implementations ====================================

void Tetris::reset() {
    for (auto& r : board_) r.fill(0);
    score_ = linesCleared_ = 0;
    gameOver_ = paused_ = false;
    rng_.seed(std::random_device{}());
    nextType_ = randPiece();
    spawn();
}

void Tetris::update() {
    // Global inputs (work even when paused / game over)
    if (IsKeyPressed(KEY_R)) reset();
    if (IsKeyPressed(KEY_P) && !gameOver_) paused_ = !paused_;

    if (gameOver_ || paused_) return;

    float dt = GetFrameTime();

    // --- Horizontal movement with DAS ---
    if (IsKeyPressed(KEY_LEFT))  move(-1, 0);
    if (IsKeyPressed(KEY_RIGHT)) move(1, 0);

    int dir = 0;
    if (IsKeyDown(KEY_LEFT))  dir = -1;
    if (IsKeyDown(KEY_RIGHT)) dir = 1;

    if (dir != moveDir_) { moveDir_ = dir; moveTimer_ = 0; dasStarted_ = false; }

    if (dir != 0) {
        moveTimer_ += dt;
        float delay = dasStarted_ ? DAS_REP : DAS_INI;
        while (moveTimer_ >= delay) {
            moveTimer_ -= delay;
            move(dir, 0);
            dasStarted_ = true;
        }
    }

    // --- Rotation ---
    if (IsKeyPressed(KEY_UP)) rotateCW();

    // --- Hard drop ---
    if (IsKeyPressed(KEY_SPACE)) hardDrop();

    // --- Gravity + soft-drop + lock delay ---
    float speed = IsKeyDown(KEY_DOWN) ? SOFT : DROP;
    dropTimer_ += dt;

    while (dropTimer_ >= speed) {
        dropTimer_ -= speed;
        if (move(0, 1)) {
            if (IsKeyDown(KEY_DOWN)) score_++;
            lockTimer_ = 0;
        } else {
            lockTimer_ += speed;
            if (lockTimer_ >= LOCK) {
                lockPiece();
                clearLines();
                spawn();
                break;
            }
        }
    }
}

void Tetris::draw() const {
    drawBoard();
    if (!gameOver_) drawPiece();
    drawPanel();
    if (gameOver_)       drawOverlay("GAME OVER", "Press R to restart", RED);
    else if (paused_)    drawOverlay("PAUSED", "Press P to resume", YELLOW);
}

// === Entry point ======================================================

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Tetris::screenW(), Tetris::screenH(), "Tetris");
    SetWindowMinSize(Tetris::screenW() / 2, Tetris::screenH() / 2);
    SetTargetFPS(60);

    Tetris game;
    game.reset();

    RenderTexture2D rt = LoadRenderTexture(Tetris::screenW(), Tetris::screenH());
    SetTextureFilter(rt.texture, TEXTURE_FILTER_POINT);

    while (!WindowShouldClose()) {
        game.update();

        BeginTextureMode(rt);
        ClearBackground(BLACK);
        game.draw();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        float scale = std::min((float)GetScreenWidth()  / Tetris::screenW(),
                               (float)GetScreenHeight() / Tetris::screenH());
        float dw = Tetris::screenW() * scale;
        float dh = Tetris::screenH() * scale;
        Rectangle src = {0, 0, (float)rt.texture.width, -(float)rt.texture.height};
        Rectangle dst = {(GetScreenWidth() - dw) * 0.5f,
                         (GetScreenHeight() - dh) * 0.5f, dw, dh};
        DrawTexturePro(rt.texture, src, dst, {0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    UnloadRenderTexture(rt);
    CloseWindow();
    return 0;
}
