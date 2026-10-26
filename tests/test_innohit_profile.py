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
        self.assertIn('#define CSWEB_WINDOW_TITLE "CloudStream Hub - INNO-Hit 55"', main)

    def test_targeted_build_and_ci_use_the_profile(self):
        script = (ROOT / "tools/build-innohit-55.sh").read_text()
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn('-DCSWEB_INNOHIT_55=ON', script)
        self.assertIn('-DCSWEB_INNOHIT_55=ON', workflow)
        self.assertIn('name: innohit-55-webos-hub-ipk', workflow)

    def test_doGior_repository_is_documented_as_optional(self):
        readme = (ROOT / "README.md").read_text()
        self.assertIn('https://raw.githubusercontent.com/doGior/doGiorsHadEnough/builds/repo.json', readme)


if __name__ == "__main__":
    unittest.main()
