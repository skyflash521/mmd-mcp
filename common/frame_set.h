#pragma once

#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class FrameSet {
    std::vector<std::ranges::iota_view<int, int>> ranges_;

public:
    static FrameSet parse(const std::string& spec) {
        FrameSet result;
        std::istringstream stream(spec);
        std::string token;

        while (std::getline(stream, token, ',')) {
            if (token.empty()) continue;

            auto dash = token.find('-');
            if (dash == std::string::npos) {
                // 単一フレーム: "5"
                int f = parseNonNegative(token);
                result.ranges_.push_back(std::views::iota(f, f + 1));
            } else {
                // 閉区間: "1-5"（両端必須）
                if (dash == 0 || dash == token.size() - 1)
                    throw std::invalid_argument("Both ends of range must be specified: '" + token + "'");
                std::string left = token.substr(0, dash);
                std::string right = token.substr(dash + 1);
                if (right.find('-') != std::string::npos)
                    throw std::invalid_argument("Invalid range: multiple dashes in '" + token + "'");
                int from = parseNonNegative(left);
                int to = parseNonNegative(right);
                if (from > to)
                    throw std::invalid_argument("Invalid range: start > end in '" + token + "'");
                result.ranges_.push_back(std::views::iota(from, to + 1));
            }
        }
        return result;
    }

    bool contains(int frame) const {
        for (auto& r : ranges_) {
            int first = *r.begin();
            if (frame >= first && static_cast<size_t>(frame - first) < r.size())
                return true;
        }
        return false;
    }

    bool empty() const { return ranges_.empty(); }

private:
    static int parseNonNegative(const std::string& s) {
        if (s.empty())
            throw std::invalid_argument("Empty number in frame range");
        for (char c : s) {
            if (c < '0' || c > '9')
                throw std::invalid_argument("Invalid character in frame range: '" + s + "'");
        }
        int val;
        try {
            val = std::stoi(s);
        } catch (const std::out_of_range&) {
            throw std::invalid_argument("Frame number out of range: '" + s + "'");
        }
        if (val < 0)
            throw std::invalid_argument("Negative frame number: " + s);
        return val;
    }
};
