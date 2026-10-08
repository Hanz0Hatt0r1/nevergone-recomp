#pragma once

#include <string>
#include <vector>

namespace nevergone::startup {

struct RuntimeConfig {
    std::string files_dir;
    std::string device_id;
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

// Clean-room semantic equivalents of the seven Lua-facing calls used directly
// by Game.StartLua. Their eventual lua_CFunction wrappers live separately so
// this contract can be tested before a Lua VM is integrated.
std::string Lua_GetPlatformString();
std::string Lua_GetDeviceUUID();

bool CAddDoString(const std::string& module_name);
void cpp_ShowLoadingUI();
void cpp_HideLoadingUI();
void cpp_ShowErrorDialogUI(std::string message);
void cpp_ShowMessageBoxUI(std::string title, std::string message);

std::vector<std::string> take_requested_modules();
std::vector<UiEvent> take_ui_events();
std::string smoke_test_report();

}  // namespace nevergone::startup
