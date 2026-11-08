"""Behavior checks for the parity comparator's type and mismatch contracts."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOL = Path(__file__).with_name("parity_tools.py")


class CompareCLITests(unittest.TestCase):
    def invoke(self, expected, actual, fields, *, all_fields=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schema = root / "schema.json"
            schema.write_text(json.dumps({"fields": fields}), encoding="utf-8")
            exp = root / "expected.jsonl"
            act = root / "actual.jsonl"
            exp.write_text("".join(json.dumps(row) + "\n" for row in expected), encoding="utf-8")
            act.write_text("".join(json.dumps(row) + "\n" for row in actual), encoding="utf-8")
            command = [sys.executable, str(TOOL), "compare", str(exp), str(act), "--schema", str(schema)]
            if all_fields:
                command.append("--all")
            return subprocess.run(command, text=True, capture_output=True, check=False)

    def test_exact_is_typed(self):
        result = self.invoke([{"case": "a", "v": 1}], [{"case": "a", "v": True}], [{"path": "v", "mode": "exact"}])
        self.assertEqual(result.returncode, 1)
        self.assertIn("case 'a', field v", result.stdout)

    def test_float64_bits_preserve_signed_zero(self):
        result = self.invoke([{"case": 0, "v": 0.0}], [{"case": 0, "v": -0.0}], [{"path": "v", "mode": "float64_bits"}])
        self.assertEqual(result.returncode, 1)
        self.assertIn("binary64 bits", result.stdout)

    def test_explicit_tolerance_and_nan_policy(self):
        rows = [{"case": "x", "v": 1.0}]
        accepted = self.invoke(rows, [{"case": "x", "v": 1.00001}], [{"path": "v", "mode": "tolerance", "abs": 0.00002}])
        rejected_nan = self.invoke([{"case": "x", "v": "NaN"}], [{"case": "x", "v": "NaN"}], [{"path": "v", "mode": "tolerance", "nan_equal": False}])
        self.assertEqual(accepted.returncode, 0, accepted.stdout)
        self.assertEqual(rejected_nan.returncode, 1)

    def test_order_and_case_ids_are_checked(self):
        expected = [{"case": "first", "v": 0}, {"case": "second", "v": 0}]
        actual = list(reversed(expected))
        result = self.invoke(expected, actual, [{"path": "v", "mode": "exact"}])
        self.assertEqual(result.returncode, 1)
        self.assertIn("case ID differs", result.stdout)

    def test_empty_outputs_fail_closed(self):
        result = self.invoke([], [], [{"path": "v", "mode": "exact"}])
        self.assertEqual(result.returncode, 1)
        self.assertIn("at least one record", result.stdout)

    def test_unknown_mode_has_clean_nonzero_exit(self):
        result = self.invoke([{"case": 1, "v": 1}], [{"case": 1, "v": 1}], [{"path": "v", "mode": "almost"}])
        self.assertEqual(result.returncode, 2)
        self.assertIn("unknown comparison mode", result.stderr)


if __name__ == "__main__":
    unittest.main()
