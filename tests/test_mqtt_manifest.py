"""Model exclusions must not weaken another model's MQTT baseline."""
import json
import contextlib
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import test_mqtt_smoke as smoke


class ModelManifestTests(unittest.TestCase):
    def test_wpl9_excludes_disabled_ww_eco_and_requires_runtime_feedback(self):
        common, model = smoke.load_required_topics("wpl9")
        self.assertNotIn("heatingpump/MANAGER/WW_ECO/state", common + model)
        self.assertIn("heatingpump/HEIZMODUL/LZ_VERD_1_HEIZBETRIEB/state", model)
        self.assertIn("heatingpump/HEIZMODUL/LZ_VERD_1_KUEHLBETRIEB/state", model)

    def test_existing_models_keep_common_ww_eco(self):
        for name in ["wpl13e", "wpl10ac", "wpl17", "wpf10"]:
            with self.subTest(model=name):
                common, _ = smoke.load_required_topics(name)
                self.assertIn("heatingpump/MANAGER/WW_ECO/state", common)

    def test_invalid_exclusion_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "_common_smoke.json").write_text(json.dumps({"required_topics": ["a"]}))
            (directory / "example_smoke.json").write_text(json.dumps({"required_topics": [], "excluded_common_topics": ["typo"]}))
            with patch.object(smoke, "MODELS_DIR", directory), patch.object(smoke, "COMMON_FILE", directory / "_common_smoke.json"):
                with self.assertRaises(ValueError):
                    smoke.load_required_topics("example")

    def test_capture_preserves_model_exclusions(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            model = directory / "example_smoke.json"
            model.write_text(json.dumps({"required_topics": [], "excluded_common_topics": ["disabled"]}))
            args = SimpleNamespace(model="example", broker="unused", port=1883, user="", password="", window=0)
            with patch.object(smoke, "MODELS_DIR", directory), patch.object(smoke, "create_client"), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(smoke.run_capture(args), 0)
            self.assertEqual(json.loads(model.read_text())["excluded_common_topics"], ["disabled"])


if __name__ == "__main__":
    unittest.main()
