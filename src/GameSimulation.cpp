#include "GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>

namespace {

Position invalidPosition() { return {-1, -1}; }

int pathOrFar(const char* maze[], Position from, Position to) {
    const int distance = shortestPathDistance(maze, from, to);
    return distance < 0 ? ROWS * COLS : distance;
}

std::vector<StartingScenario> resolveScenarios(
    const char* maze[], const SimulationConfig& config) {
    if (!config.scenarios.empty()) return config.scenarios;
    return makeDefaultScenarios(maze, config.scenarioCount);
}

void validateScenario(const StartingScenario& scenario, const char* maze[]) {
    const GameState& state = scenario.initialState;
    if (state.gameOver || !isWalkable(maze, state.attacker.row, state.attacker.col) ||
        !isWalkable(maze, state.defender.row, state.defender.col) ||
        !isWalkable(maze, state.vault.row, state.vault.col) ||
        !isWalkable(maze, state.escapePoint.row, state.escapePoint.col) ||
        isCaptured(state) ||
        (state.mode == INFILTRATE && state.attacker.row == state.vault.row &&
         state.attacker.col == state.vault.col) ||
        (state.mode == ESCAPE && (!state.vaultReached ||
                                  (state.attacker.row == state.escapePoint.row &&
                                   state.attacker.col == state.escapePoint.col))) ||
        (state.mode == INFILTRATE && state.vaultReached)) {
        throw std::invalid_argument("Invalid starting scenario: " + scenario.name);
    }
}

FitnessReport scoreGames(const char* maze[], const SimulationConfig& config,
                         const maze_gp::ExpressionTree* candidate) {
    if (config.turnLimit <= 0 || config.searchDepth < 0 ||
        config.attackerStrategies.empty()) {
        throw std::invalid_argument("Simulation requires positive turn limit, nonnegative depth, and strategies");
    }
    const std::vector<StartingScenario> scenarios = resolveScenarios(maze, config);
    if (scenarios.empty()) throw std::invalid_argument("Fitness evaluation requires scenarios");

    FitnessReport report;
    double totalFitness = 0.0;
    long long totalTurns = 0;
    for (const StartingScenario& scenario : scenarios) {
        validateScenario(scenario, maze);
        for (AttackerStrategy strategy : config.attackerStrategies) {
            StateEvaluator evaluator;
            if (candidate != nullptr) {
                evaluator = [candidate, scale = config.gpScoreScale](
                    const GameState& state, const char* board[]) {
                    return static_cast<int>(candidate->evaluate(
                        extractGpFeatures(state, board)) * scale);
                };
            }
            const SimulationResult result = simulateGame(
                maze, scenario.initialState, strategy, config.searchDepth,
                config.turnLimit, evaluator);
            ++report.games;
            totalTurns += result.turns;
            switch (result.outcome) {
                case SimulationOutcome::DefenderCapture:
                    ++report.captures;
                    totalFitness += config.captureReward + config.fastCaptureBonus *
                        (1.0 - static_cast<double>(result.turns) / config.turnLimit);
                    break;
                case SimulationOutcome::AttackerEscape:
                    ++report.escapes;
                    totalFitness -= config.escapePenalty;
                    break;
                case SimulationOutcome::Draw:
                    ++report.draws;
                    break;
            }
        }
    }
    report.averageFitness = totalFitness / report.games;
    report.averageTurns = static_cast<double>(totalTurns) / report.games;
    return report;
}

} // namespace

std::vector<StartingScenario> makeDefaultScenarios(
    const char* maze[], std::size_t scenarioCount) {
    const struct Spec {
        const char* name;
        Position attacker;
        Position defender;
        Position vault;
        Position escape;
        AttackerMode mode;
    } specs[] = {
        {"infiltrate-west", {1, 1}, {6, 7}, {4, 12}, {7, 16}, INFILTRATE},
        {"infiltrate-center", {2, 2}, {5, 6}, {4, 12}, {7, 16}, INFILTRATE},
        {"escape-center", {5, 12}, {2, 3}, {4, 12}, {7, 16}, ESCAPE},
        {"escape-east", {6, 13}, {2, 6}, {4, 12}, {7, 16}, ESCAPE}
    };

    std::vector<StartingScenario> result;
    const std::size_t count = std::min(scenarioCount, sizeof(specs) / sizeof(specs[0]));
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        GameState state{};
        state.attacker = specs[i].attacker;
        state.defender = specs[i].defender;
        state.vault = specs[i].vault;
        state.escapePoint = specs[i].escape;
        state.mode = specs[i].mode;
        state.vaultReached = specs[i].mode == ESCAPE;
        StartingScenario scenario{specs[i].name, state};
        validateScenario(scenario, maze);
        result.push_back(scenario);
    }
    return result;
}

Position chooseAttackerMove(const GameState& state, const char* maze[],
                            AttackerStrategy strategy) {
    const std::vector<Position> moves = getLegalMoves(maze, state.attacker);
    if (moves.empty()) return invalidPosition();
    const Position objective = state.mode == INFILTRATE ? state.vault : state.escapePoint;

    if (strategy == AttackerStrategy::ShortestPath) {
        const Position next = findNextStep(maze, state.attacker, objective);
        for (Position move : moves)
            if (move.row == next.row && move.col == next.col) return move;
        return invalidPosition();
    }

    Position best = invalidPosition();
    int bestScore = -1000000;
    for (Position move : moves) {
        const int toObjective = pathOrFar(maze, move, objective);
        const int fromDefender = shortestPathDistance(maze, move, state.defender);
        const int defenderDistance = fromDefender < 0 ? 0 : fromDefender;
        const int score = 2 * defenderDistance - toObjective;
        if (score > bestScore) {
            bestScore = score;
            best = move;
        }
    }
    return best;
}

SimulationResult simulateGame(const char* maze[], const GameState& initialState,
                              AttackerStrategy strategy, int searchDepth,
                              int turnLimit, const StateEvaluator& defenderEvaluator) {
    if (searchDepth < 0 || turnLimit < 0)
        throw std::invalid_argument("Search depth and turn limit cannot be negative");
    GameState state = initialState;
    int turns = 0;
    while (!state.gameOver && turns < turnLimit) {
        const Position attackerMove = chooseAttackerMove(state, maze, strategy);
        if (attackerMove.row < 0) return {SimulationOutcome::Draw, turns};
        state = applyMove(state, attackerMove, false);
        ++turns;
        if (state.gameOver) {
            return {state.attackerWon ? SimulationOutcome::AttackerEscape
                                      : SimulationOutcome::DefenderCapture, turns};
        }
        if (turns >= turnLimit) break;

        Position defenderMove = state.defender;
        minimax(state, searchDepth, true, -WIN_SCORE - 1, WIN_SCORE + 1,
                maze, &defenderMove, defenderEvaluator);
        state = applyMove(state, defenderMove, true);
        ++turns;
        if (state.gameOver) {
            return {state.attackerWon ? SimulationOutcome::AttackerEscape
                                      : SimulationOutcome::DefenderCapture, turns};
        }
    }
    return {SimulationOutcome::Draw, turns};
}

FitnessReport evaluateFitness(const maze_gp::ExpressionTree& candidate,
                              const char* maze[], const SimulationConfig& config) {
    return scoreGames(maze, config, &candidate);
}

FitnessReport evaluateBaselineFitness(const char* maze[],
                                      const SimulationConfig& config) {
    return scoreGames(maze, config, nullptr);
}

maze_gp::FitnessFunction makeFitnessCallback(const char* maze[],
                                             SimulationConfig config) {
    auto rows = std::make_shared<std::array<std::string, ROWS>>();
    for (int row = 0; row < ROWS; ++row) (*rows)[row] = maze[row];
    return [rows, config = std::move(config)](const maze_gp::ExpressionTree& candidate) {
        const char* board[ROWS];
        for (int row = 0; row < ROWS; ++row) board[row] = (*rows)[row].c_str();
        return evaluateFitness(candidate, board, config).averageFitness;
    };
}

maze_gp::ExpressionTree trainDefenderTree(
    const char* maze[], SimulationConfig simulationConfig,
    maze_gp::EvolutionConfig evolutionConfig, unsigned randomSeed) {
    const maze_gp::FitnessFunction fitness = makeFitnessCallback(maze, std::move(simulationConfig));
    return maze_gp::evolve(fitness, evolutionConfig, randomSeed);
}
