
#include <iostream>
#include <vector>
#include <queue>
using namespace std;

const int ROWS = 9;
const int COLS = 19;

const int SEARCH_DEPTH = 4;
const int WIN_SCORE = 1000000;

// ==========================================
// 1. POSITION STRUCTURE
// ==========================================

struct Position {
    int row;
    int col;
};

// ==========================================
// 2. ATTACKER OBJECTIVE
// ==========================================

enum AttackerMode {
    INFILTRATE,
    ESCAPE
};

// ==========================================
// 3. GAME STATE
// ==========================================

struct GameState {

    Position attacker;
    Position defender;

    Position vault;
    Position escapePoint;

    AttackerMode mode;

    bool vaultReached;
    bool gameOver;
    bool attackerWon;
};

// ==========================================
// 4. DISTANCE CALCULATION
// ==========================================

int distanceBetween(Position a, Position b) {

    int rowDistance = a.row - b.row;
    int colDistance = a.col - b.col;

    if (rowDistance < 0)
        rowDistance = -rowDistance;

    if (colDistance < 0)
        colDistance = -colDistance;

    return rowDistance + colDistance;
}

// ==========================================
// 5. CHECK VALID MOVEMENT
// ==========================================

bool isWalkable(
    const char* maze[],
    int row,
    int col
) {

    if (row < 0 || row >= ROWS ||
        col < 0 || col >= COLS) {

        return false;
    }

    return maze[row][col] != '#';
}

// ==========================================
// 6. GENERATE LEGAL MOVES
// ==========================================

vector<Position> getLegalMoves(
    const char* maze[],
    Position current
) {

    vector<Position> moves;

    int dr[] = {-1, 1, 0, 0};
    int dc[] = {0, 0, -1, 1};

    for (int i = 0; i < 4; i++) {

        int nr = current.row + dr[i];
        int nc = current.col + dc[i];

        if (!isWalkable(maze, nr, nc)) {
            continue;
        }

        Position next;
        next.row = nr;
        next.col = nc;

        moves.push_back(next);
    }

    return moves;
}

// ==========================================
// 7. APPLY SIMULATED MOVE
// ==========================================

GameState applyMove(
    GameState state,
    Position next,
    bool defenderTurn
) {

    if (defenderTurn) {

        state.defender = next;

    } else {

        state.attacker = next;

        // Check capture
        if (state.attacker.row == state.defender.row &&
            state.attacker.col == state.defender.col) {

            state.gameOver = true;
            state.attackerWon = false;
        }

        // Check vault
        if (!state.gameOver &&
            !state.vaultReached &&
            state.attacker.row == state.vault.row &&
            state.attacker.col == state.vault.col) {

            state.vaultReached = true;
            state.mode = ESCAPE;
        }

        // Check escape
        if (!state.gameOver &&
            state.mode == ESCAPE &&
            state.attacker.row == state.escapePoint.row &&
            state.attacker.col == state.escapePoint.col) {

            state.gameOver = true;
            state.attackerWon = true;
        }
    }

    // Capture check after either player's move
    if (state.attacker.row == state.defender.row &&
        state.attacker.col == state.defender.col) {

        state.gameOver = true;
        state.attackerWon = false;
    }

    return state;
}

// ==========================================
// 8. EVALUATION FUNCTION
// Higher score = better for Defender
// ==========================================

int evaluateState(GameState state) {

    if (state.gameOver && !state.attackerWon) {
        return WIN_SCORE;
    }

    if (state.gameOver && state.attackerWon) {
        return -WIN_SCORE;
    }

    Position target;

    if (state.mode == INFILTRATE) {
        target = state.vault;
    } else {
        target = state.escapePoint;
    }

    int attackerToTarget =
        distanceBetween(state.attacker, target);

    int defenderToAttacker =
        distanceBetween(state.defender, state.attacker);

    int score =
        (3 * attackerToTarget) -
        (2 * defenderToAttacker);

    return score;
}

// ==========================================
// 9. MINIMAX + ALPHA-BETA PRUNING
// ==========================================

int minimax(
    GameState state,
    int depth,
    bool defenderTurn,
    int alpha,
    int beta,
    const char* maze[],
    Position* bestMove = nullptr
) {

    // Terminal state or depth limit
    if (depth == 0 || state.gameOver) {
        return evaluateState(state);
    }

    // ======================================
    // DEFENDER = MAX PLAYER
    // ======================================

    if (defenderTurn) {

        int bestScore = -WIN_SCORE - 1;

        vector<Position> moves =
            getLegalMoves(maze, state.defender);

        // Default to staying in current position
        if (bestMove != nullptr) {
            *bestMove = state.defender;
        }

        if (moves.empty()) {
            return evaluateState(state);
        }

        for (Position move : moves) {

            GameState nextState =
                applyMove(state, move, true);

            int score = minimax(
                nextState,
                depth - 1,
                false,
                alpha,
                beta,
                maze
            );

            if (score > bestScore) {

                bestScore = score;

                if (bestMove != nullptr) {
                    *bestMove = move;
                }
            }

            if (bestScore > alpha) {
                alpha = bestScore;
            }

            // Alpha-Beta pruning
            if (alpha >= beta) {
                break;
            }
        }

        return bestScore;
    }

    // ======================================
    // ATTACKER = MIN PLAYER
    // ======================================

    else {

        int bestScore = WIN_SCORE + 1;

        vector<Position> moves =
            getLegalMoves(maze, state.attacker);

        if (moves.empty()) {
            return evaluateState(state);
        }

        for (Position move : moves) {

            GameState nextState =
                applyMove(state, move, false);

            int score = minimax(
                nextState,
                depth - 1,
                true,
                alpha,
                beta,
                maze
            );

            if (score < bestScore) {
                bestScore = score;
            }

            if (bestScore < beta) {
                beta = bestScore;
            }

            // Alpha-Beta pruning
            if (alpha >= beta) {
                break;
            }
        }

        return bestScore;
    }
}

// ==========================================
// 10. BFS BASELINE
// Retained for future comparison
// ==========================================

Position findNextStep(
    const char* maze[],
    Position start,
    Position target
) {

    Position invalid;
    invalid.row = -1;
    invalid.col = -1;

    bool visited[ROWS][COLS] = {};

    Position firstStep[ROWS][COLS];

    queue<Position> q;

    q.push(start);

    visited[start.row][start.col] = true;

    while (!q.empty()) {

        Position current = q.front();
        q.pop();

        if (current.row == target.row &&
            current.col == target.col) {

            if (current.row == start.row &&
                current.col == start.col) {
                return invalid;
            }

            return firstStep[current.row][current.col];
        }

        int dr[] = {-1, 1, 0, 0};
        int dc[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; i++) {

            int nr = current.row + dr[i];
            int nc = current.col + dc[i];

            if (!isWalkable(maze, nr, nc)) {
                continue;
            }

            if (visited[nr][nc]) {
                continue;
            }

            visited[nr][nc] = true;

            if (current.row == start.row &&
                current.col == start.col) {

                firstStep[nr][nc].row = nr;
                firstStep[nr][nc].col = nc;

            } else {

                firstStep[nr][nc] =
                    firstStep[current.row][current.col];
            }

            Position next;
            next.row = nr;
            next.col = nc;

            q.push(next);
        }
    }

    return invalid;
}

// ==========================================
// 11. CAPTURE DETECTION
// ==========================================

bool isCaptured(const GameState& game) {

    return game.attacker.row == game.defender.row &&
           game.attacker.col == game.defender.col;
}

// ==========================================
// 12. DISPLAY MAZE
// ==========================================

void displayMaze(
    const char* maze[],
    const GameState& game
) {

    cout << "\n";

    for (int i = 0; i < ROWS; i++) {

        for (int j = 0; j < COLS; j++) {

            if (i == game.attacker.row &&
                j == game.attacker.col) {

                cout << 'A';
            }

            else if (i == game.defender.row &&
                     j == game.defender.col) {

                cout << 'D';
            }

            else if (i == game.vault.row &&
                     j == game.vault.col) {

                cout << 'V';
            }

            else if (i == game.escapePoint.row &&
                     j == game.escapePoint.col) {

                cout << 'E';
            }

            else {

                cout << maze[i][j];
            }
        }

        cout << endl;
    }
}

// ==========================================
// 13. MAIN GAME
// ==========================================

int main() {

    const char* maze[ROWS] = {

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

    GameState game;

    // Initial attacker position
    game.attacker.row = 1;
    game.attacker.col = 1;

    // Initial defender position
    game.defender.row = 6;
    game.defender.col = 7;

    // Vault position
    game.vault.row = 4;
    game.vault.col = 12;

    // Escape position
    game.escapePoint.row = 7;
    game.escapePoint.col = 16;

    // Initial game conditions
    game.mode = INFILTRATE;

    game.vaultReached = false;
    game.gameOver = false;
    game.attackerWon = false;

    char move;

    cout << "\n====================================\n";
    cout << "       MISCELLANEOUS MAZE\n";
    cout << "====================================\n";

    cout << "\nDEFENDER AI: MINIMAX + ALPHA-BETA\n";
    cout << "SEARCH DEPTH: " << SEARCH_DEPTH << "\n";

    // ======================================
    // GAME LOOP
    // ======================================

    while (!game.gameOver) {

        displayMaze(maze, game);

        cout << "\n------------------------------------\n";

        if (game.mode == INFILTRATE) {

            cout << "OBJECTIVE: REACH THE VAULT (V)\n";

        } else {

            cout << "OBJECTIVE: REACH THE ESCAPE (E)\n";
        }

        cout << "\nA = Attacker | D = Defender\n";
        cout << "V = Vault | E = Escape\n";

        cout << "\nMove using W/A/S/D or Q to quit: ";

        cin >> move;

        // Quit
        if (move == 'q' || move == 'Q') {

            cout << "\nGame abandoned.\n";
            break;
        }

        int newRow = game.attacker.row;
        int newCol = game.attacker.col;

        // Attacker movement
        if (move == 'w' || move == 'W') {
            newRow--;
        }

        else if (move == 's' || move == 'S') {
            newRow++;
        }

        else if (move == 'a' || move == 'A') {
            newCol--;
        }

        else if (move == 'd' || move == 'D') {
            newCol++;
        }

        else {

            cout << "\nInvalid movement!\n";
            continue;
        }

        // Wall collision
        if (!isWalkable(maze, newRow, newCol)) {

            cout << "\nYou cannot move through a wall!\n";
            continue;
        }

        // Update attacker
        game.attacker.row = newRow;
        game.attacker.col = newCol;

        // Check capture after attacker movement
        if (isCaptured(game)) {

            game.gameOver = true;
            game.attackerWon = false;

            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }

        // Check vault
        if (!game.vaultReached &&
            game.attacker.row == game.vault.row &&
            game.attacker.col == game.vault.col) {

            game.vaultReached = true;
            game.mode = ESCAPE;

            cout << "\nVAULT REACHED!\n";
            cout << "NEW OBJECTIVE: REACH ESCAPE!\n";
        }

        // Check escape
        if (game.mode == ESCAPE &&
            game.attacker.row == game.escapePoint.row &&
            game.attacker.col == game.escapePoint.col) {

            game.gameOver = true;
            game.attackerWon = true;

            cout << "\nMISSION COMPLETED! ATTACKER WINS!\n";
            break;
        }

        // ==================================
        // DEFENDER AI TURN
        // ==================================

        cout << "\nDEFENDER AI THINKING...\n";

        Position bestMove;

        int bestScore = minimax(
            game,
            SEARCH_DEPTH,
            true,
            -WIN_SCORE - 1,
            WIN_SCORE + 1,
            maze,
            &bestMove
        );

        cout << "MINIMAX EVALUATION: "
             << bestScore << endl;

        // Update defender position
        game.defender = bestMove;

        cout << "DEFENDER MOVED TO: ("
             << game.defender.row << ", "
             << game.defender.col << ")\n";

        // Check capture after defender movement
        if (isCaptured(game)) {

            game.gameOver = true;
            game.attackerWon = false;

            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }
    }

    // ======================================
    // FINAL RESULT
    // ======================================

    cout << "\n====================================\n";

    if (game.gameOver && game.attackerWon) {

        cout << "       ATTACKER VICTORY\n";
    }

    else if (game.gameOver) {

        cout << "       DEFENDER VICTORY\n";
    }

    cout << "       GAME OVER\n";
    cout << "====================================\n";

    return 0;
}
