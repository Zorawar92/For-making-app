# Upstream provenance

This repository starts from the source tree of
[`sostk/cloudstream-webos-native`](https://github.com/sostk/cloudstream-webos-native), imported from
commit `1708158f020c268fb1da4eade5238af4cf49ea54` (2026-10-24). The native webOS UI, CloudStream
plugin runtime, media pipeline, tests, and build tools originate there. Its source files identify
`GPL-3.0-or-later`; see [`LICENSE`](LICENSE), preserve the file-level SPDX notices, and review the
runtime's separate third-party notices before distributing an IPK.

This fork adds an INNO-Hit 55-inch family build profile for the published IH55UWB/IH55UWB4
specifications. It does not claim either model has been tested. Changes made here are maintained in
this repository; the upstream project remains at its own URL and is not affiliated with INNO-Hit.

To update the imported base, review upstream changes and merge them deliberately. Do not overwrite
INNO-Hit-specific changes or claim upstream device-test evidence as tests performed by this fork.
