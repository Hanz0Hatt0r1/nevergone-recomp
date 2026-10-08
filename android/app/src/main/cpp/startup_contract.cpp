#include "startup_contract.h"

#include <mutex>
#include <sstream>
#include <utility>

namespace nevergone::startup {
namespace {

std::mutex g_mutex;
RuntimeConfig g_config;
std::vector<std::string> g_requested_modules;
std::vector<UiEvent> g_ui_events;

}  // namespace

void configure(RuntimeConfig config) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_config = std::move(config);
}

const RuntimeConfig& config() {
    return g_config;
}

std::string Lua_GetPlatformString() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_config.platform.empty() ? "android" : g_config.platform;
}

std::string Lua_GetDeviceUUID() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_config.device_id;
}

bool CAddDoString(const std::string& module_name) {
    if (module_name.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_requested_modules.push_back(module_name);
    return true;
}

void cpp_ShowLoadingUI() {
    std::lock_guard<std::mutex> lock(g_mutex);
    UiEvent event;
    event.type = UiEvent::Type::ShowLoading;
    g_ui_events.push_back(std::move(event));
}

void cpp_HideLoadingUI() {
    std::lock_guard<std::mutex> lock(g_mutex);
    UiEvent event;
    event.type = UiEvent::Type::HideLoading;
    g_ui_events.push_back(std::move(event));
}

void cpp_ShowErrorDialogUI(int error_code) {
    std::lock_guard<std::mutex> lock(g_mutex);
    UiEvent event;
    event.type = UiEvent::Type::ShowError;
    event.error_code = error_code;
    g_ui_events.push_back(std::move(event));
}

void cpp_ShowMessageBoxUI(
    std::string arg1,
    std::string arg2,
    std::string arg3,
    std::string arg4) {
    std::lock_guard<std::mutex> lock(g_mutex);
    UiEvent event;
    event.type = UiEvent::Type::ShowMessage;
    event.title = arg1;
    event.message = arg2;
    event.arguments = {
        std::move(arg1),
        std::move(arg2),
        std::move(arg3),
        std::move(arg4),
    };
    g_ui_events.push_back(std::move(event));
}

std::vector<std::string> take_requested_modules() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<std::string> result;
    result.swap(g_requested_modules);
    return result;
}

std::vector<UiEvent> take_ui_events() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<UiEvent> result;
    result.swap(g_ui_events);
    return result;
}

std::string smoke_test_report() {
    const std::string platform = Lua_GetPlatformString();
    const std::string device_id = Lua_GetDeviceUUID();

    const bool client_require = CAddDoString("Game.ClientRequire");
    const bool share_require = CAddDoString("ShareLogic.require");
    cpp_ShowLoadingUI();
    cpp_HideLoadingUI();
    cpp_ShowMessageBoxUI("Never Gone Recomp", "startup bridge smoke test", "ok", "cancel");
    cpp_ShowErrorDialogUI(1);

    const auto modules = take_requested_modules();
    const auto events = take_ui_events();

    const bool ui_arguments_ok =
        events.size() == 4
        && events[2].arguments.size() == 4
        && events[3].error_code == 1;

    std::ostringstream out;
    out << "startup contract: "
        << ((client_require && share_require && modules.size() == 2 && ui_arguments_ok)
                ? "ok"
                : "failed")
        << "\n";
    out << "platform: " << platform << "\n";
    out << "device id configured: " << (!device_id.empty() ? "yes" : "no") << "\n";
    out << "module requests: " << modules.size() << "\n";
    out << "UI events: " << events.size() << "\n";
    return out.str();
}

}  // namespace nevergone::startup
