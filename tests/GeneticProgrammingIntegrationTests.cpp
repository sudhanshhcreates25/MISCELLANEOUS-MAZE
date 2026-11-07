#include "../src/GameEngine.h"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace maze_gp;
using namespace std;

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

GameState makeState() {
    GameState state{};
    state.attacker = {2, 2};
    state.defender = {5, 5};
    state.vault = {2, 7};
    state.escapePoint = {7, 16};
    state.mode = INFILTRATE;
    return state;
}

int exhaustiveMinimax(GameState state, int depth, bool defenderTurn,
                      const char* board[], const StateEvaluator& evaluator) {
    if (state.gameOver) return evaluateState(state, board);
    if (depth == 0) return evaluator ? evaluator(state, board) : evaluateState(state, board);
    const vector<Position> moves = getLegalMoves(
        board, defenderTurn ? state.defender : state.attacker);
    if (moves.empty()) return evaluator ? evaluator(state, board) : evaluateState(state, board);
    int best = defenderTurn ? -WIN_SCORE - 1 : WIN_SCORE + 1;
    for (Position move : moves) {
        const GameState child = applyMove(state, move, defenderTurn);
        const int score = exhaustiveMinimax(child, depth - 1, !defenderTurn,
                                            board, evaluator);
        best = defenderTurn ? max(best, score) : min(best, score);
    }
    return best;
}

ExpressionTree gpFeatureTree() {
    Node root;
    root.kind = NodeKind::Add;
    root.left = std::make_unique<Node>();
    root.left->kind = NodeKind::Feature;
    root.left->feature = Feature::AttackerObjectiveDistance;
    root.right = std::make_unique<Node>();
    root.right->kind = NodeKind::Feature;
    root.right->feature = Feature::DefenderProximity;
    return ExpressionTree(std::move(root));
}

} // namespace

int main() {
    GameState state = makeState();
    const FeatureVector features = extractGpFeatures(state, maze);
    for (double feature : features) assert(feature >= 0.0 && feature <= 1.0);

    const int defaultScore = minimax(state, 0, true, -WIN_SCORE - 1,
                                     WIN_SCORE + 1, maze);
    assert(defaultScore == evaluateState(state, maze));
    std::cout << "PASS original heuristic remains default\n";

    const ExpressionTree tree = gpFeatureTree();
    StateEvaluator gpEvaluator = [&tree](const GameState& leaf, const char* board[]) {
        return static_cast<int>(tree.evaluate(extractGpFeatures(leaf, board)) * 1000.0);
    };
    const int gpScore = minimax(state, 0, true, -WIN_SCORE - 1,
                                WIN_SCORE + 1, maze, nullptr, gpEvaluator);
    assert(gpScore == static_cast<int>(tree.evaluate(features) * 1000.0));
    std::cout << "PASS GP evaluator can be supplied to Minimax\n";

    int evaluatorCalls = 0;
    StateEvaluator countedEvaluator = [&evaluatorCalls](const GameState&, const char*[]) {
        ++evaluatorCalls;
        return 17;
    };
    (void)minimax(state, 3, true, -WIN_SCORE - 1,
                  WIN_SCORE + 1, maze, nullptr, countedEvaluator);
    assert(evaluatorCalls > 1);
    std::cout << "PASS supplied evaluator is used recursively\n";

    GameState defenderTerminal = state;
    defenderTerminal.gameOver = true;
    defenderTerminal.attackerWon = false;
    GameState attackerTerminal = state;
    attackerTerminal.gameOver = true;
    attackerTerminal.attackerWon = true;
    assert(minimax(defenderTerminal, 2, true, -WIN_SCORE - 1,
                   WIN_SCORE + 1, maze, nullptr, countedEvaluator) == WIN_SCORE);
    assert(minimax(attackerTerminal, 2, true, -WIN_SCORE - 1,
                   WIN_SCORE + 1, maze, nullptr, countedEvaluator) == -WIN_SCORE);
    std::cout << "PASS GP cannot override terminal scores\n";

    StateEvaluator originalEvaluator = [](const GameState& leaf, const char* board[]) {
        return evaluateState(leaf, board);
    };
    for (int depth = 1; depth <= 3; ++depth) {
        assert(minimax(state, depth, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                       maze) == exhaustiveMinimax(state, depth, true, maze, {}));
        assert(minimax(state, depth, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                       maze, nullptr, gpEvaluator) ==
               exhaustiveMinimax(state, depth, true, maze, gpEvaluator));
        assert(minimax(state, depth, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                       maze, nullptr, originalEvaluator) ==
               exhaustiveMinimax(state, depth, true, maze, originalEvaluator));
    }
    std::cout << "PASS Alpha-Beta scores match exhaustive Minimax for both evaluators\n";
    std::cout << "All GP-Minimax integration tests passed.\n";
}
