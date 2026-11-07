#define main maze_interactive_main
#include "../src/Main.cpp"
#undef main

#include <cassert>
#include <iostream>

namespace {

const char* testMaze[ROWS] = {
    "###################",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "#.................#",
    "###################"
};

GameState makeState(Position attacker, Position defender,
                    Position vault, Position escapePoint,
                    AttackerMode mode = INFILTRATE) {
    GameState state{};
    state.attacker = attacker;
    state.defender = defender;
    state.vault = vault;
    state.escapePoint = escapePoint;
    state.mode = mode;
    state.vaultReached = mode == ESCAPE;
    return state;
}

int minimaxWithoutPruning(GameState state, int depth, bool defenderTurn,
                          const char* maze[]) {
    if (depth == 0 || state.gameOver) {
        return evaluateState(state, maze);
    }

    vector<Position> moves = getLegalMoves(
        maze, defenderTurn ? state.defender : state.attacker);
    if (moves.empty()) {
        return evaluateState(state, maze);
    }

    int best = defenderTurn ? -WIN_SCORE - 1 : WIN_SCORE + 1;
    for (Position move : moves) {
        GameState next = applyMove(state, move, defenderTurn);
        int score = minimaxWithoutPruning(next, depth - 1,
                                           !defenderTurn, maze);
        best = defenderTurn ? max(best, score) : min(best, score);
    }
    return best;
}

struct SearchStats {
    int nodes = 0;
    int cutoffs = 0;
};

int countedAlphaBeta(GameState state, int depth, bool defenderTurn,
                     int alpha, int beta, const char* maze[],
                     SearchStats& stats) {
    ++stats.nodes;
    if (depth == 0 || state.gameOver) {
        return evaluateState(state, maze);
    }

    vector<Position> moves = getLegalMoves(
        maze, defenderTurn ? state.defender : state.attacker);
    if (moves.empty()) {
        return evaluateState(state, maze);
    }

    int best = defenderTurn ? -WIN_SCORE - 1 : WIN_SCORE + 1;
    for (Position move : moves) {
        GameState next = applyMove(state, move, defenderTurn);
        int score = countedAlphaBeta(next, depth - 1, !defenderTurn,
                                     alpha, beta, maze, stats);
        if (defenderTurn) {
            best = max(best, score);
            alpha = max(alpha, best);
        } else {
            best = min(best, score);
            beta = min(beta, best);
        }
        if (alpha >= beta) {
            ++stats.cutoffs;
            break;
        }
    }
    return best;
}

bool samePosition(Position a, Position b) {
    return a.row == b.row && a.col == b.col;
}

} // namespace

int main() {
    int passed = 0;

    // Defender can capture by moving onto the attacker's cell.
    GameState capture = makeState({3, 4}, {3, 3}, {6, 10}, {7, 15});
    Position chosen{};
    int captureScore = minimax(capture, 1, true, -WIN_SCORE - 1,
                               WIN_SCORE + 1, testMaze, &chosen);
    assert(captureScore == WIN_SCORE);
    assert(samePosition(chosen, capture.attacker));
    // On its own turn, the attacker as MIN avoids the same losing capture.
    int attackerChoice = minimax(capture, 1, false, -WIN_SCORE - 1,
                                 WIN_SCORE + 1, testMaze);
    assert(attackerChoice < WIN_SCORE);
    ++passed;

    // Attacker's vault move switches the objective to ESCAPE.
    GameState vault = makeState({3, 3}, {6, 6}, {3, 4}, {7, 15});
    GameState afterVault = applyMove(vault, {3, 4}, false);
    assert(afterVault.vaultReached && afterVault.mode == ESCAPE);
    assert(!afterVault.gameOver);
    ++passed;

    // Reaching Escape while in ESCAPE mode ends the game for the attacker.
    GameState escape = makeState({3, 3}, {6, 6}, {2, 2}, {3, 4}, ESCAPE);
    GameState afterEscape = applyMove(escape, {3, 4}, false);
    assert(afterEscape.gameOver && afterEscape.attackerWon);
    assert(evaluateState(afterEscape, testMaze) == -WIN_SCORE);
    ++passed;

    // Defender captures instead of allowing an immediate escape on the next turn.
    GameState threat = makeState({4, 4}, {3, 4}, {1, 1}, {4, 5}, ESCAPE);
    Position defensiveMove{};
    int threatScore = minimax(threat, 2, true, -WIN_SCORE - 1,
                              WIN_SCORE + 1, testMaze, &defensiveMove);
    assert(threatScore == WIN_SCORE);
    assert(samePosition(defensiveMove, threat.attacker));
    ++passed;

    // Terminal scores retain defender-positive / attacker-negative convention.
    GameState defenderWon = capture;
    defenderWon.gameOver = true;
    defenderWon.attackerWon = false;
    GameState attackerWon = escape;
    attackerWon.gameOver = true;
    attackerWon.attackerWon = true;
    assert(minimax(defenderWon, 3, true, -WIN_SCORE - 1,
                   WIN_SCORE + 1, testMaze) == WIN_SCORE);
    assert(minimax(attackerWon, 3, true, -WIN_SCORE - 1,
                   WIN_SCORE + 1, testMaze) == -WIN_SCORE);
    ++passed;

    // Production Alpha-Beta and exhaustive Minimax agree on bounded positions.
    GameState small = makeState({2, 2}, {5, 5}, {2, 6}, {7, 16});
    for (int depth = 1; depth <= 4; ++depth) {
        int plain = minimaxWithoutPruning(small, depth, true, testMaze);
        int pruned = minimax(small, depth, true, -WIN_SCORE - 1,
                             WIN_SCORE + 1, testMaze);
        assert(plain == pruned);
    }
    ++passed;

    // Confirm cutoffs occur in an instrumented copy of the production recurrence.
    SearchStats stats;
    int counted = countedAlphaBeta(small, 4, true, -WIN_SCORE - 1,
                                  WIN_SCORE + 1, testMaze, stats);
    assert(counted == minimaxWithoutPruning(small, 4, true, testMaze));
    assert(stats.cutoffs > 0);
    assert(stats.nodes < 10000);
    ++passed;

    // Search operates on value copies and leaves the caller's state unchanged.
    GameState original = threat;
    (void)minimax(threat, 3, true, -WIN_SCORE - 1,
                  WIN_SCORE + 1, testMaze);
    assert(samePosition(threat.attacker, original.attacker));
    assert(samePosition(threat.defender, original.defender));
    assert(threat.mode == original.mode &&
           threat.vaultReached == original.vaultReached &&
           threat.gameOver == original.gameOver &&
           threat.attackerWon == original.attackerWon);
    ++passed;

    std::cout << "Passed " << passed << " Minimax/Alpha-Beta test groups.\n";
    std::cout << "Instrumented search visited " << stats.nodes
              << " nodes and observed " << stats.cutoffs << " cutoffs.\n";
}
