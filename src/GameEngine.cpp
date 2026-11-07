#include "GameEngine.h"

#include <algorithm>
#include <cstdlib>
#include <queue>

using namespace std;

bool isWalkable(const char* maze[], int row, int col) {
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return false;
    return maze[row][col] != '#';
}

int shortestPathDistance(const char* maze[], Position start, Position target) {
    if (!isWalkable(maze, start.row, start.col) ||
        !isWalkable(maze, target.row, target.col)) return -1;

    int distances[ROWS][COLS];
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            distances[row][col] = -1;

    queue<Position> q;
    q.push(start);
    distances[start.row][start.col] = 0;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};

    while (!q.empty()) {
        Position current = q.front();
        q.pop();
        if (current.row == target.row && current.col == target.col)
            return distances[current.row][current.col];

        for (int i = 0; i < 4; ++i) {
            int nr = current.row + dr[i];
            int nc = current.col + dc[i];
            if (!isWalkable(maze, nr, nc) || distances[nr][nc] != -1) continue;
            distances[nr][nc] = distances[current.row][current.col] + 1;
            q.push({nr, nc});
        }
    }
    return -1;
}

vector<Position> getLegalMoves(const char* maze[], Position current) {
    vector<Position> moves;
    const int dr[] = {-1, 1, 0, 0};
    const int dc[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nr = current.row + dr[i];
        int nc = current.col + dc[i];
        if (isWalkable(maze, nr, nc)) moves.push_back({nr, nc});
    }
    return moves;
}

GameState applyMove(GameState state, Position next, bool defenderTurn) {
    if (defenderTurn) {
        state.defender = next;
    } else {
        state.attacker = next;
        if (state.attacker.row == state.defender.row &&
            state.attacker.col == state.defender.col) {
            state.gameOver = true;
            state.attackerWon = false;
        }
        if (!state.gameOver && !state.vaultReached &&
            state.attacker.row == state.vault.row &&
            state.attacker.col == state.vault.col) {
            state.vaultReached = true;
            state.mode = ESCAPE;
        }
        if (!state.gameOver && state.mode == ESCAPE &&
            state.attacker.row == state.escapePoint.row &&
            state.attacker.col == state.escapePoint.col) {
            state.gameOver = true;
            state.attackerWon = true;
        }
    }

    if (state.attacker.row == state.defender.row &&
        state.attacker.col == state.defender.col) {
        state.gameOver = true;
        state.attackerWon = false;
    }
    return state;
}

maze_gp::FeatureVector extractGpFeatures(const GameState& state, const char* maze[]) {
    const Position objective = state.mode == INFILTRATE ? state.vault : state.escapePoint;
    const int objectiveDistance = shortestPathDistance(maze, state.attacker, objective);
    const int defenderDistance = shortestPathDistance(maze, state.defender, state.attacker);
    const double maxDistance = static_cast<double>(ROWS * COLS - 1);

    const double attackerDistanceFeature = objectiveDistance < 0
        ? 1.0
        : 1.0 - min(1.0, objectiveDistance / maxDistance);
    const double defenderProximityFeature = defenderDistance < 0
        ? 0.0
        : 1.0 - min(1.0, defenderDistance / maxDistance);

    const vector<Position> attackerMoves = getLegalMoves(maze, state.attacker);
    const double mobilityFeature = 1.0 -
        static_cast<double>(attackerMoves.size()) / 4.0;
    int restrictedMoves = 0;
    for (Position move : attackerMoves) {
        const int distance = abs(move.row - state.defender.row) +
                             abs(move.col - state.defender.col);
        if (distance <= 1) ++restrictedMoves;
    }
    const double restrictionFeature = attackerMoves.empty()
        ? 1.0
        : static_cast<double>(restrictedMoves) / attackerMoves.size();

    return {{attackerDistanceFeature, defenderProximityFeature,
             mobilityFeature, restrictionFeature,
             state.vaultReached ? 1.0 : 0.0}};
}

int evaluateState(GameState state, const char* maze[]) {
    if (state.gameOver && !state.attackerWon) return WIN_SCORE;
    if (state.gameOver && state.attackerWon) return -WIN_SCORE;

    Position target = state.mode == INFILTRATE ? state.vault : state.escapePoint;
    const int unreachableDistance = ROWS * COLS;
    int attackerToTarget = shortestPathDistance(maze, state.attacker, target);
    int defenderToAttacker = shortestPathDistance(maze, state.defender, state.attacker);
    if (attackerToTarget < 0) attackerToTarget = unreachableDistance;
    if (defenderToAttacker < 0) defenderToAttacker = unreachableDistance;
    return (3 * attackerToTarget) - (2 * defenderToAttacker);
}

namespace {

int evaluateForSearch(const GameState& state, const char* maze[],
                      const StateEvaluator& evaluator) {
    if (state.gameOver) return evaluateState(state, maze);
    return evaluator ? evaluator(state, maze) : evaluateState(state, maze);
}

} // namespace

int minimax(GameState state, int depth, bool defenderTurn, int alpha, int beta,
            const char* maze[], Position* bestMove,
            const StateEvaluator& evaluator) {
    if (depth == 0 || state.gameOver) return evaluateForSearch(state, maze, evaluator);

    if (defenderTurn) {
        int bestScore = -WIN_SCORE - 1;
        vector<Position> moves = getLegalMoves(maze, state.defender);
        if (bestMove != nullptr) *bestMove = state.defender;
        if (moves.empty()) return evaluateForSearch(state, maze, evaluator);

        for (Position move : moves) {
            GameState nextState = applyMove(state, move, true);
            int score = minimax(nextState, depth - 1, false, alpha, beta,
                                maze, nullptr, evaluator);
            if (score > bestScore) {
                bestScore = score;
                if (bestMove != nullptr) *bestMove = move;
            }
            if (bestScore > alpha) alpha = bestScore;
            if (alpha >= beta) break;
        }
        return bestScore;
    }

    int bestScore = WIN_SCORE + 1;
    vector<Position> moves = getLegalMoves(maze, state.attacker);
    if (moves.empty()) return evaluateForSearch(state, maze, evaluator);
    for (Position move : moves) {
        GameState nextState = applyMove(state, move, false);
        int score = minimax(nextState, depth - 1, true, alpha, beta,
                            maze, nullptr, evaluator);
        if (score < bestScore) bestScore = score;
        if (bestScore < beta) beta = bestScore;
        if (alpha >= beta) break;
    }
    return bestScore;
}

Position findNextStep(const char* maze[], Position start, Position target) {
    Position invalid = {-1, -1};
    if (!isWalkable(maze, start.row, start.col) ||
        !isWalkable(maze, target.row, target.col)) return invalid;

    bool visited[ROWS][COLS] = {};
    Position firstStep[ROWS][COLS];
    queue<Position> q;
    q.push(start);
    visited[start.row][start.col] = true;

    while (!q.empty()) {
        Position current = q.front();
        q.pop();
        if (current.row == target.row && current.col == target.col) {
            if (current.row == start.row && current.col == start.col) return invalid;
            return firstStep[current.row][current.col];
        }

        const int dr[] = {-1, 1, 0, 0};
        const int dc[] = {0, 0, -1, 1};
        for (int i = 0; i < 4; ++i) {
            int nr = current.row + dr[i];
            int nc = current.col + dc[i];
            if (!isWalkable(maze, nr, nc) || visited[nr][nc]) continue;
            visited[nr][nc] = true;
            if (current.row == start.row && current.col == start.col)
                firstStep[nr][nc] = {nr, nc};
            else
                firstStep[nr][nc] = firstStep[current.row][current.col];
            q.push({nr, nc});
        }
    }
    return invalid;
}

bool isCaptured(const GameState& game) {
    return game.attacker.row == game.defender.row &&
           game.attacker.col == game.defender.col;
}
