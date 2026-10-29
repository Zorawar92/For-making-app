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
        self.assertIn('set(CSWEB_VERSION 0.6.3 CACHE STRING', cmake)
        self.assertIn('VERSION="${CSWEB_VERSION:-0.6.3}"', (ROOT / "tools/build-innohit-55.sh").read_text())
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

    def test_release_uses_only_the_latest_successful_same_run_ipk(self):
        ci = (ROOT / ".github/workflows/ci.yml").read_text()
        manual_release = (ROOT / ".github/workflows/release.yml").read_text()
        automatic_release = (ROOT / ".github/workflows/publish-prebuilt-ipk.yml").read_text()

        self.assertIn("'[release-innohit]'", ci)
        required_for_release = (
            "continue-on-error: ${{ github.event_name != 'workflow_call' && "
            "!contains(github.event.head_commit.message, '[release-innohit]') }}"
        )
        self.assertEqual(ci.count(required_for_release), 4)
        self.assertIn('name: innohit-55-webos-hub-ipk', manual_release)
        self.assertIn('expected exactly one INNO-Hit IPK', manual_release)
        self.assertIn('workflows: [CI]', automatic_release)
        self.assertIn('types: [completed]', automatic_release)
        self.assertIn("github.event.workflow_run.id", automatic_release)
        self.assertNotIn('name: org.csweb.tv-ipk', automatic_release)
        self.assertIn("[release-innohit]", automatic_release)
        self.assertIn("[build-innohit-plugins]", automatic_release)
        self.assertIn("not in message", automatic_release)
        self.assertIn('gh release create "v$VERSION"', automatic_release)
        self.assertIn('not an older artifact', automatic_release)
        self.assertNotIn('workflow_dispatch:', automatic_release)



if __name__ == "__main__":
    unittest.main()
