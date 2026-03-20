#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>
#include <vector>
class HIDInterface;
enum DuckyCommandType {
    DuckyCommandType_Cmd,
    DuckyCommandType_Print,
    DuckyCommandType_Delay,
    DuckyCommandType_Comment,
    DuckyCommandType_Repeat,
    DuckyCommandType_Combination,
    DuckyCommandType_WaitForButtonPress,
    DuckyCommandType_AltChar,
    DuckyCommandType_AltString,
    DuckyCommandType_StringDelay,
    DuckyCommandType_DefaultStringDelay,
    DuckyCommandType_SetLanguage
};
struct DuckyCommand {
    const char* command;
    char key;
    DuckyCommandType type;
};
struct DuckyCombination {
    const char* command;
    char key1;
    char key2;
    char key3;
};
class DuckyParser {
private:
    HIDInterface* hid;
    bool is_running;
    bool should_stop;
    uint32_t default_delay;
    uint32_t string_delay;
    void parseLine(const std::string& line);
    const DuckyCommand* findDuckyCommand(const char* cmd);
    const DuckyCombination* findDuckyCombination(const char* cmd);
    void sendAltChar(uint8_t charCode);
    void sendAltString(const std::string& text);
    void executeCommand(const std::string& cmd, const std::string& args);
public:
    DuckyParser(HIDInterface* hid_interface);
    ~DuckyParser();
    void runScript(const std::string& script);
    void stop();
    bool isRunning() const { return is_running; }
    void typeText(const std::string& text);
};