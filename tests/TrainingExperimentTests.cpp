#include "../src/TrainingExperiment.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

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

bool sameScenario(const GameState& a, const GameState& b) {
    return a.attacker.row == b.attacker.row && a.attacker.col == b.attacker.col &&
           a.defender.row == b.defender.row && a.defender.col == b.defender.col &&
           a.mode == b.mode;
}

} // namespace

int main() {
    ExperimentConfig config;
    config.evolution.populationSize = 4;
    config.evolution.generations = 1;
    config.evolution.eliteCount = 1;
    config.evolution.treeLimits = {3, 7};
    config.simulation.scenarioCount = 2;
    config.simulation.searchDepth = 1;
    config.simulation.turnLimit = 4;
    config.simulation.attackerStrategies = {AttackerStrategy::ShortestPath};
    config.randomSeed = 314159;
    config.resultsRoot = "/tmp/maze-phase4c-results";

    const auto heldOut = makeDefaultHeldOutScenarios(maze);
    const ExperimentReport first = runTrainingExperiment(maze, config, heldOut);
    const ExperimentReport second = runTrainingExperiment(maze, config, heldOut);

    assert(first.trainingScenarios.size() == 2);
    assert(first.heldOutScenarios.size() == heldOut.size());
    for (const auto& train : first.trainingScenarios)
        for (const auto& test : first.heldOutScenarios)
            assert(!sameScenario(train.initialState, test.initialState));
    assert(first.trainingBaseline.games == first.trainingEvolved.games);
    assert(first.heldOutBaseline.games == first.heldOutEvolved.games);
    assert(std::abs(first.trainingEvolved.averageFitness -
                    second.trainingEvolved.averageFitness) < 1e-12);
    assert(std::abs(first.heldOutEvolved.averageFitness -
                    second.heldOutEvolved.averageFitness) < 1e-12);
    assert(first.bestTree.evaluate({{0.1, 0.2, 0.3, 0.4, 0.0}}) ==
           second.bestTree.evaluate({{0.1, 0.2, 0.3, 0.4, 0.0}}));
    assert(first.resultsDirectory != second.resultsDirectory);

    const std::filesystem::path runPath(first.resultsDirectory);
    assert(std::filesystem::exists(runPath / "training_config.json"));
    assert(std::filesystem::exists(runPath / "best_tree.gp"));
    assert(std::filesystem::exists(runPath / "metrics.json"));
    std::ifstream treeFile(runPath / "best_tree.gp");
    const auto loadedTree = maze_gp::ExpressionTree::load(treeFile);
    assert(loadedTree.nodeCount() == first.bestTree.nodeCount());

    std::cout << "PASS fixed-seed training reproducibility\n";
    std::cout << "PASS training/held-out scenario separation\n";
    std::cout << "PASS baseline and evolved evaluation use matching game counts\n";
    std::cout << "PASS unique experiment outputs and persisted tree reload\n";
    std::cout << "Train baseline fitness=" << first.trainingBaseline.averageFitness
              << ", captures=" << first.trainingBaseline.captures
              << ", escapes=" << first.trainingBaseline.escapes
              << ", draws=" << first.trainingBaseline.draws << '\n';
    std::cout << "Train evolved fitness=" << first.trainingEvolved.averageFitness
              << ", captures=" << first.trainingEvolved.captures
              << ", escapes=" << first.trainingEvolved.escapes
              << ", draws=" << first.trainingEvolved.draws << '\n';
    std::cout << "Held-out baseline fitness=" << first.heldOutBaseline.averageFitness
              << ", captures=" << first.heldOutBaseline.captures
              << ", escapes=" << first.heldOutBaseline.escapes
              << ", draws=" << first.heldOutBaseline.draws << '\n';
    std::cout << "Held-out evolved fitness=" << first.heldOutEvolved.averageFitness
              << ", captures=" << first.heldOutEvolved.captures
              << ", escapes=" << first.heldOutEvolved.escapes
              << ", draws=" << first.heldOutEvolved.draws << '\n';
    std::cout << "Results: " << first.resultsDirectory << '\n';
}
