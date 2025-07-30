#include "order_book.hpp"
#include <cstdlib>
#include <algorithm>

// --------- helpers for parsing simple numeric strings ----------
static inline uint32_t to_u32(const std::string& s) {
    if (s.empty()) return 0u;
    return static_cast<uint32_t>(std::strtoul(s.c_str(), nullptr, 10));
}
static inline double to_price(const std::string& s) {
    if (s.empty()) return 0.0;
    return std::strtod(s.c_str(), nullptr);
}

// ----------------------------------------------------------------

ChangeType OrderBook::processEvent(const std::string& action,
                                   const std::string& sideStr,
                                   const std::string& priceStr,
                                   const std::string& sizeStr,
                                   const std::string& orderIdStr,
                                   const std::string& flagsStr) {
    const char side = sideStr.empty() ? 'N' : sideStr[0];
    const double price = to_price(priceStr);
    const uint32_t size = to_u32(sizeStr);
    const uint32_t flags = to_u32(flagsStr);

    // Databento flags of interest
    const bool isTOB = (flags & 64) != 0; // F_TOB
    const bool isMBP = (flags & 16) != 0; // F_MBP

    // None
    if (action == "N") {
        return ChangeType::NoChange;
    }

    // Clear book
    if (action == "R") {
        clearAll();
        // reset any in-flight trade synthesis
        active_trade_ = false;
        trade_side_ = 'N';
        return ChangeType::BookChange;
    }

    // -------------------------- ADD --------------------------
    if (action == "A") {
        if (isTOB) {
            // Top-of-book normalized feed: replace the entire side
            if (isBid(side)) {
                bids_.clear();
                if (price != 0.0 && size > 0) {
                    bids_[price] = { size, 1 };
                }
            } else if (isAsk(side)) {
                asks_.clear();
                if (price != 0.0 && size > 0) {
                    asks_[price] = { size, 1 };
                }
            }
            return ChangeType::BookChange;
        }

        if (isMBP) {
            // Aggregated MBP message: set level directly; ignore order_id
            if (isBid(side)) {
                bids_[price] = { size, 1 };
            } else if (isAsk(side)) {
                asks_[price] = { size, 1 };
            }
            return ChangeType::BookChange;
        }

        // Normal MBO add
        addOrder(orderIdStr, side, price, size);
        return ChangeType::BookChange;
    }

    // -------------------------- MODIFY --------------------------
    if (action == "M") {
        if (isMBP) {
            // Aggregated MBP modify: set level directly
            if (isBid(side)) {
                bids_[price] = { size, 1 };
            } else if (isAsk(side)) {
                asks_[price] = { size, 1 };
            }
            return ChangeType::BookChange;
        }

        // Normal MBO modify (by order_id)
        modifyOrder(orderIdStr, price, size);
        return ChangeType::BookChange;
    }

    // -------------------------- CANCEL --------------------------
    if (action == "C") {
        if (isMBP) {
            // Aggregated MBP cancel: remove entire level (size often 0)
            if (isBid(side)) {
                if (size == 0) {
                    bids_.erase(price);
                } else {
                    // if size carries remaining qty, set level to that
                    if (size > 0) bids_[price] = { size, 1 };
                }
            } else if (isAsk(side)) {
                if (size == 0) {
                    asks_.erase(price);
                } else {
                    if (size > 0) asks_[price] = { size, 1 };
                }
            }
            // Trade sequence ends after cancel in most T/F/C pipelines
            active_trade_ = false;
            trade_side_ = 'N';
            return ChangeType::BookChange;
        }

        // Normal MBO cancel (full/partial by size against order_id)
        reduceOrCancel(orderIdStr, size);
        active_trade_ = false;
        trade_side_ = 'N';
        return ChangeType::BookChange;
    }

    // -------------------------- TRADE / FILL --------------------------
    if (action == "T") {
        // Start a trade sequence; T itself doesn't change the book
        if (side == 'N') {
            // per spec: ignore T with side None
            active_trade_ = false;
            trade_side_ = 'N';
            return ChangeType::NoChange;
        }
        active_trade_ = true;
        trade_side_ = side; // aggressor side
        return ChangeType::NoChange;
    }

    if (action == "F") {
        // Fill reduces the resting side.
        // For MBP-aggregated messages (isMBP), reduce the level directly by price.
        if (isMBP) {
            if (isBid(side)) {
                auto it = bids_.find(price);
                if (it != bids_.end()) {
                    if (size == 0 || it->second.size <= size) {
                        bids_.erase(it);
                    } else {
                        it->second.size -= size;
                    }
                }
            } else if (isAsk(side)) {
                auto it = asks_.find(price);
                if (it != asks_.end()) {
                    if (size == 0 || it->second.size <= size) {
                        asks_.erase(it);
                    } else {
                        it->second.size -= size;
                    }
                }
            }
            // Emit synthetic trade snapshot
            return ChangeType::SyntheticTrade;
        }

        // For normal MBO: reduce the specific order_id by 'size'
        // (This effectively applies the fill to the resting side order.)
        if (!orderIdStr.empty()) {
            reduceOrCancel(orderIdStr, size);
        }

        if (active_trade_ && trade_side_ != 'N') {
            // Emit a synthetic trade snapshot (writer will label action 'T')
            return ChangeType::SyntheticTrade;
        }
        return ChangeType::BookChange;
    }

    // Unknown action
    return ChangeType::NoChange;
}

// -------------------------- Internal Ops --------------------------

void OrderBook::addOrder(const std::string& orderId, char side, double px, uint32_t sz) {
    if (!isBid(side) && !isAsk(side)) return;

    orders_[orderId] = { px, sz, side };
    if (isBid(side)) {
        incLevel(bids_, px, sz);        // +size, +count
    } else {
        incLevel(asks_, px, sz);        // +size, +count
    }
}

void OrderBook::modifyOrder(const std::string& orderId, double newPx, uint32_t newSz) {
    auto it = orders_.find(orderId);
    if (it == orders_.end()) {
        // Defensive: if modify arrives without prior add, treat like add with side unknown
        // (You may choose to ignore instead.)
        return;
    }
    Order& o = it->second;

    // Remove old contribution completely (size + count), then re-add new
    if (isBid(o.side)) {
        subSizeCount(bids_, o.price, o.size, /*decCount=*/true);
    } else {
        subSizeCount(asks_, o.price, o.size, /*decCount=*/true);
    }

    // Update order
    o.price = newPx;
    o.size  = newSz;

    // Re-add
    if (isBid(o.side)) {
        incLevel(bids_, o.price, o.size);
    } else {
        incLevel(asks_, o.price, o.size);
    }
}

void OrderBook::reduceOrCancel(const std::string& orderId, uint32_t reduceBy) {
    auto it = orders_.find(orderId);
    if (it == orders_.end()) return;

    Order& o = it->second;
    uint32_t dec = (reduceBy == 0) ? o.size : std::min(o.size, reduceBy);

    if (isBid(o.side)) {
        // decrement size; decrement count if this order fully removed
        subSizeCount(bids_, o.price, dec, /*decCount=*/(dec == o.size));
    } else {
        subSizeCount(asks_, o.price, dec, /*decCount=*/(dec == o.size));
    }

    if (dec >= o.size) {
        // fully cancelled/filled
        orders_.erase(it);
    } else {
        // partial
        o.size -= dec;
    }
}

void OrderBook::clearAll() {
    orders_.clear();
    bids_.clear();
    asks_.clear();
}
