#ifndef MISCELLANEOUS_MAZE_GAME_SIMULATION_H
#define MISCELLANEOUS_MAZE_GAME_SIMULATION_H

#include "GameEngine.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

enum class AttackerStrategy {
    ShortestPath,
    Evasive
};

enum class SimulationOutcome {
    DefenderCapture,
    AttackerEscape,
    Draw
};

struct StartingScenario {
    std::string name;
    GameState initialState;
};

struct SimulationConfig {
    int turnLimit = 60; // individual moves (plies), including both players
    int searchDepth = 2;
    std::size_t scenarioCount = 4;
    std::vector<StartingScenario> scenarios;
    std::vector<AttackerStrategy> attackerStrategies = {
        AttackerStrategy::ShortestPath,
        AttackerStrategy::Evasive
    };
    double captureReward = 1000.0;
    double escapePenalty = 1000.0;
    double fastCaptureBonus = 100.0;
    double gpScoreScale = 1000.0;
};

struct SimulationResult {
    SimulationOutcome outcome = SimulationOutcome::Draw;
    int turns = 0;
};

struct FitnessReport {
    double averageFitness = 0.0;
    double averageTurns = 0.0;
    std::size_t captures = 0;
    std::size_t escapes = 0;
    std::size_t draws = 0;
    std::size_t games = 0;
};

std::vector<StartingScenario> makeDefaultScenarios(
    const char* maze[], std::size_t scenarioCount = 4);
Position chooseAttackerMove(const GameState& state, const char* maze[],
                            AttackerStrategy strategy);
SimulationResult simulateGame(const char* maze[], const GameState& initialState,
                              AttackerStrategy strategy,
                              int searchDepth = 2, int turnLimit = 60,
                              const StateEvaluator& defenderEvaluator = {});
FitnessReport evaluateFitness(const maze_gp::ExpressionTree& candidate,
                              const char* maze[],
                              const SimulationConfig& config = {});
FitnessReport evaluateBaselineFitness(const char* maze[],
                                      const SimulationConfig& config = {});
maze_gp::FitnessFunction makeFitnessCallback(const char* maze[],
                                             SimulationConfig config = {});
maze_gp::ExpressionTree trainDefenderTree(const char* maze[],
                                          SimulationConfig simulationConfig = {},
                                          maze_gp::EvolutionConfig evolutionConfig = {},
                                          unsigned randomSeed = 1);

#endif
