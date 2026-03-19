#ifndef SD_MANAGER_SERVICE_H
#define SD_MANAGER_SERVICE_H

#include <string>
#include <vector>

class SdManagerService {
public:
    static SdManagerService& GetInstance() {
        static SdManagerService instance;
        return instance;
    }

    struct FileInfo {
        std::string name;
        bool is_dir;
        size_t size;
    };

    bool DeleteFileOrFolder(const std::string& path);
    bool CreateFolder(const std::string& path);
    std::string ReadTextFile(const std::string& path);
    bool WriteTextFile(const std::string& path, const std::string& content);
    bool CopyFileTo(const std::string& src, const std::string& dest);
    std::string GetFileHash(const std::string& path);
    std::vector<FileInfo> ListDirectory(const std::string& path);

private:
    SdManagerService() = default;
    ~SdManagerService() = default;
};

#endif
