
#include <iostream>
#include <queue>
using namespace std;

const int ROWS = 9;
const int COLS = 19;

// Position of any object in the maze
struct Position {
    int row;
    int col;
};

// Attacker's objective
enum AttackerMode {
    INFILTRATE,
    ESCAPE
};

// Complete game information
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

// Check whether a cell is inside the maze and not a wall
bool isWalkable(const char* maze[], int row, int col) {

    if (row < 0 || row >= ROWS ||
        col < 0 || col >= COLS) {
        return false;
    }

    return maze[row][col] != '#';
}

// Find the next step along the shortest path using BFS
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

// Display the maze without destroying its original markers
void displayMaze(const char* maze[], const GameState& game) {

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

// Check whether the defender has captured the attacker
bool isCaptured(const GameState& game) {

    return game.attacker.row == game.defender.row &&
           game.attacker.col == game.defender.col;
}

// Main game
int main() {

    // Maze layout: only walls and paths are stored here
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

    // Initial positions
    game.attacker.row = 1;
    game.attacker.col = 1;

    game.defender.row = 6;
    game.defender.col = 7;

    game.vault.row = 4;
    game.vault.col = 12;

    game.escapePoint.row = 7;
    game.escapePoint.col = 16;

    // Initial game conditions
    game.mode = INFILTRATE;
    game.vaultReached = false;
    game.gameOver = false;
    game.attackerWon = false;

    char move;

    cout << "====================================\n";
    cout << "       MISCELLANEOUS MAZE\n";
    cout << "====================================\n";

    while (!game.gameOver) {

        displayMaze(maze, game);

        cout << "\n------------------------------------\n";

        if (game.mode == INFILTRATE) {
            cout << "OBJECTIVE: REACH THE VAULT (V)\n";
        } else {
            cout << "OBJECTIVE: REACH THE ESCAPE (E)\n";
        }

        cout << "A = Attacker | D = Defender\n";
        cout << "V = Vault | E = Escape\n";

        cout << "\nMove using W/A/S/D or Q to quit: ";
        cin >> move;

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

        // Check wall collision
        if (!isWalkable(maze, newRow, newCol)) {
            cout << "\nYou cannot move through a wall!\n";
            continue;
        }

        // Update attacker position
        game.attacker.row = newRow;
        game.attacker.col = newCol;

        // Check if attacker has reached the defender
        if (isCaptured(game)) {
            game.gameOver = true;
            game.attackerWon = false;
            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }

        // Check Vault objective
        if (!game.vaultReached &&
            game.attacker.row == game.vault.row &&
            game.attacker.col == game.vault.col) {

            game.vaultReached = true;
            game.mode = ESCAPE;

            cout << "\nVAULT SECURED!\n";
            cout << "NEW OBJECTIVE: REACH THE ESCAPE POINT!\n";
        }

        // Check Escape objective
        if (game.mode == ESCAPE &&
            game.attacker.row == game.escapePoint.row &&
            game.attacker.col == game.escapePoint.col) {

            game.gameOver = true;
            game.attackerWon = true;

            cout << "\nMISSION COMPLETED!\n";
            cout << "ATTACKER WINS!\n";

            break;
        }

        // Basic Defender AI: chase attacker using BFS
        Position nextMove = findNextStep(
            maze,
            game.defender,
            game.attacker
        );

        if (nextMove.row != -1) {

            game.defender.row = nextMove.row;
            game.defender.col = nextMove.col;
        }

        // Check capture after defender moves
        if (isCaptured(game)) {

            game.gameOver = true;
            game.attackerWon = false;

            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }
    }

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
