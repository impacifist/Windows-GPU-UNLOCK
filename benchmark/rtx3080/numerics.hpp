// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Ordinary numerical/reference helpers. This header has no CUDA dependency.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace baseline3080 {
constexpr int kSize = 4096;
constexpr int kSamples = 30;
constexpr int kSampledElements = 1024;
constexpr std::uint32_t kInputSeed = 30802026u;
constexpr std::uint32_t kSampleSeed = 30801024u;
constexpr double kWarmupSeconds = 2.0;
constexpr double kProcessBudgetSeconds = 120.0;
constexpr std::size_t kAllocationBudgetBytes = 512ull * 1024 * 1024;
constexpr const char* kInputGenerator = "xorshift32_uniform20_v1";
constexpr const char* kSamplingPolicy = "32x32_tiles_edges_v1";
constexpr const char* kStreamPolicy = "single_explicit_nonblocking_stream";

enum class Mode { fp32, fp16, bf16, tf32 };
struct Tolerance { double absolute; double relative; };
struct ModeSpec {
    Mode id;
    const char* name;
    const char* input_type;
    const char* compute_type;
    const char* math_mode;
};
inline const std::array<ModeSpec, 4>& modes() {
    static const std::array<ModeSpec, 4> result{{
        {Mode::fp32, "fp32_pedantic", "CUDA_R_32F", "CUBLAS_COMPUTE_32F_PEDANTIC",
         "CUBLAS_PEDANTIC_MATH"},
        {Mode::fp16, "fp16_tensor_fp32_accumulate", "CUDA_R_16F", "CUBLAS_COMPUTE_32F",
         "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"},
        {Mode::bf16, "bf16_tensor_fp32_accumulate", "CUDA_R_16BF", "CUBLAS_COMPUTE_32F",
         "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"},
        {Mode::tf32, "tf32_tensor_fp32_accumulate", "CUDA_R_32F",
         "CUBLAS_COMPUTE_32F_FAST_TF32",
         "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"}
    }};
    return result;
}
inline Tolerance small_tolerance(Mode mode) {
    return mode == Mode::tf32 ? Tolerance{0.005, 0.005} : Tolerance{2e-5, 2e-5};
}
inline Tolerance timed_tolerance(Mode mode) {
    return mode == Mode::tf32 ? Tolerance{0.05, 0.01} : Tolerance{0.002, 0.0005};
}
inline std::uint32_t float_bits(float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t), "32-bit float required");
    static_assert(std::numeric_limits<float>::is_iec559, "IEEE binary32 required");
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
inline float float_from_bits(std::uint32_t value) {
    float result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
inline std::uint32_t rounded_shift(std::uint32_t value, unsigned shift) {
    const std::uint32_t base = value >> shift;
    const std::uint32_t remainder = value & ((std::uint32_t{1} << shift) - 1);
    const std::uint32_t halfway = std::uint32_t{1} << (shift - 1);
    return base + (remainder > halfway || (remainder == halfway && (base & 1u)));
}
// IEEE binary16 round-to-nearest, ties-to-even. NaNs are quieted.
inline std::uint16_t half_bits(float value) {
    const std::uint32_t bits = float_bits(value);
    const std::uint16_t sign = static_cast<std::uint16_t>((bits >> 16) & 0x8000u);
    const unsigned exponent = (bits >> 23) & 0xffu;
    const std::uint32_t fraction = bits & 0x7fffffu;
    if (exponent == 255)
        return static_cast<std::uint16_t>(sign | 0x7c00u |
            (fraction ? (0x200u | (fraction >> 13)) : 0u));
    const int half_exponent = static_cast<int>(exponent) - 127 + 15;
    if (half_exponent >= 31) return static_cast<std::uint16_t>(sign | 0x7c00u);
    if (half_exponent <= 0) {
        if (half_exponent < -10) return sign;
        return static_cast<std::uint16_t>(sign |
            rounded_shift(fraction | 0x800000u, static_cast<unsigned>(14 - half_exponent)));
    }
    return static_cast<std::uint16_t>(sign |
        ((static_cast<unsigned>(half_exponent) << 10) + rounded_shift(fraction, 13)));
}
inline float half_value(std::uint16_t bits) {
    const bool negative = (bits & 0x8000u) != 0;
    const unsigned exponent = (bits >> 10) & 0x1fu;
    const unsigned fraction = bits & 0x3ffu;
    if (exponent == 0) {
        const float value = std::ldexp(static_cast<float>(fraction), -24);
        return negative ? -value : value;
    }
    const std::uint32_t sign = (static_cast<std::uint32_t>(bits & 0x8000u)) << 16;
    if (exponent == 31) return float_from_bits(sign | 0x7f800000u | (fraction << 13));
    return float_from_bits(sign | ((exponent + 112u) << 23) | (fraction << 13));
}
inline std::uint16_t bfloat_bits(float value) {
    const std::uint32_t bits = float_bits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu))
        return static_cast<std::uint16_t>((bits >> 16) | 0x40u);
    return static_cast<std::uint16_t>((bits + 0x7fffu + ((bits >> 16) & 1u)) >> 16);
}
inline float bfloat_value(std::uint16_t bits) {
    return float_from_bits(static_cast<std::uint32_t>(bits) << 16);
}
inline float converted_value(float value, Mode mode) {
    if (mode == Mode::fp16) return half_value(half_bits(value));
    if (mode == Mode::bf16) return bfloat_value(bfloat_bits(value));
    return value; // TF32 reference deliberately retains original FP32 input.
}
inline bool uses_16bit_input(Mode mode) { return mode == Mode::fp16 || mode == Mode::bf16; }

inline const std::array<const char*, 4>& environment_names() {
    static const std::array<const char*, 4> names{{
        "NVIDIA_TF32_OVERRIDE", "CUBLAS_WORKSPACE_CONFIG",
        "CUDA_VISIBLE_DEVICES", "CUDA_DEVICE_ORDER"
    }};
    return names;
}
// Pure validation: never reads or changes the real process environment.
// Only these enumerations/ordinal lists are publishable; never UUIDs or paths.
inline bool allowed_environment_value(const char* name, const char* value) {
    if (!name) return false;
    bool known = false;
    for (const auto* candidate : environment_names())
        known = known || std::strcmp(name, candidate) == 0;
    if (!known) return false;
    if (!value || !*value) return true;
    if (std::strcmp(name, "NVIDIA_TF32_OVERRIDE") == 0) return false;
    if (std::strcmp(name, "CUBLAS_WORKSPACE_CONFIG") == 0)
        return std::strcmp(value, ":16:8") == 0 || std::strcmp(value, ":4096:8") == 0;
    if (std::strcmp(name, "CUDA_DEVICE_ORDER") == 0)
        return std::strcmp(value, "FASTEST_FIRST") == 0 || std::strcmp(value, "PCI_BUS_ID") == 0;
    const auto length = std::strlen(value);
    if (length > 191) return false;
    std::uint64_t seen = 0;
    std::size_t position = 0;
    unsigned count = 0;
    while (position < length) {
        const auto start = position;
        unsigned ordinal = 0;
        while (position < length && value[position] != ',') {
            const char ch = value[position++];
            if (ch < '0' || ch > '9' || position - start > 2) return false;
            ordinal = ordinal * 10 + static_cast<unsigned>(ch - '0');
        }
        if (position == start || ordinal > 63 ||
            (position - start > 1 && value[start] == '0')) return false;
        const auto bit = std::uint64_t{1} << ordinal;
        if ((seen & bit) != 0 || ++count > 64) return false;
        seen |= bit;
        if (position == length) return true;
        if (++position == length) return false; // no trailing comma
    }
    return false;
}

class XorShift32 {
    std::uint32_t state_;
public:
    explicit XorShift32(std::uint32_t seed) : state_(seed) {
        if (!seed) throw std::invalid_argument("xorshift32 seed must be nonzero");
    }
    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    float uniform() {
        const auto integer = static_cast<std::int32_t>(next() >> 12) - 524288;
        return static_cast<float>(integer) * (1.0f / 524288.0f);
    }
};

struct CaseSpec {
    const char* name;
    int m, n, k;
    bool transa, transb;
    int lda, ldb, ldc;
    const char* input_pattern;
};
inline const std::array<CaseSpec, 5>& small_cases() {
    static const std::array<CaseSpec, 5> result{{
        {"signed_square", 17, 17, 17, false, false, 20, 19, 21, "exact_binary"},
        {"signed_rectangular", 31, 47, 65, false, false, 34, 69, 36, "seeded_uniform"},
        {"transpose_stride", 31, 47, 65, true, true, 69, 52, 37, "seeded_uniform"},
        {"zero_product", 13, 11, 19, false, false, 16, 23, 17, "zero_a"},
        {"identity", 19, 23, 19, false, false, 22, 24, 25, "identity_a"}
    }};
    return result;
}
inline std::size_t matrix_index(int row, int col, int ld, bool transposed) {
    return transposed ? static_cast<std::size_t>(row) * ld + col
                      : static_cast<std::size_t>(col) * ld + row;
}
inline float binary_pattern(int row, int col, int salt = 0) {
    return static_cast<float>((row * 7 + col * 11 + salt) % 33 - 16) / 16.0f;
}
inline std::size_t a_elements(const CaseSpec& spec) {
    return static_cast<std::size_t>(spec.lda) * (spec.transa ? spec.m : spec.k);
}
inline std::size_t b_elements(const CaseSpec& spec) {
    return static_cast<std::size_t>(spec.ldb) * (spec.transb ? spec.k : spec.n);
}
inline void make_small_inputs(const CaseSpec& spec, Mode mode,
                              std::vector<float>& a, std::vector<float>& b) {
    // Padding differs from logical elements, exposing accidental wrong strides.
    a.assign(a_elements(spec), 93.25f);
    b.assign(b_elements(spec), -71.5f);
    XorShift32 random(kInputSeed);
    for (int col = 0; col < spec.k; ++col) for (int row = 0; row < spec.m; ++row) {
        float value = std::strcmp(spec.input_pattern, "seeded_uniform") == 0
            ? random.uniform() : binary_pattern(row, col, 3);
        if (std::strcmp(spec.input_pattern, "zero_a") == 0) value = 0.0f;
        if (std::strcmp(spec.input_pattern, "identity_a") == 0) value = row == col ? 1.0f : 0.0f;
        a[matrix_index(row, col, spec.lda, spec.transa)] = converted_value(value, mode);
    }
    for (int col = 0; col < spec.n; ++col) for (int row = 0; row < spec.k; ++row) {
        const float value = std::strcmp(spec.input_pattern, "seeded_uniform") == 0
            ? random.uniform() : binary_pattern(row, col, 9);
        b[matrix_index(row, col, spec.ldb, spec.transb)] = converted_value(value, mode);
    }
}
inline double reference_dot(const CaseSpec& spec, const std::vector<float>& a,
                            const std::vector<float>& b, int row, int col) {
    double result = 0.0;
    for (int inner = 0; inner < spec.k; ++inner)
        result += static_cast<double>(a[matrix_index(row, inner, spec.lda, spec.transa)]) *
                  static_cast<double>(b[matrix_index(inner, col, spec.ldb, spec.transb)]);
    return result;
}
inline float known_diagonal(int row) {
    static constexpr float diagonal[5] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
    return diagonal[row % 5];
}
inline double known_reference(int row, int col) {
    return static_cast<double>(known_diagonal(row)) * binary_pattern(row, col);
}

struct Position { int row; int col; };
inline std::array<Position, kSampledElements> sampled_positions() {
    std::array<Position, kSampledElements> positions{};
    XorShift32 random(kSampleSeed);
    int index = 0;
    for (int tile_row = 0; tile_row < 32; ++tile_row) {
        for (int tile_col = 0; tile_col < 32; ++tile_col) {
            const int row_offset = static_cast<int>(random.next() % 128u);
            const int col_offset = static_cast<int>(random.next() % 128u);
            const int row = tile_row == 0 ? 0 : tile_row == 31 ? kSize - 1
                : tile_row * 128 + row_offset;
            const int col = tile_col == 0 ? 0 : tile_col == 31 ? kSize - 1
                : tile_col * 128 + col_offset;
            positions[index++] = Position{row, col};
        }
    }
    return positions;
}

struct Metrics {
    Tolerance tolerance;
    std::size_t checked = 0;
    double max_absolute_error = 0.0;
    double max_scaled_error = 0.0;
    double max_reference_magnitude = 0.0;
    bool passed = true;
    explicit Metrics(Tolerance rule) : tolerance(rule) {}
    bool observe(double actual, double reference) {
        ++checked;
        if (!std::isfinite(actual) || !std::isfinite(reference)) {
            passed = false;
            max_absolute_error = max_scaled_error = std::numeric_limits<double>::infinity();
            return false;
        }
        max_reference_magnitude = std::max(max_reference_magnitude, std::abs(reference));
        const double error = std::abs(actual - reference);
        const double denominator = tolerance.absolute + tolerance.relative * std::abs(reference);
        const double scaled = denominator > 0.0 ? error / denominator :
            error == 0.0 ? 0.0 : std::numeric_limits<double>::infinity();
        max_absolute_error = std::max(max_absolute_error, error);
        max_scaled_error = std::max(max_scaled_error, scaled);
        passed = passed && scaled <= 1.0;
        return scaled <= 1.0;
    }
    void merge(const Metrics& other) {
        if (tolerance.absolute != other.tolerance.absolute || tolerance.relative != other.tolerance.relative)
            throw std::invalid_argument("Cannot merge different tolerance rules");
        checked += other.checked;
        passed = passed && other.passed;
        max_absolute_error = std::max(max_absolute_error, other.max_absolute_error);
        max_scaled_error = std::max(max_scaled_error, other.max_scaled_error);
        max_reference_magnitude = std::max(max_reference_magnitude, other.max_reference_magnitude);
    }
};

struct Sample { int index; double elapsed_ms; int repetitions; };
inline double median_ms(const std::vector<Sample>& samples) {
    if (samples.size() != kSamples) throw std::invalid_argument("Exactly 30 samples required");
    std::vector<double> normalized;
    normalized.reserve(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const auto& sample = samples[index];
        if (sample.index != static_cast<int>(index) || sample.repetitions <= 0 ||
            !std::isfinite(sample.elapsed_ms) || sample.elapsed_ms <= 0)
            throw std::invalid_argument("Invalid timing sample");
        normalized.push_back(sample.elapsed_ms / sample.repetitions);
    }
    std::sort(normalized.begin(), normalized.end());
    return normalized[14] * 0.5 + normalized[15] * 0.5;
}
inline double tflops(double milliseconds) {
    if (!std::isfinite(milliseconds) || milliseconds <= 0)
        throw std::invalid_argument("Invalid median time");
    return (2.0 * kSize * kSize * kSize) / (milliseconds * 1e9);
}

inline std::uint32_t parse_hex(const std::string& text) {
    if (text.empty() || text.size() > 8) throw std::invalid_argument("Invalid PCI field");
    std::uint32_t value = 0;
    for (char ch : text) {
        unsigned digit;
        if (ch >= '0' && ch <= '9') digit = static_cast<unsigned>(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = static_cast<unsigned>(ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') digit = static_cast<unsigned>(ch - 'A' + 10);
        else throw std::invalid_argument("Invalid PCI hexadecimal digit");
        value = value * 16u + digit;
    }
    return value;
}
inline std::string canonical_pci(const std::string& input) {
    const auto first = input.find(':');
    if (first != 4 && first != 8) throw std::invalid_argument("PCI requires 4/8-digit domain");
    if (input.size() != first + 8 || input[first + 3] != ':' || input[first + 6] != '.')
        throw std::invalid_argument("PCI must be domain:bus:device.function");
    const auto domain = parse_hex(input.substr(0, first));
    const auto bus = parse_hex(input.substr(first + 1, 2));
    const auto device = parse_hex(input.substr(first + 4, 2));
    const auto function = parse_hex(input.substr(first + 7, 1));
    if (device > 31 || function > 7) throw std::invalid_argument("PCI device/function out of range");
    const char* digits = "0123456789abcdef";
    std::string result = "00000000:00:00.0";
    for (int i = 0; i < 8; ++i) result[7 - i] = digits[(domain >> (i * 4)) & 15u];
    result[9] = digits[bus >> 4]; result[10] = digits[bus & 15u];
    result[12] = digits[device >> 4]; result[13] = digits[device & 15u];
    result[15] = digits[function];
    return result;
}
} // namespace baseline3080
