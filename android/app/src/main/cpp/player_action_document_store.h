#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "enemy_actions_wbg_document.h"

namespace nevergone::player_action_document_store {

constexpr std::size_t kDefaultMaxBytes = 64u * 1024u * 1024u;

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

struct Snapshot {
    LoadStatus status = LoadStatus::kIdle;
    std::uint64_t load_attempt_count = 0;
    std::string relative_path;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t bytes_consumed = 0;
    std::size_t trailing_bytes = 0;
    std::size_t primary_record_count = 0;
};

void reset();
bool load_from_assets(
        const std::string& files_dir,
        const std::string& relative_path,
        std::size_t max_bytes = kDefaultMaxBytes);
Snapshot snapshot();
std::optional<enemy_actions_wbg_document::Document> current_document();
const char* status_name(LoadStatus status);
std::string status_report();

}  // namespace nevergone::player_action_document_store
