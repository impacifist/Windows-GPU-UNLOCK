// SPDX-License-Identifier: GPL-2.0-only
// Ordinary CUDA baseline on current settings; protocol 3.
// No firmware, driver, clock, power or security changes.
// GPU execution requires --run --expected-pci. No arguments launch no workload.
#include "numerics.hpp"
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>

namespace {
using namespace baseline3080;
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t interrupted = 0;
void on_interrupt(int) { interrupted = 1; }

class Budget {
    Clock::time_point start_ = Clock::now();
public:
    void check() const {
        if (interrupted) throw std::runtime_error("Cancelled; result is incomplete");
        if (std::chrono::duration<double>(Clock::now() - start_).count() >= kProcessBudgetSeconds)
            throw std::runtime_error("120-second cooperative budget exceeded; result is incomplete");
    }
};
void cuda_check(cudaError_t status, const char* operation) {
    if (status != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(status));
}
void blas_check(cublasStatus_t status, const char* operation) {
    if (status != CUBLAS_STATUS_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": cuBLAS status " +
                                 std::to_string(static_cast<int>(status)));
}
struct Stream {
    cudaStream_t value = nullptr;
    Stream() { cuda_check(cudaStreamCreateWithFlags(&value, cudaStreamNonBlocking), "Create stream"); }
    ~Stream() { if (value) cudaStreamDestroy(value); }
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
};
struct BlasHandle {
    cublasHandle_t value = nullptr;
    BlasHandle() { blas_check(cublasCreate(&value), "Create cuBLAS handle"); }
    ~BlasHandle() { if (value) cublasDestroy(value); }
    BlasHandle(const BlasHandle&) = delete;
    BlasHandle& operator=(const BlasHandle&) = delete;
};
struct Event {
    cudaEvent_t value = nullptr;
    Event() { cuda_check(cudaEventCreate(&value), "Create event"); }
    ~Event() { if (value) cudaEventDestroy(value); }
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
};
struct AllocationLedger { std::size_t current = 0, peak = 0; };
class DeviceBuffer {
    AllocationLedger& ledger_;
    std::size_t bytes_;
    void* pointer_ = nullptr;
public:
    DeviceBuffer(AllocationLedger& ledger, std::size_t bytes, const Budget& budget)
        : ledger_(ledger), bytes_(bytes) {
        budget.check();
        if (bytes > kAllocationBudgetBytes || ledger.current > kAllocationBudgetBytes - bytes)
            throw std::runtime_error("Application GPU allocation budget exceeded");
        cuda_check(cudaMalloc(&pointer_, bytes), "Allocate matrix buffer");
        ledger_.current += bytes;
        ledger_.peak = std::max(ledger_.peak, ledger_.current);
    }
    ~DeviceBuffer() {
        if (pointer_) cudaFree(pointer_);
        ledger_.current -= bytes_;
    }
    void* get() const { return pointer_; }
    DeviceBuffer(const DeviceBuffer&) = delete;
    DeviceBuffer& operator=(const DeviceBuffer&) = delete;
};
struct CaseResult { CaseSpec spec; Metrics metrics; };
struct ModeResult {
    ModeSpec spec;
    Metrics small, known, timed;
    std::vector<CaseResult> cases;
    std::vector<Sample> samples;
    double median = 0.0, rate = 0.0;
    explicit ModeResult(ModeSpec mode)
        : spec(mode), small(small_tolerance(mode.id)), known(Tolerance{0, 0}),
          timed(timed_tolerance(mode.id)) {}
};
struct EnvironmentValue { const char* name = nullptr; bool present = false; std::string value; };
using Environment = std::array<EnvironmentValue, 4>;
Environment validated_environment() {
    Environment result{};
    for (std::size_t index = 0; index < environment_names().size(); ++index) {
        const char* name = environment_names()[index];
        const char* value = std::getenv(name);
        if (!allowed_environment_value(name, value))
            throw std::runtime_error(std::string("Environment value refused for ") + name);
        result[index] = EnvironmentValue{name, value != nullptr, value ? value : ""};
    }
    return result;
}
struct Trial {
    std::string pci, name;
    Environment environment;
    int cuda_runtime = 0, cuda_driver = 0, cublas_version = 0;
    std::size_t peak_allocated_bytes = 0;
    std::vector<ModeResult> results;
};
const CaseSpec large_case{"timed_random", kSize, kSize, kSize, false, false,
                          kSize, kSize, kSize, "seeded_uniform"};

cudaDataType_t input_type(Mode mode) {
    if (mode == Mode::fp16) return CUDA_R_16F;
    if (mode == Mode::bf16) return CUDA_R_16BF;
    return CUDA_R_32F;
}
cublasComputeType_t compute_type(Mode mode) {
    if (mode == Mode::fp32) return CUBLAS_COMPUTE_32F_PEDANTIC;
    if (mode == Mode::tf32) return CUBLAS_COMPUTE_32F_FAST_TF32;
    return CUBLAS_COMPUTE_32F;
}
void gemm(cublasHandle_t handle, Mode mode, const CaseSpec& spec,
          const DeviceBuffer& a, const DeviceBuffer& b, const DeviceBuffer& c,
          const Budget& budget) {
    budget.check();
    const float alpha = 1.0f, beta = 0.0f;
    blas_check(cublasGemmEx(handle, spec.transa ? CUBLAS_OP_T : CUBLAS_OP_N,
        spec.transb ? CUBLAS_OP_T : CUBLAS_OP_N, spec.m, spec.n, spec.k,
        &alpha, a.get(), input_type(mode), spec.lda, b.get(), input_type(mode), spec.ldb,
        &beta, c.get(), CUDA_R_32F, spec.ldc, compute_type(mode), CUBLAS_GEMM_DEFAULT),
        "cublasGemmEx");
}
std::vector<std::uint16_t> pack_input(const std::vector<float>& values, Mode mode,
                                     const Budget& budget) {
    std::vector<std::uint16_t> result(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        if ((i & 65535u) == 0) budget.check();
        result[i] = mode == Mode::fp16 ? half_bits(values[i]) : bfloat_bits(values[i]);
    }
    return result;
}
void upload_inputs(const std::vector<float>& a, const std::vector<float>& b,
                   Mode mode, const DeviceBuffer& da, const DeviceBuffer& db,
                   cudaStream_t stream, const Budget& budget) {
    budget.check();
    if (uses_16bit_input(mode)) {
        const auto a16 = pack_input(a, mode, budget);
        const auto b16 = pack_input(b, mode, budget);
        cuda_check(cudaMemcpyAsync(da.get(), a16.data(), a16.size() * sizeof(std::uint16_t),
                                   cudaMemcpyHostToDevice, stream), "Copy 16-bit A");
        cuda_check(cudaMemcpyAsync(db.get(), b16.data(), b16.size() * sizeof(std::uint16_t),
                                   cudaMemcpyHostToDevice, stream), "Copy 16-bit B");
        cuda_check(cudaStreamSynchronize(stream), "Finish 16-bit input copies");
    } else {
        cuda_check(cudaMemcpyAsync(da.get(), a.data(), a.size() * sizeof(float),
                                   cudaMemcpyHostToDevice, stream), "Copy FP32 A");
        cuda_check(cudaMemcpyAsync(db.get(), b.data(), b.size() * sizeof(float),
                                   cudaMemcpyHostToDevice, stream), "Copy FP32 B");
        cuda_check(cudaStreamSynchronize(stream), "Finish FP32 input copies");
    }
    budget.check();
}
void download(const DeviceBuffer& source, std::vector<float>& output,
              cudaStream_t stream, const Budget& budget) {
    budget.check();
    cuda_check(cudaMemcpyAsync(output.data(), source.get(), output.size() * sizeof(float),
                               cudaMemcpyDeviceToHost, stream), "Copy FP32 output");
    cuda_check(cudaStreamSynchronize(stream), "Finish output copy");
    budget.check();
}
void require_valid(const Metrics& metrics, std::size_t expected, const std::string& label) {
    if (!metrics.passed || metrics.checked != expected)
        throw std::runtime_error("Numerical validation failed: " + label + "; result is incomplete");
}
void validate_small(ModeResult& result, cublasHandle_t handle, cudaStream_t stream,
                    AllocationLedger& ledger, const Budget& budget) {
    for (const auto& spec : small_cases()) {
        budget.check();
        std::vector<float> a, b;
        make_small_inputs(spec, result.spec.id, a, b);
        std::vector<float> output(static_cast<std::size_t>(spec.ldc) * spec.n);
        const std::size_t input_bytes = uses_16bit_input(result.spec.id) ? 2 : 4;
        DeviceBuffer da(ledger, a.size() * input_bytes, budget);
        DeviceBuffer db(ledger, b.size() * input_bytes, budget);
        DeviceBuffer dc(ledger, output.size() * sizeof(float), budget);
        upload_inputs(a, b, result.spec.id, da, db, stream, budget);
        gemm(handle, result.spec.id, spec, da, db, dc, budget);
        download(dc, output, stream, budget);
        Metrics metrics(small_tolerance(result.spec.id));
        for (int col = 0; col < spec.n; ++col) {
            budget.check();
            for (int row = 0; row < spec.m; ++row)
                metrics.observe(output[static_cast<std::size_t>(col) * spec.ldc + row],
                                reference_dot(spec, a, b, row, col));
        }
        require_valid(metrics, static_cast<std::size_t>(spec.m) * spec.n,
                      std::string(result.spec.name) + "/" + spec.name);
        result.small.merge(metrics);
        result.cases.push_back(CaseResult{spec, metrics});
    }
    require_valid(result.small, 3783, std::string(result.spec.name) + "/small_full");
}
void fill_known_inputs(std::vector<float>& a, std::vector<float>& b, const Budget& budget) {
    for (int col = 0; col < kSize; ++col) {
        budget.check();
        for (int row = 0; row < kSize; ++row) {
            const auto index = static_cast<std::size_t>(col) * kSize + row;
            a[index] = row == col ? known_diagonal(row) : 0.0f;
            b[index] = binary_pattern(row, col);
        }
    }
}
void fill_random_inputs(std::vector<float>& a, std::vector<float>& b, Mode mode,
                        const Budget& budget) {
    XorShift32 random(kInputSeed);
    for (auto* values : {&a, &b}) {
        for (std::size_t i = 0; i < values->size(); ++i) {
            if ((i & 65535u) == 0) budget.check();
            (*values)[i] = converted_value(random.uniform(), mode);
        }
    }
}
ModeResult measure(ModeSpec spec, cublasHandle_t handle, cudaStream_t stream,
                   AllocationLedger& ledger, const Budget& budget) {
    budget.check();
    const auto math_mode = spec.id == Mode::fp32 ? CUBLAS_PEDANTIC_MATH :
        static_cast<cublasMath_t>(CUBLAS_DEFAULT_MATH |
                                 CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION);
    blas_check(cublasSetMathMode(handle, math_mode), "Set math mode");
    ModeResult result(spec);
    validate_small(result, handle, stream, ledger, budget);
    const std::size_t elements = static_cast<std::size_t>(kSize) * kSize;
    const std::size_t input_bytes = uses_16bit_input(spec.id) ? 2 : 4;
    const std::size_t required = elements * (2 * input_bytes + sizeof(float));
    std::size_t free_bytes = 0, total_bytes = 0;
    cuda_check(cudaMemGetInfo(&free_bytes, &total_bytes), "Read available VRAM");
    if (free_bytes < required + 256ull * 1024 * 1024)
        throw std::runtime_error("Insufficient free VRAM including 256 MiB workspace headroom");
    DeviceBuffer da(ledger, elements * input_bytes, budget);
    DeviceBuffer db(ledger, elements * input_bytes, budget);
    DeviceBuffer dc(ledger, elements * sizeof(float), budget);
    std::vector<float> a(elements), b(elements), output(elements);
    fill_known_inputs(a, b, budget);
    upload_inputs(a, b, spec.id, da, db, stream, budget);
    gemm(handle, spec.id, large_case, da, db, dc, budget);
    download(dc, output, stream, budget);
    for (int col = 0; col < kSize; ++col) {
        budget.check();
        for (int row = 0; row < kSize; ++row)
            result.known.observe(output[static_cast<std::size_t>(col) * kSize + row],
                                 known_reference(row, col));
    }
    require_valid(result.known, elements, std::string(spec.name) + "/known_answer_full");

    // Reference arrays retain the actual converted storage values for 16-bit modes.
    fill_random_inputs(a, b, spec.id, budget);
    upload_inputs(a, b, spec.id, da, db, stream, budget);
    const auto warmup_start = Clock::now();
    do {
        gemm(handle, spec.id, large_case, da, db, dc, budget);
        cuda_check(cudaStreamSynchronize(stream), "Synchronize warmup");
        budget.check();
    } while (std::chrono::duration<double>(Clock::now() - warmup_start).count() < kWarmupSeconds);

    Event start, stop;
    result.samples.reserve(kSamples);
    for (int index = 0; index < kSamples; ++index) {
        budget.check();
        cuda_check(cudaEventRecord(start.value, stream), "Record start event");
        gemm(handle, spec.id, large_case, da, db, dc, budget);
        cuda_check(cudaEventRecord(stop.value, stream), "Record stop event");
        cuda_check(cudaEventSynchronize(stop.value), "Synchronize stop event");
        float elapsed_ms = 0;
        cuda_check(cudaEventElapsedTime(&elapsed_ms, start.value, stop.value), "Read event duration");
        budget.check();
        result.samples.push_back(Sample{index, static_cast<double>(elapsed_ms), 1});
    }
    result.median = median_ms(result.samples);
    result.rate = tflops(result.median);

    // This is the final timed output, not an extra recomputation.
    download(dc, output, stream, budget);
    for (std::size_t i = 0; i < elements; ++i) {
        if ((i & 65535u) == 0) budget.check();
        if (!std::isfinite(output[i]))
            throw std::runtime_error("Non-finite timed output; result is incomplete");
    }
    for (const auto& position : sampled_positions()) {
        budget.check();
        const auto index = static_cast<std::size_t>(position.col) * kSize + position.row;
        result.timed.observe(output[index], reference_dot(large_case, a, b, position.row, position.col));
    }
    require_valid(result.timed, kSampledElements, std::string(spec.name) + "/timed_sampled");
    return result;
}
Trial execute(const std::string& expected_pci, const Budget& budget) {
    budget.check();
    Trial trial;
    // Snapshot validated publishable values before the first CUDA call.
    trial.environment = validated_environment();
    int count = 0, selected = -1;
    cuda_check(cudaGetDeviceCount(&count), "Enumerate CUDA devices");
    if (count < 1 || count > 64) throw std::runtime_error("Unexpected CUDA device count");
    for (int device = 0; device < count; ++device) {
        budget.check();
        cudaDeviceProp properties{};
        cuda_check(cudaGetDeviceProperties(&properties, device), "Read CUDA device properties");
        char pci[64]{};
        cuda_check(cudaDeviceGetPCIBusId(pci, sizeof(pci), device), "Read CUDA PCI identity");
        if (canonical_pci(pci) == expected_pci &&
            std::strcmp(properties.name, "NVIDIA GeForce RTX 3080") == 0 &&
            properties.major == 8 && properties.minor == 6) {
            if (selected != -1) throw std::runtime_error("Multiple matching devices");
            selected = device;
            trial.pci = canonical_pci(pci);
            trial.name = properties.name;
        }
    }
    if (selected < 0) throw std::runtime_error("Exact RTX 3080 / capability 8.6 / expected PCI device not found");
    budget.check();
    cuda_check(cudaSetDevice(selected), "Select matched CUDA device");
    Stream stream;
    BlasHandle handle;
    blas_check(cublasSetStream(handle.value, stream.value), "Bind explicit cuBLAS stream");
    blas_check(cublasSetPointerMode(handle.value, CUBLAS_POINTER_MODE_HOST), "Select FP32 host scalars");
    cuda_check(cudaRuntimeGetVersion(&trial.cuda_runtime), "Read CUDA runtime version");
    cuda_check(cudaDriverGetVersion(&trial.cuda_driver), "Read CUDA driver version");
    blas_check(cublasGetVersion(handle.value, &trial.cublas_version), "Read cuBLAS version");
    AllocationLedger ledger;
    for (const auto& mode : modes()) {
        trial.results.push_back(measure(mode, handle.value, stream.value, ledger, budget));
    }
    trial.peak_allocated_bytes = ledger.peak;
    budget.check();
    return trial;
}

void quote(std::ostream& out, const std::string& text) {
    out << '"';
    const char* hex = "0123456789abcdef";
    for (unsigned char ch : text) {
        if (ch == '"' || ch == '\\') out << '\\' << static_cast<char>(ch);
        else if (ch < 0x20) out << "\\u00" << hex[ch >> 4] << hex[ch & 15];
        else out << static_cast<char>(ch);
    }
    out << '"';
}
void config_json(std::ostream& out) {
    out << "{\"m\":" << kSize << ",\"n\":" << kSize << ",\"k\":" << kSize
        << ",\"warmup_seconds\":" << kWarmupSeconds << ",\"sample_count\":" << kSamples
        << ",\"input_seed\":" << kInputSeed << ",\"input_generator\":";
    quote(out, kInputGenerator);
    out << ",\"stream_policy\":"; quote(out, kStreamPolicy);
    out << ",\"gpu_allocation_budget_mib\":512,\"process_budget_seconds\":120"
        << ",\"layout\":\"column_major\",\"transa\":\"N\",\"transb\":\"N\""
        << ",\"lda\":4096,\"ldb\":4096,\"ldc\":4096}";
}
void metrics_fields(std::ostream& out, const Metrics& metrics, std::size_t total,
                    const char* coverage) {
    out << "\"passed\":" << (metrics.passed ? "true" : "false")
        << ",\"checked_elements\":" << metrics.checked << ",\"total_elements\":" << total
        << ",\"coverage\":"; quote(out, coverage);
    out << ",\"reference_precision\":\"fp64\""
        << ",\"absolute_tolerance\":" << metrics.tolerance.absolute
        << ",\"relative_tolerance\":" << metrics.tolerance.relative
        << ",\"max_absolute_error\":" << metrics.max_absolute_error
        << ",\"max_scaled_error\":" << metrics.max_scaled_error
        << ",\"max_reference_magnitude\":" << metrics.max_reference_magnitude;
}
void mode_json(std::ostream& out, const ModeResult& result) {
    const auto& mode = result.spec;
    out << "{\"name\":"; quote(out, mode.name);
    out << ",\"input_type\":"; quote(out, mode.input_type);
    out << ",\"compute_type\":"; quote(out, mode.compute_type);
    out << ",\"output_type\":\"CUDA_R_32F\",\"alpha\":1,\"beta\":0"
        << ",\"api\":\"cublasGemmEx\",\"scalar_type\":\"CUDA_R_32F\",\"math_mode\":";
    quote(out, mode.math_mode);
    out << ",\"algorithm\":\"CUBLAS_GEMM_DEFAULT\",\"samples\":[";
    for (std::size_t i = 0; i < result.samples.size(); ++i) {
        if (i) out << ',';
        const auto& sample = result.samples[i];
        out << "{\"index\":" << sample.index << ",\"elapsed_ms\":" << sample.elapsed_ms
            << ",\"repetitions\":" << sample.repetitions << '}';
    }
    out << "],\"median_ms\":" << result.median << ",\"tflops\":" << result.rate
        << ",\"validation\":{\"small_full\":{";
    metrics_fields(out, result.small, 3783, "full");
    out << ",\"cases\":[";
    for (std::size_t i = 0; i < result.cases.size(); ++i) {
        if (i) out << ',';
        const auto& item = result.cases[i];
        const auto& spec = item.spec;
        out << '{';
        metrics_fields(out, item.metrics, static_cast<std::size_t>(spec.m) * spec.n, "full");
        out << ",\"name\":"; quote(out, spec.name);
        out << ",\"m\":" << spec.m << ",\"n\":" << spec.n << ",\"k\":" << spec.k
            << ",\"transa\":\"" << (spec.transa ? 'T' : 'N')
            << "\",\"transb\":\"" << (spec.transb ? 'T' : 'N')
            << "\",\"lda\":" << spec.lda << ",\"ldb\":" << spec.ldb << ",\"ldc\":" << spec.ldc
            << ",\"input_pattern\":"; quote(out, spec.input_pattern);
        out << '}';
    }
    const auto total = static_cast<std::size_t>(kSize) * kSize;
    out << "]},\"known_answer_full\":{";
    metrics_fields(out, result.known, total, "full");
    out << ",\"input_pattern\":\"signed_diagonal_binary\"},\"timed_sampled\":{";
    metrics_fields(out, result.timed, total, "sampled");
    out << ",\"finite_checked_elements\":" << total << ",\"finite_passed\":true"
        << ",\"sample_seed\":" << kSampleSeed << ",\"sampling_policy\":";
    quote(out, kSamplingPolicy);
    out << ",\"positions\":[";
    const auto positions = sampled_positions();
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (i) out << ',';
        out << '[' << positions[i].row << ',' << positions[i].col << ']';
    }
    out << "]}}}";
}
void environment_json(std::ostream& out, const Environment& environment) {
    out << '{';
    for (std::size_t i = 0; i < environment.size(); ++i) {
        if (i) out << ',';
        quote(out, environment[i].name); out << ':';
        if (environment[i].present) quote(out, environment[i].value); else out << "null";
    }
    out << '}';
}
void inspect_only() {
    // Deliberately no CUDA/cuBLAS function calls, device enumeration or context.
    std::cout << "{\"benchmark_protocol\":3,\"gpu_workload_run\":false"
              << ",\"inspection_scope\":\"compile_time_metadata_no_cuda_calls\",\"config\":";
    config_json(std::cout);
    std::cout << ",\"compile\":{\"cuda_runtime\":" << CUDART_VERSION;
#ifdef _MSC_VER
    std::cout << ",\"msvc\":" << _MSC_VER;
#endif
#ifdef __CUDACC_VER_MAJOR__
    std::cout << ",\"nvcc_major\":" << __CUDACC_VER_MAJOR__
              << ",\"nvcc_minor\":" << __CUDACC_VER_MINOR__
              << ",\"nvcc_build\":" << __CUDACC_VER_BUILD__;
#endif
    std::cout << "},\"modes\":[";
    for (std::size_t i = 0; i < modes().size(); ++i) {
        if (i) std::cout << ',';
        quote(std::cout, modes()[i].name);
    }
    std::cout << "]}\n";
}
} // namespace

int main(int argc, char** argv) {
    std::cout.imbue(std::locale::classic());
    std::cout << std::setprecision(17);
    if (argc == 2 && std::strcmp(argv[1], "--inspect-only") == 0) {
        inspect_only();
        return 0;
    }
    const char* usage = "Usage: baseline --inspect-only | --run --expected-pci DOMAIN:BUS:DEVICE.FUNCTION\n";
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        std::cout << usage;
        return 0;
    }
    if (argc != 4 || std::strcmp(argv[1], "--run") != 0 ||
        std::strcmp(argv[2], "--expected-pci") != 0) {
        std::cerr << usage << "No GPU workload was started.\n";
        return 2;
    }
    try {
        const auto expected_pci = canonical_pci(argv[3]);
        std::signal(SIGINT, on_interrupt);
        std::signal(SIGTERM, on_interrupt);
#ifdef SIGBREAK
        std::signal(SIGBREAK, on_interrupt);
#endif
        Budget budget;
        const Trial trial = execute(expected_pci, budget);
        budget.check();
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(17);
        output << "{\"benchmark_protocol\":3,\"gpu_workload_run\":true,\"gpu\":{\"name\":";
        quote(output, trial.name);
        output << ",\"pci_bus_id\":"; quote(output, trial.pci);
        output << ",\"compute_capability\":\"8.6\"},\"runtime\":{\"cuda_runtime\":"
               << trial.cuda_runtime << ",\"cuda_driver\":" << trial.cuda_driver
               << ",\"cublas_version\":" << trial.cublas_version << "},\"config\":";
        config_json(output);
        output << ",\"environment\":"; environment_json(output, trial.environment);
        output << ",\"peak_application_gpu_allocation_bytes\":" << trial.peak_allocated_bytes
               << ",\"modes\":[";
        for (std::size_t i = 0; i < trial.results.size(); ++i) {
            if (i) output << ',';
            mode_json(output, trial.results[i]);
        }
        output << "]}\n";
        budget.check();
        std::cout << output.str();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Baseline failed: " << error.what() << '\n';
        // No successful JSON document is emitted for partial/failed execution.
        return 1;
    }
}
