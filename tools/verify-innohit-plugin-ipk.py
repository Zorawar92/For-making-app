#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify that the INNO-Hit IPK contains the exact plugin archives in its provenance manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import tarfile
from io import BytesIO
from pathlib import Path, PurePosixPath

SOURCE_REF = "ce3e8e4eee533a6b03ccd17426edadf5ac88bc06"
PLUGIN_NAMES = {"StreamingCommunity", "AnimeUnity", "AnimeWorld"}


def verify_bundle(ipk_path: Path, manifest_path: Path) -> list[str]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(manifest, dict):
        raise ValueError("plugin manifest must be a JSON object")
    if manifest.get("schemaVersion") != 1 or manifest.get("sourceRef") != SOURCE_REF:
        raise ValueError("plugin manifest has an unexpected schema or source ref")

    entries = manifest.get("plugins")
    if not isinstance(entries, list) or len(entries) != len(PLUGIN_NAMES):
        raise ValueError("plugin manifest must contain exactly three plugin entries")
    if not all(isinstance(entry, dict) for entry in entries):
        raise ValueError("plugin manifest contains a malformed plugin entry")
    by_name = {entry.get("internalName"): entry for entry in entries}
    if len(by_name) != len(entries) or set(by_name) != PLUGIN_NAMES:
        raise ValueError(f"plugin manifest must list exactly {sorted(PLUGIN_NAMES)}")

    try:
        members = subprocess.run(["ar", "t", str(ipk_path)], check=True, capture_output=True, text=True).stdout.splitlines()
    except (OSError, subprocess.CalledProcessError) as exc:
        raise ValueError(f"cannot list IPK ar members: {exc}") from exc
    data_members = [name for name in members if PurePosixPath(name).name.startswith("data.tar")]
    if len(data_members) != 1:
        raise ValueError(f"expected one data.tar member in IPK, found {data_members}")
    try:
        data_tar = subprocess.run(
            ["ar", "p", str(ipk_path), data_members[0]], check=True, capture_output=True
        ).stdout
        with tarfile.open(fileobj=BytesIO(data_tar), mode="r:*") as archive:
            packaged = {}
            for member in archive.getmembers():
                filename = PurePosixPath(member.name).name
                if filename.endswith(".cs3"):
                    if filename in packaged:
                        raise ValueError(f"duplicate plugin archive in IPK: {filename}")
                    if not member.isfile():
                        raise ValueError(f"plugin archive is not a regular file: {member.name}")
                    stream = archive.extractfile(member)
                    if stream is None:
                        raise ValueError(f"cannot read plugin archive: {member.name}")
                    packaged[filename] = stream.read()
    except (OSError, subprocess.CalledProcessError, tarfile.TarError) as exc:
        raise ValueError(f"cannot read IPK data archive: {exc}") from exc

    expected_files = {f"{name}.cs3" for name in PLUGIN_NAMES}
    if set(packaged) != expected_files:
        raise ValueError(f"IPK must contain exactly {sorted(expected_files)}; found {sorted(packaged)}")

    for name in sorted(PLUGIN_NAMES):
        entry = by_name[name]
        filename = f"{name}.cs3"
        data = packaged[filename]
        if entry.get("file") != filename:
            raise ValueError(f"manifest filename mismatch for {name}")
        digest = entry.get("sha256", "")
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError(f"invalid SHA-256 in manifest for {name}")
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f"IPK content SHA-256 mismatch for {name}")
        if entry.get("size") != len(data):
            raise ValueError(f"IPK content size mismatch for {name}")

    return sorted(PLUGIN_NAMES)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ipk", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    try:
        names = verify_bundle(args.ipk, args.manifest)
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        parser.error(str(exc))
    print("Verified bundled IPK plugins: " + ", ".join(names))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
