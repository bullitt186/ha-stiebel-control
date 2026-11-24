"""Unit tests for check_doc_paths.PATH_RE. Run: python3 -m unittest discover -s tests -p 'test_check_*.py'"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_doc_paths import PATH_RE, doc_path  # noqa: E402


class PathRegexTest(unittest.TestCase):
    def found(self, text: str) -> list:
        return PATH_RE.findall(text)

    def test_dot_directory_keeps_its_dot(self) -> None:
        self.assertEqual(self.found("`.github/workflows/mutation.yml` runs"), [".github/workflows/mutation.yml"])

    def test_plain_paths(self) -> None:
        self.assertEqual(self.found("see `docs/ARCHITECTURE.md` and `config.h`"), ["docs/ARCHITECTURE.md", "config.h"])

    def test_doc_path_keeps_leading_dot_directory(self) -> None:
        # lstrip("./") turned ".github/x.yml" into "github/x.yml" before.
        self.assertEqual(doc_path(".github/workflows/mutation.yml"), ".github/workflows/mutation.yml")
        self.assertEqual(doc_path("./scripts/mutate.sh"), "scripts/mutate.sh")


if __name__ == "__main__":
    unittest.main()
