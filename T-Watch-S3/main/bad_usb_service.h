#ifndef BAD_USB_SERVICE_H
#define BAD_USB_SERVICE_H
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include <string>
#include "USBHIDKeyboard.h"
#include "ducky_parser.h"
enum class BadUsbCommandType { RUN_SCRIPT, TYPE_TEXT, STOP };
struct BadUsbMessage {
    BadUsbCommandType type;
    char payload[512];
    char lang[16];
};
class BadUsbService {
public:
    BadUsbService();
    ~BadUsbService();
    void Start();
    bool RunScript(const std::string& script, const std::string& lang = "en_US");
    bool TypeText(const std::string& text, const std::string& lang = "en_US");
    void Stop();
    std::string GetStatusJSON();
    bool IsRunning() const;
private:
    std::unique_ptr<USBHIDKeyboard> hid_keyboard_;
    std::unique_ptr<DuckyParser> ducky_parser_;
    TaskHandle_t task_handle_;
    QueueHandle_t command_queue_;
    bool is_service_running_;
    static void UsbTaskWrapper(void* arg);
    void UsbTask();
    void EnterCombatMode();
    void ExitCombatMode();
};
#endif