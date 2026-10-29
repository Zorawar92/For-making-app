# Licensing and redistribution

## Current licence: GPL-3.0-or-later

This repository is a derivative of [CSWeb TV](https://github.com/sostk/cloudstream-webos-native), imported at the revision recorded in [`UPSTREAM.md`](UPSTREAM.md). The app, the pinned Native App Kit, and the imported source files carry `GPL-3.0-or-later` notices; the repository's [`LICENSE`](LICENSE) contains the GPL version 3 text. The package also includes components with their own licences and notices, collected under `runtime/licenses*` and in the Native App Kit's third-party notices.

The GPL permits commercial use and redistribution if its conditions are followed. Therefore this repository **does not currently prohibit commercial use**. Replacing the root `LICENSE`, or adding a non-commercial condition to GPL-covered code, would not change the rights already granted by the upstream copyright holders and would conflict with the existing GPL grant. A non-commercial licence is also not an OSI-approved Open Source licence.

A non-commercial licence for the whole app would require a rights review and permission to relicense every relevant upstream contribution and dependency, or a clean-room replacement of code that cannot be relicensed. This repository does not document such permissions, so it does not claim or publish a whole-project non-commercial licence. Obtain qualified legal advice before relying on a different licensing interpretation.

## Third-party code, data, and plugins

- Keep each dependency's copyright notices, licence text, source-offer obligations, and package notices. The app's licence does not replace them.
- Plugin archives are separate third-party works and are not relicensed by this repository. At the maintainer's explicit direction, public INNO-Hit releases include the selected StreamingCommunity, AnimeUnity, and AnimeWorld archives; the maintainer confirms they are open source and redistributable. The release pins their source revision and publishes `plugins-manifest.json` with versions, sizes, and SHA-256 hashes. This manifest records provenance and integrity, not a substitute licence or a certification that the plugins work on a particular TV. Each work retains its own authorship and applicable terms.
- Stream URLs, media, subtitles, and add-on content are not relicensed by this repository.

## Stremio interoperability

The implementation described in [`docs/research/stremio-webos-player.md`](docs/research/stremio-webos-player.md) is an independent implementation of a documented link/data format. No Stremio source code is bundled by it. Stremio and its names/logos remain the property of their respective rights holders; this project is not affiliated with Stremio.
