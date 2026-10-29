import hashlib
import importlib.util
import io
import json
import shutil
import subprocess
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("fetch_dogior_plugins", ROOT / "tools/fetch-dogior-plugins.py")
BUNDLE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUNDLE)
VERIFY_SPEC = importlib.util.spec_from_file_location(
    "verify_innohit_plugin_ipk", ROOT / "tools/verify-innohit-plugin-ipk.py"
)
VERIFY = importlib.util.module_from_spec(VERIFY_SPEC)
VERIFY_SPEC.loader.exec_module(VERIFY)


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

    def test_local_innohit_build_bundles_plugins_by_default(self):
        cmake = (ROOT / "apps/csweb-tv/CMakeLists.txt").read_text()
        build = (ROOT / "tools/build-innohit-55.sh").read_text()
        manager = (ROOT / "apps/csweb-tv/src/plugins/plugin_manager.cpp").read_text()
        self.assertIn("CSWEB_BUNDLED_PLUGINS_DIR", cmake)
        base_dirs = cmake.index('set(_csweb_extra_dirs "${_csweb_fonts}" "${_csweb_icons}" "${_csweb_brand}")')
        bundled_dir = cmake.index('list(APPEND _csweb_extra_dirs "${CSWEB_BUNDLED_PLUGINS_DIR}")')
        package = cmake.index("wnk_add_webos_package(")
        self.assertLess(base_dirs, bundled_dir)
        self.assertLess(bundled_dir, package)
        self.assertIn('CSWEB_BUNDLE_DOGIOR_PLUGINS:-1', build)
        self.assertIn("CSWEB_BUNDLE_DOGIOR_PLUGINS must be 0 or 1", build)
        self.assertIn("read_store_with_bundled_plugins", manager)
        self.assertIn("do not overwrite a user's installed or updated version", manager)

    def test_ci_bundles_plugins_for_release_markers_and_workflow_calls(self):
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn("[build-innohit-plugins]", workflow)
        self.assertIn("bundle_plugins", workflow)
        self.assertIn("inputs.bundle_plugins", workflow)
        self.assertIn("plugins-manifest.json", workflow)
        self.assertIn("verify-innohit-plugin-ipk.py", workflow)
        self.assertIn("CSWEB_BUNDLED_PLUGINS_DIR", workflow)
        self.assertIn("/plugins/StreamingCommunity.cs3 /plugins/AnimeUnity.cs3 /plugins/AnimeWorld.cs3", workflow)
        self.assertIn("GITHUB_STEP_SUMMARY", workflow)
        self.assertIn("Summarize plugin-load result", workflow)
        self.assertIn("runtime tail:", workflow)
        self.assertIn("Selected plugin fetch failed", workflow)

    def test_ipk_verifier_checks_archive_bytes_against_provenance(self):
        if not shutil.which("ar"):
            self.skipTest("ar is required to construct an IPK fixture")
        names = ("StreamingCommunity", "AnimeUnity", "AnimeWorld")
        payloads = {name: f"fixture-bytes-{name}".encode() for name in names}
        plugins = [
            {"internalName": name, "file": f"{name}.cs3", "size": len(data),
             "sha256": hashlib.sha256(data).hexdigest()}
            for name, data in payloads.items()
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data_tar = root / "data.tar.gz"
            with tarfile.open(data_tar, "w:gz") as archive:
                for name, data in payloads.items():
                    member = tarfile.TarInfo(f"./usr/palm/applications/org.csweb.tv/bundled-plugins/{name}.cs3")
                    member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
            (root / "debian-binary").write_text("2.0\n")
            ipk = root / "fixture.ipk"
            subprocess.run(["ar", "rc", str(ipk), "debian-binary", "data.tar.gz"], cwd=root, check=True)
            manifest = root / "plugins-manifest.json"
            manifest.write_text(json.dumps({
                "schemaVersion": 1,
                "sourceRef": BUNDLE.BUILD_REF,
                "plugins": plugins,
            }))
            self.assertEqual(VERIFY.verify_bundle(ipk, manifest), sorted(names))

            plugins[0]["sha256"] = "0" * 64
            manifest.write_text(json.dumps({
                "schemaVersion": 1,
                "sourceRef": BUNDLE.BUILD_REF,
                "plugins": plugins,
            }))
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                VERIFY.verify_bundle(ipk, manifest)

    def test_invalid_archive_is_rejected(self):
        with self.assertRaises(RuntimeError):
            BUNDLE.validate_cs3(b"not a zip", "bad.cs3")


if __name__ == "__main__":
    unittest.main()
