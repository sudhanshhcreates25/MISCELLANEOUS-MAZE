#include "../src/GeneticProgramming.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace maze_gp;

namespace {

Node terminal(double value) {
    Node node;
    node.kind = NodeKind::Constant;
    node.value = value;
    return node;
}

Node featureNode(Feature feature) {
    Node node;
    node.kind = NodeKind::Feature;
    node.feature = feature;
    return node;
}

Node operation(NodeKind kind, Node lhs, Node rhs) {
    Node node;
    node.kind = kind;
    node.left = std::make_unique<Node>(std::move(lhs));
    node.right = std::make_unique<Node>(std::move(rhs));
    return node;
}

} // namespace

int main() {
    const FeatureVector features{{0.2, 0.4, 0.6, 0.8, 1.0}};

    ExpressionTree feature(featureNode(Feature::AttackerMobility));
    assert(std::abs(feature.evaluate(features) - 0.6) < 1e-12);

    assert(std::abs(ExpressionTree(operation(NodeKind::Add, terminal(2), terminal(3))).evaluate(features) - 5) < 1e-12);
    assert(std::abs(ExpressionTree(operation(NodeKind::Subtract, terminal(7), terminal(4))).evaluate(features) - 3) < 1e-12);
    assert(std::abs(ExpressionTree(operation(NodeKind::Multiply, terminal(3), terminal(4))).evaluate(features) - 12) < 1e-12);
    assert(std::abs(ExpressionTree(operation(NodeKind::Divide, terminal(9), terminal(3))).evaluate(features) - 3) < 1e-12);
    assert(std::abs(ExpressionTree(operation(NodeKind::Minimum, terminal(9), terminal(3))).evaluate(features) - 3) < 1e-12);
    assert(std::abs(ExpressionTree(operation(NodeKind::Maximum, terminal(9), terminal(3))).evaluate(features) - 9) < 1e-12);
    std::cout << "PASS known trees and all operators\n";

    const double protectedZero = ExpressionTree(operation(NodeKind::Divide, terminal(9), terminal(0))).evaluate(features);
    const double protectedNearZero = ExpressionTree(operation(NodeKind::Divide, terminal(9), terminal(1e-12))).evaluate(features);
    assert(protectedZero == 1.0 && protectedNearZero == 1.0);
    std::cout << "PASS protected division for zero and near-zero denominators\n";

    ExpressionTree original(operation(NodeKind::Add, terminal(2), terminal(4)));
    ExpressionTree copied = original;
    std::mt19937 copyMutationRng(5);
    copied.mutate(copyMutationRng);
    assert(std::abs(original.evaluate(features) - 6.0) < 1e-12);
    std::cout << "PASS deep-copy independence\n";

    TreeLimits tight{4, 9};
    std::mt19937 rng(12345);
    for (int i = 0; i < 100; ++i) {
        ExpressionTree generated = ExpressionTree::random(rng, tight);
        assert(generated.depth() <= tight.maxDepth);
        assert(generated.nodeCount() <= tight.maxNodes);
    }
    std::cout << "PASS random generation limits\n";

    ExpressionTree parentA = ExpressionTree::random(rng, tight);
    ExpressionTree parentB = ExpressionTree::random(rng, tight);
    ExpressionTree child = ExpressionTree::crossover(parentA, parentB, rng);
    assert(child.depth() <= tight.maxDepth && child.nodeCount() <= tight.maxNodes);
    child.mutate(rng);
    assert(child.depth() <= tight.maxDepth && child.nodeCount() <= tight.maxNodes);
    std::cout << "PASS crossover and mutation limits\n";

    std::mt19937 rngA(88), rngB(88);
    ExpressionTree repeatA = ExpressionTree::random(rngA, tight);
    ExpressionTree repeatB = ExpressionTree::random(rngB, tight);
    assert(repeatA.depth() == repeatB.depth());
    assert(repeatA.nodeCount() == repeatB.nodeCount());
    assert(repeatA.evaluate(features) == repeatB.evaluate(features));
    std::cout << "PASS fixed-seed reproducibility\n";

    ExpressionTree extreme(operation(NodeKind::Multiply,
        terminal(std::numeric_limits<double>::infinity()),
        terminal(std::numeric_limits<double>::infinity())));
    const double output = extreme.evaluate(features);
    assert(std::isfinite(output));
    assert(std::abs(output) <= 1.0e6);
    std::cout << "PASS finite and bounded evaluation output\n";

    std::stringstream serialized;
    original.save(serialized);
    ExpressionTree loaded = ExpressionTree::load(serialized);
    assert(loaded.nodeCount() == original.nodeCount());
    assert(loaded.depth() == original.depth());
    assert(loaded.evaluate(features) == original.evaluate(features));
    std::cout << "PASS versioned tree serialization round trip\n";

    const char* invalidTrees[] = {
        "MAZE_GP_TREE 9\nFEATURE_SCHEMA 1\nLIMITS 3 7\nTREE\nCONSTANT 1\nEND\n",
        "MAZE_GP_TREE 1\nFEATURE_SCHEMA 9\nLIMITS 3 7\nTREE\nCONSTANT 1\nEND\n",
        "MAZE_GP_TREE 1\nFEATURE_SCHEMA 1\nLIMITS 3 7\nTREE\nMYSTERY\nEND\n",
        "MAZE_GP_TREE 1\nFEATURE_SCHEMA 1\nLIMITS 3 7\nTREE\nFEATURE 99\nEND\n",
        "MAZE_GP_TREE 1\nFEATURE_SCHEMA 1\nLIMITS 3 7\nTREE\nCONSTANT nan\nEND\n",
        "MAZE_GP_TREE 1\nFEATURE_SCHEMA 1\nLIMITS 1 1\nTREE\nADD\nCONSTANT 1\nCONSTANT 2\nEND\n"
    };
    for (const char* input : invalidTrees) {
        bool rejected = false;
        try {
            std::stringstream bad(input);
            (void)ExpressionTree::load(bad);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }
    std::cout << "PASS malformed, incompatible and over-limit tree rejection\n";

    EvolutionConfig config;
    config.populationSize = 12;
    config.generations = 5;
    config.eliteCount = 2;
    config.treeLimits = tight;
    const auto fitness = [&](const ExpressionTree& tree) {
        return -std::abs(tree.evaluate(features) - 0.75);
    };
    ExpressionTree evolvedA = evolve(fitness, config, 777);
    ExpressionTree evolvedB = evolve(fitness, config, 777);
    assert(evolvedA.depth() <= tight.maxDepth && evolvedA.nodeCount() <= tight.maxNodes);
    assert(evolvedA.evaluate(features) == evolvedB.evaluate(features));
    std::cout << "PASS evolution, elitism configuration, and seeded repeatability\n";
    std::cout << "All Genetic Programming core tests passed.\n";
}
