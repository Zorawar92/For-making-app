import importlib.util
import io
import json
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("fetch_dogior_plugins", ROOT / "tools/fetch-dogior-plugins.py")
BUNDLE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUNDLE)


class DogiorBundleTest(unittest.TestCase):
    def test_only_the_selected_plugins_are_pinned(self):
        self.assertEqual(BUNDLE.PLUGIN_NAMES, ("StreamingCommunity", "AnimeUnity", "AnimeWorld"))
        self.assertRegex(BUNDLE.BUILD_REF, r"^[0-9a-f]{40}$")

    def test_plugin_filename_is_confined_to_bundle_directory(self):
        self.assertEqual(BUNDLE.safe_plugin_filename("StreamingCommunity"), "StreamingCommunity.cs3")
        for name in ("../escape", "a/b", "a\\b", ""):
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                BUNDLE.safe_plugin_filename(name)

    def test_valid_cs3_archive_has_cloudstream_manifest(self):
        data = io.BytesIO()
        with zipfile.ZipFile(data, "w") as archive:
            archive.writestr("manifest.json", json.dumps({"pluginClassName": "example.Plugin"}))
            archive.writestr("classes.dex", b"dex")
        BUNDLE.validate_cs3(data.getvalue(), "example.cs3")

    def test_bundle_is_personal_build_opt_in(self):
        cmake = (ROOT / "apps/csweb-tv/CMakeLists.txt").read_text()
        build = (ROOT / "tools/build-innohit-55.sh").read_text()
        manager = (ROOT / "apps/csweb-tv/src/plugins/plugin_manager.cpp").read_text()
        self.assertIn("CSWEB_BUNDLED_PLUGINS_DIR", cmake)
        self.assertIn("CSWEB_BUNDLE_DOGIOR_PLUGINS", build)
        self.assertIn("read_store_with_bundled_plugins", manager)
        self.assertIn("do not overwrite a user's installed or updated version", manager)

    def test_invalid_archive_is_rejected(self):
        with self.assertRaises(RuntimeError):
            BUNDLE.validate_cs3(b"not a zip", "bad.cs3")


if __name__ == "__main__":
    unittest.main()
