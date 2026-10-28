import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class InnoHitProfileTest(unittest.TestCase):
    def test_profile_selects_legacy_runtime_and_tv_branding(self):
        cmake = (ROOT / "apps/csweb-tv/CMakeLists.txt").read_text()
        main = (ROOT / "apps/csweb-tv/src/main.cpp").read_text()
        self.assertIn('option(CSWEB_INNOHIT_55', cmake)
        self.assertIn('set(CSWEB_LEGACY ON CACHE BOOL', cmake)
        self.assertIn('set(CSWEB_TITLE "CloudStream Hub - INNO-Hit 55")', cmake)
        self.assertIn('set(CSWEB_VERSION 0.6.2 CACHE STRING', cmake)
        self.assertIn('#define CSWEB_WINDOW_TITLE "CloudStream Hub - INNO-Hit 55"', main)

    def test_targeted_build_and_ci_use_the_profile(self):
        script = (ROOT / "tools/build-innohit-55.sh").read_text()
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn('-DCSWEB_INNOHIT_55=ON', script)
        self.assertIn('-DCSWEB_INNOHIT_55=ON', workflow)
        self.assertIn('name: innohit-55-webos-hub-ipk', workflow)

    def test_tv_profile_does_not_preinstall_third_party_plugins(self):
        app = (ROOT / "apps/csweb-tv/src/app/application.cpp").read_text()
        self.assertNotIn('doGiorsHadEnough', app)

    def test_release_attaches_prebuilt_innohit_package_without_building_on_each_push(self):
        ci = (ROOT / ".github/workflows/ci.yml").read_text()
        release = (ROOT / ".github/workflows/release.yml").read_text()
        publish = (ROOT / ".github/workflows/publish-prebuilt-ipk.yml").read_text()
        self.assertIn("github.event_name == 'workflow_dispatch' || github.event_name == 'workflow_call'", ci)
        self.assertIn('name: innohit-55-webos-hub-ipk', release)
        self.assertIn('name: innohit-55-webos-hub-ipk', publish)
        self.assertIn('run-id:', publish)
        self.assertIn('gh release create', publish)


if __name__ == "__main__":
    unittest.main()
