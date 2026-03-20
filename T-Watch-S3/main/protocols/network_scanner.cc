#include "network_scanner.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_netif_net_stack.h>
#include <esp_wifi.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/icmp.h>
#include <lwip/raw.h>
#include <lwip/etharp.h>
#include <lwip/inet_chksum.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board.h"
#include "display.h"

#define TAG "NetworkScanner"

static const int SCAN_TIMEOUT_MS = 200;

const uint16_t NetworkScanner::TOP_PORTS[] = {
    21, 22, 23, 25, 53, 80, 110, 111, 135, 139, 143, 443, 445, 993, 995,
    1723, 3306, 3389, 5900, 8080, 8443
};
const size_t NetworkScanner::TOP_PORTS_COUNT = sizeof(NetworkScanner::TOP_PORTS) / sizeof(NetworkScanner::TOP_PORTS[0]);

NetworkScanner& NetworkScanner::GetInstance() {
    static NetworkScanner instance;
    return instance;
}

NetworkScanner::NetworkScanner() : is_scanning_(false), stop_requested_(false) {}

NetworkScanner::~NetworkScanner() {}

void NetworkScanner::StopScan() {
    stop_requested_ = true;
}

bool NetworkScanner::GetNetworkInfo(uint32_t& ip, uint32_t& netmask) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGE(TAG, "No WiFi STA interface found");
        return false;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info");
        return false;
    }

    if (ip_info.ip.addr == 0) {
        ESP_LOGW(TAG, "WiFi not connected (IP is 0)");
        return false;
    }

    ip = ip_info.ip.addr;
    netmask = ip_info.netmask.addr;
    return true;
}

bool NetworkScanner::CheckPort(const std::string& ip_str, uint16_t port, int timeout_ms) {
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(ip_str.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        return false;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    int res = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (res < 0 && errno != EINPROGRESS) {
        close(sock);
        return false;
    }

    if (res == 0) {

        close(sock);
        return true;
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    res = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    if (res == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0) {
            close(sock);
            return true;
        }
    }

    close(sock);
    return false;
}

void NetworkScanner::CheckPortsBatch(const std::vector<std::string>& ips, const std::vector<uint16_t>& ports, int timeout_ms, std::set<std::string>& out_active_ips, std::set<uint16_t>& out_active_ports) {
    if (ips.empty() || ports.empty()) return;

    int max_fd = 0;
    fd_set fdset;
    FD_ZERO(&fdset);

    struct SocketInfo {
        std::string ip;
        uint16_t port;
    };
    std::map<int, SocketInfo> fd_to_info;

    for (const auto& ip : ips) {
        for (uint16_t port : ports) {
            int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
            if (sock < 0) continue;

            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);

            struct sockaddr_in dest_addr;
            dest_addr.sin_addr.s_addr = inet_addr(ip.c_str());
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(port);

            int res = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (res == 0) {
                out_active_ips.insert(ip);
                out_active_ports.insert(port);
                close(sock);
            } else if (res < 0 && errno == EINPROGRESS) {
                FD_SET(sock, &fdset);
                fd_to_info[sock] = {ip, port};
                if (sock > max_fd) max_fd = sock;
            } else {
                close(sock);
            }
        }
    }

    if (max_fd > 0) {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int res = select(max_fd + 1, nullptr, &fdset, nullptr, &tv);
        if (res > 0) {
            for (auto const& pair : fd_to_info) {
                int sock = pair.first;
                if (FD_ISSET(sock, &fdset)) {
                    int so_error = 0;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if (so_error == 0) {
                        out_active_ips.insert(pair.second.ip);
                        out_active_ports.insert(pair.second.port);
                    }
                }
            }
        }

        for (auto const& pair : fd_to_info) {
            close(pair.first);
        }
    }
}

std::vector<HostInfo> NetworkScanner::DiscoverHosts() {
    std::vector<HostInfo> active_hosts;
    if (is_scanning_) {
        ESP_LOGW(TAG, "Scan already in progress");
        return active_hosts;
    }

    auto display = Board::GetInstance().GetDisplay();

    uint32_t my_ip_addr, netmask_addr;
    if (!GetNetworkInfo(my_ip_addr, netmask_addr)) {
        if (display) display->ShowNotification("Not connected\nto WiFi!");
        return active_hosts;
    }

    esp_netif_t* esp_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!esp_netif) {
        ESP_LOGE(TAG, "WIFI_STA_DEF not found");
        return active_hosts;
    }

    struct netif *net_iface = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    if (!net_iface) {
        ESP_LOGE(TAG, "LWIP netif not found");
        return active_hosts;
    }

    is_scanning_ = true;
    stop_requested_ = false;

    uint32_t network_addr = my_ip_addr & netmask_addr;
    uint32_t broadcast_addr = network_addr | ~netmask_addr;

    uint32_t start_ip = ntohl(network_addr) + 1;
    uint32_t end_ip = ntohl(broadcast_addr) - 1;

    if (end_ip - start_ip > 254) {
        end_ip = start_ip + 254;

    }

    if (display) display->ShowNotification("Please wait,\nScanning...");
    vTaskDelay(pdMS_TO_TICKS(1500));

    etharp_cleanup_netif(net_iface);

    char display_buf[64];
    int table_read_counter = 0;

    for (uint32_t curr = start_ip; curr <= end_ip && !stop_requested_; ++curr) {
        struct in_addr addr;
        addr.s_addr = htonl(curr);
        std::string ip_str = inet_ntoa(addr);

        if (addr.s_addr == my_ip_addr) continue;

        if (curr % 15 == 0) {
            snprintf(display_buf, sizeof(display_buf), "Scanning:\n%s...", ip_str.c_str());
            if (display) display->ShowNotification(display_buf);
        }

        ip4_addr_t ip_be;
        ip_be.addr = addr.s_addr;

        etharp_request(net_iface, &ip_be);
        table_read_counter++;

        vTaskDelay(pdMS_TO_TICKS(15));

        if (table_read_counter >= 10 || curr == end_ip) {

            for (int i = 0; i < ARP_TABLE_SIZE; ++i) {
                ip4_addr_t *ip_ret;
                netif *iface_ret;
                eth_addr *eth_ret;

                if (etharp_get_entry(i, &ip_ret, &iface_ret, &eth_ret)) {
                    if (ip_ret == nullptr || eth_ret == nullptr) continue;

                    std::string found_ip = inet_ntoa(*(in_addr*)&ip_ret->addr);

                    if (ip_ret->addr != my_ip_addr && ip_ret->addr != 0) {

                        bool exists = false;
                        for (const auto& h : active_hosts) {
                            if (h.ip == found_ip) { exists = true; break; }
                        }

                        if (!exists) {
                            HostInfo host;
                            host.ip = found_ip;

                            char mac_buf[18];
                            snprintf(mac_buf, sizeof(mac_buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                                eth_ret->addr[0], eth_ret->addr[1], eth_ret->addr[2],
                                eth_ret->addr[3], eth_ret->addr[4], eth_ret->addr[5]);

                            host.mac = mac_buf;
                            host.is_active = true;
                            active_hosts.push_back(host);

                            snprintf(display_buf, sizeof(display_buf), "%s\n%s", found_ip.c_str(), mac_buf);
                            if (display) display->ShowNotification(display_buf);

                            vTaskDelay(pdMS_TO_TICKS(1000));
                            ESP_LOGI(TAG, "ARP Found host: %s MAC: %s", found_ip.c_str(), mac_buf);
                        }
                    }
                }
            }

            etharp_cleanup_netif(net_iface);
            table_read_counter = 0;
        }
    }

    if (!stop_requested_) {
        snprintf(display_buf, sizeof(display_buf), "Scan done!\n%d devices", (int)active_hosts.size());
        if (display) display->ShowNotification(display_buf);
    }

    is_scanning_ = false;
    return active_hosts;
}

std::vector<PortInfo> NetworkScanner::ScanPorts(const std::string& target_ip, const std::string& port_range) {
    std::vector<PortInfo> open_ports;
    if (is_scanning_) {
        ESP_LOGW(TAG, "Scan already in progress");
        return open_ports;

    }

    auto display = Board::GetInstance().GetDisplay();

    is_scanning_ = true;
    stop_requested_ = false;

    char display_buf[64];
    snprintf(display_buf, sizeof(display_buf), "%s:\nPorts...", target_ip.c_str());
    if (display) display->ShowNotification(display_buf);
    vTaskDelay(pdMS_TO_TICKS(1500));

    std::vector<std::string> batch_ips = {target_ip};
    std::vector<uint16_t> batch_ports;

    const int MAX_BATCH_PORTS = 8;

    for (size_t i = 0; i < TOP_PORTS_COUNT && !stop_requested_; ++i) {
        batch_ports.push_back(TOP_PORTS[i]);

        if (batch_ports.size() >= MAX_BATCH_PORTS || i == TOP_PORTS_COUNT - 1) {
            std::set<std::string> active_ips;
            std::set<uint16_t> active_ports;

            CheckPortsBatch(batch_ips, batch_ports, 400, active_ips, active_ports);

            for (uint16_t p : active_ports) {
                PortInfo pi;
                pi.port = p;
                pi.is_open = true;
                open_ports.push_back(pi);

                snprintf(display_buf, sizeof(display_buf), "Port %d OPEN!", p);
                if (display) display->ShowNotification(display_buf);
                ESP_LOGI(TAG, "Host %s Port %d is OPEN", target_ip.c_str(), p);

                vTaskDelay(pdMS_TO_TICKS(1500));
            }

            batch_ports.clear();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    if (!stop_requested_) {
        snprintf(display_buf, sizeof(display_buf), "Port Scan\ndone!");
        if (display) display->ShowNotification(display_buf);
    }

    is_scanning_ = false;
    return open_ports;
}

std::string NetworkScanner::ResolveDomain(const std::string& domain) {
    auto display = Board::GetInstance().GetDisplay();

    char display_buf[64];
    snprintf(display_buf, sizeof(display_buf), "Resolving:\n%s", domain.c_str());
    if (display) display->ShowNotification(display_buf);

    struct hostent* he = gethostbyname(domain.c_str());
    if (he == nullptr || he->h_addr_list[0] == nullptr) {
        ESP_LOGE(TAG, "Failed to resolve domain: %s", domain.c_str());
        snprintf(display_buf, sizeof(display_buf), "Not found:\n%s", domain.c_str());
        if (display) display->ShowNotification(display_buf);
        return "";
    }

    struct in_addr** addr_list = (struct in_addr**)he->h_addr_list;
    std::string ip_str = inet_ntoa(*addr_list[0]);

    ESP_LOGI(TAG, "Resolved %s to %s", domain.c_str(), ip_str.c_str());

    snprintf(display_buf, sizeof(display_buf), "IP Address:\n%s", ip_str.c_str());
    if (display) display->ShowNotification(display_buf);

    return ip_str;
}