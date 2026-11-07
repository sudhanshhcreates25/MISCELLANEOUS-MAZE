#include "GameEngine.h"

#include "raylib.h"

#include <vector>

namespace {

constexpr int CELL_SIZE = 48;
constexpr int MARGIN = 24;
constexpr int PANEL_WIDTH = 310;
constexpr int WINDOW_WIDTH = MARGIN * 3 + COLS * CELL_SIZE + PANEL_WIDTH;
constexpr int WINDOW_HEIGHT = MARGIN * 2 + ROWS * CELL_SIZE + 55;

const char* MAZE[ROWS] = {
    "###################",
    "#......#..........#",
    "#......#..........#",
    "#......#######....#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "###################"
};

constexpr Color BACKGROUND = {18, 24, 34, 255};
constexpr Color FLOOR = {44, 57, 68, 255};
constexpr Color WALL = {27, 34, 44, 255};
constexpr Color GRID = {71, 85, 96, 255};
constexpr Color ATTACKER_COLOR = {67, 190, 230, 255};
constexpr Color DEFENDER_COLOR = {234, 92, 85, 255};
constexpr Color VAULT_COLOR = {242, 190, 74, 255};
constexpr Color ESCAPE_COLOR = {118, 213, 137, 255};
constexpr Color TEXT_COLOR = {235, 240, 244, 255};
constexpr Color MUTED_TEXT = {160, 174, 186, 255};

GameState initialGameState() {
    GameState state{};
    state.attacker = {1, 1};
    state.defender = {6, 7};
    state.vault = {4, 12};
    state.escapePoint = {7, 16};
    state.mode = INFILTRATE;
    return state;
}

bool isSameCell(Position a, Position b) {
    return a.row == b.row && a.col == b.col;
}

bool tryAttackerMove(GameState& game, Position destination) {
    const std::vector<Position> legalMoves = getLegalMoves(MAZE, game.attacker);
    bool legal = false;
    for (Position move : legalMoves) {
        if (isSameCell(move, destination)) {
            legal = true;
            break;
        }
    }
    if (!legal) return false;

    game = applyMove(game, destination, false);
    if (game.gameOver) return true;

    Position defenderMove = game.defender;
    StateEvaluator evaluator;
    minimax(game, SEARCH_DEPTH, true, -WIN_SCORE - 1, WIN_SCORE + 1,
            MAZE, &defenderMove, evaluator);
    game = applyMove(game, defenderMove, true);
    return true;
}

void drawMaze(const GameState& game, int gridX, int gridY) {
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            const Rectangle cell = {
                static_cast<float>(gridX + col * CELL_SIZE),
                static_cast<float>(gridY + row * CELL_SIZE),
                static_cast<float>(CELL_SIZE),
                static_cast<float>(CELL_SIZE)
            };
            DrawRectangleRec(cell, MAZE[row][col] == '#' ? WALL : FLOOR);
            DrawRectangleLinesEx(cell, 1.0f, GRID);

            const Vector2 center = {
                cell.x + CELL_SIZE * 0.5f,
                cell.y + CELL_SIZE * 0.5f
            };

            if (isSameCell(game.vault, {row, col})) {
                DrawCircleV(center, CELL_SIZE * 0.29f, VAULT_COLOR);
                DrawText("V", static_cast<int>(center.x) - 8,
                         static_cast<int>(center.y) - 12, 24, WALL);
            } else if (isSameCell(game.escapePoint, {row, col})) {
                DrawCircleV(center, CELL_SIZE * 0.29f, ESCAPE_COLOR);
                DrawText("E", static_cast<int>(center.x) - 8,
                         static_cast<int>(center.y) - 12, 24, WALL);
            }

            if (isSameCell(game.attacker, {row, col})) {
                DrawCircleV(center, CELL_SIZE * 0.36f, ATTACKER_COLOR);
                DrawText("A", static_cast<int>(center.x) - 9,
                         static_cast<int>(center.y) - 13, 26, WALL);
            }
            if (isSameCell(game.defender, {row, col})) {
                DrawCircleV(center, CELL_SIZE * 0.36f, DEFENDER_COLOR);
                DrawText("D", static_cast<int>(center.x) - 10,
                         static_cast<int>(center.y) - 13, 26, WALL);
            }
        }
    }
}

void drawPanel(const GameState& game, int panelX) {
    const int textX = panelX + 18;
    DrawText("MISCELLANEOUS MAZE", textX, 42, 23, TEXT_COLOR);
    DrawText("2D GRAPHICAL VERSION", textX, 72, 15, MUTED_TEXT);
    DrawLine(panelX + 16, 105, panelX + PANEL_WIDTH - 8, 105, GRID);

    DrawText("OBJECTIVE", textX, 126, 15, MUTED_TEXT);
    const bool infiltrating = game.mode == INFILTRATE;
    DrawText(infiltrating ? "INFILTRATE" : "ESCAPE", textX, 150, 23,
             infiltrating ? VAULT_COLOR : ESCAPE_COLOR);
    DrawText(infiltrating ? "Reach the Vault (V)" : "Reach the Escape (E)",
             textX, 182, 16, TEXT_COLOR);

    DrawLine(panelX + 16, 220, panelX + PANEL_WIDTH - 8, 220, GRID);
    DrawText("PIECES", textX, 238, 15, MUTED_TEXT);
    DrawCircle(textX + 10, 277, 10, ATTACKER_COLOR);
    DrawText("Attacker", textX + 30, 267, 17, TEXT_COLOR);
    DrawCircle(textX + 10, 310, 10, DEFENDER_COLOR);
    DrawText("Defender AI", textX + 30, 300, 17, TEXT_COLOR);

    DrawLine(panelX + 16, 344, panelX + PANEL_WIDTH - 8, 344, GRID);
    DrawText("MOVE: WASD / ARROWS", textX, 366, 16, TEXT_COLOR);
    DrawText("QUIT: ESC", textX, 392, 16, TEXT_COLOR);

    if (game.gameOver) {
        const char* result = game.attackerWon ? "ATTACKER ESCAPED" : "DEFENDER CAPTURED";
        DrawText("GAME OVER", textX, 434, 18, VAULT_COLOR);
        DrawText(result, textX, 460, 16, TEXT_COLOR);
        DrawText("Close window to exit", textX, 488, 14, MUTED_TEXT);
    }
}

} // namespace

int main() {
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Miscellaneous Maze");
    SetTargetFPS(60);

    GameState game = initialGameState();
    const int gridX = MARGIN;
    const int gridY = MARGIN;
    const int panelX = gridX + COLS * CELL_SIZE + MARGIN;

    while (!WindowShouldClose()) {
        if (!game.gameOver) {
            Position destination = game.attacker;
            bool moveRequested = false;
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                --destination.row;
                moveRequested = true;
            } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
                ++destination.row;
                moveRequested = true;
            } else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
                --destination.col;
                moveRequested = true;
            } else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
                ++destination.col;
                moveRequested = true;
            }
            if (moveRequested) tryAttackerMove(game, destination);
        }

        BeginDrawing();
        ClearBackground(BACKGROUND);
        drawMaze(game, gridX, gridY);
        drawPanel(game, panelX);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
