#ifndef MISCELLANEOUS_MAZE_GENETIC_PROGRAMMING_H
#define MISCELLANEOUS_MAZE_GENETIC_PROGRAMMING_H

#include <array>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <random>

namespace maze_gp {

inline constexpr unsigned TREE_FORMAT_VERSION = 1;
inline constexpr unsigned FEATURE_SCHEMA_VERSION = 1;

// Every feature is normalized to [0, 1]. Larger values are Defender-favorable,
// except VaultReached, which is a phase flag (0 = infiltrate, 1 = escape).
enum class Feature : std::size_t {
    AttackerObjectiveDistance, // 1 means far from the objective; 0 means at it.
    DefenderProximity,         // 1 means adjacent/capturing range; 0 means far.
    AttackerMobility,          // 1 means few legal exits; 0 means many.
    DefenderRestriction,       // 1 means the Defender contests most exits.
    VaultReached,              // phase indicator; not a quality score.
    Count
};

using FeatureVector = std::array<double, static_cast<std::size_t>(Feature::Count)>;

struct TreeLimits {
    unsigned maxDepth = 6;
    std::size_t maxNodes = 63;
};

enum class NodeKind { Constant, Feature, Add, Subtract, Multiply, Divide, Minimum, Maximum };

struct Node {
    NodeKind kind = NodeKind::Constant;
    double value = 0.0;
    Feature feature = Feature::AttackerObjectiveDistance;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    Node() = default;
    Node(const Node& other);
    Node& operator=(const Node& other);
    Node(Node&&) noexcept = default;
    Node& operator=(Node&&) noexcept = default;
};

class ExpressionTree {
public:
    ExpressionTree();
    explicit ExpressionTree(Node root, TreeLimits limits = {});
    ExpressionTree(const ExpressionTree& other);
    ExpressionTree& operator=(const ExpressionTree& other);
    ExpressionTree(ExpressionTree&&) noexcept = default;
    ExpressionTree& operator=(ExpressionTree&&) noexcept = default;

    double evaluate(const FeatureVector& features) const;
    std::size_t nodeCount() const;
    unsigned depth() const;
    const TreeLimits& limits() const;
    void save(std::ostream& output) const;
    static ExpressionTree load(std::istream& input);
    static ExpressionTree load(std::istream& input, TreeLimits maximumLimits);

    static ExpressionTree random(std::mt19937& rng, TreeLimits limits = {});
    static ExpressionTree crossover(const ExpressionTree& first,
                                    const ExpressionTree& second,
                                    std::mt19937& rng);
    void mutate(std::mt19937& rng);

private:
    Node root_;
    TreeLimits limits_;
};

struct EvolutionConfig {
    std::size_t populationSize = 40;
    unsigned generations = 20;
    std::size_t eliteCount = 2;
    std::size_t tournamentSize = 3;
    double mutationRate = 0.15;
    double crossoverRate = 0.80;
    TreeLimits treeLimits{};
};

using FitnessFunction = std::function<double(const ExpressionTree&)>;

ExpressionTree evolve(const FitnessFunction& fitness,
                      const EvolutionConfig& config = {},
                      unsigned seed = 1);

} // namespace maze_gp

#endif
