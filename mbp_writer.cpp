#include "mbp_writer.hpp"
#include <stdexcept>
#include <iomanip>

// Helper: write CSV field with optional trailing comma
static void write_csv_field(std::ofstream& out, const std::string& s, bool end = false) {
    out << s;
    if (!end) out << ",";
}

MBPWriter::MBPWriter(const std::string& filename, const ColIdx& idx)
    : idx_(idx) {
    out_.open(filename, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) {
        throw std::runtime_error("Cannot open output file: " + filename);
    }
}

MBPWriter::~MBPWriter() {
    if (out_.is_open()) out_.close();
}

void MBPWriter::writeHeader() {
    if (header_written_) return;

    // Exact header order based on Databento MBP-10
    out_ << "ts_recv,ts_event,rtype,publisher_id,instrument_id,action,side,depth,price,size,flags,ts_in_delta,sequence,";
    for (int i = 0; i < 10; ++i) {
        out_ << "bid_px_" << std::setw(2) << std::setfill('0') << i << ","
             << "bid_sz_" << std::setw(2) << std::setfill('0') << i << ","
             << "bid_ct_" << std::setw(2) << std::setfill('0') << i << ","
             << "ask_px_" << std::setw(2) << std::setfill('0') << i << ","
             << "ask_sz_" << std::setw(2) << std::setfill('0') << i << ","
             << "ask_ct_" << std::setw(2) << std::setfill('0') << i;
        if (i < 9) out_ << ",";
    }
    out_ << ",symbol,order_id\n";

    header_written_ = true;
}

std::string MBPWriter::getOrEmpty(const std::vector<std::string>& row, int i) {
    if (i < 0 || i >= static_cast<int>(row.size())) return "";
    return row[i];
}

void MBPWriter::writeSnapshot(const OrderBook& book,
                              const std::vector<std::string>& mboRow,
                              bool syntheticTrade) {
    if (!header_written_) writeHeader();

    // Override action to 'T' for synthetic trades
    auto actionVal = getOrEmpty(mboRow, idx_.action);
    if (syntheticTrade) {
        actionVal = "T";
    }

    // Write original MBO fields
    write_csv_field(out_, getOrEmpty(mboRow, idx_.ts_recv));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.ts_event));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.rtype));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.publisher_id));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.instrument_id));
    write_csv_field(out_, actionVal);
    write_csv_field(out_, getOrEmpty(mboRow, idx_.side));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.depth));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.price));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.size));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.flags));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.ts_in_delta));
    write_csv_field(out_, getOrEmpty(mboRow, idx_.sequence));

    // Write top-10 levels
    int wroteLevels = 0;
    auto bidIt = book.bids().begin();
    auto askIt = book.asks().begin();

    // Force 9 decimal places (e.g., 5.510000000)
    out_ << std::fixed << std::setprecision(9);

    while (wroteLevels < 10) {
        double bid_px = 0.0, ask_px = 0.0;
        uint32_t bid_sz = 0, ask_sz = 0, bid_ct = 0, ask_ct = 0;

        if (bidIt != book.bids().end()) {
            bid_px = bidIt->first;
            bid_sz = bidIt->second.size;
            bid_ct = bidIt->second.count;
            ++bidIt;
        }

        if (askIt != book.asks().end()) {
            ask_px = askIt->first;
            ask_sz = askIt->second.size;
            ask_ct = askIt->second.count;
            ++askIt;
        }

        out_ << "," << bid_px << "," << bid_sz << "," << bid_ct
             << "," << ask_px << "," << ask_sz << "," << ask_ct;

        ++wroteLevels;
    }

    // Trailing fields: symbol and order_id
    out_ << "," << getOrEmpty(mboRow, idx_.symbol)
         << "," << getOrEmpty(mboRow, idx_.order_id)
         << "\n";
}
