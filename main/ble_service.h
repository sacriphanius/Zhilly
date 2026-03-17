#pragma once
#include "sdkconfig.h"

#if defined(CONFIG_BT_ENABLED)

#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "bad_usb/ducky_parser.h"

class BleKeyboard;

class BleService {
private:
    BleKeyboard* ble_keyboard;
    DuckyParser* ducky_parser;

    TaskHandle_t task_handle;
    QueueHandle_t command_queue;

    bool is_running;
    bool ble_active;

    static void taskLoop(void* arg);
    void processCommand(const std::string& cmd);

public:
    BleService();
    ~BleService();

    // Start or stop the BLE radio
    bool start();
    void stop();

    // Queue a badusb script or raw text to be executed over BLE
    bool execute(const std::string& script);
    bool typeText(const std::string& text);
    
    // Stop currently executing script
    void stopScript();

    bool isBleActive() const { return ble_active; }
    bool isConnected() const;
    size_t getQueueSize() const;
};

#else // !CONFIG_BT_ENABLED

#include <string>

class BleService {
public:
    BleService() {}
    ~BleService() {}

    bool start() { return false; }
    void stop() {}
    bool execute(const std::string& script) { return false; }
    bool typeText(const std::string& text) { return false; }
    void stopScript() {}
    bool isBleActive() const { return false; }
    bool isConnected() const { return false; }
    size_t getQueueSize() const { return 0; }
};

#endif // CONFIG_BT_ENABLED
