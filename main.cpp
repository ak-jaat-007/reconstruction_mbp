#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include "fast_csv.hpp"
#include "order_book.hpp"
#include "mbp_writer.hpp"

// Helper: get column index by name (returns -1 if not found)
static int idxOf(const std::unordered_map<std::string, size_t>& m, const char* key) {
    auto it = m.find(key);
    if (it == m.end()) return -1;
    return static_cast<int>(it->second);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: reconstruction_mbp <mbo.csv>\n";
        return 1;
    }

    const std::string inputFile = argv[1];
    const std::string outputFile = "mbp.csv";

    try {
        // Open input CSV
        FastCSV csv(inputFile);
        const auto& headers = csv.headers();

        // Map header -> index
        std::unordered_map<std::string, size_t> colIndex;
        colIndex.reserve(headers.size());
        for (size_t i = 0; i < headers.size(); ++i) {
            colIndex[headers[i]] = i;
        }

        // Build column indices for fields we need to copy to MBP output
        ColIdx idx;
        idx.ts_recv       = idxOf(colIndex, "ts_recv");
        idx.ts_event      = idxOf(colIndex, "ts_event");
        idx.rtype         = idxOf(colIndex, "rtype");
        idx.publisher_id  = idxOf(colIndex, "publisher_id");
        idx.instrument_id = idxOf(colIndex, "instrument_id");
        idx.action        = idxOf(colIndex, "action");
        idx.side          = idxOf(colIndex, "side");
        idx.depth         = idxOf(colIndex, "depth");      // may be missing (=-1)
        idx.price         = idxOf(colIndex, "price");
        idx.size          = idxOf(colIndex, "size");
        idx.flags         = idxOf(colIndex, "flags");
        idx.ts_in_delta   = idxOf(colIndex, "ts_in_delta");
        idx.sequence      = idxOf(colIndex, "sequence");
        idx.symbol        = idxOf(colIndex, "symbol");
        idx.order_id      = idxOf(colIndex, "order_id");

        OrderBook book;
        MBPWriter writer(outputFile, idx);

        std::vector<std::string> row;
        bool ignored_first_clear = false;

        while (csv.nextRow(row)) {
            // If the row is shorter than expected, pad it so getOrEmpty() is safe
            if (row.size() < headers.size()) row.resize(headers.size());

            const std::string action = (idx.action >= 0 ? row[idx.action] : "");

            // Spec: Ignore the initial clear 'R'
            if (!ignored_first_clear && action == "R") {
                ignored_first_clear = true;
                continue;
            }

            const std::string side   = (idx.side   >= 0 ? row[idx.side]   : "");
            const std::string price  = (idx.price  >= 0 ? row[idx.price]  : "");
            const std::string size   = (idx.size   >= 0 ? row[idx.size]   : "");
            const std::string ord_id = (idx.order_id >= 0 ? row[idx.order_id] : "");
            const std::string flags  = (idx.flags  >= 0 ? row[idx.flags]  : "");

            // Process event and decide what to emit
            ChangeType ch = book.processEvent(action, side, price, size, ord_id, flags);

            switch (ch) {
                case ChangeType::BookChange:
                    // Normal A/M/C/R updates → write snapshot with action as-is
                    writer.writeSnapshot(book, row, /*syntheticTrade=*/false);
                    break;
                case ChangeType::SyntheticTrade:
                    // F (during an active T) or MBP-aggregated fill → write snapshot as Trade 'T'
                    writer.writeSnapshot(book, row, /*syntheticTrade=*/true);
                    break;
                case ChangeType::NoChange:
                default:
                    // Do nothing
                    break;
            }
        }

        std::cout << "Reconstruction complete. Output: " << outputFile << "\n";
        return 0;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
