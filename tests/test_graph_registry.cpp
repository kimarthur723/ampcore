#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <memory>
#include <string>

import node_base;
import processor_graph;
import fuzz;
import primitives;

namespace {

NodeBase* factory(const std::string& type, ProcessorGraph& graph, void*)
{
    if (type == "Fuzz") return new Fuzz(graph, 1);
    if (type == "Gain") return new Gain(graph, 1);
    if (type == "Biquad") return new Biquad(graph, 1, 44100);
    return nullptr;
}

void destroy(NodeBase* node, void*) { delete node; }

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

TEST_CASE("a node may outlive its graph", "[registry]")
{
    auto graph = std::make_unique<ProcessorGraph>(1);
    auto a = std::make_unique<Fuzz>(*graph, 1);
    graph->registerNode("Fuzz", a.get());

    graph.reset();
    a.reset(); // must not call back into the freed graph
}

TEST_CASE("preset round-trips nodes, params, edges and biquad mode", "[preset]")
{
    ProcessorGraph src(1);
    Fuzz fuzz(src, 1, 7.0f, 0.4f);
    Biquad bq(src, 1, 44100, Biquad::Mode::HighShelf, 3000.0f, 1.2f, 4.5f);
    src.registerNode("Fuzz", &fuzz);
    src.registerNode("Biquad", &bq);
    src.connect(&fuzz, &bq);
    src.connectToOutput(&bq);
    std::string json = src.serializeToJson();

    ProcessorGraph dst(1);
    REQUIRE(dst.loadFromJson(json, factory, destroy));

    const auto& nodes = dst.getRegisteredNodes();
    REQUIRE(nodes.size() == 2);
    REQUIRE(nodes[0].typeName == "Fuzz");
    REQUIRE(nodes[0].node->getParameterValue(0) == Catch::Approx(7.0f));
    REQUIRE(nodes[1].typeName == "Biquad");
    REQUIRE(nodes[1].node->getParameterValue(0) == Catch::Approx(3000.0f));
    REQUIRE(nodes[1].node->getParameterValue(3) == Catch::Approx(static_cast<float>(Biquad::Mode::HighShelf)));
    REQUIRE(dst.serializeToJson() == json);

    for (const auto& rn : dst.getRegisteredNodes()) destroy(rn.node, nullptr);
}

TEST_CASE("loading a preset replaces registered nodes and leaves others alone", "[preset]")
{
    ProcessorGraph graph(1);
    Gain host(graph, 1);
    auto* old = new Fuzz(graph, 1);
    graph.registerNode("Fuzz", old);
    graph.connect(&host, old);

    int destroyed = 0;
    auto counting = [](NodeBase* n, void* user) { ++*static_cast<int*>(user); delete n; };
    REQUIRE(graph.loadFromJson(R"({"nodes":[{"id":5,"type":"Gain"}],"connections":[]})",
                               factory, counting, &destroyed));

    REQUIRE(destroyed == 1);
    REQUIRE(graph.getRegisteredNodes().size() == 1);
    REQUIRE(graph.getRegisteredNodes()[0].id == 5);
    REQUIRE(graph.findNodeId(&host) == -1);

    delete graph.getRegisteredNodes()[0].node;
}

TEST_CASE("loading fails without side effects on unknown type or pending commands", "[preset]")
{
    ProcessorGraph graph(1);
    auto* keep = new Fuzz(graph, 1);
    graph.registerNode("Fuzz", keep);

    REQUIRE_FALSE(graph.loadFromJson(R"({"nodes":[{"id":1,"type":"Nope"}],"connections":[]})",
                                     factory, destroy));
    REQUIRE(graph.getRegisteredNodes().size() == 1);
    REQUIRE(graph.getRegisteredNodes()[0].node == keep);

    Gain other(graph, 1);
    REQUIRE(graph.postConnect(keep, &other));
    REQUIRE(graph.hasPendingCommands());
    REQUIRE_FALSE(graph.loadFromJson(R"({"nodes":[],"connections":[]})", factory, destroy));
    REQUIRE(graph.getRegisteredNodes().size() == 1);

    float buf[8];
    graph.read(buf, 8);
    REQUIRE_FALSE(graph.hasPendingCommands());
    delete keep;
}
