#include <catch2/catch_test_macros.hpp>

#include "ampcore_capi.h"

TEST_CASE("c api rejects a handle of the wrong node type", "[capi]")
{
    AmpcoreNode graph = nullptr, fuzz = nullptr, gain = nullptr;
    REQUIRE(ampcore_graph_create(1, &graph) == AMPCORE_OK);
    REQUIRE(ampcore_fuzz_create(graph, 1, 1.0f, 0.5f, &fuzz) == AMPCORE_OK);
    REQUIRE(ampcore_gain_create(graph, 1, 1.0f, &gain) == AMPCORE_OK);

    REQUIRE(ampcore_fuzz_set_gain(fuzz, 2.0f) == AMPCORE_OK);
    REQUIRE(ampcore_fuzz_set_gain(gain, 2.0f) == AMPCORE_ERROR_INVALID_ARG);
    REQUIRE(ampcore_meter_get_rms(fuzz, 0) == 0.0f);

    REQUIRE(ampcore_graph_connect(graph, fuzz, gain) == AMPCORE_OK);
    REQUIRE(ampcore_node_get_parameter_count(gain) > 0);

    ampcore_fuzz_destroy(fuzz);
    ampcore_gain_destroy(gain);
    ampcore_graph_destroy(graph);
}

TEST_CASE("c api nodes may be destroyed after their graph", "[capi]")
{
    AmpcoreNode graph = nullptr, fuzz = nullptr, gain = nullptr;
    REQUIRE(ampcore_graph_create(1, &graph) == AMPCORE_OK);
    REQUIRE(ampcore_fuzz_create(graph, 1, 1.0f, 0.5f, &fuzz) == AMPCORE_OK);
    REQUIRE(ampcore_gain_create(graph, 1, 1.0f, &gain) == AMPCORE_OK);
    REQUIRE(ampcore_graph_connect(graph, fuzz, gain) == AMPCORE_OK);
    REQUIRE(ampcore_graph_connect_to_output(graph, gain) == AMPCORE_OK);

    ampcore_graph_destroy(graph);
    ampcore_gain_destroy(gain);
    ampcore_fuzz_destroy(fuzz);
}
