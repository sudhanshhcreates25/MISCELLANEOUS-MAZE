#ifndef MISCELLANEOUS_MAZE_GAME_ENGINE_H
#define MISCELLANEOUS_MAZE_GAME_ENGINE_H

#include "GeneticProgramming.h"

#include <functional>
#include <vector>

inline constexpr int ROWS = 9;
inline constexpr int COLS = 19;
inline constexpr int SEARCH_DEPTH = 4;
inline constexpr int WIN_SCORE = 1000000;

struct Position {
    int row;
    int col;
};

enum AttackerMode {
    INFILTRATE,
    ESCAPE
};

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

using StateEvaluator = std::function<int(const GameState&, const char* maze[])>;

bool isWalkable(const char* maze[], int row, int col);
int shortestPathDistance(const char* maze[], Position start, Position target);
std::vector<Position> getLegalMoves(const char* maze[], Position current);
GameState applyMove(GameState state, Position next, bool defenderTurn);
maze_gp::FeatureVector extractGpFeatures(const GameState& state, const char* maze[]);
int evaluateState(GameState state, const char* maze[]);
int minimax(GameState state, int depth, bool defenderTurn, int alpha, int beta,
            const char* maze[], Position* bestMove = nullptr,
            const StateEvaluator& evaluator = StateEvaluator{});
Position findNextStep(const char* maze[], Position start, Position target);
bool isCaptured(const GameState& game);

#endif
