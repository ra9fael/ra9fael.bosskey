// qrcode.h - a small, real QR encoder: Version 3, ECC level L, byte mode.
//
// The blue screen's QR code is a genuine one — scanning it opens the same
// stopcode page the text below names, so the cover survives the one check a
// suspicious reader can run. Version 3 is 29x29 modules; at level L it holds
// 53 bytes, enough for the URL with room to spare.
//
// Scope: one version, one ECC level, one mode. That is everything this needs,
// and it keeps the encoder readable: GF(256) arithmetic, the Reed-Solomon
// generator polynomial, the 15-bit format word, the module layout (finders,
// separators, timing, the Version-3 alignment pattern, the dark module), the
// eight masks with their penalty rules.
//
// Reference: ISO/IEC 18004; the layout and mask formulas follow Nayuki's
// reference implementation.
#ifndef BOSSKEY_QRCODE_H
#define BOSSKEY_QRCODE_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace bosskey::qr {

constexpr int kVersion = 3;
constexpr int kSize = kVersion * 4 + 17; // 29
constexpr int kDataCodewords = 55;       // Version 3, level L: one block
constexpr int kEcCodewords = 15;

struct Code {
    bool ok = false;
    int size = 0;
    // size rows of size characters, '1' dark and '0' light, joined by '\n'.
    std::string rows;
};

namespace detail {

// ---- GF(256), primitive polynomial 0x11D ------------------------------------

struct Galois {
    std::array<int, 512> exp{};
    std::array<int, 256> log{};

    Galois()
    {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            exp[i] = x;
            log[x] = i;
            x <<= 1;
            if (x & 0x100)
                x ^= 0x11D;
        }
        for (int i = 255; i < 512; ++i)
            exp[i] = exp[i - 255];
    }

    int mul(int a, int b) const { return (a != 0 && b != 0) ? exp[log[a] + log[b]] : 0; }
};

inline const Galois& galois()
{
    static const Galois table;
    return table;
}

// (x - a^0)(x - a^1)...(x - a^(degree-1)), highest coefficient first.
inline std::vector<int> generatorPolynomial(int degree)
{
    const Galois& g = galois();
    std::vector<int> poly{1};
    for (int i = 0; i < degree; ++i) {
        std::vector<int> next(poly.size() + 1, 0);
        for (std::size_t j = 0; j < poly.size(); ++j) {
            next[j] ^= poly[j];
            next[j + 1] ^= g.mul(poly[j], g.exp[i]);
        }
        poly = std::move(next);
    }
    return poly;
}

// The Reed-Solomon check codewords: the remainder of data * x^degree divided
// by the generator polynomial (polynomial long division over GF(256)).
inline std::vector<int> ecCodewords(const std::vector<int>& data, const std::vector<int>& gen)
{
    const Galois& g = galois();
    std::vector<int> message(data.begin(), data.end());
    message.resize(data.size() + gen.size() - 1, 0);
    for (std::size_t i = 0; i < data.size(); ++i) {
        const int factor = message[i];
        if (factor == 0)
            continue;
        for (std::size_t j = 0; j < gen.size(); ++j)
            message[i + j] ^= g.mul(factor, gen[j]);
    }
    return std::vector<int>(message.end() - (gen.size() - 1), message.end());
}

// ---- the bit stream ---------------------------------------------------------

struct Bits {
    std::vector<std::uint8_t> bytes;
    int size = 0; // bits written

    void append(int value, int count)
    {
        for (int i = count - 1; i >= 0; --i) {
            if ((size & 7) == 0)
                bytes.push_back(0);
            if ((value >> i) & 1)
                bytes.back() |= static_cast<std::uint8_t>(0x80 >> (size & 7));
            ++size;
        }
    }
};

// Byte mode: mode indicator 0100, an 8-bit character count, the bytes, the
// terminator, padding to a codeword boundary, then the 0xEC/0x11 pad words.
inline std::vector<int> dataCodewords(const std::string& text, bool& ok)
{
    ok = false;
    if (text.size() > static_cast<std::size_t>(kDataCodewords))
        return {};

    Bits bits;
    bits.append(0x4, 4);
    bits.append(static_cast<int>(text.size()), 8);
    for (unsigned char c : text)
        bits.append(static_cast<int>(c), 8);

    const int capacity = kDataCodewords * 8;
    if (bits.size > capacity)
        return {};
    bits.append(0, std::min(4, capacity - bits.size));
    while ((bits.size & 7) != 0)
        bits.append(0, 1);

    int pad = 0xEC;
    while (static_cast<int>(bits.bytes.size()) < kDataCodewords) {
        bits.bytes.push_back(static_cast<std::uint8_t>(pad));
        pad = (pad == 0xEC) ? 0x11 : 0xEC;
    }
    ok = true;
    return std::vector<int>(bits.bytes.begin(), bits.bytes.end());
}

// ---- the module grid --------------------------------------------------------

struct Grid {
    // 0 light, 1 dark. Everything starts light: the modules the data stream
    // leaves over (Version 3 has 7 remainder bits) must be light before the
    // mask flips them, as the specification says.
    std::vector<int8_t> value;
    std::vector<uint8_t> fixed; // function modules: masks never touch them

    Grid() : value(kSize * kSize, 0), fixed(kSize * kSize, 0) {}

    int8_t at(int row, int col) const { return value[row * kSize + col]; }
    bool isFixed(int row, int col) const { return fixed[row * kSize + col] != 0; }

    void set(int row, int col, int bit)
    {
        value[row * kSize + col] = static_cast<int8_t>(bit);
        fixed[row * kSize + col] = 1;
    }
};

inline void drawFinder(Grid& g, int row, int col)
{
    for (int dr = -1; dr <= 7; ++dr) {
        for (int dc = -1; dc <= 7; ++dc) {
            const int r = row + dr;
            const int c = col + dc;
            if (r < 0 || r >= kSize || c < 0 || c >= kSize)
                continue;
            const bool inside = dr >= 0 && dr <= 6 && dc >= 0 && dc <= 6;
            const bool dark = inside && (dr == 0 || dr == 6 || dc == 0 || dc == 6
                                         || (dr >= 2 && dr <= 4 && dc >= 2 && dc <= 4));
            g.set(r, c, dark ? 1 : 0);
        }
    }
}

inline void drawAlignment(Grid& g, int row, int col)
{
    for (int dr = -2; dr <= 2; ++dr)
        for (int dc = -2; dc <= 2; ++dc) {
            const bool dark = std::max(std::abs(dr), std::abs(dc)) != 1;
            g.set(row + dr, col + dc, dark ? 1 : 0);
        }
}

// The 15-bit format word: 2 ECC-level bits (L = 01), 3 mask bits, BCH(15,5),
// XOR'd with the specified mask 0x5412. Both copies, plus the dark module.
inline void drawFormat(Grid& g, int mask)
{
    const int data = (1 << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ (((rem >> 9) & 1) ? 0x537 : 0);
    const int bits = ((data << 10) | rem) ^ 0x5412;
    auto bit = [bits](int i) { return (bits >> i) & 1; };

    // Copy 1: the vertical strip up beside the top-left finder, then the
    // horizontal one. Note the row/column order — the reference writes these
    // as (x, y); swapping them silently transposes the whole format block and
    // no reader can recover the mask.
    for (int i = 0; i <= 5; ++i)
        g.set(i, 8, bit(i));
    g.set(7, 8, bit(6));
    g.set(8, 8, bit(7));
    g.set(8, 7, bit(8));
    for (int i = 9; i <= 14; ++i)
        g.set(8, 14 - i, bit(i));

    // Copy 2: split between the other two finders — eight bits along row 8
    // from the right edge, then seven down column 8.
    for (int i = 0; i <= 7; ++i)
        g.set(8, kSize - 1 - i, bit(i));
    for (int i = 8; i <= 14; ++i)
        g.set(kSize - 15 + i, 8, bit(i));

    g.set(kSize - 8, 8, 1); // the dark module, at (4 * version + 9, 8)
}

// Two-column zigzag from the bottom right, skipping the timing column.
inline void drawData(Grid& g, const std::vector<int>& codewords)
{
    const int total = static_cast<int>(codewords.size()) * 8;
    auto bitAt = [&](int i) { return (codewords[i >> 3] >> (7 - (i & 7))) & 1; };

    int i = 0;
    for (int right = kSize - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < kSize; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int col = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int row = upward ? kSize - 1 - vert : vert;
                if (!g.isFixed(row, col) && i < total) {
                    g.value[row * kSize + col] = static_cast<int8_t>(bitAt(i));
                    ++i;
                }
            }
        }
    }
}

inline bool maskBit(int mask, int row, int col)
{
    switch (mask) {
    case 0: return (row + col) % 2 == 0;
    case 1: return row % 2 == 0;
    case 2: return col % 3 == 0;
    case 3: return (row + col) % 3 == 0;
    case 4: return (row / 2 + col / 3) % 2 == 0;
    case 5: return (row * col) % 2 + (row * col) % 3 == 0;
    case 6: return ((row * col) % 2 + (row * col) % 3) % 2 == 0;
    case 7: return ((row + col) % 2 + (row * col) % 3) % 2 == 0;
    default: return false;
    }
}

// The four penalty rules (N1 runs, N2 blocks, N3 finder lookalikes, N4 the
// dark ratio), used only to pick the least noisy mask.
inline int penalty(const Grid& g)
{
    int score = 0;

    for (int dir = 0; dir < 2; ++dir) {
        for (int line = 0; line < kSize; ++line) {
            int run = 1;
            int prev = -1;
            for (int k = 0; k < kSize; ++k) {
                const int v = dir == 0 ? g.at(line, k) : g.at(k, line);
                if (v == prev) {
                    ++run;
                    if (run == 5)
                        score += 3;
                    else if (run > 5)
                        score += 1;
                } else {
                    prev = v;
                    run = 1;
                }
            }
        }
    }

    for (int r = 0; r + 1 < kSize; ++r)
        for (int c = 0; c + 1 < kSize; ++c) {
            const int v = g.at(r, c);
            if (v == g.at(r, c + 1) && v == g.at(r + 1, c) && v == g.at(r + 1, c + 1))
                score += 3;
        }

    static const int kPatterns[2][11] = {
        {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0},
        {0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1},
    };
    for (int dir = 0; dir < 2; ++dir)
        for (int line = 0; line < kSize; ++line)
            for (int start = 0; start + 11 <= kSize; ++start) {
                bool first = true;
                bool second = true;
                for (int k = 0; k < 11; ++k) {
                    const int v = dir == 0 ? g.at(line, start + k) : g.at(start + k, line);
                    first = first && v == kPatterns[0][k];
                    second = second && v == kPatterns[1][k];
                }
                if (first)
                    score += 40;
                if (second)
                    score += 40;
            }

    int dark = 0;
    for (std::size_t i = 0; i < g.value.size(); ++i)
        dark += g.value[i] == 1;
    const int total = kSize * kSize;
    score += std::abs(dark * 100 - total * 50) / (total * 5) * 10;

    return score;
}

} // namespace detail

// Encodes text (up to 53 bytes) as a Version 3, level L QR code. ok is false
// when the text does not fit.
inline Code encode(const std::string& text)
{
    using namespace detail;

    Code out;
    bool haveData = false;
    auto data = dataCodewords(text, haveData);
    if (!haveData)
        return out;

    const auto gen = generatorPolynomial(kEcCodewords);
    auto ec = ecCodewords(data, gen);
    std::vector<int> codewords = data;
    codewords.insert(codewords.end(), ec.begin(), ec.end());

    Grid base;
    drawFinder(base, 0, 0);
    drawFinder(base, 0, kSize - 7);
    drawFinder(base, kSize - 7, 0);
    for (int i = 8; i < kSize - 8; ++i) {
        base.set(6, i, i % 2 == 0 ? 1 : 0);
        base.set(i, 6, i % 2 == 0 ? 1 : 0);
    }
    drawAlignment(base, 22, 22); // the only alignment pattern in Version 3
    drawFormat(base, 0);         // reserves the format area for drawData
    drawData(base, codewords);

    Grid best;
    int bestScore = -1;
    for (int mask = 0; mask < 8; ++mask) {
        Grid g = base;
        for (int r = 0; r < kSize; ++r)
            for (int c = 0; c < kSize; ++c)
                if (!g.isFixed(r, c) && maskBit(mask, r, c))
                    g.value[r * kSize + c] ^= 1;
        drawFormat(g, mask);

        const int score = penalty(g);
        if (bestScore < 0 || score < bestScore) {
            bestScore = score;
            best = std::move(g);
        }
    }

    out.ok = true;
    out.size = kSize;
    out.rows.reserve(kSize * (kSize + 1));
    for (int r = 0; r < kSize; ++r) {
        for (int c = 0; c < kSize; ++c)
            out.rows.push_back(best.at(r, c) == 1 ? '1' : '0');
        if (r + 1 < kSize)
            out.rows.push_back('\n');
    }
    return out;
}

} // namespace bosskey::qr

#endif // BOSSKEY_QRCODE_H
