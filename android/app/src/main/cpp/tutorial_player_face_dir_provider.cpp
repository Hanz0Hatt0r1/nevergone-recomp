#include "tutorial_player_face_dir_provider.h"

#include <sstream>

namespace nevergone::tutorial_player_face_dir_provider {

Resolution resolve(const player_control_runtime_state::Snapshot& control) {
    Resolution out;
    out.face_dir.control_generation = control.generation;

    if (!control.primary_state.has_value()) return out;

    const auto expected_flip = player_control_contract::facing_flip_x(
        *control.primary_state);
    if (!expected_flip.has_value() || *expected_flip != control.flip_x) {
        return out;
    }

    out.control_consistent = true;
    out.face_dir.reviewed = true;
    out.face_dir.value = control.flip_x ? 1 : 0;
    return out;
}

std::string status_report(const Resolution& resolution) {
    std::ostringstream out;
    out << "Tutorial PLAYERFACEDIR provider\n";
    out << "ready: " << (resolution.ready() ? "yes" : "no") << "\n";
    out << "control consistent: "
        << (resolution.control_consistent ? "yes" : "no") << "\n";
    out << "control generation: " << resolution.face_dir.control_generation << "\n";
    out << "reviewed: " << (resolution.face_dir.reviewed ? "yes" : "no") << "\n";
    if (resolution.face_dir.reviewed) {
        out << "PLAYERFACEDIR: " << resolution.face_dir.value << "\n";
    }
    return out.str();
}

}  // namespace nevergone::tutorial_player_face_dir_provider
