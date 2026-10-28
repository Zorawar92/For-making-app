# Stremio links and the native webOS player

Research checked against Stremio Web commit `8329565d17b75d2af8ba5822aa8059058a5d934e` and Stremio Core commit `3417c2e7ed45cdbfd86b3226f69814c2dc4947ad` (2026-10-28). This is a source-code review, not a test on the INNO-Hit TV.

## What Stremio does

- In Stremio Web, a pasted `http://` or `https://` media URL is encoded as a stream and routed to its internal player (`src/common/usePlayUrl.ts`). A magnet URL instead goes through `createTorrentFromMagnet`; playback depends on a reachable Stremio Streaming Server.
- Stremio Core builds a player deep link in `src/deep_links/mod.rs` with the form `stremio:///player/{encodedStream}/{streamTransportUrl}/{metaTransportUrl}/{type}/{id}/{videoId}`, and may append `?position=<seconds>`. The stream is standard Base64 around a zlib stream of JSON (`src/types/resource/stream.rs`, `Stream::encode`). At the checked Core revision it uses `Compression::none()`.
- A stream object can contain a direct `url`, a `name`, subtitle URLs, and `behaviorHints.proxyHeaders.request`. Other stream source types include torrents, YouTube, archives, and external/player-frame sources; those are not equivalent to a direct HTTP media URL.
- Stremio's data model has an optional `externalPlayer.webos` payload, populated only when an add-on stream source supplies `webosUrl`. In the inspected sources this is an externally supplied payload, not a universal webOS URI scheme. No general-purpose webOS Stremio hand-off was established by this review.

Source references:

- [Stremio Web `usePlayUrl.ts`](https://github.com/Stremio/stremio-web/blob/8329565d17b75d2af8ba5822aa8059058a5d934e/src/common/usePlayUrl.ts)
- [Stremio Web stream type](https://github.com/Stremio/stremio-web/blob/8329565d17b75d2af8ba5822aa8059058a5d934e/src/core/types/Stream.d.ts)
- [Stremio Core deep links](https://github.com/Stremio/stremio-core/blob/3417c2e7ed45cdbfd86b3226f69814c2dc4947ad/src/deep_links/mod.rs)
- [Stremio Core stream serialization](https://github.com/Stremio/stremio-core/blob/3417c2e7ed45cdbfd86b3226f69814c2dc4947ad/src/types/resource/stream.rs)

## Implemented in this repository

The app accepts launch parameters with a direct `target`/`url` HTTP(S) stream URL and Stremio's `stremio:///player/...` link when the encoded stream contains a direct HTTP(S) URL. It passes the URL to the existing **native webOS** player pipeline (Starfish/NDL with the app's media sources), and carries over the stream title, request headers, external subtitle URLs, and resume position where present. Unsupported links get an explanatory toast instead of being silently sent to a provider.

The Base64/zlib reader intentionally accepts only the stored-DEFLATE blocks emitted by the checked Stremio Core encoder. It validates the zlib header, block lengths, limits the decoded payload to 64 KiB, verifies Adler-32, and rejects unknown compression. It does not fetch Stremio metadata/add-ons or contact the transport URLs in the deep link.

## Limits and verification status

- Only direct HTTP(S) media URLs are handled. The app does not include Stremio Core, the Stremio Streaming Server, a torrent client, or Stremio's add-on/catalog/account stack.
- Magnet/torrent, YouTube IDs, archive extraction, player-frame sources, DRM that the current webOS player cannot handle, and add-on URLs requiring Stremio proxying are not supported by this hand-off.
- The hand-off works only if webOS (or a caller/add-on) launches this app and forwards the link as launch parameter `target` or `url`. Registering a custom URI scheme or selecting this app from Stremio on a TV has **not** been verified and is not claimed.
- The C++ parser has host unit tests. No full CMake build, IPK, or device test has been completed in this environment. Compatibility on IH55UWB/IH55UWB4 remains unverified.
