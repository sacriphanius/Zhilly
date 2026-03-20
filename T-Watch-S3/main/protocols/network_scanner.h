#ifndef NETWORK_SCANNER_H
#define NETWORK_SCANNER_H

#include <vector>
#include <string>
#include <cstdint>
#include <set>
#include <map>

struct HostInfo {
    std::string ip;
    std::string mac;
    bool is_active;
};

struct PortInfo {
    uint16_t port;
    bool is_open;
    std::string service_name;
};

class NetworkScanner {
public:
    static NetworkScanner& GetInstance();

    std::vector<HostInfo> DiscoverHosts();

    std::vector<PortInfo> ScanPorts(const std::string& target_ip, const std::string& port_range = "");

    std::string ResolveDomain(const std::string& domain);

    void StopScan();
    bool IsScanning() const { return is_scanning_; }

private:
    NetworkScanner();
    ~NetworkScanner();

    bool GetNetworkInfo(uint32_t& ip, uint32_t& netmask);

    volatile bool is_scanning_;
    volatile bool stop_requested_;

    bool CheckPort(const std::string& ip, uint16_t port, int timeout_ms = 500);

    void CheckPortsBatch(const std::vector<std::string>& ips, const std::vector<uint16_t>& ports, int timeout_ms, std::set<std::string>& out_active_ips, std::set<uint16_t>& out_active_ports);

    static const uint16_t TOP_PORTS[];
    static const size_t TOP_PORTS_COUNT;
};

#endif
