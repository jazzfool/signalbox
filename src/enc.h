#pragma once

#include "util.h"

#include <vector>
#include <string>
#include <ostream>
#include <istream>
#include <string_view>
#include <span>
#include <sstream>

void enc_encode_string(std::ostream& os, std::string_view str);
std::string enc_decode_string(std::istream& is);

void enc_encode_bytes(std::ostream& os, std::span<const uint8> bytes);
void enc_encode_exact_bytes(std::ostream& os, std::span<const uint8> bytes);
std::vector<uint8> enc_decode_bytes(std::istream& is);
std::vector<uint8> enc_decode_exact_bytes(std::istream& is, size_t count);

template <typename T>
void enc_encode_span(std::ostream& os, std::span<const T> sp) {
    enc_encode_bytes(os, std::span{(const uint8*)sp.data(), sizeof(T) * sp.size()});
}

template <typename T>
std::vector<T> enc_decode_vec(std::istream& is) {
    const auto bytes = enc_decode_bytes(is);
    sb_ASSERT(bytes.size() % sizeof(T) == 0);
    std::vector<T> out;
    out.resize(bytes.size() / sizeof(T));
    memcpy(out.data(), bytes.data(), bytes.size());
    return out;
}

template<typename T>
void enc_encode_one(std::ostream& os, const T& v) {
    enc_encode_exact_bytes(os, std::span{(const uint8*)(&v), sizeof(T)});
}

template<typename T>
T enc_decode_one(std::istream& is) {
    const auto bytes = enc_decode_exact_bytes(is, sizeof(T));
    T out;
    memcpy(&out, bytes.data(), sizeof(T));
    return out;
}
