"""Unit tests for check_format. Run: python3 -m unittest discover -s tests -p 'test_check_*.py'"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_format import clang_format_clean, clang_format_pathspec, shell_scripts  # noqa: E402


class ShellScriptsTest(unittest.TestCase):
    def test_all_tracked_scripts_outside_archive(self) -> None:
        tracked = ["scripts/mutate.sh", "helperscripts/register_probe.sh", "archive/build.sh", "Makefile"]
        self.assertEqual(shell_scripts(tracked), ["helperscripts/register_probe.sh", "scripts/mutate.sh"])


class ClangFormatTest(unittest.TestCase):
    def test_vendored_code_is_excluded(self) -> None:
        spec = clang_format_pathspec()
        self.assertIn(":(exclude)tests/catch2/", spec)
        self.assertIn(":(exclude)esphome/ha-stiebel-control/elster/", spec)
        self.assertIn("esphome", spec)

    def test_clean_output(self) -> None:
        self.assertTrue(clang_format_clean("no modified files to format\n"))
        self.assertTrue(clang_format_clean("clang-format did not modify any files\n"))
        self.assertTrue(clang_format_clean(""))

    def test_diff_output_is_not_clean(self) -> None:
        diff = "diff --git a/esphome/x.h b/esphome/x.h\n-int  a;\n+int a;\n"
        self.assertFalse(clang_format_clean(diff))


if __name__ == "__main__":
    unittest.main()
