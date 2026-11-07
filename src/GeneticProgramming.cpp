#include "GeneticProgramming.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace maze_gp {
namespace {

constexpr double kMaxOutput = 1.0e6;
constexpr double kDivisionEpsilon = 1.0e-9;
constexpr double kDivisionFallback = 1.0;

std::unique_ptr<Node> clone(const Node* node) {
    return node ? std::make_unique<Node>(*node) : nullptr;
}

bool isOperator(NodeKind kind) {
    return kind >= NodeKind::Add && kind <= NodeKind::Maximum;
}

void validateNode(const Node& node) {
    if (node.kind == NodeKind::Constant || node.kind == NodeKind::Feature) {
        if (node.left || node.right) throw std::invalid_argument("Terminal node has children");
        if (node.kind == NodeKind::Feature && node.feature >= Feature::Count)
            throw std::invalid_argument("Invalid feature index");
        return;
    }
    if (!isOperator(node.kind) || !node.left || !node.right)
        throw std::invalid_argument("Operator node must have two children");
    validateNode(*node.left);
    validateNode(*node.right);
}

double safeValue(double value) {
    if (std::isnan(value)) return 0.0;
    if (value > kMaxOutput || value == std::numeric_limits<double>::infinity()) return kMaxOutput;
    if (value < -kMaxOutput || value == -std::numeric_limits<double>::infinity()) return -kMaxOutput;
    return value;
}

double eval(const Node& node, const FeatureVector& features) {
    if (node.kind == NodeKind::Constant) return safeValue(node.value);
    if (node.kind == NodeKind::Feature)
        return std::max(0.0, std::min(1.0,
            std::isfinite(features[static_cast<std::size_t>(node.feature)])
                ? features[static_cast<std::size_t>(node.feature)] : 0.0));

    const double left = eval(*node.left, features);
    const double right = eval(*node.right, features);
    double result = 0.0;
    switch (node.kind) {
        case NodeKind::Add: result = left + right; break;
        case NodeKind::Subtract: result = left - right; break;
        case NodeKind::Multiply: result = left * right; break;
        case NodeKind::Divide:
            result = std::abs(right) <= kDivisionEpsilon
                ? kDivisionFallback : left / right;
            break;
        case NodeKind::Minimum: result = std::min(left, right); break;
        case NodeKind::Maximum: result = std::max(left, right); break;
        default: break;
    }
    return safeValue(result);
}

std::size_t count(const Node& node) {
    return 1 + (node.left ? count(*node.left) : 0) +
               (node.right ? count(*node.right) : 0);
}

unsigned depthNode(const Node& node) {
    if (!node.left && !node.right) return 1;
    return 1 + std::max(node.left ? depthNode(*node.left) : 0,
                        node.right ? depthNode(*node.right) : 0);
}

void collect(Node& node, std::vector<Node*>& out) {
    out.push_back(&node);
    if (node.left) collect(*node.left, out);
    if (node.right) collect(*node.right, out);
}

void collect(const Node& node, std::vector<const Node*>& out) {
    out.push_back(&node);
    if (node.left) collect(*node.left, out);
    if (node.right) collect(*node.right, out);
}

Node randomNode(std::mt19937& rng, unsigned remainingDepth) {
    std::uniform_int_distribution<int> choice(0, remainingDepth == 1 ? 1 : 3);
    if (choice(rng) <= 1) {
        Node node;
        if (std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
            node.kind = NodeKind::Feature;
            node.feature = static_cast<Feature>(std::uniform_int_distribution<std::size_t>(
                0, static_cast<std::size_t>(Feature::Count) - 1)(rng));
        } else {
            node.kind = NodeKind::Constant;
            node.value = std::uniform_real_distribution<double>(-1.0, 1.0)(rng);
        }
        return node;
    }

    Node node;
    node.kind = static_cast<NodeKind>(std::uniform_int_distribution<int>(
        static_cast<int>(NodeKind::Add), static_cast<int>(NodeKind::Maximum))(rng));
    node.left = std::make_unique<Node>(randomNode(rng, remainingDepth - 1));
    node.right = std::make_unique<Node>(randomNode(rng, remainingDepth - 1));
    return node;
}

const ExpressionTree& tournament(const std::vector<ExpressionTree>& population,
                                const std::vector<double>& fitness,
                                std::size_t size, std::mt19937& rng) {
    std::size_t best = std::uniform_int_distribution<std::size_t>(0, population.size() - 1)(rng);
    for (std::size_t i = 1; i < size; ++i) {
        const std::size_t candidate = std::uniform_int_distribution<std::size_t>(0, population.size() - 1)(rng);
        if (fitness[candidate] > fitness[best]) best = candidate;
    }
    return population[best];
}

} // namespace

Node::Node(const Node& other)
    : kind(other.kind), value(other.value), feature(other.feature),
      left(clone(other.left.get())), right(clone(other.right.get())) {}

Node& Node::operator=(const Node& other) {
    if (this != &other) {
        kind = other.kind;
        value = other.value;
        feature = other.feature;
        left = clone(other.left.get());
        right = clone(other.right.get());
    }
    return *this;
}

ExpressionTree::ExpressionTree() = default;

ExpressionTree::ExpressionTree(Node root, TreeLimits limits)
    : root_(std::move(root)), limits_(limits) {
    validateNode(root_);
    if (limits_.maxDepth == 0 || limits_.maxNodes == 0 ||
        depth() > limits_.maxDepth || nodeCount() > limits_.maxNodes)
        throw std::invalid_argument("Expression exceeds tree limits");
}

ExpressionTree::ExpressionTree(const ExpressionTree& other)
    : root_(other.root_), limits_(other.limits_) {}

ExpressionTree& ExpressionTree::operator=(const ExpressionTree& other) {
    if (this != &other) {
        root_ = other.root_;
        limits_ = other.limits_;
    }
    return *this;
}

double ExpressionTree::evaluate(const FeatureVector& features) const { return eval(root_, features); }
std::size_t ExpressionTree::nodeCount() const { return count(root_); }
unsigned ExpressionTree::depth() const { return depthNode(root_); }
const TreeLimits& ExpressionTree::limits() const { return limits_; }

ExpressionTree ExpressionTree::random(std::mt19937& rng, TreeLimits limits) {
    if (limits.maxDepth == 0 || limits.maxNodes == 0)
        throw std::invalid_argument("Tree limits must be positive");
    // Full binary trees of depth d have at most 2^d-1 nodes; generate bounded
    // trees and retry if the independent node budget is tighter.
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        Node root = randomNode(rng, limits.maxDepth);
        if (count(root) <= limits.maxNodes) return ExpressionTree(std::move(root), limits);
    }
    Node terminal;
    terminal.kind = NodeKind::Constant;
    return ExpressionTree(std::move(terminal), limits);
}

ExpressionTree ExpressionTree::crossover(const ExpressionTree& first,
                                         const ExpressionTree& second,
                                         std::mt19937& rng) {
    std::vector<const Node*> sourceNodes;
    collect(second.root_, sourceNodes);
    std::vector<Node*> destinationNodes;
    ExpressionTree child(first);
    collect(child.root_, destinationNodes);
    std::vector<std::size_t> destinationOrder(destinationNodes.size());
    std::vector<std::size_t> sourceOrder(sourceNodes.size());
    for (std::size_t i = 0; i < destinationOrder.size(); ++i) destinationOrder[i] = i;
    for (std::size_t i = 0; i < sourceOrder.size(); ++i) sourceOrder[i] = i;
    std::shuffle(destinationOrder.begin(), destinationOrder.end(), rng);
    std::shuffle(sourceOrder.begin(), sourceOrder.end(), rng);

    for (std::size_t destinationIndex : destinationOrder) {
        for (std::size_t sourceIndex : sourceOrder) {
            Node candidate(child.root_);
            std::vector<Node*> candidates;
            collect(candidate, candidates);
            *candidates[destinationIndex] = *sourceNodes[sourceIndex];
            if (depthNode(candidate) <= child.limits_.maxDepth &&
                count(candidate) <= child.limits_.maxNodes) {
                child.root_ = std::move(candidate);
                return child;
            }
        }
    }
    return first;
}

void ExpressionTree::mutate(std::mt19937& rng) {
    std::vector<Node*> nodes;
    collect(root_, nodes);
    std::vector<std::size_t> order(nodes.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::shuffle(order.begin(), order.end(), rng);
    for (std::size_t targetIndex : order) {
        for (unsigned candidateDepth = 1; candidateDepth <= limits_.maxDepth; ++candidateDepth) {
            Node replacement = randomNode(rng, candidateDepth);
            Node candidate(root_);
            std::vector<Node*> candidateNodes;
            collect(candidate, candidateNodes);
            *candidateNodes[targetIndex] = std::move(replacement);
            if (depthNode(candidate) <= limits_.maxDepth && count(candidate) <= limits_.maxNodes) {
                root_ = std::move(candidate);
                return;
            }
        }
    }
}

ExpressionTree evolve(const FitnessFunction& fitness, const EvolutionConfig& config,
                      unsigned seed) {
    if (!fitness || config.populationSize == 0 || config.treeLimits.maxDepth == 0 ||
        config.treeLimits.maxNodes == 0 || config.eliteCount > config.populationSize ||
        config.tournamentSize == 0 || config.mutationRate < 0.0 || config.mutationRate > 1.0 ||
        config.crossoverRate < 0.0 || config.crossoverRate > 1.0)
        throw std::invalid_argument("Invalid genetic programming configuration");

    std::mt19937 rng(seed);
    std::vector<ExpressionTree> population;
    population.reserve(config.populationSize);
    for (std::size_t i = 0; i < config.populationSize; ++i)
        population.push_back(ExpressionTree::random(rng, config.treeLimits));

    std::vector<double> scores(population.size());
    for (unsigned generation = 0; generation <= config.generations; ++generation) {
        for (std::size_t i = 0; i < population.size(); ++i) {
            const double value = fitness(population[i]);
            scores[i] = std::isfinite(value) ? value : -std::numeric_limits<double>::infinity();
        }
        if (generation == config.generations) break;

        std::vector<std::size_t> order(population.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return scores[a] > scores[b];
        });

        std::vector<ExpressionTree> next;
        next.reserve(config.populationSize);
        for (std::size_t i = 0; i < config.eliteCount; ++i) next.push_back(population[order[i]]);
        while (next.size() < config.populationSize) {
            ExpressionTree child = tournament(population, scores,
                std::min(config.tournamentSize, population.size()), rng);
            if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < config.crossoverRate) {
                child = ExpressionTree::crossover(child,
                    tournament(population, scores,
                        std::min(config.tournamentSize, population.size()), rng), rng);
            }
            if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < config.mutationRate)
                child.mutate(rng);
            next.push_back(std::move(child));
        }
        population = std::move(next);
    }

    const auto best = std::max_element(scores.begin(), scores.end());
    return population[static_cast<std::size_t>(best - scores.begin())];
}

} // namespace maze_gp
