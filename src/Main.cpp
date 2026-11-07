
#include "GameEngine.h"

#include <iostream>

using namespace std;

void displayMaze(const char* maze[], const GameState& game) {
    cout << "\n";
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            if (row == game.attacker.row && col == game.attacker.col) cout << 'A';
            else if (row == game.defender.row && col == game.defender.col) cout << 'D';
            else if (row == game.vault.row && col == game.vault.col) cout << 'V';
            else if (row == game.escapePoint.row && col == game.escapePoint.col) cout << 'E';
            else cout << maze[row][col];
        }
        cout << endl;
    }
}

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

    GameState game{};
    game.attacker = {1, 1};
    game.defender = {6, 7};
    game.vault = {4, 12};
    game.escapePoint = {7, 16};
    game.mode = INFILTRATE;

    // Replace this placeholder with a trained tree and set true to select GP.
    const bool useGpEvaluator = false;
    const maze_gp::ExpressionTree gpEvaluator;

    char move;
    cout << "\n====================================\n";
    cout << "       MISCELLANEOUS MAZE\n";
    cout << "====================================\n";
    cout << "\nDEFENDER AI: MINIMAX + ALPHA-BETA\n";
    cout << "SEARCH DEPTH: " << SEARCH_DEPTH << "\n";

    while (!game.gameOver) {
        displayMaze(maze, game);
        cout << "\n------------------------------------\n";
        cout << (game.mode == INFILTRATE
            ? "OBJECTIVE: REACH THE VAULT (V)\n"
            : "OBJECTIVE: REACH THE ESCAPE (E)\n");
        cout << "\nA = Attacker | D = Defender\n";
        cout << "V = Vault | E = Escape\n";
        cout << "\nMove using W/A/S/D or Q to quit: ";

        if (!(cin >> move)) break;
        if (move == 'q' || move == 'Q') {
            cout << "\nGame abandoned.\n";
            break;
        }

        Position nextAttacker = game.attacker;
        if (move == 'w' || move == 'W') --nextAttacker.row;
        else if (move == 's' || move == 'S') ++nextAttacker.row;
        else if (move == 'a' || move == 'A') --nextAttacker.col;
        else if (move == 'd' || move == 'D') ++nextAttacker.col;
        else {
            cout << "\nInvalid movement!\n";
            continue;
        }

        if (!isWalkable(maze, nextAttacker.row, nextAttacker.col)) {
            cout << "\nYou cannot move through a wall!\n";
            continue;
        }

        const bool hadVault = game.vaultReached;
        game = applyMove(game, nextAttacker, false);
        if (game.gameOver && !game.attackerWon) {
            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }
        if (!hadVault && game.vaultReached) {
            cout << "\nVAULT REACHED!\n";
            cout << "NEW OBJECTIVE: REACH ESCAPE!\n";
        }
        if (game.gameOver && game.attackerWon) {
            cout << "\nMISSION COMPLETED! ATTACKER WINS!\n";
            break;
        }

        cout << "\nDEFENDER AI THINKING...\n";
        Position bestMove;
        StateEvaluator evaluator;
        if (useGpEvaluator) {
            evaluator = [&gpEvaluator](const GameState& state, const char* currentMaze[]) {
                return static_cast<int>(gpEvaluator.evaluate(
                    extractGpFeatures(state, currentMaze)));
            };
        }
        const int bestScore = minimax(game, SEARCH_DEPTH, true,
            -WIN_SCORE - 1, WIN_SCORE + 1, maze, &bestMove, evaluator);
        cout << "MINIMAX EVALUATION: " << bestScore << endl;

        game = applyMove(game, bestMove, true);
        cout << "DEFENDER MOVED TO: (" << game.defender.row << ", "
             << game.defender.col << ")\n";
        if (game.gameOver && !game.attackerWon) {
            cout << "\nDEFENDER WINS! ATTACKER CAPTURED!\n";
            break;
        }
    }

    cout << "\n====================================\n";
    if (game.gameOver && game.attackerWon) cout << "       ATTACKER VICTORY\n";
    else if (game.gameOver) cout << "       DEFENDER VICTORY\n";
    cout << "       GAME OVER\n";
    cout << "====================================\n";
    return 0;
}
