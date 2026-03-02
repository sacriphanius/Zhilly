#include "emoji_collection.h"

#include <esp_log.h>
#include <unordered_map>
#include <string>

#define TAG "EmojiCollection"

void EmojiCollection::AddEmoji(const std::string& name, LvglImage* image) {
    emoji_collection_[name] = image;
}

const LvglImage* EmojiCollection::GetEmojiImage(const char* name) {
    auto it = emoji_collection_.find(name);
    if (it != emoji_collection_.end()) {
        return it->second;
    }

    ESP_LOGW(TAG, "Emoji not found: %s", name);
    return nullptr;
}

EmojiCollection::~EmojiCollection() {
    for (auto it = emoji_collection_.begin(); it != emoji_collection_.end(); ++it) {
        delete it->second;
    }
    emoji_collection_.clear();
}

extern const lv_image_dsc_t emoji_1f636_32; 
extern const lv_image_dsc_t emoji_1f642_32; 
extern const lv_image_dsc_t emoji_1f606_32; 
extern const lv_image_dsc_t emoji_1f602_32; 
extern const lv_image_dsc_t emoji_1f614_32; 
extern const lv_image_dsc_t emoji_1f620_32; 
extern const lv_image_dsc_t emoji_1f62d_32; 
extern const lv_image_dsc_t emoji_1f60d_32; 
extern const lv_image_dsc_t emoji_1f633_32; 
extern const lv_image_dsc_t emoji_1f62f_32; 
extern const lv_image_dsc_t emoji_1f631_32; 
extern const lv_image_dsc_t emoji_1f914_32; 
extern const lv_image_dsc_t emoji_1f609_32; 
extern const lv_image_dsc_t emoji_1f60e_32; 
extern const lv_image_dsc_t emoji_1f60c_32; 
extern const lv_image_dsc_t emoji_1f924_32; 
extern const lv_image_dsc_t emoji_1f618_32; 
extern const lv_image_dsc_t emoji_1f60f_32; 
extern const lv_image_dsc_t emoji_1f634_32; 
extern const lv_image_dsc_t emoji_1f61c_32; 
extern const lv_image_dsc_t emoji_1f644_32; 

Twemoji32::Twemoji32() {
    AddEmoji("neutral", new LvglSourceImage(&emoji_1f636_32));
    AddEmoji("happy", new LvglSourceImage(&emoji_1f642_32));
    AddEmoji("laughing", new LvglSourceImage(&emoji_1f606_32));
    AddEmoji("funny", new LvglSourceImage(&emoji_1f602_32));
    AddEmoji("sad", new LvglSourceImage(&emoji_1f614_32));
    AddEmoji("angry", new LvglSourceImage(&emoji_1f620_32));
    AddEmoji("crying", new LvglSourceImage(&emoji_1f62d_32));
    AddEmoji("loving", new LvglSourceImage(&emoji_1f60d_32));
    AddEmoji("embarrassed", new LvglSourceImage(&emoji_1f633_32));
    AddEmoji("surprised", new LvglSourceImage(&emoji_1f62f_32));
    AddEmoji("shocked", new LvglSourceImage(&emoji_1f631_32));
    AddEmoji("thinking", new LvglSourceImage(&emoji_1f914_32));
    AddEmoji("winking", new LvglSourceImage(&emoji_1f609_32));
    AddEmoji("cool", new LvglSourceImage(&emoji_1f60e_32));
    AddEmoji("relaxed", new LvglSourceImage(&emoji_1f60c_32));
    AddEmoji("delicious", new LvglSourceImage(&emoji_1f924_32));
    AddEmoji("kissy", new LvglSourceImage(&emoji_1f618_32));
    AddEmoji("confident", new LvglSourceImage(&emoji_1f60f_32));
    AddEmoji("sleepy", new LvglSourceImage(&emoji_1f634_32));
    AddEmoji("silly", new LvglSourceImage(&emoji_1f61c_32));
    AddEmoji("confused", new LvglSourceImage(&emoji_1f644_32));
}

extern const lv_image_dsc_t emoji_1f636_64; 
extern const lv_image_dsc_t emoji_1f642_64; 
extern const lv_image_dsc_t emoji_1f606_64; 
extern const lv_image_dsc_t emoji_1f602_64; 
extern const lv_image_dsc_t emoji_1f614_64; 
extern const lv_image_dsc_t emoji_1f620_64; 
extern const lv_image_dsc_t emoji_1f62d_64; 
extern const lv_image_dsc_t emoji_1f60d_64; 
extern const lv_image_dsc_t emoji_1f633_64; 
extern const lv_image_dsc_t emoji_1f62f_64; 
extern const lv_image_dsc_t emoji_1f631_64; 
extern const lv_image_dsc_t emoji_1f914_64; 
extern const lv_image_dsc_t emoji_1f609_64; 
extern const lv_image_dsc_t emoji_1f60e_64; 
extern const lv_image_dsc_t emoji_1f60c_64; 
extern const lv_image_dsc_t emoji_1f924_64; 
extern const lv_image_dsc_t emoji_1f618_64; 
extern const lv_image_dsc_t emoji_1f60f_64; 
extern const lv_image_dsc_t emoji_1f634_64; 
extern const lv_image_dsc_t emoji_1f61c_64; 
extern const lv_image_dsc_t emoji_1f644_64; 

Twemoji64::Twemoji64() {
    AddEmoji("neutral", new LvglSourceImage(&emoji_1f636_64));
    AddEmoji("happy", new LvglSourceImage(&emoji_1f642_64));
    AddEmoji("laughing", new LvglSourceImage(&emoji_1f606_64));
    AddEmoji("funny", new LvglSourceImage(&emoji_1f602_64));
    AddEmoji("sad", new LvglSourceImage(&emoji_1f614_64));
    AddEmoji("angry", new LvglSourceImage(&emoji_1f620_64));
    AddEmoji("crying", new LvglSourceImage(&emoji_1f62d_64));
    AddEmoji("loving", new LvglSourceImage(&emoji_1f60d_64));
    AddEmoji("embarrassed", new LvglSourceImage(&emoji_1f633_64));
    AddEmoji("surprised", new LvglSourceImage(&emoji_1f62f_64));
    AddEmoji("shocked", new LvglSourceImage(&emoji_1f631_64));
    AddEmoji("thinking", new LvglSourceImage(&emoji_1f914_64));
    AddEmoji("winking", new LvglSourceImage(&emoji_1f609_64));
    AddEmoji("cool", new LvglSourceImage(&emoji_1f60e_64));
    AddEmoji("relaxed", new LvglSourceImage(&emoji_1f60c_64));
    AddEmoji("delicious", new LvglSourceImage(&emoji_1f924_64));
    AddEmoji("kissy", new LvglSourceImage(&emoji_1f618_64));
    AddEmoji("confident", new LvglSourceImage(&emoji_1f60f_64));
    AddEmoji("sleepy", new LvglSourceImage(&emoji_1f634_64));
    AddEmoji("silly", new LvglSourceImage(&emoji_1f61c_64));
    AddEmoji("confused", new LvglSourceImage(&emoji_1f644_64));
}
