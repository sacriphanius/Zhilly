#ifndef BAD_BLE_SERVICE_H
#define BAD_BLE_SERVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include <string>
#include "bad_usb/ble_keyboard.h"
#include "bad_usb/ducky_parser.h"

enum class BadBleCommandType { RUN_SCRIPT, TYPE_TEXT, STOP };

struct BadBleMessage {
    BadBleCommandType type;
    char payload[512];  
    char lang[16];
};

class BadBleService {
public:
    BadBleService();
    ~BadBleService();

    void Start();
    bool RunScript(const std::string& script, const std::string& lang = "en_US");
    bool TypeText(const std::string& text, const std::string& lang = "en_US");
    void Stop();
    std::string GetStatusJSON();
    bool IsRunning() const;
    bool IsConnected() const;
    void SetDeviceName(const std::string& name);

private:
    std::unique_ptr<BleKeyboard> ble_keyboard_;
    std::unique_ptr<DuckyParser> ducky_parser_;
    TaskHandle_t task_handle_;
    QueueHandle_t command_queue_;
    bool is_service_running_;

    static void BleTaskWrapper(void* arg);
    void BleTask();
    
    void EnterCombatMode();
    void ExitCombatMode();
};

#endif
