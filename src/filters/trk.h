#pragma once

#include "filter.h"

#include <memory>
#include <array>

#define NOTE_HI(_N) ((_N >> 8) & 0xFF)
#define NOTE_LO(_N) ((_N)&0xFF)

enum class tracker_note_mode : uint8 { empty, on, off, _max };

enum class tracker_fx_mode : uint8 { empty, volume, pitch_up, pitch_down, _max };

static constexpr uint8 MAX_NOTE_GATE = 64;        // 2^6
static constexpr uint8 MAX_NOTE_MODE = 4;         // 2^2
static constexpr uint8 MAX_NOTE_VALUE = 128 - 21; // MIDI

struct tracker_line final {
    // GGGGGGMMVVVVVVVV
    // G = gate (6 bits)
    // M = mode (2 bits)
    // V = note (8 bits)
    std::array<uint16, 12> notes;
    // MMMMMMMMVVVVVVVV
    // M = mode (8 bits)
    // V = val  (8 bits)
    std::array<uint16, 8> fx;
    uint8 num_notes = 1;
    uint8 num_fx = 1;
};

inline tracker_note_mode get_note_mode(uint16 note) {
    return (tracker_note_mode)(NOTE_HI(note) & 0b11);
}

inline uint8 get_note_gate(uint16 note);
inline void set_note_mode(uint16& note, tracker_note_mode mode) {
    note = (get_note_gate(note) << 10) | (((uint8)mode & 0b11) << 8) | NOTE_LO(note);
}

inline uint8 get_note_gate(uint16 note) {
    return (NOTE_HI(note) & (!0b11)) >> 2;
}

inline void set_note_gate(uint16& note, uint8 gate) {
    note = (gate << 10) | ((uint8)get_note_mode(note) << 8) | NOTE_LO(note);
}

inline tracker_fx_mode get_fx_mode(uint16 fx) {
    return (tracker_fx_mode)NOTE_HI(fx);
}

inline void set_fx_mode(uint16& fx, tracker_fx_mode mode) {
    fx = (((uint8)mode) << 8) | NOTE_LO(fx);
}

inline uint8 get_trkline_value(uint16 x) {
    return (uint8)NOTE_LO(x);
}

inline void set_trkline_value(uint16& x, uint8 v) {
    x = (NOTE_HI(x) << 8) | v;
}

inline std::string midi_str(uint8 note) {
    note += 21;
    static const char* notes = "C-C#D-D#E-F-F#G-G#A-A#B-";
    const auto octave = note / 12 - 1;
    auto note_str = std::string{notes + (note % 12) * 2, 2};
    note_str += ('0' + (char)octave);
    return note_str;
}

std::unique_ptr<filter_base> fltr_trk_sampler();
std::unique_ptr<filter_base> fltr_trk_sample_browser();
std::unique_ptr<filter_base> fltr_trk_tracker();
