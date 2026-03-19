#include "sd_manager_service.h"
#include <esp_log.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <cstdio>
#include "mbedtls/md5.h"

#define TAG "SdManager"

static std::string GetFullPath(const std::string& path) {
    if (path.find("/sdcard") == 0) {
        return path;
    }
    if (path.empty() || path[0] != '/') {
        return std::string("/sdcard/") + path;
    }
    return std::string("/sdcard") + path;
}

bool SdManagerService::DeleteFileOrFolder(const std::string& path) {
    std::string full_path = GetFullPath(path);
    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (rmdir(full_path.c_str()) == 0) {
                ESP_LOGI(TAG, "Deleted folder: %s", full_path.c_str());
                return true;
            }
        } else {
            if (unlink(full_path.c_str()) == 0) {
                ESP_LOGI(TAG, "Deleted file: %s", full_path.c_str());
                return true;
            }
        }
    }
    ESP_LOGE(TAG, "Failed to delete: %s", full_path.c_str());
    return false;
}

bool SdManagerService::CreateFolder(const std::string& path) {
    std::string full_path = GetFullPath(path);
    if (mkdir(full_path.c_str(), 0755) == 0) {
        ESP_LOGI(TAG, "Created folder: %s", full_path.c_str());
        return true;
    }
    ESP_LOGE(TAG, "Failed to create folder: %s", full_path.c_str());
    return false;
}

std::string SdManagerService::ReadTextFile(const std::string& path) {
    std::string full_path = GetFullPath(path);
    FILE* f = fopen(full_path.c_str(), "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for reading: %s", full_path.c_str());
        return "";
    }
    
    std::string result;
    char buffer[256];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        result.append(buffer, bytes);
        if (result.size() > 200 * 1024) { 
            // Anti-OOM limit
            ESP_LOGW(TAG, "File too large, truncating to 200KB: %s", full_path.c_str());
            break; 
        }
    }
    fclose(f);
    return result;
}

bool SdManagerService::WriteTextFile(const std::string& path, const std::string& content) {
    std::string full_path = GetFullPath(path);
    FILE* f = fopen(full_path.c_str(), "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", full_path.c_str());
        return false;
    }
    fwrite(content.c_str(), 1, content.size(), f);
    fclose(f);
    ESP_LOGI(TAG, "Wrote to file: %s", full_path.c_str());
    return true;
}

bool SdManagerService::CopyFileTo(const std::string& src, const std::string& dest) {
    std::string full_src = GetFullPath(src);
    std::string full_dest = GetFullPath(dest);
    
    FILE* source = fopen(full_src.c_str(), "rb");
    if (!source) {
        ESP_LOGE(TAG, "Copy: source not found %s", full_src.c_str());
        return false;
    }
    FILE* destination = fopen(full_dest.c_str(), "wb");
    if (!destination) {
        ESP_LOGE(TAG, "Copy: dest cannot output %s", full_dest.c_str());
        fclose(source);
        return false;
    }
    
    char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, bytes, destination);
    }
    fclose(source);
    fclose(destination);
    ESP_LOGI(TAG, "Copied %s -> %s", full_src.c_str(), full_dest.c_str());
    return true;
}

std::string SdManagerService::GetFileHash(const std::string& path) {
    std::string full_path = GetFullPath(path);
    FILE* f = fopen(full_path.c_str(), "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for hashing: %s", full_path.c_str());
        return "";
    }

    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    
    unsigned char buffer[1024];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        mbedtls_md5_update(&ctx, buffer, bytes);
    }
    fclose(f);

    unsigned char md5sum[16];
    mbedtls_md5_finish(&ctx, md5sum);
    mbedtls_md5_free(&ctx);

    char hex[33];
    for(int i = 0; i < 16; ++i) {
        sprintf(&hex[i*2], "%02x", md5sum[i]);
    }
    return std::string(hex);
}

std::vector<SdManagerService::FileInfo> SdManagerService::ListDirectory(const std::string& path) {
    std::vector<FileInfo> files;
    std::string full_path = GetFullPath(path);
    DIR *dir = opendir(full_path.c_str());
    if (dir == nullptr) {
        ESP_LOGE(TAG, "Failed to open directory: %s", full_path.c_str());
        return files;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        FileInfo info;
        info.name = ent->d_name;
        info.is_dir = (ent->d_type == DT_DIR);
        
        std::string item_path = full_path;
        if (item_path.back() != '/') item_path += "/";
        item_path += info.name;
        
        struct stat st;
        if (stat(item_path.c_str(), &st) == 0) {
            info.size = st.st_size;
        } else {
            info.size = 0;
        }
        files.push_back(info);
    }
    closedir(dir);
    return files;
}

