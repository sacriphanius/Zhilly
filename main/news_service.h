#ifndef NEWS_SERVICE_H
#define NEWS_SERVICE_H

#include <map>
#include <string>

 
 

class NewsService {
public:
    static NewsService& GetInstance() {
        static NewsService instance;
        return instance;
    }

     
     
     
    std::string GetTopHeadlines(const std::string& country, const std::string& category);

private:
    NewsService() = default;
    ~NewsService() = default;

     
    NewsService(const NewsService&) = delete;
    NewsService& operator=(const NewsService&) = delete;

    std::string api_key_ = "b109915a04274ecfb70bcfb6f46d85f6";
};

#endif   
