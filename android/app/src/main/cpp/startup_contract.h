#pragma once

#include <string>
#include <vector>

namespace nevergone::startup {

struct RuntimeConfig {
    std::string files_dir;
    std::string device_id;
    std::string app_version;
    std::string platform = "android";
};

struct UiEvent {
    enum class Type {
        ShowLoading,
        HideLoading,
        ShowError,
        ShowMessage,
    };

    Type type;
    std::string title;
    std::string message;
};

void configure(RuntimeConfig config);
const RuntimeConfig& config();

// Clean-room semantic equivalents of Lua-facing calls used by the recovered
// startup/runtime scripts. Their lua_CFunction wrappers live separately so
// this contract can be tested before a full renderer is integrated.
std::string Lua_GetPlatformString();
std::string Lua_GetDeviceUUID();
std::string Lua_GetBundleVersion();
void Lua_SetConsoleColor(int level);

bool CAddDoString(const std::string& module_name);
void cpp_ShowLoadingUI();
void cpp_HideLoadingUI();
void cpp_ShowErrorDialogUI(std::string message);
void cpp_ShowMessageBoxUI(std::string title, std::string message);

std::vector<std::string> take_requested_modules();
std::vector<UiEvent> take_ui_events();
std::string smoke_test_report();

}  // namespace nevergone::startup
