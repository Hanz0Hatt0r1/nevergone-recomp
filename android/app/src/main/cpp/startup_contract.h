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
    int error_code = 0;
    std::string title;
    std::string message;
    std::vector<std::string> arguments;
};

void configure(RuntimeConfig config);
const RuntimeConfig& config();

// Clean-room semantic equivalents of the seven Lua-facing calls used directly
// by Game.StartLua. lua_runtime.cpp exposes these through lua_CFunction wrappers.
std::string Lua_GetPlatformString();
std::string Lua_GetDeviceUUID();

// Records a requested Script/<module>.lua execution. The Lua VM layer performs
// the actual load/pcall so this contract remains independently testable.
bool CAddDoString(const std::string& module_name);
void cpp_ShowLoadingUI();
void cpp_HideLoadingUI();

// Recovered Thumb behavior: ShowError receives one integer error code, while
// ShowMessageBox reads four string arguments from Lua.
void cpp_ShowErrorDialogUI(int error_code);
void cpp_ShowMessageBoxUI(
    std::string arg1,
    std::string arg2,
    std::string arg3,
    std::string arg4);

std::vector<std::string> take_requested_modules();
std::vector<UiEvent> take_ui_events();
std::string smoke_test_report();

}  // namespace nevergone::startup
