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

    // Performs a ping sweep on the local subnet (e.g., 192.168.1.1-254)
    // Non-blocking scan, reports results via EmoteDisplay notifications
    std::vector<HostInfo> DiscoverHosts();

    // Performs a TCP connect scan on the specified IP for top common ports
    std::vector<PortInfo> ScanPorts(const std::string& target_ip, const std::string& port_range = "");

    // Resolves a given domain name to its IPv4 address
    std::string ResolveDomain(const std::string& domain);

    void StopScan();
    bool IsScanning() const { return is_scanning_; }

private:
    NetworkScanner();
    ~NetworkScanner();

    // Gets local IP and subnet
    bool GetNetworkInfo(uint32_t& ip, uint32_t& netmask);

    volatile bool is_scanning_;
    volatile bool stop_requested_;

    // Helper for TCP connect
    bool CheckPort(const std::string& ip, uint16_t port, int timeout_ms = 500);
    
    // Helper to evaluate multiple IPs and ports in a single select() call for maximum speed
    void CheckPortsBatch(const std::vector<std::string>& ips, const std::vector<uint16_t>& ports, int timeout_ms, std::set<std::string>& out_active_ips, std::set<uint16_t>& out_active_ports);

    // List of top ports to cover
    static const uint16_t TOP_PORTS[];
    static const size_t TOP_PORTS_COUNT;
};

#endif // NETWORK_SCANNER_H
