# SPDX-License-Identifier: GPL-2.0-only
"""Strict offline reader for the ordinary RTX 3080 benchmark protocol.

This module performs no GPU queries, process launches, or system changes.
Result validation checks the internal evidence contract; JSON cannot prove
that the program actually ran, that every calculation is correct, or that
three records came from fresh processes. The runner owns process creation.
"""

from copy import deepcopy
import hashlib
import json
import math
from pathlib import Path
import re
import statistics


BENCHMARK_PROTOCOL = 3
EXPECTED_GPU = "NVIDIA GeForce RTX 3080"
EXPECTED_PCI = "0000:07:00.0"
EXPECTED_CAPABILITY = "8.6"
ENVIRONMENT_FIELDS = {
    "NVIDIA_TF32_OVERRIDE", "CUBLAS_WORKSPACE_CONFIG",
    "CUDA_VISIBLE_DEVICES", "CUDA_DEVICE_ORDER",
}
CONFIG = {
    "m": 4096, "n": 4096, "k": 4096,
    "warmup_seconds": 2, "sample_count": 30,
    "input_seed": 30802026, "input_generator": "xorshift32_uniform20_v1",
    "stream_policy": "single_explicit_nonblocking_stream",
    "gpu_allocation_budget_mib": 512, "process_budget_seconds": 120,
    "layout": "column_major", "transa": "N", "transb": "N",
    "lda": 4096, "ldb": 4096, "ldc": 4096,
}
MODE_SPECS = (
    ("fp32_pedantic", "CUDA_R_32F", "CUBLAS_COMPUTE_32F_PEDANTIC",
     "CUBLAS_PEDANTIC_MATH"),
    ("fp16_tensor_fp32_accumulate", "CUDA_R_16F", "CUBLAS_COMPUTE_32F",
     "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"),
    ("bf16_tensor_fp32_accumulate", "CUDA_R_16BF", "CUBLAS_COMPUTE_32F",
     "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"),
    ("tf32_tensor_fp32_accumulate", "CUDA_R_32F", "CUBLAS_COMPUTE_32F_FAST_TF32",
     "CUBLAS_DEFAULT_MATH|CUBLAS_MATH_DISALLOW_REDUCED_PRECISION_REDUCTION"),
)
MODE_NAMES = tuple(spec[0] for spec in MODE_SPECS)
SMALL_CASES = (
    {"name": "signed_square", "m": 17, "n": 17, "k": 17,
     "transa": "N", "transb": "N", "lda": 20, "ldb": 19, "ldc": 21,
     "input_pattern": "exact_binary"},
    {"name": "signed_rectangular", "m": 31, "n": 47, "k": 65,
     "transa": "N", "transb": "N", "lda": 34, "ldb": 69, "ldc": 36,
     "input_pattern": "seeded_uniform"},
    {"name": "transpose_stride", "m": 31, "n": 47, "k": 65,
     "transa": "T", "transb": "T", "lda": 69, "ldb": 52, "ldc": 37,
     "input_pattern": "seeded_uniform"},
    {"name": "zero_product", "m": 13, "n": 11, "k": 19,
     "transa": "N", "transb": "N", "lda": 16, "ldb": 23, "ldc": 17,
     "input_pattern": "zero_a"},
    {"name": "identity", "m": 19, "n": 23, "k": 19,
     "transa": "N", "transb": "N", "lda": 22, "ldb": 24, "ldc": 25,
     "input_pattern": "identity_a"},
)
SAMPLE_SEED = 30801024
SAMPLE_POLICY = "32x32_tiles_edges_v1"
TOTAL_OUTPUTS = 4096 * 4096
SMALL_OUTPUTS = sum(case["m"] * case["n"] for case in SMALL_CASES)
METRIC_FIELDS = {
    "passed", "checked_elements", "total_elements", "coverage",
    "reference_precision", "absolute_tolerance", "relative_tolerance",
    "max_absolute_error", "max_scaled_error", "max_reference_magnitude",
}
MODE_FIELDS = {
    "name", "input_type", "compute_type", "output_type", "alpha", "beta",
    "api", "scalar_type", "math_mode", "algorithm", "samples",
    "median_ms", "tflops", "validation",
}


class ProtocolError(ValueError):
    """A saved record does not satisfy protocol 3."""


def _require(condition, message):
    if not condition:
        raise ProtocolError(message)


def _finite_number(value, path, *, positive=False, nonnegative=False):
    _require(type(value) in (int, float), f"{path}: expected a number, not a boolean")
    try:
        finite = math.isfinite(value)
    except (OverflowError, ValueError):
        finite = False
    _require(finite, f"{path}: expected a finite number")
    if positive:
        _require(value > 0, f"{path}: expected a positive number")
    if nonnegative:
        _require(value >= 0, f"{path}: expected a nonnegative number")
    return value


def _integer(value, path, *, positive=False):
    _require(type(value) is int, f"{path}: expected an integer, not a boolean")
    if positive:
        _require(value > 0, f"{path}: expected a positive integer")
    return value


def _fixed(value, expected, path):
    if type(expected) is int:
        _integer(value, path)
    else:
        _require(type(value) is type(expected), f"{path}: wrong value type")
    _require(value == expected, f"{path}: expected {expected!r}")


def _keys(value, expected, path):
    _require(type(value) is dict, f"{path}: expected an object")
    _require(all(type(key) is str for key in value), f"{path}: invalid object key")
    missing = set(expected) - set(value)
    extra = set(value) - set(expected)
    _require(not missing and not extra,
             f"{path}: fields differ (missing={sorted(missing)}, extra={sorted(extra)})")


def _json_values(value, path="$", ancestors=None, depth=0):
    _require(depth <= 64, f"{path}: JSON nesting limit exceeded")
    if ancestors is None:
        ancestors = set()
    if type(value) in (dict, list):
        _require(id(value) not in ancestors, f"{path}: cyclic object")
        ancestors.add(id(value))
        try:
            if type(value) is dict:
                _require(all(type(key) is str for key in value),
                         f"{path}: JSON object keys must be strings")
                for key, item in value.items():
                    _json_values(item, f"{path}.{key}", ancestors, depth + 1)
            else:
                for index, item in enumerate(value):
                    _json_values(item, f"{path}[{index}]", ancestors, depth + 1)
        finally:
            ancestors.remove(id(value))
    elif type(value) in (int, float):
        _finite_number(value, path)
    else:
        _require(value is None or type(value) in (str, bool),
                 f"{path}: unsupported JSON value type")


def _unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        _require(key not in result, f"JSON: duplicate key {key!r}")
        result[key] = value
    return result


def _invalid_constant(value):
    raise ProtocolError(f"JSON: non-finite number {value}")


def load_text(text):
    """Parse an object using strict JSON, rejecting duplicates and nonfinite numbers."""
    _require(type(text) is str, "JSON: expected text")
    try:
        value = json.loads(text.removeprefix("\ufeff"),
                           object_pairs_hook=_unique_pairs,
                           parse_constant=_invalid_constant)
    except (json.JSONDecodeError, RecursionError) as exc:
        raise ProtocolError(f"Invalid JSON: {exc}") from exc
    _require(type(value) is dict, "JSON: top-level value must be an object")
    _json_values(value)
    return value


def normalize_sha256(value):
    """Normalize a complete SHA-256 hex digest without accepting whitespace."""
    _require(type(value) is str and re.fullmatch(r"[0-9a-fA-F]{64}", value) is not None,
             "Invalid SHA256: expected 64 hexadecimal characters")
    return value.lower()


def load_file(path, *, expected_sha256=None):
    """Read UTF-8 JSON; optional digest checks saved bytes, including whitespace."""
    data = Path(path).read_bytes()
    if expected_sha256 is not None:
        expected = normalize_sha256(expected_sha256)
        _require(hashlib.sha256(data).hexdigest() == expected,
                 f"{Path(path).name}: SHA256 mismatch; file changed or is corrupt")
    try:
        text = data.decode("utf-8-sig")
    except UnicodeError as exc:
        raise ProtocolError(f"{Path(path).name}: invalid UTF-8 JSON") from exc
    return load_text(text)


def normalize_pci(value):
    _require(type(value) is str, "gpu.pci_bus_id: expected text")
    match = re.fullmatch(
        r"([0-9a-fA-F]{4}|[0-9a-fA-F]{8}):([0-9a-fA-F]{2}):"
        r"([0-9a-fA-F]{2})\.([0-7])", value)
    _require(match is not None, "gpu.pci_bus_id: invalid PCI location")
    parts = tuple(int(part, 16) for part in match.groups())
    _require(parts == (0, 7, 0, 0), "gpu.pci_bus_id: expected RTX 3080 at 07:00.0")
    return EXPECTED_PCI

def validate_environment(environment):
    """Validate publishable CUDA settings without echoing rejected private values."""
    _keys(environment, ENVIRONMENT_FIELDS, "environment")
    for key, value in environment.items():
        _require(value is None or type(value) is str,
                 f"environment.{key}: expected a string or null")
    allowed = {
        "NVIDIA_TF32_OVERRIDE": (None, ""),
        "CUBLAS_WORKSPACE_CONFIG": (None, "", ":16:8", ":4096:8"),
        "CUDA_DEVICE_ORDER": (None, "", "FASTEST_FIRST", "PCI_BUS_ID"),
    }
    for key, choices in allowed.items():
        _require(environment[key] in choices,
                 f"environment.{key}: unsupported setting")
    visible = environment["CUDA_VISIBLE_DEVICES"]
    if visible not in (None, ""):
        _require(len(visible) <= 191,
                 "environment.CUDA_VISIBLE_DEVICES: ordinal list is too long")
        indices = visible.split(",")
        _require(1 <= len(indices) <= 64 and len(set(indices)) == len(indices),
                 "environment.CUDA_VISIBLE_DEVICES: expected 1 to 64 unique indices")
        _require(all(re.fullmatch(r"0|[1-9][0-9]?", index) is not None
                     and int(index) <= 63 for index in indices),
                 "environment.CUDA_VISIBLE_DEVICES: expected canonical ordinal indices 0 to 63")
    return deepcopy(environment)


def expected_positions():
    """Return the exact version-1 stratified sample, including both edge draws."""
    state = SAMPLE_SEED
    positions = []
    for tile_row in range(32):
        for tile_column in range(32):
            offsets = []
            for _ in range(2):
                state ^= (state << 13) & 0xFFFFFFFF
                state ^= state >> 17
                state ^= (state << 5) & 0xFFFFFFFF
                state &= 0xFFFFFFFF
                offsets.append(state % 128)
            row = (0 if tile_row == 0 else 4095 if tile_row == 31
                   else tile_row * 128 + offsets[0])
            column = (0 if tile_column == 0 else 4095 if tile_column == 31
                      else tile_column * 128 + offsets[1])
            positions.append([row, column])
    return positions


def _metrics(value, path, *, checked, total, coverage, atol, rtol, extra=()):
    _keys(value, METRIC_FIELDS | set(extra), path)
    _require(value["passed"] is True, f"{path}.passed: numerical validation failed")
    _fixed(value["checked_elements"], checked, f"{path}.checked_elements")
    _fixed(value["total_elements"], total, f"{path}.total_elements")
    _fixed(value["coverage"], coverage, f"{path}.coverage")
    _fixed(value["reference_precision"], "fp64", f"{path}.reference_precision")
    for key, expected in (("absolute_tolerance", atol), ("relative_tolerance", rtol)):
        actual = _finite_number(value[key], f"{path}.{key}", nonnegative=True)
        _require(actual == expected, f"{path}.{key}: tolerance differs from protocol")
    absolute = _finite_number(value["max_absolute_error"],
                              f"{path}.max_absolute_error", nonnegative=True)
    scaled = _finite_number(value["max_scaled_error"],
                            f"{path}.max_scaled_error", nonnegative=True)
    reference = _finite_number(value["max_reference_magnitude"],
                               f"{path}.max_reference_magnitude", nonnegative=True)
    limit = atol + rtol * reference
    _finite_number(limit, f"{path}.maximum_error_limit", nonnegative=True)
    _require(absolute <= limit or math.isclose(absolute, limit, rel_tol=1e-12, abs_tol=0),
             f"{path}.max_absolute_error: exceeds recorded reference/tolerance bound")
    if limit > 0:
        _require(scaled >= absolute / limit or
                 math.isclose(scaled, absolute / limit, rel_tol=1e-12, abs_tol=0),
                 f"{path}: scaled error contradicts absolute error/reference bound")
    if atol > 0:
        _require(scaled <= absolute / atol or
                 math.isclose(scaled, absolute / atol, rel_tol=1e-12, abs_tol=0),
                 f"{path}: scaled error contradicts absolute error/absolute tolerance")
    _require(scaled <= 1, f"{path}.max_scaled_error: error exceeds fixed tolerance")
    _require((absolute == 0) == (scaled == 0),
             f"{path}: absolute and scaled errors are inconsistent")
    if atol == 0 and rtol == 0:
        _require(absolute == 0 and scaled == 0,
                 f"{path}: exact known-answer validation requires zero error")


def _validation(value, mode_index, path):
    _keys(value, {"small_full", "known_answer_full", "timed_sampled"}, path)
    small_atol, small_rtol = ((0.005, 0.005) if mode_index == 3 else (2e-5, 2e-5))
    timed_atol, timed_rtol = ((0.05, 0.01) if mode_index == 3 else (0.002, 0.0005))
    small = value["small_full"]
    _metrics(small, f"{path}.small_full", checked=SMALL_OUTPUTS,
             total=SMALL_OUTPUTS, coverage="full", atol=small_atol,
             rtol=small_rtol, extra={"cases"})
    cases = small["cases"]
    _require(type(cases) is list and len(cases) == len(SMALL_CASES),
             f"{path}.small_full.cases: expected all five ordered validation cases")
    for index, (case, spec) in enumerate(zip(cases, SMALL_CASES)):
        case_path = f"{path}.small_full.cases[{index}]"
        count = spec["m"] * spec["n"]
        _metrics(case, case_path, checked=count, total=count, coverage="full",
                 atol=small_atol, rtol=small_rtol, extra=set(spec))
        for key, expected in spec.items():
            _fixed(case[key], expected, f"{case_path}.{key}")
    for key in ("max_absolute_error", "max_scaled_error", "max_reference_magnitude"):
        _require(small[key] == max(case[key] for case in cases),
                 f"{path}.small_full.{key}: inconsistent case aggregate")

    known = value["known_answer_full"]
    _metrics(known, f"{path}.known_answer_full", checked=TOTAL_OUTPUTS,
             total=TOTAL_OUTPUTS, coverage="full", atol=0, rtol=0,
             extra={"input_pattern"})
    _fixed(known["input_pattern"], "signed_diagonal_binary",
           f"{path}.known_answer_full.input_pattern")

    sampled = value["timed_sampled"]
    sampled_path = f"{path}.timed_sampled"
    _metrics(sampled, sampled_path, checked=1024, total=TOTAL_OUTPUTS,
             coverage="sampled", atol=timed_atol, rtol=timed_rtol,
             extra={"finite_checked_elements", "finite_passed", "sample_seed",
                    "sampling_policy", "positions"})
    _fixed(sampled["finite_checked_elements"], TOTAL_OUTPUTS,
           f"{sampled_path}.finite_checked_elements")
    _require(sampled["finite_passed"] is True,
             f"{sampled_path}.finite_passed: full output finite scan failed")
    _fixed(sampled["sample_seed"], SAMPLE_SEED, f"{sampled_path}.sample_seed")
    _fixed(sampled["sampling_policy"], SAMPLE_POLICY, f"{sampled_path}.sampling_policy")
    positions = sampled["positions"]
    _require(type(positions) is list and len(positions) == 1024,
             f"{sampled_path}.positions: expected 1024 ordered sampled positions")
    for index, position in enumerate(positions):
        position_path = f"{sampled_path}.positions[{index}]"
        _require(type(position) is list and len(position) == 2,
                 f"{position_path}: expected [row, column]")
        row = _integer(position[0], f"{position_path}[0]")
        column = _integer(position[1], f"{position_path}[1]")
        tile_row, tile_column = divmod(index, 32)
        _require(0 <= row < 4096 and 0 <= column < 4096,
                 f"{position_path}: position outside output")
        _require(row // 128 == tile_row and column // 128 == tile_column,
                 f"{position_path}: wrong stratum or acquisition order")
        if tile_row in (0, 31):
            _require(row == (0 if tile_row == 0 else 4095),
                     f"{position_path}: missing prescribed boundary row")
        if tile_column in (0, 31):
            _require(column == (0 if tile_column == 0 else 4095),
                     f"{position_path}: missing prescribed boundary column")
    _require(positions == expected_positions(),
             f"{sampled_path}.positions: does not match fixed seed/sampling policy")
    _require(len({tuple(position) for position in positions}) == 1024,
             f"{sampled_path}.positions: duplicate samples")


def _timing(mode, path):
    samples = mode["samples"]
    _require(type(samples) is list and len(samples) == 30,
             f"{path}.samples: expected exactly 30 ordered raw timings")
    normalized = []
    for index, sample in enumerate(samples):
        sample_path = f"{path}.samples[{index}]"
        _keys(sample, {"index", "elapsed_ms", "repetitions"}, sample_path)
        _fixed(sample["index"], index, f"{sample_path}.index")
        elapsed = _finite_number(sample["elapsed_ms"],
                                 f"{sample_path}.elapsed_ms", positive=True)
        repetitions = _integer(sample["repetitions"],
                               f"{sample_path}.repetitions", positive=True)
        try:
            duration = elapsed / repetitions
        except (OverflowError, ZeroDivisionError) as exc:
            raise ProtocolError(f"{sample_path}: timing normalization failed") from exc
        _finite_number(duration, f"{sample_path}.normalized_ms", positive=True)
        normalized.append(duration)
    ordered = sorted(normalized)
    median = ordered[14] + (ordered[15] - ordered[14]) / 2
    _finite_number(median, f"{path}.recomputed_median_ms", positive=True)
    throughput = (2 * CONFIG["m"] * CONFIG["n"] * CONFIG["k"] / 1e9) / median
    _finite_number(throughput, f"{path}.recomputed_tflops", positive=True)
    for key, expected in (("median_ms", median), ("tflops", throughput)):
        reported = _finite_number(mode[key], f"{path}.{key}", positive=True)
        _require(math.isclose(reported, expected, rel_tol=1e-9, abs_tol=0),
                 f"{path}.{key}: does not match normalized raw timings")
    return median, throughput


def validate_trial(trial):
    """Validate one complete protocol-3 trial and return a detached canonical copy.

    Timings remain in acquisition order. Only PCI spelling and the two derived
    timing summaries are normalized. This does not attest process freshness.
    """
    _json_values(trial)
    _require(type(trial) is dict, "trial: expected an object")
    _fixed(trial.get("benchmark_protocol"), BENCHMARK_PROTOCOL, "benchmark_protocol")
    _keys(trial, {"benchmark_protocol", "gpu_workload_run", "gpu", "runtime",
                  "config", "environment", "peak_application_gpu_allocation_bytes",
                  "modes"}, "trial")
    _require(trial["gpu_workload_run"] is True,
             "gpu_workload_run: a completed benchmark record is required")
    gpu = trial["gpu"]
    _keys(gpu, {"name", "pci_bus_id", "compute_capability"}, "gpu")
    _fixed(gpu["name"], EXPECTED_GPU, "gpu.name")
    canonical_pci = normalize_pci(gpu["pci_bus_id"])
    _fixed(gpu["compute_capability"], EXPECTED_CAPABILITY, "gpu.compute_capability")
    runtime = trial["runtime"]
    _keys(runtime, {"cuda_runtime", "cuda_driver", "cublas_version"}, "runtime")
    for key, value in runtime.items():
        _integer(value, f"runtime.{key}", positive=True)
    config = trial["config"]
    _keys(config, set(CONFIG), "config")
    for key, expected in CONFIG.items():
        _fixed(config[key], expected, f"config.{key}")
    validate_environment(trial["environment"])
    allocated = _integer(trial["peak_application_gpu_allocation_bytes"],
                         "peak_application_gpu_allocation_bytes", positive=True)
    _require(allocated <= config["gpu_allocation_budget_mib"] * 1024 * 1024,
             "peak_application_gpu_allocation_bytes: exceeds allocation budget")
    modes = trial["modes"]
    _require(type(modes) is list and len(modes) == len(MODE_SPECS),
             "modes: expected exactly four ordered precision modes")
    canonical = deepcopy(trial)
    canonical["gpu"]["pci_bus_id"] = canonical_pci
    for index, (mode, spec) in enumerate(zip(modes, MODE_SPECS)):
        path = f"modes[{index}]"
        _keys(mode, MODE_FIELDS, path)
        name, input_type, compute_type, math_mode = spec
        expected_fields = {
            "name": name, "input_type": input_type, "compute_type": compute_type,
            "math_mode": math_mode, "output_type": "CUDA_R_32F",
            "scalar_type": "CUDA_R_32F", "api": "cublasGemmEx",
            "algorithm": "CUBLAS_GEMM_DEFAULT",
        }
        for key, expected in expected_fields.items():
            _fixed(mode[key], expected, f"{path}.{key}")
        for key, expected in (("alpha", 1), ("beta", 0)):
            actual = _finite_number(mode[key], f"{path}.{key}")
            _require(actual == expected, f"{path}.{key}: expected {expected}")
        _validation(mode["validation"], index, f"{path}.validation")
        median, throughput = _timing(mode, path)
        canonical["modes"][index]["median_ms"] = median
        canonical["modes"][index]["tflops"] = throughput
    return canonical


def summarize_trials(trials):
    """Summarize exactly three compatible records; the runner owns fresh launches.

    The aggregate is the median of three per-process TFLOPS values, not TFLOPS
    derived from a pooled set of raw samples or the mean of their reciprocals.
    """
    _require(type(trials) is list and len(trials) == 3,
             "Expected exactly three complete process records")
    validated = [validate_trial(trial) for trial in trials]
    first = validated[0]
    for index, trial in enumerate(validated[1:], start=2):
        for key in ("benchmark_protocol", "gpu", "runtime", "config", "environment"):
            _require(trial[key] == first[key],
                     f"trial {index}: {key} differs between process records")
    rows = []
    for index, name in enumerate(MODE_NAMES):
        durations = [trial["modes"][index]["median_ms"] for trial in validated]
        rates = [trial["modes"][index]["tflops"] for trial in validated]
        rows.append({"name": name, "per_process_median_ms": durations,
                     "per_process_tflops": rates,
                     "median_tflops": statistics.median(rates)})
    return {
        "benchmark_protocol": BENCHMARK_PROTOCOL,
        "gpu_workload_run": True, "process_count": 3,
        "gpu": deepcopy(first["gpu"]), "runtime": deepcopy(first["runtime"]),
        "config": deepcopy(first["config"]),
        "environment": deepcopy(first["environment"]),
        "per_process_peak_application_gpu_allocation_bytes": [
            trial["peak_application_gpu_allocation_bytes"] for trial in validated],
        "trials": validated, "modes": rows,
        "limitations": (
            "Three fresh process launches must be established by runner evidence. "
            "JSON validation checks recorded consistency, not execution authenticity. "
            "Timed arithmetic coverage is 1024 sampled outputs per mode; the full "
            "finite scan does not establish arithmetic correctness of every output. "
            "Ordinary CUDA results do not establish firmware state or an unlock."
        ),
    }
