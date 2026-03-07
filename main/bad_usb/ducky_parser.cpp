#include "ducky_parser.h"
#include <cstring>
#include <string_view>
#include "USBHIDKeyboard.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char* TAG = "DuckyParser";
extern const DuckyCommand duckyCmds[];
extern const DuckyCombination duckyComb[];
const DuckyCombination duckyComb_local[]{
    {"CTRL-ALT", KEY_LEFT_CTRL, KEY_LEFT_ALT, 0},
    {"CTRL-SHIFT", KEY_LEFT_CTRL, KEY_LEFT_SHIFT, 0},
    {"CTRL-GUI", KEY_LEFT_CTRL, KEY_LEFT_GUI, 0},
    {"CTRL-ESCAPE", KEY_LEFT_CTRL, KEY_ESC, 0},
    {"ALT-SHIFT", KEY_LEFT_ALT, KEY_LEFT_SHIFT, 0},
    {"ALT-GUI", KEY_LEFT_ALT, KEY_LEFT_GUI, 0},
    {"GUI-SHIFT", KEY_LEFT_GUI, KEY_LEFT_SHIFT, 0},
    {"GUI-SPACE", KEY_LEFT_GUI, KEY_SPACE, 0},
    {"CTRL-ALT-SHIFT", KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_LEFT_SHIFT},
    {"CTRL-ALT-GUI", KEY_LEFT_CTRL, KEY_LEFT_ALT, KEY_LEFT_GUI},
    {"ALT-SHIFT-GUI", KEY_LEFT_ALT, KEY_LEFT_SHIFT, KEY_LEFT_GUI},
    {"CTRL-SHIFT-GUI", KEY_LEFT_CTRL, KEY_LEFT_SHIFT, KEY_LEFT_GUI},
    {"SYSREQ", KEY_LEFT_ALT, KEY_PRINT_SCREEN, 0}};
const DuckyCommand duckyCmds_local[]{
    {"REM", 0, DuckyCommandType_Comment},
    {"
    {"STRING", 0, DuckyCommandType_Print},
    {"STRINGLN", 0, DuckyCommandType_Print},
    {"DELAY", 0, DuckyCommandType_Delay},
    {"DEFAULTDELAY", 100, DuckyCommandType_Delay},
    {"DEFAULT_DELAY", 100, DuckyCommandType_Delay},
    {"ALTCHAR", 0, DuckyCommandType_AltChar},
    {"ALTSTRING", 0, DuckyCommandType_AltString},
    {"ALTCODE", 0, DuckyCommandType_AltString},
    {"CTRL-ALT", 0, DuckyCommandType_Combination},
    {"CTRL-SHIFT", 0, DuckyCommandType_Combination},
    {"CTRL-GUI", 0, DuckyCommandType_Combination},
    {"CTRL-ESCAPE", 0, DuckyCommandType_Combination},
    {"ALT-SHIFT", 0, DuckyCommandType_Combination},
    {"ALT-GUI", 0, DuckyCommandType_Combination},
    {"GUI-SHIFT", 0, DuckyCommandType_Combination},
    {"GUI-SPACE", 0, DuckyCommandType_Combination},
    {"CTRL-ALT-SHIFT", 0, DuckyCommandType_Combination},
    {"CTRL-ALT-GUI", 0, DuckyCommandType_Combination},
    {"ALT-SHIFT-GUI", 0, DuckyCommandType_Combination},
    {"CTRL-SHIFT-GUI", 0, DuckyCommandType_Combination},
    {"SYSREQ", 0, DuckyCommandType_Combination},
    {"BACKSPACE", KEYBACKSPACE, DuckyCommandType_Cmd},
    {"DELETE", KEY_DELETE, DuckyCommandType_Cmd},
    {"ALT", KEY_LEFT_ALT, DuckyCommandType_Cmd},
    {"CTRL", KEY_LEFT_CTRL, DuckyCommandType_Cmd},
    {"CONTROL", KEY_LEFT_CTRL, DuckyCommandType_Cmd},
    {"GUI", KEY_LEFT_GUI, DuckyCommandType_Cmd},
    {"WINDOWS", KEY_LEFT_GUI, DuckyCommandType_Cmd},
    {"SHIFT", KEY_LEFT_SHIFT, DuckyCommandType_Cmd},
    {"ESCAPE", KEY_ESC, DuckyCommandType_Cmd},
    {"ESC", KEY_ESC, DuckyCommandType_Cmd},
    {"TAB", KEYTAB, DuckyCommandType_Cmd},
    {"ENTER", KEY_RETURN, DuckyCommandType_Cmd},
    {"DOWNARROW", KEY_DOWN_ARROW, DuckyCommandType_Cmd},
    {"DOWN", KEY_DOWN_ARROW, DuckyCommandType_Cmd},
    {"LEFTARROW", KEY_LEFT_ARROW, DuckyCommandType_Cmd},
    {"LEFT", KEY_LEFT_ARROW, DuckyCommandType_Cmd},
    {"RIGHTARROW", KEY_RIGHT_ARROW, DuckyCommandType_Cmd},
    {"RIGHT", KEY_RIGHT_ARROW, DuckyCommandType_Cmd},
    {"UPARROW", KEY_UP_ARROW, DuckyCommandType_Cmd},
    {"UP", KEY_UP_ARROW, DuckyCommandType_Cmd},
    {"BREAK", KEY_PAUSE, DuckyCommandType_Cmd},
    {"PAUSE", KEY_PAUSE, DuckyCommandType_Cmd},
    {"CAPSLOCK", KEY_CAPS_LOCK, DuckyCommandType_Cmd},
    {"END", KEY_END, DuckyCommandType_Cmd},
    {"HOME", KEY_HOME, DuckyCommandType_Cmd},
    {"INSERT", KEY_INSERT, DuckyCommandType_Cmd},
    {"NUMLOCK", LED_NUMLOCK, DuckyCommandType_Cmd},
    {"PAGEUP", KEY_PAGE_UP, DuckyCommandType_Cmd},
    {"PAGEDOWN", KEY_PAGE_DOWN, DuckyCommandType_Cmd},
    {"PRINTSCREEN", KEY_PRINT_SCREEN, DuckyCommandType_Cmd},
    {"SCROLLOCK", KEY_SCROLL_LOCK, DuckyCommandType_Cmd},
    {"MENU", KEY_MENU, DuckyCommandType_Cmd},
    {"APP", KEY_MENU, DuckyCommandType_Cmd},
    {"SPACE", KEY_SPACE, DuckyCommandType_Cmd},
};
DuckyParser::DuckyParser(HIDInterface* hid_interface)
    : hid(hid_interface),
      is_running(false),
      should_stop(false),
      default_delay(100),
      string_delay(0) {}
DuckyParser::~DuckyParser() {}
void DuckyParser::stop() { should_stop = true; }
void DuckyParser::runScript(const std::string& script) {
    is_running = true;
    should_stop = false;
    std::string_view sv(script);
    size_t start = 0;
    size_t end = sv.find('\n');
    while (end != std::string_view::npos) {
        if (should_stop)
            break;
        std::string line(sv.substr(start, end - start));
        parseLine(line);
        start = end + 1;
        end = sv.find('\n', start);
    }
    if (!should_stop && start < sv.length()) {
        std::string line(sv.substr(start));
        parseLine(line);
    }
    hid->releaseAll();
    is_running = false;
}
void DuckyParser::typeText(const std::string& text) {
    if (!hid->isConnected())
        return;
    is_running = true;
    should_stop = false;
    for (char c : text) {
        if (should_stop)
            break;
        hid->write(c);
        if (string_delay > 0)
            vTaskDelay(pdMS_TO_TICKS(string_delay));
    }
    hid->releaseAll();
    is_running = false;
}
const DuckyCommand* DuckyParser::findDuckyCommand(const char* cmd) {
    for (const auto& cmds : duckyCmds_local) {
        if (strcasecmp(cmd, cmds.command) == 0)
            return &cmds;
    }
    return nullptr;
}
const DuckyCombination* DuckyParser::findDuckyCombination(const char* cmd) {
    for (const auto& comb : duckyComb_local) {
        if (strcasecmp(cmd, comb.command) == 0)
            return &comb;
    }
    return nullptr;
}
void DuckyParser::parseLine(const std::string& line) {
    if (line.empty() || line[0] == '\r' || line[0] == '\n')
        return;
    size_t space_idx = line.find(' ');
    std::string cmd_str = (space_idx != std::string::npos) ? line.substr(0, space_idx) : line;
    std::string args_str = (space_idx != std::string::npos && space_idx + 1 < line.length())
                               ? line.substr(space_idx + 1)
                               : "";
    if (!args_str.empty() && args_str.back() == '\r') {
        args_str.pop_back();
    }
    if (!cmd_str.empty() && cmd_str.back() == '\r') {
        cmd_str.pop_back();
    }
    executeCommand(cmd_str, args_str);
    if (default_delay > 0 && !should_stop) {
        vTaskDelay(pdMS_TO_TICKS(default_delay));
    }
}
void DuckyParser::executeCommand(const std::string& cmd, const std::string& args) {
    if (!hid->isConnected())
        return;
    const DuckyCommand* dcmd = findDuckyCommand(cmd.c_str());
    if (dcmd) {
        switch (dcmd->type) {
            case DuckyCommandType_Comment:
                break;
            case DuckyCommandType_Delay: {
                int ms = 0;
                if (!args.empty()) {
                    ms = std::stoi(args);
                } else {
                    ms = (strcmp(cmd.c_str(), "DEFAULTDELAY") == 0 ||
                          strcmp(cmd.c_str(), "DEFAULT_DELAY") == 0)
                             ? std::stoi(args)
                             : default_delay;
                    if (ms > 0 && args.empty())
                        default_delay = ms;  
                }
                if (args.empty() && ms == 0)
                    break;  
                const int chunk_ms = 50;
                int remaining = ms;
                while (remaining > 0 && !should_stop) {
                    vTaskDelay(pdMS_TO_TICKS(remaining > chunk_ms ? chunk_ms : remaining));
                    remaining -= chunk_ms;
                }
                break;
            }
            case DuckyCommandType_Print:
                if (strcmp(dcmd->command, "STRINGLN") == 0) {
                    typeText(args + "\n");
                } else {
                    typeText(args);
                }
                break;
            case DuckyCommandType_Cmd:
                if (!args.empty()) {
                    hid->press(dcmd->key);
                    hid->press(args[0]);
                    hid->releaseAll();
                } else {
                    hid->write(dcmd->key);
                }
                break;
            case DuckyCommandType_Combination: {
                const DuckyCombination* comb = findDuckyCombination(cmd.c_str());
                if (comb) {
                    hid->press(comb->key1);
                    hid->press(comb->key2);
                    if (comb->key3 != 0)
                        hid->press(comb->key3);
                    if (!args.empty()) {
                        hid->press(args[0]);
                    }
                    hid->releaseAll();
                }
                break;
            }
            default:
                break;
        }
    } else {
        ESP_LOGI(TAG, "Unknown Command: %s", cmd.c_str());
    }
}
