

#include "mcp_server.h"
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_pthread.h>
#include <algorithm>
#include <cstring>

#include "application.h"
#include "bad_usb_service.h"
#include "board.h"
#include "display.h"
#include "ir_service.h"
#include "lvgl_display.h"
#include "lvgl_theme.h"
#include "news_service.h"
#include "oled_display.h"
#include "settings.h"

#define TAG "MCP"

McpServer::McpServer() {}

McpServer::~McpServer() {
    for (auto tool : tools_) {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools() {
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    AddTool("self.get_device_status",
            "Provides the real-time information of the device, including the current status of the "
            "audio speaker, screen, battery, network, etc.\n"
            "Use this tool for: \n"
            "1. Answering questions about current condition (e.g. what is the current volume of "
            "the audio speaker?)\n"
            "2. As the first step to control the device (e.g. turn up / down the volume of the "
            "audio speaker, etc.)",
            PropertyList(), [&board](const PropertyList& properties) -> ReturnValue {
                return board.GetDeviceStatusJson();
            });

    AddTool("self.audio_speaker.set_volume",
            "Set the volume of the audio speaker. If the current volume is unknown, you must call "
            "`self.get_device_status` tool first and then call this tool.",
            PropertyList({Property("volume", kPropertyTypeInteger, 0, 100)}),
            [&board](const PropertyList& properties) -> ReturnValue {
                auto codec = board.GetAudioCodec();
                codec->SetOutputVolume(properties["volume"].value<int>());
                return true;
            });

    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness", "Set the brightness of the screen.",
                PropertyList({Property("brightness", kPropertyTypeInteger, 0, 100)}),
                [backlight](const PropertyList& properties) -> ReturnValue {
                    uint8_t brightness =
                        static_cast<uint8_t>(properties["brightness"].value<int>());
                    backlight->SetBrightness(brightness, true);
                    return true;
                });
    }

#ifdef HAVE_LVGL
    auto display = board.GetDisplay();
    if (display && display->GetTheme() != nullptr) {
        AddTool("self.screen.set_theme",
                "Set the theme of the screen. The theme can be `light` or `dark`.",
                PropertyList({Property("theme", kPropertyTypeString)}),
                [display](const PropertyList& properties) -> ReturnValue {
                    auto theme_name = properties["theme"].value<std::string>();
                    auto& theme_manager = LvglThemeManager::GetInstance();
                    auto theme = theme_manager.GetTheme(theme_name);
                    if (theme != nullptr) {
                        display->SetTheme(theme);
                        return true;
                    }
                    return false;
                });
    }

    auto camera = board.GetCamera();
    if (camera) {
        AddTool("self.camera.take_photo",
                "Always remember you have a camera. If the user asks you to see something, use "
                "this tool to take a photo and then explain it.\n"
                "Args:\n"
                "  `question`: The question that you want to ask about the photo.\n"
                "Return:\n"
                "  A JSON object that provides the photo information.",
                PropertyList({Property("question", kPropertyTypeString)}),
                [camera](const PropertyList& properties) -> ReturnValue {
                    TaskPriorityReset priority_reset(1);

                    if (!camera->Capture()) {
                        throw std::runtime_error("Failed to capture photo");
                    }
                    auto question = properties["question"].value<std::string>();
                    return camera->Explain(question);
                });
    }
#endif

    auto& cc1101 = Application::GetInstance().GetCc1101Service();

    AddTool("self.cc1101.get_info", "CC1101 module status and diagnostics.", PropertyList(),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return std::string("CC1101 Durumu: ") +
                       (cc1101.GetStatus() ? "Ready" : "Not initialized");
            });

    AddTool("self.cc1101.set_frequency",
            "CC1101 RF frekansini ayarla (MHz).\n"
            "Yaygin: 315 (Kuzey Amerika garaj), 433 (Avrupa kumanda/IoT), 868 (Avrupa LoRa), 915 "
            "(Kuzey Amerika ISM).\n"
            "Parametre: mhz (300-928)",
            PropertyList({Property("mhz", kPropertyTypeInteger, 300, 928)}),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return cc1101.SetFrequency(static_cast<float>(properties["mhz"].value<int>()));
            });

    AddTool(
        "self.cc1101.set_modulation",
        "CC1101 modulasyon turunu ayarla: 'ASK'/'OOK' (kumandalar/garaj) veya 'FSK' (veri/sensor).",
        PropertyList({Property("type", kPropertyTypeString)}),
        [&cc1101](const PropertyList& properties) -> ReturnValue {
            return cc1101.SetModulation(properties["type"].value<std::string>());
        });

    AddTool("self.cc1101.read_rssi",
            "Anlik sinyal gucunu oku (dBm). -60 uzeri: Guclu, -90 alti: Zayif/yok.", PropertyList(),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return std::to_string(cc1101.ReadRssi());
            });

    AddTool("self.cc1101.load_presets",
            "Load CC1101 preset JSON file from SD card.\n"
            "Parameter: path (default: /sdcard/cc1101_presets.json)",
            PropertyList({Property("path", kPropertyTypeString,
                                   std::string("/sdcard/cc1101_presets.json"))}),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                std::string path = "/sdcard/cc1101_presets.json";
                if (properties.HasProperty("path"))
                    path = properties["path"].value<std::string>();
                bool ok = cc1101.LoadPresets(path);
                return ok ? std::string("Presetler yuklendi: ") + path
                          : std::string("Preset yuklenemedi: ") + path;
            });

    AddTool("self.cc1101.replay_sub_file",
            "Replay a .sub file from SD card (Replay Attack).\n"
            "First see the file list using self.sdcard.list_files.\n"
            "Parametre: filename (ornek: 'garaj.sub' veya '/sdcard/garaj.sub')",
            PropertyList({Property("filename", kPropertyTypeString)}),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                std::string name = properties["filename"].value<std::string>();
                return cc1101.ReplaySubFile(name)
                           ? std::string("Replay started: ") + name
                           : std::string("Replay failed. Does file exist? ") + name;
            });

    AddTool("self.cc1101.stop_replay", "Stop ongoing replay process.", PropertyList(),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return cc1101.StopReplay() ? std::string("Replay stopped.")
                                           : std::string("Replay zaten calismiyordu.");
            });

    AddTool("self.cc1101.start_jammer",
            "Start RF Jammer (noise broadcast on current frequency).\n"
            "Parametre: duration_ms (0=surekli, maks 600000)",
            PropertyList({Property("duration_ms", kPropertyTypeInteger, 0, 600000)}),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return cc1101.StartJammer(properties["duration_ms"].value<int>())
                           ? std::string("RF Jammer started.")
                           : std::string("Failed to start Jammer.");
            });

    AddTool("self.cc1101.stop_jammer", "Stop active RF Jammer.", PropertyList(),
            [&cc1101](const PropertyList& properties) -> ReturnValue {
                return cc1101.StopJammer() ? std::string("RF Jammer durduruldu.")
                                           : std::string("Jammer zaten calismiyordu.");
            });

    auto& ir = Application::GetInstance().GetIrService();

    AddTool(
        "self.ir.replay_file",
        "Send an infrared command from a .ir file on the SD card.\n"
        "Flipper Zero / Bruce uyumlu format desteklenir (NEC, RC5, RC6, Samsung32, SIRC, RAW).\n"
        "Ornekler: ir.replay_file({file: 'tv.ir', command: 'Power'})\n"
        "  - file: Name or full path of .ir file (e.g. \'samsung_tv.ir\' or "
        "'/sdcard/ir/tv.ir')\n"
        "  - command: gondermek istediginiz komut adi. Bos birakilirsa ilk komut gonderilir.",
        PropertyList({Property("file", kPropertyTypeString),
                      Property("command", kPropertyTypeString, std::string(""))}),
        [&ir](const PropertyList& properties) -> ReturnValue {
            std::string file = properties["file"].value<std::string>();
            std::string cmd =
                properties.HasProperty("command") ? properties["command"].value<std::string>() : "";
            if (file.find('/') == std::string::npos)
                file = std::string("/sdcard/ir/") + file;
            return ir.ReplayFile(file, cmd)
                       ? std::string("IR gonderildi: ") + file +
                             (cmd.empty() ? "" : " [" + cmd + "]")
                       : std::string("Failed to send IR. Does file exist? ") + file;
        });

    AddTool("self.ir.tv_b_gone",
            "TV-B-Gone: Tüm bilinen TV markalarinin guc kapama kodlarini sirayla gonderir.\n"
            "Etraftaki TV'leri, projektörleri ve ekranlari kapatmak icin kullanilir.\n"
            "Call self.ir.stop_tv_b_gone to stop it.\n"
            "Parametre: region ('eu' (Avrupa) veya 'us' (Amerika/Asya) olarak belirtilmelidir).",
            PropertyList({Property("region", kPropertyTypeString)}),
            [&ir](const PropertyList& properties) -> ReturnValue {
                std::string region = properties["region"].value<std::string>();
                return ir.StartTvBGone(region) ? std::string("TV-B-Gone started (Region: ") +
                                                     region + "). Tum kodlar sirayla gonderiliyor."
                                               : std::string("Failed to start TV-B-Gone.");
            });

    AddTool("self.ir.stop_tv_b_gone", "Stop ongoing TV-B-Gone operation.", PropertyList(),
            [&ir](const PropertyList& properties) -> ReturnValue {
                ir.StopTvBGone();
                return std::string("TV-B-Gone stopped.");
            });

    AddTool(
        "self.ir.start_jammer",
        "Start IR Jammer: Prevents IR receiver devices from being controlled remotely.\n"
        "Modlar: 'basic' (38kHz sabit), 'sweep' (33-56kHz tarama), 'random' (rastgele)\n"
        "  - mode: 'basic' | 'sweep' | 'random' (varsayilan: 'sweep')\n"
        "  - duration_ms: calisma suresi ms (0 = surekli, maks 300000)",
        PropertyList({Property("mode", kPropertyTypeString, std::string("sweep")),
                      Property("duration_ms", kPropertyTypeInteger, 0, 300000)}),
        [&ir](const PropertyList& properties) -> ReturnValue {
            std::string mode_str = "sweep";
            int dur = 0;
            if (properties.HasProperty("mode"))
                mode_str = properties["mode"].value<std::string>();
            if (properties.HasProperty("duration_ms"))
                dur = properties["duration_ms"].value<int>();
            IrJamMode mode = IrJamMode::SWEEP;
            if (mode_str == "basic")
                mode = IrJamMode::BASIC;
            if (mode_str == "random")
                mode = IrJamMode::RANDOM;
            return ir.StartJammer(mode, (uint32_t)dur)
                       ? std::string("IR Jammer started (mode: ") + mode_str + ")"
                       : std::string("Failed to start IR Jammer.");
        });

    AddTool("self.ir.stop_jammer", "Aktif IR Jammer'i durdur.", PropertyList(),
            [&ir](const PropertyList& properties) -> ReturnValue {
                ir.StopJammer();
                return std::string("IR Jammer stopped.");
            });

    AddTool("news.get_top_headlines",
            "Get the latest top headlines from NewsAPI.org. Use this when the user asks for news.\n"
            "Parameters:\n"
            "  - country: The 2-letter ISO 3166-1 code of the country (e.g., 'tr' for Turkey, 'us' "
            "for USA, 'gb' for UK). Default is 'tr'.\n"
            "  - category: The category you want to get headlines for. Options: business, "
            "entertainment, general, health, science, sports, technology. Leave empty for all "
            "categories.\n"
            "Returns a formatted string containing the top headlines.",
            PropertyList({Property("country", kPropertyTypeString, std::string("tr")),
                          Property("category", kPropertyTypeString, std::string(""))}),
            [](const PropertyList& properties) -> ReturnValue {
                std::string country = "tr";
                std::string category = "";

                if (properties.HasProperty("country")) {
                    country = properties["country"].value<std::string>();
                }
                if (properties.HasProperty("category")) {
                    category = properties["category"].value<std::string>();
                }

                auto& news = NewsService::GetInstance();
                return news.GetTopHeadlines(country, category);
            });

    AddTool("self.sdcard.list_files",
            "List all files on SD card (.sub, .json, .txt etc.) with labels.\n"
            "To see available files before replay or loading preset "
            "kullan.\n"
            "Shows labels by file type: [sub], [csv], [json], [txt], [folder], [file]",
            PropertyList(), [&cc1101](const PropertyList& properties) -> ReturnValue {
                return cc1101.ListSdFiles();
            });

    auto bad_usb = Application::GetInstance().GetBadUsbService();
    if (bad_usb) {
        AddTool(
            "self.usb.bad_usb_run",
            "Hedef cihaza komutlar gondermek icin DuckyScript calistirir.\n"
            "Ornek script: GUI r\\nDELAY 100\\nSTRING cmd\\nENTER\n"
            "Komutlar asenkron olarak arka planda calistirilir (Wi-Fi ve mikrofon aktif kalir).",
            PropertyList({Property("script", kPropertyTypeString)}),
            [bad_usb](const PropertyList& properties) -> ReturnValue {
                std::string script = properties["script"].value<std::string>();
                bool ok = bad_usb->RunScript(script);
                return ok ? std::string("DuckyScript kuyruga eklendi.")
                          : std::string("Error: Queue full.");
            });

        AddTool("self.usb.bad_usb_type",
                "Hedef bilgisayara dogrudan metin yazar. DuckyScript olmadan duz metin tuslamak "
                "icin kullanilir.",
                PropertyList({Property("text", kPropertyTypeString)}),
                [bad_usb](const PropertyList& properties) -> ReturnValue {
                    std::string text = properties["text"].value<std::string>();
                    bool ok = bad_usb->TypeText(text);
                    return ok ? std::string("Metin kuyruga eklendi.")
                              : std::string("Error: Queue full.");
                });

        AddTool("self.usb.bad_usb_stop",
                "Instantly stops active BadUSB script and its delay processes.",
                PropertyList(), [bad_usb](const PropertyList& properties) -> ReturnValue {
                    bad_usb->Stop();
                    return std::string("BadUSB commands stopped.");
                });

        AddTool("self.usb.get_status",
                "USB (TinyUSB) baglanti durumunu ve su an aktif bir yazma/script islemi olup "
                "olmadigini gosterir.",
                PropertyList(), [bad_usb](const PropertyList& properties) -> ReturnValue {
                    return bad_usb->GetStatusJSON();
                });
    }

    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddUserOnlyTools() {
    AddUserOnlyTool("self.get_system_info", "Get the system information", PropertyList(),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto& board = Board::GetInstance();
                        return board.GetSystemInfoJson();
                    });

    AddUserOnlyTool("self.reboot", "Reboot the system", PropertyList(),
                    [this](const PropertyList& properties) -> ReturnValue {
                        auto& app = Application::GetInstance();
                        app.Schedule([&app]() {
                            ESP_LOGW(TAG, "User requested reboot");
                            vTaskDelay(pdMS_TO_TICKS(1000));

                            app.Reboot();
                        });
                        return true;
                    });

    AddUserOnlyTool(
        "self.upgrade_firmware",
        "Upgrade firmware from a specific URL. This will download and install the firmware, then "
        "reboot the device.",
        PropertyList({Property("url", kPropertyTypeString,
                               "The URL of the firmware binary file to download and install")}),
        [this](const PropertyList& properties) -> ReturnValue {
            auto url = properties["url"].value<std::string>();
            ESP_LOGI(TAG, "User requested firmware upgrade from URL: %s", url.c_str());

            auto& app = Application::GetInstance();
            app.Schedule([url, &app]() {
                bool success = app.UpgradeFirmware(url);
                if (!success) {
                    ESP_LOGE(TAG, "Firmware upgrade failed");
                }
            });

            return true;
        });

#ifdef HAVE_LVGL
    auto display = dynamic_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
    if (display) {
        AddUserOnlyTool("self.screen.get_info",
                        "Information about the screen, including width, height, etc.",
                        PropertyList(), [display](const PropertyList& properties) -> ReturnValue {
                            cJSON* json = cJSON_CreateObject();
                            cJSON_AddNumberToObject(json, "width", display->width());
                            cJSON_AddNumberToObject(json, "height", display->height());
                            if (dynamic_cast<OledDisplay*>(display)) {
                                cJSON_AddBoolToObject(json, "monochrome", true);
                            } else {
                                cJSON_AddBoolToObject(json, "monochrome", false);
                            }
                            return json;
                        });

#if CONFIG_LV_USE_SNAPSHOT
        AddUserOnlyTool(
            "self.screen.snapshot", "Snapshot the screen and upload it to a specific URL",
            PropertyList({Property("url", kPropertyTypeString),
                          Property("quality", kPropertyTypeInteger, 80, 1, 100)}),
            [display](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                auto quality = properties["quality"].value<int>();

                std::string jpeg_data;
                if (!display->SnapshotToJpeg(jpeg_data, quality)) {
                    throw std::runtime_error("Failed to snapshot screen");
                }

                ESP_LOGI(TAG, "Upload snapshot %u bytes to %s", jpeg_data.size(), url.c_str());

                std::string boundary = "----ESP32_SCREEN_SNAPSHOT_BOUNDARY";

                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);
                http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
                if (!http->Open("POST", url)) {
                    throw std::runtime_error("Failed to open URL: " + url);
                }
                {
                    std::string file_header;
                    file_header += "--" + boundary + "\r\n";
                    file_header +=
                        "Content-Disposition: form-data; name=\"file\"; "
                        "filename=\"screenshot.jpg\"\r\n";
                    file_header += "Content-Type: image/jpeg\r\n";
                    file_header += "\r\n";
                    http->Write(file_header.c_str(), file_header.size());
                }

                http->Write((const char*)jpeg_data.data(), jpeg_data.size());

                {
                    std::string multipart_footer;
                    multipart_footer += "\r\n--" + boundary + "--\r\n";
                    http->Write(multipart_footer.c_str(), multipart_footer.size());
                }
                http->Write("", 0);

                if (http->GetStatusCode() != 200) {
                    throw std::runtime_error("Unexpected status code: " +
                                             std::to_string(http->GetStatusCode()));
                }
                std::string result = http->ReadAll();
                http->Close();
                ESP_LOGI(TAG, "Snapshot screen result: %s", result.c_str());
                return true;
            });

        AddUserOnlyTool(
            "self.screen.preview_image", "Preview an image on the screen",
            PropertyList({Property("url", kPropertyTypeString)}),
            [display](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);

                if (!http->Open("GET", url)) {
                    throw std::runtime_error("Failed to open URL: " + url);
                }
                int status_code = http->GetStatusCode();
                if (status_code != 200) {
                    throw std::runtime_error("Unexpected status code: " +
                                             std::to_string(status_code));
                }

                size_t content_length = http->GetBodyLength();
                char* data = (char*)heap_caps_malloc(content_length, MALLOC_CAP_8BIT);
                if (data == nullptr) {
                    throw std::runtime_error("Failed to allocate memory for image: " + url);
                }
                size_t total_read = 0;
                while (total_read < content_length) {
                    int ret = http->Read(data + total_read, content_length - total_read);
                    if (ret < 0) {
                        heap_caps_free(data);
                        throw std::runtime_error("Failed to download image: " + url);
                    }
                    if (ret == 0) {
                        break;
                    }
                    total_read += ret;
                }
                http->Close();

                auto image = std::make_unique<LvglAllocatedImage>(data, content_length);
                display->SetPreviewImage(std::move(image));
                return true;
            });
#endif
    }
#endif

    auto& assets = Assets::GetInstance();
    if (assets.partition_valid()) {
        AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
                        PropertyList({Property("url", kPropertyTypeString)}),
                        [](const PropertyList& properties) -> ReturnValue {
                            auto url = properties["url"].value<std::string>();
                            Settings settings("assets", true);
                            settings.SetString("download_url", url);
                            return true;
                        });
    }
}

void McpServer::AddTool(McpTool* tool) {
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool* t) {
            return t->name() == tool->name();
        }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s%s", tool->name().c_str(), tool->user_only() ? " [user]" : "");
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string& name, const std::string& description,
                        const PropertyList& properties,
                        std::function<ReturnValue(const PropertyList&)> callback) {
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::AddUserOnlyTool(const std::string& name, const std::string& description,
                                const PropertyList& properties,
                                std::function<ReturnValue(const PropertyList&)> callback) {
    auto tool = new McpTool(name, description, properties, callback);
    tool->set_user_only(true);
    AddTool(tool);
}

void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON* json) {
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) ||
        strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }

    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        return;
    }

    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) {
        return;
    }

    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;

    if (method_str == "initialize") {
        if (cJSON_IsObject(params)) {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities)) {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message =
            "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{"
            "\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    } else if (method_str == "tools/list") {
        std::string cursor_str = "";
        bool list_user_only_tools = false;
        if (params != nullptr) {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = std::string(cursor->valuestring);
            }
            auto with_user_tools = cJSON_GetObjectItem(params, "withUserTools");
            if (cJSON_IsBool(with_user_tools)) {
                list_user_only_tools = with_user_tools->valueint == 1;
            }
        }
        GetToolsList(id_int, cursor_str, list_user_only_tools);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name)) {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments)) {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string& result) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string& message) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string& cursor, bool list_user_only_tools) {
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";

    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";

    while (it != tools_.end()) {
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }

        if (!list_user_only_tools && (*it)->user_only()) {
            ++it;
            continue;
        }

        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            next_cursor = (*it)->name();
            break;
        }

        json += tool_json;
        ++it;
    }

    if (json.back() == ',') {
        json.pop_back();
    }

    if (json.back() == '[' && !tools_.empty()) {
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit",
                 next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }

    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), [&tool_name](const McpTool* tool) {
        return tool->name() == tool_name;
    });

    if (tool_iter == tools_.end()) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try {
        for (auto& argument : arguments) {
            bool found = false;
            if (cJSON_IsObject(tool_arguments)) {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value)) {
                    argument.set_value<int>(value->valueint);
                    found = true;
                } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found) {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    auto& app = Application::GetInstance();
    app.Schedule([this, id, tool_iter, arguments = std::move(arguments)]() {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        }
    });
}
