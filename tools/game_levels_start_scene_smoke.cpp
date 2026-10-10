#include <cassert>
#include <cstddef>
#include <string>

#include "game_levels_layer_tail.h"
#include "game_levels_port_node_section.h"
#include "game_levels_start_scene.h"

namespace {
using SceneSection = nevergone::game_levels_layer_tail::SceneSection;
using SceneRecord = nevergone::game_levels_layer_tail::SceneRecord;
using PortSection = nevergone::game_levels_port_node_section::Section;
using PortRecord = nevergone::game_levels_port_node_section::PortNodeRecord;

SceneRecord scene(const std::string& guid) {
    SceneRecord value;
    value.string_value = guid;
    return value;
}

PortRecord port(const std::string& guid, bool start) {
    PortRecord value;
    value.first_string = guid;
    value.first_bool = start;
    return value;
}
}  // namespace

int main() {
    namespace start = nevergone::game_levels_start_scene;

    {
        SceneSection scenes;
        PortSection ports;
        const auto selected = start::resolve(scenes, ports);
        assert(!selected.port_node_index.has_value());
        assert(!selected.scene_index.has_value());
        assert(selected.scene_guid.empty());
        assert(!selected.used_first_port_fallback);
    }

    {
        SceneSection scenes;
        scenes.scenes = {scene("town"), scene("stage-a"), scene("stage-b")};
        PortSection ports;
        ports.port_nodes = {port("town", false), port("stage-b", true)};
        const auto selected = start::resolve(scenes, ports);
        assert(selected.port_node_index == 1u);
        assert(selected.scene_index == 2u);
        assert(selected.scene_guid == "stage-b");
        assert(!selected.used_first_port_fallback);
    }

    {
        // GetStartScenePortNode() falls back to node zero when no start flag is set.
        SceneSection scenes;
        scenes.scenes = {scene("fallback")};
        PortSection ports;
        ports.port_nodes = {port("fallback", false), port("other", false)};
        const auto selected = start::resolve(scenes, ports);
        assert(selected.port_node_index == 0u);
        assert(selected.scene_index == 0u);
        assert(selected.used_first_port_fallback);
    }

    {
        // The original port scan stops at the first marked start node.
        SceneSection scenes;
        scenes.scenes = {scene("first"), scene("second")};
        PortSection ports;
        ports.port_nodes = {port("first", true), port("second", true)};
        const auto selected = start::resolve(scenes, ports);
        assert(selected.port_node_index == 0u);
        assert(selected.scene_index == 0u);
    }

    {
        // GetSceneDataWithGUID() also returns the first matching scene record.
        SceneSection scenes;
        scenes.scenes = {scene("dup"), scene("dup")};
        PortSection ports;
        ports.port_nodes = {port("dup", true)};
        const auto selected = start::resolve(scenes, ports);
        assert(selected.scene_index == 0u);
    }

    {
        SceneSection scenes;
        scenes.scenes = {scene("known")};
        PortSection ports;
        ports.port_nodes = {port("missing", true)};
        const auto selected = start::resolve(scenes, ports);
        assert(selected.port_node_index == 0u);
        assert(selected.scene_guid == "missing");
        assert(!selected.scene_index.has_value());
    }

    return 0;
}
