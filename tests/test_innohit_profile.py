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
        self.assertIn('set(CSWEB_VERSION 0.6.1 CACHE STRING', cmake)
        self.assertIn('#define CSWEB_WINDOW_TITLE "CloudStream Hub - INNO-Hit 55"', main)

    def test_targeted_build_and_ci_use_the_profile(self):
        script = (ROOT / "tools/build-innohit-55.sh").read_text()
        workflow = (ROOT / ".github/workflows/ci.yml").read_text()
        self.assertIn('-DCSWEB_INNOHIT_55=ON', script)
        self.assertIn('-DCSWEB_INNOHIT_55=ON', workflow)
        self.assertIn('name: innohit-55-webos-hub-ipk', workflow)

    def test_doGior_repository_is_seeded_and_documented_for_the_tv_profile(self):
        readme = (ROOT / "README.md").read_text()
        app = (ROOT / "apps/csweb-tv/src/app/application.cpp").read_text()
        url = 'https://raw.githubusercontent.com/doGior/doGiorsHadEnough/refs/heads/builds/repo.json'
        self.assertIn(url, readme)
        self.assertIn(url, app)
        self.assertIn('plugins_->add_repository(kInnoHitRepository', app)
        self.assertIn('innohit_repo_seed_attempted_', app)


if __name__ == "__main__":
    unittest.main()
