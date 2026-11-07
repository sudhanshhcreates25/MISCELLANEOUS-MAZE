#ifndef MISCELLANEOUS_MAZE_TRAINING_EXPERIMENT_H
#define MISCELLANEOUS_MAZE_TRAINING_EXPERIMENT_H

#include "GameSimulation.h"

#include <string>
#include <vector>

struct ExperimentConfig {
    maze_gp::EvolutionConfig evolution;
    SimulationConfig simulation;
    unsigned randomSeed = 1;
    std::string resultsRoot = "results";
};

struct ExperimentReport {
    maze_gp::ExpressionTree bestTree;
    FitnessReport trainingBaseline;
    FitnessReport trainingEvolved;
    FitnessReport heldOutBaseline;
    FitnessReport heldOutEvolved;
    std::vector<StartingScenario> trainingScenarios;
    std::vector<StartingScenario> heldOutScenarios;
    std::string resultsDirectory;
};

std::vector<StartingScenario> makeDefaultHeldOutScenarios(const char* maze[]);
ExperimentReport runTrainingExperiment(
    const char* maze[], const ExperimentConfig& config,
    const std::vector<StartingScenario>& heldOutScenarios);

#endif
