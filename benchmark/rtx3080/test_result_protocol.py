# SPDX-License-Identifier: GPL-2.0-only
"""Synthetic protocol-3 evidence tests; no CUDA, NVIDIA tools, or GPU access."""

from copy import deepcopy
import hashlib
import json
from pathlib import Path
import statistics
import tempfile
import unittest

from result_protocol import (
    CONFIG, MODE_NAMES, MODE_SPECS, SMALL_CASES, ProtocolError,
    expected_positions, load_file, load_text, normalize_sha256,
    summarize_trials, validate_trial,
)


def _positions():
    # Independent fixture construction of the specified unsigned 32-bit draws.
    state = 30801024
    result = []
    for row_tile in range(32):
        for column_tile in range(32):
            offsets = []
            for unused in range(2):
                state = (state ^ (state << 13)) & ((1 << 32) - 1)
                state = (state ^ (state >> 17)) & ((1 << 32) - 1)
                state = (state ^ (state << 5)) & ((1 << 32) - 1)
                offsets.append(state & 127)
            row = row_tile * 128 + offsets[0]
            column = column_tile * 128 + offsets[1]
            if row_tile == 0:
                row = 0
            elif row_tile == 31:
                row = 4095
            if column_tile == 0:
                column = 0
            elif column_tile == 31:
                column = 4095
            result.append([row, column])
    return result


def _metrics(checked, total, coverage, atol, rtol):
    return {
        "passed": True, "checked_elements": checked, "total_elements": total,
        "coverage": coverage, "reference_precision": "fp64",
        "absolute_tolerance": atol, "relative_tolerance": rtol,
        "max_absolute_error": 0.0, "max_scaled_error": 0.0,
        "max_reference_magnitude": 1.0,
    }


def _refresh_timing(mode):
    normalized = [sample["elapsed_ms"] / sample["repetitions"]
                  for sample in mode["samples"]]
    mode["median_ms"] = statistics.median(normalized)
    mode["tflops"] = 2 * 4096 ** 3 / (mode["median_ms"] * 1e9)


def make_trial(*, timing_scale=1.0, pci_bus_id="0000:07:00.0"):
    """Return wholly synthetic, valid native evidence for offline runner tests."""
    modes = []
    for index, (name, input_type, compute_type, math_mode) in enumerate(MODE_SPECS):
        small_atol, small_rtol = ((0.005, 0.005) if index == 3 else (2e-5, 2e-5))
        timed_atol, timed_rtol = ((0.05, 0.01) if index == 3 else (0.002, 0.0005))
        small = _metrics(3783, 3783, "full", small_atol, small_rtol)
        small["cases"] = [
            dict(_metrics(spec["m"] * spec["n"], spec["m"] * spec["n"],
                          "full", small_atol, small_rtol), **spec)
            for spec in SMALL_CASES
        ]
        known = _metrics(4096 ** 2, 4096 ** 2, "full", 0, 0)
        known["input_pattern"] = "signed_diagonal_binary"
        timed = _metrics(1024, 4096 ** 2, "sampled", timed_atol, timed_rtol)
        timed.update(
            finite_checked_elements=4096 ** 2, finite_passed=True,
            sample_seed=30801024, sampling_policy="32x32_tiles_edges_v1",
            positions=_positions(),
        )
        mode = {
            "name": name, "input_type": input_type, "compute_type": compute_type,
            "output_type": "CUDA_R_32F", "alpha": 1, "beta": 0,
            "api": "cublasGemmEx", "scalar_type": "CUDA_R_32F",
            "math_mode": math_mode, "algorithm": "CUBLAS_GEMM_DEFAULT",
            "samples": [
                {"index": i, "elapsed_ms": timing_scale * (2 + index + i / 100),
                 "repetitions": 1}
                for i in range(30)
            ],
            "validation": {"small_full": small, "known_answer_full": known,
                           "timed_sampled": timed},
        }
        _refresh_timing(mode)
        modes.append(mode)
    return {
        "benchmark_protocol": 3, "gpu_workload_run": True,
        "gpu": {"name": "NVIDIA GeForce RTX 3080", "pci_bus_id": pci_bus_id,
                "compute_capability": "8.6"},
        "runtime": {"cuda_runtime": 12060, "cuda_driver": 13020,
                    "cublas_version": 120600},
        "config": deepcopy(CONFIG), "modes": modes,
        "environment": {"NVIDIA_TF32_OVERRIDE": None, "CUBLAS_WORKSPACE_CONFIG": None,
                        "CUDA_VISIBLE_DEVICES": None, "CUDA_DEVICE_ORDER": None},
        "peak_application_gpu_allocation_bytes": 192 * 1024 * 1024,
    }


class JsonEvidenceTests(unittest.TestCase):
    def test_valid_json_roundtrip_and_utf8_bom(self):
        original = make_trial()
        encoded = json.dumps(original, allow_nan=False)
        self.assertEqual(load_text(encoded), original)
        self.assertEqual(load_text("\ufeff" + encoded), original)

    def test_duplicate_keys_rejected_at_every_level(self):
        for text in ('{"a":1,"a":2}', '{"nested":{"a":1,"a":2}}',
                     '{"array":[{"a":1,"a":2}]}'):
            with self.subTest(text=text), self.assertRaisesRegex(ProtocolError, "duplicate"):
                load_text(text)

    def test_nonfinite_literals_and_overflow_are_rejected(self):
        for token in ("NaN", "Infinity", "-Infinity", "1e9999", "-1e9999"):
            with self.subTest(token=token), self.assertRaisesRegex(ProtocolError, "finite"):
                load_text('{"value":' + token + "}")

    def test_invalid_json_and_nonobject_are_rejected(self):
        for text in ("", "{", "[]", "null", "true", '{} {}', '{"a":1,}'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                load_text(text)

    def test_nontext_rejected(self):
        for value in (None, b"{}", {}, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                load_text(value)

    def test_sha256_normalization_and_whitespace_refusal(self):
        self.assertEqual(normalize_sha256("AB" * 32), "ab" * 32)
        for digest in ("g" * 64, "a" * 63, "a" * 65, " a" * 32, True, None):
            with self.subTest(digest=digest), self.assertRaises(ValueError):
                normalize_sha256(digest)

    def test_raw_file_hash_checks_case_and_detects_whitespace_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trial.json"
            payload = json.dumps(make_trial(), allow_nan=False).encode("utf-8")
            path.write_bytes(payload)
            digest = hashlib.sha256(payload).hexdigest()
            self.assertEqual(load_file(path, expected_sha256=digest.upper()), make_trial())
            path.write_bytes(payload + b"\n")
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                load_file(path, expected_sha256=digest)

    def test_invalid_utf8_is_a_protocol_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "trial.json"
            path.write_bytes(b"\xff\xfe")
            with self.assertRaisesRegex(ProtocolError, "UTF-8"):
                load_file(path)

    def test_direct_non_json_and_cyclic_values_rejected(self):
        for item in (float("nan"), float("inf"), {1: "value"}, {1, 2}):
            trial = make_trial()
            trial["extra"] = item
            with self.subTest(item=item), self.assertRaises(ValueError):
                validate_trial(trial)
        cyclic = {}
        cyclic["self"] = cyclic
        with self.assertRaisesRegex(ValueError, "cyclic"):
            validate_trial(cyclic)


class TrialProtocolTests(unittest.TestCase):
    def setUp(self):
        self.trial = make_trial()

    def mode(self):
        return self.trial["modes"][0]

    def metrics(self, which="timed_sampled"):
        return self.mode()["validation"][which]

    def rejects(self):
        with self.assertRaises(ValueError):
            validate_trial(self.trial)

    def test_valid_detached_idempotent_record(self):
        self.trial["gpu"]["pci_bus_id"] = "00000000:07:00.0"
        original = deepcopy(self.trial)
        validated = validate_trial(self.trial)
        self.assertEqual(self.trial, original)
        self.assertEqual(validated["gpu"]["pci_bus_id"], "0000:07:00.0")
        self.assertEqual(validate_trial(validated), validated)
        validated["modes"][0]["samples"][0]["elapsed_ms"] = 123
        self.assertNotEqual(validated, self.trial)

    def test_protocols_zero_one_two_and_bool_rejected(self):
        for value in (0, 1, 2, 4, True, "3", 3.0, None):
            with self.subTest(value=value):
                self.trial["benchmark_protocol"] = value
                self.rejects()

    def test_inspect_only_or_partial_workload_rejected(self):
        for value in (False, 1, "true", None):
            with self.subTest(value=value):
                self.trial["gpu_workload_run"] = value
                self.rejects()

    def test_exact_gpu_identity_required(self):
        for key, value in (
            ("name", "NVIDIA GeForce RTX 3090"), ("name", "GeForce RTX 3080"),
            ("pci_bus_id", "0000:01:00.0"), ("pci_bus_id", "0001:07:00.0"),
            ("pci_bus_id", "0000:07:00.1"), ("pci_bus_id", "07:00.0"),
            ("pci_bus_id", True), ("compute_capability", "8.9"),
            ("compute_capability", 8.6),
        ):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["gpu"][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_runtime_fields_positive_integer_and_complete(self):
        for field in self.trial["runtime"]:
            for value in (0, -1, True, 12060.0, "12060", None):
                with self.subTest(field=field, value=value):
                    trial = make_trial()
                    trial["runtime"][field] = value
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_exact_fixed_config_and_types(self):
        for key, value in (
            ("m", 8192), ("n", 2048), ("k", 2048), ("m", True),
            ("warmup_seconds", True), ("warmup_seconds", 2.0),
            ("sample_count", 29), ("input_seed", 1),
            ("input_generator", "all_ones"), ("stream_policy", "default"),
            ("gpu_allocation_budget_mib", 1024), ("process_budget_seconds", 240),
            ("layout", "row_major"), ("transa", "T"), ("ldb", 4097),
        ):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["config"][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_unknown_and_missing_fields_rejected(self):
        for location in ((), ("gpu",), ("runtime",), ("config",), ("environment",)):
            for action in ("extra", "missing"):
                with self.subTest(location=location, action=action):
                    trial = make_trial()
                    target = trial
                    for part in location:
                        target = target[part]
                    if action == "extra":
                        target["unexpected"] = 1
                    else:
                        target.pop(next(iter(target)))
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_all_four_modes_in_fixed_order_required(self):
        for action in ("missing", "duplicate", "reverse", "extra"):
            with self.subTest(action=action):
                trial = make_trial()
                if action == "missing":
                    trial["modes"].pop()
                elif action == "duplicate":
                    trial["modes"][-1] = deepcopy(trial["modes"][0])
                elif action == "reverse":
                    trial["modes"].reverse()
                else:
                    trial["modes"].append(deepcopy(trial["modes"][0]))
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_environment_values_are_explicit_strings_or_null(self):
        self.trial["environment"].update(
            NVIDIA_TF32_OVERRIDE="", CUBLAS_WORKSPACE_CONFIG=":4096:8",
            CUDA_VISIBLE_DEVICES="0", CUDA_DEVICE_ORDER="PCI_BUS_ID")
        validate_trial(self.trial)
        for key in self.trial["environment"]:
            for value in (True, 0, [], {}):
                with self.subTest(key=key, value=value):
                    trial = make_trial()
                    trial["environment"][key] = value
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_any_nonempty_tf32_override_is_refused(self):
        for value in ("0", "1", "false", " "):
            with self.subTest(value=value):
                self.trial["environment"]["NVIDIA_TF32_OVERRIDE"] = value
                self.rejects()

    def test_gpu_uuid_and_private_path_environment_values_are_rejected(self):
        for key in ("CUBLAS_WORKSPACE_CONFIG", "CUDA_VISIBLE_DEVICES", "CUDA_DEVICE_ORDER"):
            for value in ("GPU-00000000-0000-0000-0000-000000000001", "C:/private/example"):
                with self.subTest(key=key, value=value):
                    trial = make_trial()
                    trial["environment"][key] = value
                    with self.assertRaises(ValueError) as error:
                        validate_trial(trial)
                    self.assertNotIn(value, str(error.exception))

    def test_visible_device_ordinal_bounds_and_canonical_spelling(self):
        for value in ("00", "01", "-1", "+1", "64", "0,0", "0, 1", ",0", "0,",
                      "0,,1", "0 1", ",".join(str(i) for i in range(65)), "1" * 192):
            with self.subTest(value=value):
                self.trial["environment"]["CUDA_VISIBLE_DEVICES"] = value
                self.rejects()
        self.trial["environment"]["CUDA_VISIBLE_DEVICES"] = ",".join(map(str, range(64)))
        validate_trial(self.trial)

    def test_only_declared_workspace_and_device_order_settings_are_allowed(self):
        for key, value in (("CUBLAS_WORKSPACE_CONFIG", ":32:8"),
                           ("CUBLAS_WORKSPACE_CONFIG", " :16:8"),
                           ("CUDA_DEVICE_ORDER", "pci_bus_id")):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["environment"][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_allocation_budget_requires_positive_integer_bytes(self):
        field = "peak_application_gpu_allocation_bytes"
        for value in (0, -1, True, 192.0, "192", 512 * 1024 * 1024 + 1):
            with self.subTest(value=value):
                self.trial[field] = value
                self.rejects()
        self.trial[field] = 512 * 1024 * 1024
        validate_trial(self.trial)

    def test_environment_and_allocation_evidence_cannot_be_omitted(self):
        for key in ("environment", "peak_application_gpu_allocation_bytes"):
            with self.subTest(key=key):
                trial = make_trial()
                trial.pop(key)
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_wrong_precision_api_output_and_scalar_contract_rejected(self):
        for field, value in (
            ("name", "fp32"), ("input_type", "CUDA_R_16F"),
            ("compute_type", "CUBLAS_COMPUTE_32F"),
            ("output_type", "CUDA_R_16F"), ("alpha", 0), ("alpha", True),
            ("beta", 1), ("beta", False), ("api", "cublasSgemm"),
            ("scalar_type", "CUDA_R_16F"), ("math_mode", "CUBLAS_DEFAULT_MATH"),
            ("algorithm", "CUBLAS_GEMM_ALGO0"),
        ):
            with self.subTest(field=field, value=value):
                trial = make_trial()
                trial["modes"][0][field] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_fp16_bf16_tf32_are_not_interchangeable(self):
        for index, field, value in (
            (1, "input_type", "CUDA_R_16BF"), (2, "input_type", "CUDA_R_16F"),
            (3, "input_type", "CUDA_R_16F"), (3, "compute_type", "CUBLAS_COMPUTE_32F"),
            (1, "output_type", "CUDA_R_16F"),
        ):
            with self.subTest(index=index, field=field):
                trial = make_trial()
                trial["modes"][index][field] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_sample_count_and_index_order_are_strict(self):
        for action in ("missing", "extra", "reverse", "duplicate_index", "bool_index"):
            with self.subTest(action=action):
                trial = make_trial()
                samples = trial["modes"][0]["samples"]
                if action == "missing":
                    samples.pop()
                elif action == "extra":
                    samples.append(deepcopy(samples[-1]))
                elif action == "reverse":
                    samples.reverse()
                elif action == "duplicate_index":
                    samples[1]["index"] = 0
                else:
                    samples[0]["index"] = False
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_invalid_elapsed_and_repetition_values_rejected(self):
        for field, values in (
            ("elapsed_ms", (0, -1, True, "2", float("nan"), float("inf"))),
            ("repetitions", (0, -1, True, 1.0, "1", float("inf"))),
        ):
            for value in values:
                with self.subTest(field=field, value=value):
                    trial = make_trial()
                    trial["modes"][0]["samples"][0][field] = value
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_normalization_precedes_middle_pair_median_and_reciprocal(self):
        mode = self.mode()
        # Acquisition order is intentionally not sorted.
        values = list(range(16, 31)) + list(range(1, 16))
        mode["samples"] = [
            {"index": i, "elapsed_ms": value * (i + 1), "repetitions": i + 1}
            for i, value in enumerate(values)
        ]
        _refresh_timing(mode)
        result = validate_trial(self.trial)["modes"][0]
        self.assertEqual(result["median_ms"], 15.5)
        self.assertAlmostEqual(result["tflops"], 2 * 4096 ** 3 / (15.5 * 1e9))
        self.assertEqual(result["samples"], mode["samples"])
        reciprocal_median = statistics.median(2 * 4096 ** 3 / (x * 1e9) for x in values)
        self.assertNotAlmostEqual(result["tflops"], reciprocal_median)

    def test_derived_timings_cannot_be_fabricated(self):
        for field, value in (("median_ms", 99), ("tflops", 99),
                             ("median_ms", True), ("tflops", False)):
            with self.subTest(field=field, value=value):
                trial = make_trial()
                trial["modes"][0][field] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_validation_is_not_a_boolean_attestation(self):
        self.mode()["validation"] = {"reference_checked": True}
        self.rejects()

    def test_small_throughput_cannot_exploit_absolute_rounding_allowance(self):
        for sample in self.mode()["samples"]:
            sample["elapsed_ms"] = 1e20
        _refresh_timing(self.mode())
        validate_trial(self.trial)
        self.mode()["tflops"] = 1e-13
        self.rejects()

    def test_fixed_tolerances_cannot_be_widened(self):
        for name in ("small_full", "known_answer_full", "timed_sampled"):
            for key in ("absolute_tolerance", "relative_tolerance"):
                with self.subTest(name=name, key=key):
                    trial = make_trial()
                    trial["modes"][0]["validation"][name][key] = 1000
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_validation_errors_must_be_finite_nonnegative_numbers(self):
        for key in ("max_absolute_error", "max_scaled_error", "max_reference_magnitude"):
            for value in (True, -0.001, float("nan"), float("inf"), "0"):
                with self.subTest(key=key, value=value):
                    trial = make_trial()
                    trial["modes"][0]["validation"]["timed_sampled"][key] = value
                    with self.assertRaises(ValueError):
                        validate_trial(trial)

    def test_true_pass_with_excessive_or_inconsistent_error_is_rejected(self):
        for absolute, scaled, reference in (
            (0.01, 2, 1), (10, 0.5, 1), (0.001, 0, 1),
            (0, 0.2, 1), (0.001, 0.01, 1), (0.00001, 0.5, 1),
        ):
            with self.subTest(absolute=absolute, scaled=scaled, reference=reference):
                trial = make_trial()
                trial["modes"][0]["validation"]["timed_sampled"].update(
                    max_absolute_error=absolute, max_scaled_error=scaled,
                    max_reference_magnitude=reference,
                )
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_valid_nonzero_error_with_recorded_bound_is_accepted(self):
        self.metrics().update(max_absolute_error=0.001, max_scaled_error=0.5,
                              max_reference_magnitude=1.0)
        validate_trial(self.trial)

    def test_known_answer_must_be_exact_and_fully_checked(self):
        for key, value in (("max_absolute_error", 1e-12),
                           ("checked_elements", 2), ("coverage", "sampled"),
                           ("input_pattern", "all_ones"), ("passed", False)):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["modes"][0]["validation"]["known_answer_full"][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_small_case_counts_shapes_layout_and_aggregates_checked(self):
        for key, value in (("checked_elements", 2), ("k", 17), ("transa", "N"),
                           ("lda", 31), ("input_pattern", "all_ones")):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["modes"][0]["validation"]["small_full"]["cases"][2][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)
        self.metrics("small_full")["cases"].pop()
        self.rejects()

    def test_small_case_errors_cannot_be_hidden_by_aggregate(self):
        case = self.metrics("small_full")["cases"][0]
        case.update(max_absolute_error=1e-5, max_scaled_error=0.5)
        self.rejects()
        self.metrics("small_full").update(max_absolute_error=1e-5, max_scaled_error=0.5)
        validate_trial(self.trial)

    def test_sampled_coverage_cannot_claim_full_arithmetic_validation(self):
        for key, value in (
            ("coverage", "full"), ("checked_elements", 4096 ** 2),
            ("total_elements", 1024), ("finite_checked_elements", 1024),
            ("finite_passed", False), ("passed", False),
            ("reference_precision", "fp32"), ("sample_seed", 123),
            ("sampling_policy", "endpoints_only"),
        ):
            with self.subTest(key=key, value=value):
                trial = make_trial()
                trial["modes"][0]["validation"]["timed_sampled"][key] = value
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_sampled_positions_are_exact_unique_stratified_and_ordered(self):
        for action in ("short", "duplicate", "reorder", "edge", "interior", "bool"):
            with self.subTest(action=action):
                trial = make_trial()
                positions = trial["modes"][0]["validation"]["timed_sampled"]["positions"]
                if action == "short":
                    positions.pop()
                elif action == "duplicate":
                    positions[1] = positions[0][:]
                elif action == "reorder":
                    positions[34], positions[35] = positions[35], positions[34]
                elif action == "edge":
                    positions[0][0] = 1
                elif action == "interior":
                    positions[33][0] = 128 + ((positions[33][0] - 128 + 1) % 128)
                else:
                    positions[0][0] = False
                with self.assertRaises(ValueError):
                    validate_trial(trial)

    def test_sample_policy_edges_and_interior_count(self):
        positions = expected_positions()
        self.assertEqual(positions, _positions())
        self.assertEqual(positions[0], [0, 0])
        self.assertEqual(positions[31], [0, 4095])
        self.assertEqual(positions[992], [4095, 0])
        self.assertEqual(positions[-1], [4095, 4095])
        self.assertEqual(len(set(map(tuple, positions))), 1024)
        self.assertEqual(sum(0 < r < 4095 and 0 < c < 4095 for r, c in positions), 900)


class AggregateTests(unittest.TestCase):
    def test_three_process_medians_and_full_evidence_are_preserved(self):
        trials = [make_trial(timing_scale=scale) for scale in (1.0, 2.0, 0.5)]
        summary = summarize_trials(trials)
        self.assertEqual(summary["process_count"], 3)
        self.assertEqual(tuple(mode["name"] for mode in summary["modes"]), MODE_NAMES)
        for index, row in enumerate(summary["modes"]):
            values = [trial["modes"][index]["tflops"] for trial in trials]
            self.assertEqual(len(row["per_process_median_ms"]), 3)
            for actual, expected in zip(row["per_process_tflops"], values):
                self.assertAlmostEqual(actual, expected)
            self.assertAlmostEqual(row["median_tflops"], values[0])
        self.assertEqual(summary["trials"][1]["modes"][0]["samples"],
                         trials[1]["modes"][0]["samples"])
        self.assertIn("fresh process", summary["limitations"])
        self.assertIn("sampled outputs", summary["limitations"])

    def test_requires_exactly_three_records(self):
        for records in ([], [make_trial()], [make_trial()] * 2,
                        [make_trial()] * 4, (make_trial(),) * 3):
            with self.subTest(count=len(records)), self.assertRaises(ValueError):
                summarize_trials(records)

    def test_runtime_changes_between_processes_refused(self):
        for key in ("cuda_runtime", "cuda_driver", "cublas_version"):
            trials = [make_trial() for _ in range(3)]
            trials[1]["runtime"][key] += 1
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "differs"):
                summarize_trials(trials)

    def test_protocol_mixing_and_partial_trials_refused(self):
        for protocol in (1, 2, None):
            trials = [make_trial() for _ in range(3)]
            trials[1]["benchmark_protocol"] = protocol
            with self.subTest(protocol=protocol), self.assertRaises(ValueError):
                summarize_trials(trials)

    def test_domain_padding_is_not_a_different_gpu(self):
        trials = [make_trial(), make_trial(pci_bus_id="00000000:07:00.0"), make_trial()]
        summary = summarize_trials(trials)
        self.assertEqual(summary["gpu"]["pci_bus_id"], "0000:07:00.0")

    def test_environment_changes_between_processes_refused(self):
        for key, value in (("CUBLAS_WORKSPACE_CONFIG", ":16:8"),
                           ("CUDA_VISIBLE_DEVICES", "0"),
                           ("CUDA_DEVICE_ORDER", "PCI_BUS_ID")):
            trials = [make_trial() for _ in range(3)]
            trials[1]["environment"][key] = value
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "environment differs"):
                summarize_trials(trials)

    def test_summary_does_not_modify_or_alias_caller_records(self):
        trials = [make_trial() for _ in range(3)]
        original = deepcopy(trials)
        summary = summarize_trials(trials)
        self.assertEqual(trials, original)
        summary["trials"][0]["modes"][0]["samples"][0]["elapsed_ms"] = 88
        summary["config"]["m"] = 88
        self.assertEqual(trials, original)


if __name__ == "__main__":
    unittest.main()
