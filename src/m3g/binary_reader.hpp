#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace slop {
namespace m3g {

class BinaryReader {
public:
    BinaryReader(const std::uint8_t *data, std::size_t size, std::string label = "buffer")
        : data_(data), size_(size), label_(std::move(label)) {}

    explicit BinaryReader(const std::vector<std::uint8_t> &bytes, std::string label = "buffer")
        : BinaryReader(bytes.data(), bytes.size(), std::move(label)) {}

    std::size_t position() const { return position_; }
    std::size_t size() const { return size_; }
    std::size_t remaining() const { return size_ - position_; }
    bool is_eof() const { return position_ >= size_; }

    std::uint8_t read_u8() {
        ensure_available(1);
        return data_[position_++];
    }

    std::int8_t read_i8() { return static_cast<std::int8_t>(read_u8()); }

    bool read_bool_byte() { return read_u8() != 0; }

    std::int32_t read_i32_le() {
        ensure_available(4);
        std::int32_t result = static_cast<std::int32_t>(data_[position_]) |
                              (static_cast<std::int32_t>(data_[position_ + 1]) << 8) |
                              (static_cast<std::int32_t>(data_[position_ + 2]) << 16) |
                              (static_cast<std::int32_t>(data_[position_ + 3]) << 24);
        position_ += 4;
        return result;
    }

    std::uint32_t read_u32_le() { return static_cast<std::uint32_t>(read_i32_le()); }

    std::int16_t read_i16_le() {
        ensure_available(2);
        std::int16_t result = static_cast<std::int16_t>(
            data_[position_] | (static_cast<std::uint16_t>(data_[position_ + 1]) << 8));
        position_ += 2;
        return result;
    }

    std::uint16_t read_u16_le() { return static_cast<std::uint16_t>(read_i16_le()); }

    float read_f32_le() {
        std::uint32_t bits = read_u32_le();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::vector<std::uint8_t> read_bytes(std::size_t length) {
        ensure_available(length);
        std::vector<std::uint8_t> slice(data_ + position_, data_ + position_ + length);
        position_ += length;
        return slice;
    }

    std::string read_cstring() {
        std::size_t terminator = size_;
        for (std::size_t i = position_; i < size_; ++i) {
            if (data_[i] == 0) {
                terminator = i;
                break;
            }
        }
        if (terminator >= size_) {
            fail("Missing null terminator while reading C string");
        }
        std::string result(reinterpret_cast<const char *>(data_ + position_), terminator - position_);
        position_ = terminator + 1;
        return result;
    }

    BinaryReader read_sub_reader(std::size_t length, const std::string &child_label) {
        auto bytes = read_bytes(length);
        owned_chunks_.push_back(std::move(bytes));
        const auto &chunk = owned_chunks_.back();
        return BinaryReader(chunk.data(), chunk.size(), child_label);
    }

    std::vector<std::uint8_t> read_remaining_bytes() { return read_bytes(remaining()); }

    void ensure_fully_consumed(const std::string &context) {
        if (!is_eof()) {
            fail(context + " left " + std::to_string(remaining()) + " unread byte(s)");
        }
    }

private:
    void ensure_available(std::size_t length) {
        if (remaining() < length) {
            fail("Attempted to read " + std::to_string(length) + " byte(s) with only " +
                 std::to_string(remaining()) + " remaining");
        }
    }

    [[noreturn]] void fail(const std::string &message) const {
        throw std::runtime_error(label_ + " @ " + std::to_string(position_) + ": " + message);
    }

    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
    std::string label_;
    // Keep ownership when spawning sub-readers from temporary vectors.
    std::vector<std::vector<std::uint8_t>> owned_chunks_;
};

} // namespace m3g
} // namespace slop
