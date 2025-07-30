#pragma once
#include <map>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <functional>

struct Order {
    double    price;
    uint32_t  size;
    char      side;    // 'B' or 'A'
};

// Per-price aggregated level
struct LevelAgg {
    uint32_t size = 0;   // total size at this price
    uint32_t count = 0;  // number of active orders at this price
};

enum class ChangeType {
    NoChange = 0,
    BookChange = 1,        // A/M/C/R
    SyntheticTrade = 2     // F (during an active T) reduced the resting side
};

class OrderBook {
public:
    // Returns what to emit
    ChangeType processEvent(const std::string& action,
                            const std::string& sideStr,
                            const std::string& priceStr,
                            const std::string& sizeStr,
                            const std::string& orderIdStr,
                            const std::string& flagsStr);

    // Aggregated books
    const std::map<double, LevelAgg, std::greater<double>>& bids() const { return bids_; }
    const std::map<double, LevelAgg>& asks() const { return asks_; }

    // Helpers for MBP writer
    static inline bool isBid(char s) { return s == 'B' || s == 'b'; }
    static inline bool isAsk(char s) { return s == 'A' || s == 'a'; }

private:
    // State
    std::unordered_map<std::string, Order> orders_;  // order_id -> Order
    std::map<double, LevelAgg, std::greater<double>> bids_; // bid: high→low
    std::map<double, LevelAgg> asks_;                           // ask: low→high

    // Trade synthesis state
    bool active_trade_ = false;   // seen a T and side != 'N'
    char trade_side_ = 'N';       // aggressor side from T ('B' or 'A')

    // Internal ops
    void addOrder(const std::string& orderId, char side, double price, uint32_t size);
    void modifyOrder(const std::string& orderId, double newPrice, uint32_t newSize);
    void reduceOrCancel(const std::string& orderId, uint32_t reduceBy); // used by C and F
    void clearAll();

    // Level helpers
    static void incLevel(std::map<double, LevelAgg, std::greater<double>>& book, double px, uint32_t sz) {
        auto& lvl = book[px];
        lvl.size += sz;
        lvl.count += 1;
    }
    static void incLevel(std::map<double, LevelAgg>& book, double px, uint32_t sz) {
        auto& lvl = book[px];
        lvl.size += sz;
        lvl.count += 1;
    }
    static void addSizeOnly(std::map<double, LevelAgg, std::greater<double>>& book, double px, uint32_t sz) {
        auto& lvl = book[px];
        lvl.size += sz;
    }
    static void addSizeOnly(std::map<double, LevelAgg>& book, double px, uint32_t sz) {
        auto& lvl = book[px];
        lvl.size += sz;
    }
    static void subSizeCount(std::map<double, LevelAgg, std::greater<double>>& book, double px, uint32_t sz, bool decCount) {
        auto it = book.find(px);
        if (it == book.end()) return;
        if (it->second.size > sz) it->second.size -= sz; else it->second.size = 0;
        if (decCount && it->second.count > 0) it->second.count -= 1;
        if (it->second.size == 0 && it->second.count == 0) book.erase(it);
    }
    static void subSizeCount(std::map<double, LevelAgg>& book, double px, uint32_t sz, bool decCount) {
        auto it = book.find(px);
        if (it == book.end()) return;
        if (it->second.size > sz) it->second.size -= sz; else it->second.size = 0;
        if (decCount && it->second.count > 0) it->second.count -= 1;
        if (it->second.size == 0 && it->second.count == 0) book.erase(it);
    }
};
