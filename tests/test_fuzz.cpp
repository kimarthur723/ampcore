#include <catch2/catch_test_macros.hpp>

import processor_graph;
import fuzz;

TEST_CASE("Fuzz clips samples above threshold", "[fuzz]")
{
    ProcessorGraph graph(1);
    Fuzz fuzz(graph, 1, /*gain=*/1.0f, /*threshold=*/0.5f);

    float input[4]  = {0.1f, -0.1f, 0.9f, -0.9f};
    float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    fuzz.process(output, input, 4);

    REQUIRE(output[0] == 0.1f);
    REQUIRE(output[1] == -0.1f);
    REQUIRE(output[2] == 0.5f);
    REQUIRE(output[3] == -0.5f);
}

TEST_CASE("Fuzz applies gain before clipping", "[fuzz]")
{
    ProcessorGraph graph(1);
    Fuzz fuzz(graph, 1, /*gain=*/10.0f, /*threshold=*/1.0f);

    float input[1]  = {0.05f};
    float output[1] = {0.0f};

    fuzz.process(output, input, 1);

    REQUIRE(output[0] == 0.5f);
}

TEST_CASE("Fuzz parameter get/set round-trips", "[fuzz]")
{
    ProcessorGraph graph(1);
    Fuzz fuzz(graph, 1);

    fuzz.setParameterValue(0, 42.0f);
    fuzz.setParameterValue(1, 0.8f);

    REQUIRE(fuzz.getParameterValue(0) == 42.0f);
    REQUIRE(fuzz.getParameterValue(1) == 0.8f);
}
