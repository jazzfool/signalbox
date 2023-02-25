#include "enc.h"

void enc_encode_string(std::ostream& os, std::string_view str) {
    const auto sz = (uint32)str.size();
    os.write((const char*)(&sz), sizeof(sz));
    os.write(str.data(), str.size());
}

std::string enc_decode_string(std::istream& is) {
    uint32 sz = 0;
    is.read((char*)(&sz), sizeof(sz));
    std::string s;
    s.resize(sz);
    is.read(s.data(), sz);
    return s;
}

void enc_encode_bytes(std::ostream& os, std::span<const uint8> bytes) {
    const auto sz = (uint32)bytes.size();
    os.write((const char*)(&sz), sizeof(sz));
    os.write((const char*)bytes.data(), sz);
}

void enc_encode_exact_bytes(std::ostream& os, std::span<const uint8> bytes) {
    os.write((const char*)bytes.data(), bytes.size());
}

std::vector<uint8> enc_decode_bytes(std::istream& is) {
    uint32 sz = 0;
    is.read((char*)(&sz), sizeof(sz));
    std::vector<uint8> bytes;
    bytes.resize(sz);
    is.read((char*)bytes.data(), sz);
    return bytes;
}

std::vector<uint8> enc_decode_exact_bytes(std::istream& is, size_t count) {
    std::vector<uint8> bytes;
    bytes.resize(count);
    is.read((char*)bytes.data(), count);
    return bytes;
}

std::vector<uint8> enc_sstream_to_bytes(const std::stringstream& ss) {
    const auto s = ss.str();
    std::vector<uint8> bytes;
    bytes.assign((const uint8*)(&s[0]), (const uint8*)(&s[s.size() - 1]));
    return bytes;
}
