#include "TrainingExperiment.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

bool samePosition(Position a, Position b) {
    return a.row == b.row && a.col == b.col;
}

bool sameScenario(const GameState& a, const GameState& b) {
    return samePosition(a.attacker, b.attacker) &&
           samePosition(a.defender, b.defender) &&
           samePosition(a.vault, b.vault) &&
           samePosition(a.escapePoint, b.escapePoint) &&
           a.mode == b.mode && a.vaultReached == b.vaultReached;
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        if (ch == '"' || ch == '\\') out << '\\' << static_cast<char>(ch);
        else if (ch == '\n') out << "\\n";
        else if (ch < 0x20) out << '?';
        else out << static_cast<char>(ch);
    }
    return out.str();
}

void writeMetrics(std::ostream& out, const char* label, const FitnessReport& m) {
    out << "  \"" << label << "\": {\n"
        << "    \"total_games\": " << m.games << ",\n"
        << "    \"captures\": " << m.captures << ",\n"
        << "    \"capture_rate\": " << (m.games ? static_cast<double>(m.captures) / m.games : 0.0) << ",\n"
        << "    \"escapes\": " << m.escapes << ",\n"
        << "    \"escape_rate\": " << (m.games ? static_cast<double>(m.escapes) / m.games : 0.0) << ",\n"
        << "    \"draws\": " << m.draws << ",\n"
        << "    \"draw_rate\": " << (m.games ? static_cast<double>(m.draws) / m.games : 0.0) << ",\n"
        << "    \"average_turns\": " << m.averageTurns << ",\n"
        << "    \"average_fitness\": " << m.averageFitness << "\n"
        << "  }";
}

void writeScenarioList(std::ostream& out,
                       const std::vector<StartingScenario>& scenarios) {
    out << '[';
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        if (i) out << ", ";
        const auto& state = scenarios[i].initialState;
        out << "{\"name\": \"" << jsonEscape(scenarios[i].name)
            << "\", \"mode\": \"" << (state.mode == INFILTRATE ? "infiltrate" : "escape")
            << "\", \"attacker\": [" << state.attacker.row << ", " << state.attacker.col
            << "], \"defender\": [" << state.defender.row << ", " << state.defender.col
            << "], \"vault\": [" << state.vault.row << ", " << state.vault.col
            << "], \"escape\": [" << state.escapePoint.row << ", " << state.escapePoint.col
            << "]}";
    }
    out << ']';
}

std::string createRunDirectory(const std::string& root, unsigned seed) {
    namespace fs = std::filesystem;
    fs::create_directories(root);
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    const std::string base = "experiment_seed" + std::to_string(seed) + "_" + std::to_string(stamp);
    for (unsigned suffix = 0; suffix < 10000; ++suffix) {
        const std::string name = base + (suffix == 0 ? "" : "_" + std::to_string(suffix));
        const fs::path candidate = fs::path(root) / name;
        std::error_code error;
        if (fs::create_directory(candidate, error)) return candidate.string();
        if (error) throw fs::filesystem_error("Cannot create experiment result directory", candidate, error);
    }
    throw std::runtime_error("Unable to allocate a unique experiment result directory");
}

void saveExperiment(const ExperimentReport& report, const ExperimentConfig& config) {
    namespace fs = std::filesystem;
    const fs::path directory(report.resultsDirectory);
    {
        std::ofstream out(directory / "training_config.json");
        if (!out) throw std::runtime_error("Cannot write training configuration");
        out << std::setprecision(12)
            << "{\n  \"random_seed\": " << config.randomSeed << ",\n"
            << "  \"population_size\": " << config.evolution.populationSize << ",\n"
            << "  \"generations\": " << config.evolution.generations << ",\n"
            << "  \"elite_count\": " << config.evolution.eliteCount << ",\n"
            << "  \"tournament_size\": " << config.evolution.tournamentSize << ",\n"
            << "  \"mutation_rate\": " << config.evolution.mutationRate << ",\n"
            << "  \"crossover_rate\": " << config.evolution.crossoverRate << ",\n"
            << "  \"max_tree_depth\": " << config.evolution.treeLimits.maxDepth << ",\n"
            << "  \"max_tree_nodes\": " << config.evolution.treeLimits.maxNodes << ",\n"
            << "  \"feature_schema_version\": " << maze_gp::FEATURE_SCHEMA_VERSION << ",\n"
            << "  \"minimax_search_depth\": " << config.simulation.searchDepth << ",\n"
            << "  \"turn_limit_plies\": " << config.simulation.turnLimit << ",\n"
            << "  \"capture_reward\": " << config.simulation.captureReward << ",\n"
            << "  \"escape_penalty\": " << config.simulation.escapePenalty << ",\n"
            << "  \"fast_capture_bonus\": " << config.simulation.fastCaptureBonus << ",\n"
            << "  \"gp_score_scale\": " << config.simulation.gpScoreScale << ",\n"
            << "  \"attacker_strategies\": [";
        for (std::size_t i = 0; i < config.simulation.attackerStrategies.size(); ++i) {
            if (i) out << ", ";
            out << '"' << (config.simulation.attackerStrategies[i] == AttackerStrategy::ShortestPath
                ? "shortest_path" : "evasive") << '"';
        }
        out << "],\n  \"training_scenarios\": ";
        writeScenarioList(out, report.trainingScenarios);
        out << ",\n  \"held_out_scenarios\": ";
        writeScenarioList(out, report.heldOutScenarios);
        out << "\n}\n";
        if (!out) throw std::runtime_error("Failed writing training configuration");
    }
    {
        std::ofstream out(directory / "best_tree.gp");
        if (!out) throw std::runtime_error("Cannot write evolved tree");
        report.bestTree.save(out);
    }
    {
        std::ofstream out(directory / "metrics.json");
        if (!out) throw std::runtime_error("Cannot write experiment metrics");
        out << std::setprecision(12) << "{\n";
        writeMetrics(out, "training_baseline", report.trainingBaseline);
        out << ",\n";
        writeMetrics(out, "training_evolved", report.trainingEvolved);
        out << ",\n";
        writeMetrics(out, "held_out_baseline", report.heldOutBaseline);
        out << ",\n";
        writeMetrics(out, "held_out_evolved", report.heldOutEvolved);
        out << "\n}\n";
        if (!out) throw std::runtime_error("Failed writing experiment metrics");
    }
}

} // namespace

std::vector<StartingScenario> makeDefaultHeldOutScenarios(const char* maze[]) {
    const StartingScenario heldOut[] = {
        {"heldout-infiltrate-north", {{1, 3}, {6, 11}, {4, 12}, {7, 16}, INFILTRATE, false, false, false}},
        {"heldout-infiltrate-east", {{2, 15}, {5, 4}, {4, 12}, {7, 16}, INFILTRATE, false, false, false}},
        {"heldout-escape-west", {{5, 10}, {2, 14}, {4, 12}, {7, 16}, ESCAPE, true, false, false}},
        {"heldout-escape-south", {{6, 15}, {2, 4}, {4, 12}, {7, 16}, ESCAPE, true, false, false}}
    };
    std::vector<StartingScenario> result(std::begin(heldOut), std::end(heldOut));
    for (const auto& scenario : result) {
        const GameState& state = scenario.initialState;
        if (!isWalkable(maze, state.attacker.row, state.attacker.col) ||
            !isWalkable(maze, state.defender.row, state.defender.col) ||
            !isWalkable(maze, state.vault.row, state.vault.col) ||
            !isWalkable(maze, state.escapePoint.row, state.escapePoint.col) ||
            isCaptured(state))
            throw std::invalid_argument("Invalid held-out scenario: " + scenario.name);
    }
    return result;
}

ExperimentReport runTrainingExperiment(
    const char* maze[], const ExperimentConfig& config,
    const std::vector<StartingScenario>& heldOutScenarios) {
    if (heldOutScenarios.empty()) throw std::invalid_argument("Held-out scenarios are required");

    ExperimentReport report;
    report.trainingScenarios = config.simulation.scenarios.empty()
        ? makeDefaultScenarios(maze, config.simulation.scenarioCount)
        : config.simulation.scenarios;
    report.heldOutScenarios = heldOutScenarios;
    for (const auto& training : report.trainingScenarios) {
        for (const auto& heldOut : report.heldOutScenarios) {
            if (sameScenario(training.initialState, heldOut.initialState))
                throw std::invalid_argument("Training and held-out scenarios overlap");
        }
    }

    SimulationConfig trainingConfig = config.simulation;
    trainingConfig.scenarios = report.trainingScenarios;
    report.bestTree = trainDefenderTree(maze, trainingConfig, config.evolution,
                                        config.randomSeed);
    report.trainingBaseline = evaluateBaselineFitness(maze, trainingConfig);
    report.trainingEvolved = evaluateFitness(report.bestTree, maze, trainingConfig);

    SimulationConfig heldOutConfig = trainingConfig;
    heldOutConfig.scenarios = report.heldOutScenarios;
    heldOutConfig.scenarioCount = report.heldOutScenarios.size();
    report.heldOutBaseline = evaluateBaselineFitness(maze, heldOutConfig);
    report.heldOutEvolved = evaluateFitness(report.bestTree, maze, heldOutConfig);

    report.resultsDirectory = createRunDirectory(config.resultsRoot, config.randomSeed);
    saveExperiment(report, config);
    return report;
}
