#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "trap_actions_document.h"

namespace nevergone::scene_action_pair_document_store {

constexpr std::size_t kMaxSceneActionDocumentBytes = 64u * 1024u * 1024u;

enum class Slot : std::uint8_t {
    kBorn = 0,
    kIdle = 1,
};

enum class LoadStatus {
    kIdle = 0,
    kFilesDirUnconfigured,
    kInvalidRelativePath,
    kAssetRootUnavailable,
    kPathEscapesAssetRoot,
    kMissing,
    kNotRegularFile,
    kTooLarge,
    kReadFailed,
    kParseFailed,
    kReady,
};

struct SlotSnapshot {
    LoadStatus status = LoadStatus::kIdle;
    std::uint64_t load_attempt_count = 0;
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::size_t bytes_consumed = 0;
    std::size_t trailing_bytes = 0;
    std::size_t primary_record_count = 0;
};

struct Snapshot {
    int variant = 0;
    SlotSnapshot born;
    SlotSnapshot idle;
    bool pair_ready = false;
};

void reset();
bool load_from_assets(
        Slot slot,
        int variant,
        const std::string& files_dir,
        const std::string& relative_path,
        std::size_t max_bytes = kMaxSceneActionDocumentBytes);
Snapshot snapshot();
std::optional<trap_actions_document::Document> current_document(Slot slot);
const char* status_name(LoadStatus status);
std::string status_report();

}  // namespace nevergone::scene_action_pair_document_store
