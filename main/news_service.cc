#include "news_service.h"
#include <esp_http_client.h>
#include <esp_log.h>
#include <cJSON.h>
#include <sstream>
#include <vector>

#define TAG "NewsService"

esp_err_t _http_event_handle(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (!esp_http_client_is_chunked_response(evt->client)) {
            std::string* response_str = (std::string*)evt->user_data;
            response_str->append((char*)evt->data, evt->data_len);
        }
    }
    return ESP_OK;
}

std::string NewsService::GetTopHeadlines(const std::string& country, const std::string& category) {
    std::ostringstream url_ss;
    url_ss << "https://newsapi.org/v2/top-headlines?country=" << (country.empty() ? "tr" : country);

    if (!category.empty()) {
        url_ss << "&category=" << category;
    }
    url_ss << "&apiKey=" << api_key_;

    std::string url = url_ss.str();
    std::string response_data;

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.event_handler = _http_event_handle;
    config.user_data = &response_data;

    config.timeout_ms = 10000;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return "Failed to start HTTP client.";
    }

    ESP_LOGI(TAG, "Fetching news from: %s", url.c_str());
    esp_err_t err = esp_http_client_perform(client);

    std::string result_str;

    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", status_code,
                 esp_http_client_get_content_length(client));

        if (status_code == 200) {
            cJSON* json = cJSON_Parse(response_data.c_str());
            if (json == nullptr) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                result_str = "Failed to read news (JSON parse error).";
            } else {
                cJSON* status = cJSON_GetObjectItemCaseSensitive(json, "status");
                if (status && cJSON_IsString(status) && std::string(status->valuestring) == "ok") {
                    cJSON* articles = cJSON_GetObjectItemCaseSensitive(json, "articles");
                    if (articles && cJSON_IsArray(articles)) {
                        int count = cJSON_GetArraySize(articles);
                        if (count == 0) {
                            result_str = "No recent news found at the moment.";
                        } else {
                            std::ostringstream res_ss;
                            res_ss << "Latest News (" << (country.empty() ? "tr" : country)
                                   << "):\n";

                            int limit = (count < 5) ? count : 5;
                            for (int i = 0; i < limit; i++) {
                                cJSON* article = cJSON_GetArrayItem(articles, i);
                                cJSON* title = cJSON_GetObjectItemCaseSensitive(article, "title");
                                if (title && cJSON_IsString(title)) {
                                    res_ss << "- " << title->valuestring << "\n";
                                }
                            }
                            result_str = res_ss.str();
                        }
                    } else {
                        result_str = "No news found.";
                    }
                } else {
                    ESP_LOGE(TAG, "NewsAPI returned non-ok status");
                    cJSON* message = cJSON_GetObjectItemCaseSensitive(json, "message");
                    if (message && cJSON_IsString(message)) {
                        result_str = std::string("API Error: ") + message->valuestring;
                    } else {
                        result_str = "Error returned from News API server.";
                    }
                }
                cJSON_Delete(json);
            }
        } else {
            result_str = "News unreachable. Server error: " + std::to_string(status_code);
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        result_str = "Internet connection error or news unreachable.";
    }

    esp_http_client_cleanup(client);
    return result_str;
}
