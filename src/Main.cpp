#include <iostream>
using namespace std;

int main() {

    char maze[9][20] = {
        "###################",
        "#A.....#..........#",
        "#......#..........#",
        "#......#######....#",
        "#...........V.....#",
        "#.................#",
        "#......D..........#",
        "#...............E.#",
        "###################"
    };

    // Attacker's starting position
    int attackerRow = 1;
    int attackerCol = 1;

    char move;

    while (true) {

        // Display maze
        for (int i = 0; i < 9; i++) {
            for (int j = 0; j < 19; j++) {
                cout << maze[i][j];
            }
            cout << endl;
        }

        cout << "\nMove attacker (W/A/S/D) or Q to quit: ";
        cin >> move;

        // Quit
        if (move == 'q' || move == 'Q') {
            break;
        }

        // Calculate possible new position
        int newRow = attackerRow;
        int newCol = attackerCol;

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
            cout << "Invalid input!\n";
            continue;
        }

        // Check whether the new position is a wall
        if (maze[newRow][newCol] == '#') {
            cout << "You cannot move through a wall!\n";
            continue;
        }

        // Remove attacker from old position
        maze[attackerRow][attackerCol] = '.';

        // Update attacker position
        attackerRow = newRow;
        attackerCol = newCol;

        // Put attacker at new position
        maze[attackerRow][attackerCol] = 'A';
    }

    cout << "\nGame ended.\n";

    return 0;
}