#include "../src/GameSimulation.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>

namespace {

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

GameState makeState(Position attacker, Position defender, Position vault,
                    Position escape, AttackerMode mode = INFILTRATE) {
    GameState state{};
    state.attacker = attacker;
    state.defender = defender;
    state.vault = vault;
    state.escapePoint = escape;
    state.mode = mode;
    state.vaultReached = mode == ESCAPE;
    return state;
}

maze_gp::ExpressionTree constantTree(double value) {
    maze_gp::Node node;
    node.kind = maze_gp::NodeKind::Constant;
    node.value = value;
    return maze_gp::ExpressionTree(std::move(node));
}

} // namespace

int main() {
    const auto defaults = makeDefaultScenarios(maze, 4);
    assert(defaults.size() == 4);
    assert(defaults[0].initialState.mode == INFILTRATE);
    assert(defaults[2].initialState.mode == ESCAPE);
    std::cout << "PASS deterministic Vault and Escape scenarios\n";

    GameState captureState = makeState({3, 3}, {3, 4}, {3, 4}, {7, 16});
    Position shortest = chooseAttackerMove(captureState, maze, AttackerStrategy::ShortestPath);
    assert(shortest.row == 3 && shortest.col == 4);
    const auto capture = simulateGame(maze, captureState,
        AttackerStrategy::ShortestPath, 1, 10);
    assert(capture.outcome == SimulationOutcome::DefenderCapture);
    assert(capture.turns == 1);
    std::cout << "PASS capture uses shared applyMove rules\n";

    GameState escapeState = makeState({3, 3}, {6, 6}, {2, 2}, {3, 4}, ESCAPE);
    const auto escape = simulateGame(maze, escapeState,
        AttackerStrategy::ShortestPath, 1, 10);
    assert(escape.outcome == SimulationOutcome::AttackerEscape);
    assert(escape.turns == 1);
    std::cout << "PASS Escape uses shared applyMove rules\n";

    GameState drawState = makeState({1, 1}, {6, 7}, {4, 12}, {7, 16});
    const auto draw = simulateGame(maze, drawState,
        AttackerStrategy::ShortestPath, 1, 1);
    assert(draw.outcome == SimulationOutcome::Draw && draw.turns == 1);
    std::cout << "PASS explicit turn-limit draw\n";

    const Position shortestAgain = chooseAttackerMove(
        drawState, maze, AttackerStrategy::ShortestPath);
    assert(shortest.row == 3 && shortest.col == 4);
    const Position evasiveA = chooseAttackerMove(
        drawState, maze, AttackerStrategy::Evasive);
    const Position evasiveB = chooseAttackerMove(
        drawState, maze, AttackerStrategy::Evasive);
    assert(shortestAgain.row == 2 && shortestAgain.col == 1);
    assert(evasiveA.row == evasiveB.row && evasiveA.col == evasiveB.col);
    std::cout << "PASS deterministic shortest and evasive strategies\n";

    SimulationConfig captureConfig;
    captureConfig.turnLimit = 10;
    captureConfig.searchDepth = 1;
    captureConfig.scenarios = {{"capture-score", captureState}};
    captureConfig.attackerStrategies = {AttackerStrategy::ShortestPath};
    const auto captureFitness = evaluateFitness(constantTree(0.25), maze, captureConfig);
    assert(captureFitness.games == 1 && captureFitness.captures == 1);
    assert(captureFitness.escapes == 0 && captureFitness.draws == 0);
    assert(std::abs(captureFitness.averageFitness - 1090.0) < 1e-9);
    assert(captureFitness.averageTurns == 1.0);
    std::cout << "PASS capture formula and report: fitness="
              << captureFitness.averageFitness << ", captures="
              << captureFitness.captures << ", turns="
              << captureFitness.averageTurns << "\n";

    SimulationConfig escapeConfig;
    escapeConfig.turnLimit = 5;
    escapeConfig.searchDepth = 1;
    escapeConfig.scenarios = {{"escape-score", escapeState}};
    escapeConfig.attackerStrategies = {AttackerStrategy::ShortestPath};
    const auto escapeFitness = evaluateFitness(constantTree(0.0), maze, escapeConfig);
    assert(escapeFitness.escapes == 1 && escapeFitness.averageFitness == -1000.0);
    std::cout << "PASS escape penalty\n";

    const auto callback = makeFitnessCallback(maze, captureConfig);
    const auto candidateA = constantTree(0.0);
    const auto candidateB = constantTree(1.0);
    assert(callback(candidateA) == callback(candidateA));
    assert(callback(candidateB) == callback(candidateB));
    assert(callback(candidateA) == captureFitness.averageFitness);
    std::cout << "PASS stable shared scenarios and GP callback\n";

    maze_gp::EvolutionConfig evolutionConfig;
    evolutionConfig.populationSize = 4;
    evolutionConfig.generations = 1;
    evolutionConfig.eliteCount = 1;
    evolutionConfig.treeLimits = {3, 7};
    const auto trained = trainDefenderTree(maze, captureConfig, evolutionConfig, 9);
    assert(trained.depth() <= evolutionConfig.treeLimits.maxDepth);
    assert(trained.nodeCount() <= evolutionConfig.treeLimits.maxNodes);
    std::cout << "PASS explicit GP evolution using simulation fitness\n";

    const auto baseline = evaluateBaselineFitness(maze, captureConfig);
    assert(baseline.captures == 1 && baseline.averageFitness == 1090.0);
    std::cout << "PASS original heuristic baseline; fitness="
              << baseline.averageFitness << "\n";

    SimulationConfig sampleConfig;
    sampleConfig.turnLimit = 20;
    sampleConfig.searchDepth = 1;
    sampleConfig.scenarioCount = 4;
    const auto sample = evaluateFitness(constantTree(0.0), maze, sampleConfig);
    assert(sample.games == 8);
    assert(sample.captures + sample.escapes + sample.draws == sample.games);
    std::cout << "Sample 4-scenario GP fitness: " << sample.averageFitness
              << ", captures=" << sample.captures
              << ", escapes=" << sample.escapes
              << ", draws=" << sample.draws
              << ", average plies=" << sample.averageTurns << "\n";
    std::cout << "All GameSimulation tests passed.\n";
}
