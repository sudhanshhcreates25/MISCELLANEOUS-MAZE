#include "../src/GameEngine.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

namespace {

const char* maze[ROWS] = {
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

int previousEvaluation(const GameState& state) {
    if (state.gameOver) return state.attackerWon ? -WIN_SCORE : WIN_SCORE;
    const Position objective = state.mode == INFILTRATE
        ? state.vault : state.escapePoint;
    int attackerDistance = shortestPathDistance(maze, state.attacker, objective);
    int defenderDistance = shortestPathDistance(maze, state.defender, state.attacker);
    if (attackerDistance < 0) attackerDistance = ROWS * COLS;
    if (defenderDistance < 0) defenderDistance = ROWS * COLS;
    return 3 * attackerDistance - 2 * defenderDistance;
}

int previousMinimax(GameState state, int depth, bool defenderTurn,
                    int alpha, int beta, Position* bestMove = nullptr) {
    if (depth == 0 || state.gameOver) return previousEvaluation(state);
    const std::vector<Position> moves = getLegalMoves(
        maze, defenderTurn ? state.defender : state.attacker);
    if (moves.empty()) return previousEvaluation(state);
    int best = defenderTurn ? -WIN_SCORE - 1 : WIN_SCORE + 1;
    if (defenderTurn && bestMove) *bestMove = state.defender;
    for (Position move : moves) {
        const GameState next = applyMove(state, move, defenderTurn);
        const int score = previousMinimax(next, depth - 1, !defenderTurn,
                                          alpha, beta);
        if (defenderTurn) {
            if (score > best) {
                best = score;
                if (bestMove) *bestMove = move;
            }
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, score);
            beta = std::min(beta, best);
        }
        if (alpha >= beta) break;
    }
    return best;
}

bool at(Position position, int row, int col) {
    return position.row == row && position.col == col;
}

GameState interceptionState(AttackerMode mode) {
    GameState state{};
    state.attacker = {1, 1};
    state.defender = {2, 2};
    state.vault = mode == INFILTRATE ? Position{2, 1} : Position{7, 17};
    state.escapePoint = {2, 1};
    state.mode = mode;
    state.vaultReached = mode == ESCAPE;
    return state;
}

} // namespace

int main() {
    GameState capture{};
    capture.attacker = {1, 1};
    capture.defender = {1, 2};
    capture.vault = {5, 10};
    capture.escapePoint = {7, 17};
    capture.mode = INFILTRATE;
    Position captureMove{};
    const int captureScore = minimax(capture, 1, true,
        -WIN_SCORE - 1, WIN_SCORE + 1, maze, &captureMove);
    assert(at(captureMove, 1, 1));
    assert(captureScore == WIN_SCORE);
    std::cout << "PASS immediate legal capture wins over heuristic terms\n";

    for (AttackerMode mode : {INFILTRATE, ESCAPE}) {
        const GameState state = interceptionState(mode);
        Position improved{};
        Position previous{};
        minimax(state, 1, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                maze, &improved);
        previousMinimax(state, 1, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                        &previous);
        assert(at(improved, 2, 1));
        assert(at(previous, 1, 2));
        const char* phase = mode == INFILTRATE ? "Vault" : "Escape";
        std::cout << "PASS " << phase << " interception: new blocks objective; previous heuristic chases\n";
    }

    // With the same approach, blocking the objective prevents the Attacker's
    // immediate route while an unnecessary chase leaves that cell open.
    const GameState blockState = interceptionState(INFILTRATE);
    Position blockMove{};
    Position chaseMove{};
    minimax(blockState, 1, true, -WIN_SCORE - 1, WIN_SCORE + 1,
            maze, &blockMove);
    previousMinimax(blockState, 1, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                    &chaseMove);
    assert(at(blockMove, blockState.vault.row, blockState.vault.col));
    assert(!at(chaseMove, blockState.vault.row, blockState.vault.col));
    std::cout << "PASS useful blocking move preferred over unnecessary chase\n";
    std::cout << "All interception tests passed.\n";
}
