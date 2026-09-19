#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>
#include <string>

import node_base;
import processor_graph;
import fuzz;
import primitives;

namespace {

bool hasEdge(const std::string& json, const std::string& edge)
{
    return json.find(edge) != std::string::npos;
}

} // namespace

TEST_CASE("connect records an edge between registered nodes", "[registry]")
{
    ProcessorGraph graph(1);
    Fuzz a(graph, 1);
    Gain b(graph, 1);
    int ia = graph.registerNode("Fuzz", &a);
    int ib = graph.registerNode("Gain", &b);
    REQUIRE(ia != ib);

    graph.connect(&a, &b);
    graph.connectToOutput(&b);

    std::string json = graph.serializeToJson();
    REQUIRE(hasEdge(json, "\"from\": " + std::to_string(ia) + ", \"to\": " + std::to_string(ib)));
    REQUIRE(hasEdge(json, "\"from\": " + std::to_string(ib) + ", \"to\": \"output\""));
}

TEST_CASE("a new edge from a node replaces its old one", "[registry]")
{
    ProcessorGraph graph(1);
    Fuzz a(graph, 1);
    Gain b(graph, 1), c(graph, 1);
    int ia = graph.registerNode("Fuzz", &a);
    int ib = graph.registerNode("Gain", &b);
    int ic = graph.registerNode("Gain", &c);

    graph.connect(&a, &b);
    graph.connect(&a, &c);

    std::string json = graph.serializeToJson();
    REQUIRE_FALSE(hasEdge(json, "\"from\": " + std::to_string(ia) + ", \"to\": " + std::to_string(ib)));
    REQUIRE(hasEdge(json, "\"from\": " + std::to_string(ia) + ", \"to\": " + std::to_string(ic)));
}

TEST_CASE("edges to unregistered nodes are not recorded", "[registry]")
{
    ProcessorGraph graph(1);
    Fuzz a(graph, 1);
    Gain b(graph, 1);
    graph.registerNode("Fuzz", &a);

    graph.connect(&a, &b);
    REQUIRE_FALSE(hasEdge(graph.serializeToJson(), "\"from\""));
}

TEST_CASE("postDisconnect and postRemove drop registry entries", "[registry]")
{
    ProcessorGraph graph(1);
    Fuzz a(graph, 1);
    Gain b(graph, 1);
    graph.registerNode("Fuzz", &a);
    graph.registerNode("Gain", &b);
    REQUIRE(graph.postConnect(&a, &b));
    REQUIRE(hasEdge(graph.serializeToJson(), "\"from\""));

    REQUIRE(graph.postDisconnect(&a));
    REQUIRE_FALSE(hasEdge(graph.serializeToJson(), "\"from\""));

    REQUIRE(graph.postRemove(&b));
    REQUIRE(graph.findNodeId(&b) == -1);
    REQUIRE(graph.getRegisteredNodes().size() == 1);

    float buf[8];
    graph.read(buf, 8);
    NodeBase* collected = nullptr;
    graph.collectGarbage([&](NodeBase* n) { collected = n; });
    REQUIRE(collected == &b);
}

TEST_CASE("destroying a registered node unregisters it", "[registry]")
{
    ProcessorGraph graph(1);
    auto a = std::make_unique<Fuzz>(graph, 1);
    graph.registerNode("Fuzz", a.get());
    REQUIRE(graph.getRegisteredNodes().size() == 1);

    a.reset();
    REQUIRE(graph.getRegisteredNodes().empty());
}
