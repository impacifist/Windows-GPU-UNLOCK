// SPDX-License-Identifier: GPL-2.0-only
// CPU-only tests: includes no CUDA headers, links no CUDA library, launches no GPU work.
#include "numerics.hpp"
#include <functional>
#include <iostream>
#include <set>
using namespace baseline3080;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejected(F action) {
    bool threw = false;
    try { action(); } catch (const std::invalid_argument&) { threw = true; }
    check(threw, "Invalid input was accepted");
}
void half_golden() {
    check(half_bits(0.0f) == 0 && half_bits(-0.0f) == 0x8000, "Half signed zero");
    check(half_bits(1.0f) == 0x3c00 && half_bits(-2.0f) == 0xc000, "Half exact values");
    check(half_bits(65504.0f) == 0x7bff && half_bits(65520.0f) == 0x7c00, "Half overflow");
    check(half_bits(1.0f + 1.0f / 2048) == 0x3c00, "Half even tie down");
    check(half_bits(1.0f + 3.0f / 2048) == 0x3c02, "Half even tie up");
    check(half_bits(std::ldexp(1.0f, -24)) == 1, "Half minimum subnormal");
    check(half_bits(std::ldexp(1.0f, -25)) == 0, "Half underflow tie");
    check(half_bits(std::nextafter(std::ldexp(1.0f, -25), 1.0f)) == 1,
          "Half above underflow tie");
    check(half_value(1) == std::ldexp(1.0f, -24), "Half decode subnormal");
    check(std::isnan(half_value(half_bits(std::numeric_limits<float>::quiet_NaN()))), "Half NaN");
    check(std::isinf(half_value(0xfc00)), "Half negative infinity");
    check(std::signbit(half_value(0x8000)), "Half zero sign preserved");
}
void all_half_roundtrips() {
    for (unsigned bits = 0; bits < 65536; ++bits) {
        if ((bits & 0x7c00u) == 0x7c00u && (bits & 0x3ffu)) continue;
        check(half_bits(half_value(static_cast<std::uint16_t>(bits))) == bits,
              "Representable half value did not round-trip");
    }
}
void bfloat_golden() {
    check(bfloat_bits(1.0f) == 0x3f80 && bfloat_bits(-2.0f) == 0xc000, "BF16 exact values");
    check(bfloat_bits(1.0f + 1.0f / 256) == 0x3f80, "BF16 even tie down");
    check(bfloat_bits(1.0f + 3.0f / 256) == 0x3f82, "BF16 even tie up");
    check(bfloat_bits(-0.0f) == 0x8000, "BF16 zero sign");
    check(std::isnan(bfloat_value(bfloat_bits(std::numeric_limits<float>::quiet_NaN()))), "BF16 NaN");
    check(bfloat_value(1) == std::ldexp(1.0f, -133), "BF16 minimum subnormal");
}
void all_bfloat_roundtrips() {
    for (unsigned bits = 0; bits < 65536; ++bits) {
        if ((bits & 0x7f80u) == 0x7f80u && (bits & 0x7fu)) continue;
        check(bfloat_bits(bfloat_value(static_cast<std::uint16_t>(bits))) == bits,
              "Representable BF16 value did not round-trip");
    }
}
void random_contract() {
    XorShift32 golden(1);
    check(golden.next() == 270369u, "First xorshift golden draw");
    check(golden.next() == 67634689u, "Second xorshift golden draw");
    check(golden.next() == 2647435461u, "Third xorshift golden draw");
    XorShift32 a(kInputSeed), b(kInputSeed);
    int negative = 0, positive = 0;
    for (int i = 0; i < 10000; ++i) {
        const float value = a.uniform();
        check(value == b.uniform(), "Input reproducibility");
        check(value >= -1.0f && value < 1.0f, "Input bound");
        check(std::floor(value * 524288.0f) == value * 524288.0f, "Exact binary input spacing");
        negative += value < 0; positive += value > 0;
    }
    check(negative > 4000 && positive > 4000, "Signed input coverage");
    rejected([] { XorShift32 invalid(0); });
}
void independent_rectangular_reference() {
    const CaseSpec normal{"manual", 2, 2, 3, false, false, 4, 5, 3, "manual"};
    const std::vector<float> a{1,-2,99,99, 2,0,99,99, -1,3,99,99};
    const std::vector<float> b{1,2,-2,99,99, -1,3,4,99,99};
    const double expected[2][2]{{7,1},{-8,14}};
    const CaseSpec transposed{"manual_T", 2, 2, 3, true, true, 5, 4, 3, "manual"};
    const std::vector<float> at{1,2,-1,99,99, -2,0,3,99,99};
    const std::vector<float> bt{1,-1,99,99, 2,3,99,99, -2,4,99,99};
    for (int row = 0; row < 2; ++row) for (int col = 0; col < 2; ++col) {
        check(reference_dot(normal, a, b, row, col) == expected[row][col],
              "Independent rectangular golden product");
        check(reference_dot(transposed, at, bt, row, col) == expected[row][col],
              "Transposed/strided golden product");
    }
}
void converted_references() {
    check(converted_value(0.1f, Mode::fp16) == 0.0999755859375f, "FP16 actual stored value");
    check(converted_value(0.1f, Mode::bf16) == 0.10009765625f, "BF16 actual stored value");
    check(converted_value(0.1f, Mode::tf32) == 0.1f, "TF32 reference must retain FP32 input");
    const CaseSpec spec{"quantized",1,1,2,false,false,1,2,1,"manual"};
    for (const auto mode : {Mode::fp16, Mode::bf16}) {
        const float stored = converted_value(0.1f, mode);
        const std::vector<float> a{stored, -stored}, b{stored, -stored};
        const double expected = 2 * static_cast<double>(stored) * stored;
        check(reference_dot(spec, a, b, 0, 0) == expected, "Reference uses quantized operands");
        check(expected != 2 * static_cast<double>(0.1f) * 0.1f, "Reference must not use original operands");
    }
}
void small_case_contract() {
    std::size_t checked = 0;
    for (const auto& spec : small_cases()) {
        checked += static_cast<std::size_t>(spec.m) * spec.n;
        for (const auto& mode : modes()) {
            std::vector<float> a, b;
            make_small_inputs(spec, mode.id, a, b);
            check(a.size() == a_elements(spec) && b.size() == b_elements(spec), "Small input sizes");
            for (int col = 0; col < spec.n; ++col) for (int row = 0; row < spec.m; ++row) {
                const double value = reference_dot(spec, a, b, row, col);
                check(std::isfinite(value), "Small reference finite");
                if (std::strcmp(spec.name, "zero_product") == 0) check(value == 0, "Zero case");
                if (std::strcmp(spec.name, "identity") == 0)
                    check(value == b[matrix_index(row,col,spec.ldb,false)], "Identity case");
            }
        }
    }
    check(checked == 3783, "Exact full small coverage");
}
void corruption_and_nonfinite() {
    const double reference[9]{1,-2,0,4,-5,6,0,8,9};
    double output[9]{1,-2,0,4,-4,6,0,8,9};
    check(output[0] == reference[0] && output[8] == reference[8], "Endpoints unchanged");
    Metrics metric(Tolerance{2e-5,2e-5});
    for (int i = 0; i < 9; ++i) metric.observe(output[i], reference[i]);
    check(!metric.passed && metric.checked == 9 && metric.max_absolute_error == 1,
          "Interior corruption must be detected");
    check(metric.max_reference_magnitude == 9, "Reference magnitude");
    for (const auto bad : {std::numeric_limits<double>::infinity(),
                          -std::numeric_limits<double>::infinity(),
                          std::numeric_limits<double>::quiet_NaN()}) {
        Metrics nonfinite(Tolerance{0.1,0.1});
        check(!nonfinite.observe(bad,1) && !nonfinite.passed, "Nonfinite actual rejected");
        Metrics bad_reference(Tolerance{0.1,0.1});
        check(!bad_reference.observe(1,bad), "Nonfinite reference rejected");
    }
}
void exact_metrics() {
    Metrics exact(Tolerance{0,0});
    check(exact.observe(-0.0,+0.0), "Signed zeros compare equal");
    check(exact.observe(0.125,0.125) && exact.max_scaled_error == 0, "Exact denominator zero");
    check(!exact.observe(0.25,0.125), "Exact mismatch must fail");
    Metrics bound(Tolerance{0.25,0.125});
    check(bound.observe(1.5,1.0) == false, "Combined tolerance failure");
    Metrics left(Tolerance{0.1,0}), right(Tolerance{0.1,0});
    left.observe(1.01,1); right.observe(-4.02,-4); left.merge(right);
    check(left.checked == 2 && left.max_reference_magnitude == 4 && left.passed,
          "Merged metric coverage");
    rejected([&] { left.merge(Metrics(Tolerance{0.2,0})); });
}
void stratified_positions() {
    const auto positions = sampled_positions();
    std::set<std::pair<int,int>> unique;
    for (int index = 0; index < kSampledElements; ++index) {
        const auto position = positions[index];
        const int tile_row = index / 32, tile_col = index % 32;
        check(position.row / 128 == tile_row && position.col / 128 == tile_col, "One sample per tile");
        if (tile_row == 0) check(position.row == 0, "Top edge sample");
        if (tile_row == 31) check(position.row == 4095, "Bottom edge sample");
        if (tile_col == 0) check(position.col == 0, "Left edge sample");
        if (tile_col == 31) check(position.col == 4095, "Right edge sample");
        unique.emplace(position.row,position.col);
    }
    check(unique.size() == 1024, "Sample uniqueness");
    check(unique.count({0,0}) && unique.count({0,4095}) && unique.count({4095,0}) &&
          unique.count({4095,4095}), "All corners sampled");
}
void known_answer_precision() {
    for (const auto& mode : modes()) for (int row = 0; row < 165; ++row) {
        for (int col = 0; col < 33; ++col) {
            const float diagonal = converted_value(known_diagonal(row), mode.id);
            const float input = converted_value(binary_pattern(row,col), mode.id);
            const float output = diagonal * input;
            check(static_cast<double>(output) == known_reference(row,col), "Exact diagonal reference");
        }
    }
}
void timing_contract() {
    std::vector<Sample> samples;
    for (int i = 0; i < 30; ++i) samples.push_back({i,static_cast<double>(30-i)*2,2});
    check(median_ms(samples) == 15.5, "Median must normalize repetitions");
    check(samples.front().elapsed_ms == 60 && samples.back().elapsed_ms == 2, "Raw order preserved");
    check(std::abs(tflops(1)-137.438953472) < 1e-12, "TFLOPS conversion");
    for (const double bad : {0.0,-1.0,std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN()}) {
        auto altered = samples; altered[4].elapsed_ms = bad;
        rejected([&] { median_ms(altered); });
        rejected([&] { tflops(bad); });
    }
    auto altered = samples; altered[5].repetitions = 0;
    rejected([&] { median_ms(altered); });
    altered = samples; altered[5].index = 6;
    rejected([&] { median_ms(altered); });
    altered.pop_back();
    rejected([&] { median_ms(altered); });
}
void environment_privacy_contract() {
    for (const auto* name : environment_names()) {
        check(allowed_environment_value(name, nullptr), "Absent environment value");
        check(allowed_environment_value(name, ""), "Empty environment value");
        for (const auto* private_text : {"GPU-example-identifier", "C:\\private\\file", "secret text"})
            check(!allowed_environment_value(name, private_text), "Private environment text refused");
    }
    check(!allowed_environment_value("NVIDIA_TF32_OVERRIDE", "0"), "TF32 override zero refused");
    check(!allowed_environment_value("NVIDIA_TF32_OVERRIDE", "1"), "TF32 override one refused");
    for (const auto* allowed : {":16:8", ":4096:8"})
        check(allowed_environment_value("CUBLAS_WORKSPACE_CONFIG", allowed), "Documented workspace configuration");
    for (const auto* denied : {":32:8", ":16:8 ", "4096:8", ":4096:8:16:8"})
        check(!allowed_environment_value("CUBLAS_WORKSPACE_CONFIG", denied), "Other workspace values refused");
    for (const auto* allowed : {"FASTEST_FIRST", "PCI_BUS_ID"})
        check(allowed_environment_value("CUDA_DEVICE_ORDER", allowed), "Documented device order");
    check(!allowed_environment_value("CUDA_DEVICE_ORDER", "pci_bus_id"), "Device order case strict");
    for (const auto* allowed : {"0", "63", "0,1,2", "63,0"})
        check(allowed_environment_value("CUDA_VISIBLE_DEVICES", allowed), "Canonical ordinal list");
    for (const auto* denied : {"-1", "64", "01", "1,1", "0, 1", "0,", ",0", "0,,1", "1.0", "1;2"})
        check(!allowed_environment_value("CUDA_VISIBLE_DEVICES", denied), "Invalid ordinal list refused");
    std::string all;
    for (int index = 0; index < 64; ++index) {
        if (index) all += ',';
        all += std::to_string(index);
    }
    check(allowed_environment_value("CUDA_VISIBLE_DEVICES", all.c_str()), "All 64 distinct ordinals");
    all += ",0";
    check(!allowed_environment_value("CUDA_VISIBLE_DEVICES", all.c_str()), "65th ordinal refused");
    const std::string oversized(1000, '1');
    check(!allowed_environment_value("CUDA_VISIBLE_DEVICES", oversized.c_str()), "Bounded environment length");
    check(!allowed_environment_value("UNRECOGNIZED", nullptr), "Unknown environment key refused");
    check(!allowed_environment_value(nullptr, nullptr), "Null environment key refused");
}
void pci_contract() {
    check(canonical_pci("0000:07:00.0") == "00000000:07:00.0", "Short domain normalization");
    check(canonical_pci("00000000:07:00.0") == "00000000:07:00.0", "Full domain normalization");
    check(canonical_pci("ABCD:0F:1F.7") == "0000abcd:0f:1f.7", "PCI case normalization");
    for (const auto* invalid : {"07:00.0","0000:07:20.0","0000:07:00.8",
                                "0000:07:00.0x","0000;07:00.0","0000:GG:00.0",""})
        rejected([&] { canonical_pci(invalid); });
}
}
int main() {
    const std::vector<std::pair<const char*,std::function<void()>>> tests{
        {"half_golden",half_golden}, {"all_half_roundtrips",all_half_roundtrips},
        {"bfloat_golden",bfloat_golden}, {"all_bfloat_roundtrips",all_bfloat_roundtrips},
        {"random_contract",random_contract},
        {"independent_rectangular_reference",independent_rectangular_reference},
        {"converted_references",converted_references}, {"small_case_contract",small_case_contract},
        {"corruption_and_nonfinite",corruption_and_nonfinite}, {"exact_metrics",exact_metrics},
        {"stratified_positions",stratified_positions}, {"known_answer_precision",known_answer_precision},
        {"timing_contract",timing_contract}, {"pci_contract",pci_contract},
        {"environment_privacy_contract",environment_privacy_contract}
    };
    int failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& error) {
            ++failures; std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failures << "/" << tests.size()
              << " CPU-only numerical test groups passed; no GPU workload.\n";
    return failures ? 1 : 0;
}
