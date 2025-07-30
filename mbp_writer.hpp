#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include "order_book.hpp"

// Column indices we need from the input row (MBO)
struct ColIdx {
    int ts_recv = -1, ts_event = -1, rtype = -1, publisher_id = -1, instrument_id = -1;
    int action = -1, side = -1, depth = -1, price = -1, size = -1, flags = -1, ts_in_delta = -1, sequence = -1;
    int symbol = -1, order_id = -1;
};

class MBPWriter {
public:
    MBPWriter(const std::string& filename, const ColIdx& idx);
    ~MBPWriter();

    // Write MBP-10 snapshot, possibly overriding action to "T" for synthetic trades
    void writeSnapshot(const OrderBook& book,
                       const std::vector<std::string>& mboRow,
                       bool syntheticTrade);

private:
    std::ofstream out_;
    ColIdx idx_;
    bool header_written_ = false;

    void writeHeader();
    static std::string getOrEmpty(const std::vector<std::string>& row, int i);
};
