#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <stdexcept>

class FastCSV {
public:
    FastCSV(const std::string& filename) {
        file_.open(filename);
        if (!file_.is_open()) {
            throw std::runtime_error("Cannot open CSV file: " + filename);
        }
        // Read header
        std::string line;
        if (std::getline(file_, line)) {
            parseLine(line, headers_);
        } else {
            throw std::runtime_error("CSV is empty: " + filename);
        }
    }

    // Read next line
    bool nextRow(std::vector<std::string>& row) {
        std::string line;
        if (std::getline(file_, line)) {
            row.clear();
            parseLine(line, row);
            return true;
        }
        return false;
    }

    const std::vector<std::string>& headers() const { return headers_; }

private:
    std::ifstream file_;
    std::vector<std::string> headers_;

    void parseLine(const std::string& line, std::vector<std::string>& out) {
        out.clear();
        std::stringstream ss(line);
        std::string item;
        while (std::getline(ss, item, ',')) {
            out.push_back(item);
        }
    }
};
