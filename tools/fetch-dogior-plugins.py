#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fetch the selected community .cs3 plugins into a staged IPK build directory.

Plugin binaries stay out of Git. The staged manifest records the pinned source ref, versions,
file sizes and SHA-256 hashes and accompanies the INNO-Hit IPK in the release workflow.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from io import BytesIO
from pathlib import Path

OWNER = "doGior"
REPOSITORY = "doGiorsHadEnough"
BUILD_REF = "ce3e8e4eee533a6b03ccd17426edadf5ac88bc06"
REPOSITORY_URL = "https://raw.githubusercontent.com/doGior/doGiorsHadEnough/refs/heads/builds/repo.json"
PLUGIN_NAMES = ("StreamingCommunity", "AnimeUnity", "AnimeWorld")
API_ROOT = f"https://api.github.com/repos/{OWNER}/{REPOSITORY}/contents"
MAX_PLUGIN_BYTES = 32 * 1024 * 1024


def read_github_file(path: str) -> bytes:
    query = urllib.parse.urlencode({"ref": BUILD_REF})
    url = f"{API_ROOT}/{urllib.parse.quote(path)}?{query}"
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "For-making-app-local-plugin-bundler",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            data = json.load(response)
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"GitHub API download failed for {path}: {exc}") from exc
    if data.get("encoding") != "base64" or not isinstance(data.get("content"), str):
        raise RuntimeError(f"GitHub API did not return base64 content for {path}")
    try:
        content = base64.b64decode(data["content"], validate=False)
    except ValueError as exc:
        raise RuntimeError(f"Invalid base64 content for {path}") from exc
    if len(content) > MAX_PLUGIN_BYTES:
        raise RuntimeError(f"{path} exceeds the {MAX_PLUGIN_BYTES}-byte size limit")
    return content


def read_plugin_list() -> list[dict]:
    try:
        plugins = json.loads(read_github_file("plugins.json"))
    except json.JSONDecodeError as exc:
        raise RuntimeError("The pinned plugins.json is not valid JSON") from exc
    if not isinstance(plugins, list):
        raise RuntimeError("The pinned plugins.json is not an array")
    return plugins


def safe_plugin_filename(name: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", name):
        raise RuntimeError(f"Unsafe plugin name from repository: {name!r}")
    return f"{name}.cs3"


def validate_cs3(data: bytes, filename: str) -> None:
    try:
        with zipfile.ZipFile(BytesIO(data)) as archive:
            bad_member = archive.testzip()
            if bad_member:
                raise RuntimeError(f"Corrupt entry {bad_member!r} in {filename}")
            if "manifest.json" not in archive.namelist():
                raise RuntimeError(f"{filename} has no CloudStream manifest.json")
            manifest = json.loads(archive.read("manifest.json"))
            if not isinstance(manifest, dict) or not manifest.get("pluginClassName"):
                raise RuntimeError(f"{filename} has an invalid CloudStream manifest")
    except (zipfile.BadZipFile, json.JSONDecodeError, KeyError) as exc:
        raise RuntimeError(f"{filename} is not a valid CloudStream .cs3 archive: {exc}") from exc


def collect_plugins() -> tuple[dict, list[tuple[dict, bytes]]]:
    by_name = {}
    for entry in read_plugin_list():
        if isinstance(entry, dict) and entry.get("internalName") in PLUGIN_NAMES:
            by_name[entry["internalName"]] = entry

    missing = [name for name in PLUGIN_NAMES if name not in by_name]
    if missing:
        raise RuntimeError("Pinned plugin list is missing: " + ", ".join(missing))

    packaged = []
    for name in PLUGIN_NAMES:
        entry = by_name[name]
        if entry.get("status", 1) == 0:
            raise RuntimeError(f"doGior marks {name} as not working; refusing to bundle it")
        filename = safe_plugin_filename(name)
        data = read_github_file(filename)
        actual_hash = hashlib.sha256(data).hexdigest()
        expected = entry.get("fileHash", "")
        if not expected.startswith("sha256-") or expected[7:].lower() != actual_hash:
            raise RuntimeError(f"SHA-256 mismatch for {filename}: got {actual_hash}, expected {expected!r}")
        validate_cs3(data, filename)
        if entry.get("fileSize") and int(entry["fileSize"]) != len(data):
            raise RuntimeError(f"Size mismatch for {filename}: got {len(data)}, expected {entry['fileSize']}")
        packaged.append((entry, data))

    repository = {
        "name": "doGior's Had Enough",
        "url": REPOSITORY_URL,
        "iconUrl": "https://raw.githubusercontent.com/doGior/doGiorsHadEnough/master/repo_icon.png",
    }
    return repository, packaged


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True, type=Path, help="package input directory (basename must be bundled-plugins)")
    args = parser.parse_args()
    if args.out.name != "bundled-plugins":
        parser.error("--out must end in a directory named bundled-plugins")

    try:
        repository, packaged = collect_plugins()
        args.out.mkdir(parents=True, exist_ok=True)
        index = {"schemaVersion": 1, "sourceRef": BUILD_REF, "repository": repository, "plugins": []}
        for entry, data in packaged:
            name = entry["internalName"]
            filename = safe_plugin_filename(name)
            (args.out / filename).write_bytes(data)
            index["plugins"].append(
                {
                    "file": filename,
                    "internalName": name,
                    "name": entry.get("name") or name,
                    "url": entry["url"],
                    "version": int(entry["version"]),
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "size": len(data),
                    "tvTypes": entry.get("tvTypes", []),
                    "language": entry.get("language", ""),
                    "iconUrl": entry.get("iconUrl", ""),
                }
            )
        (args.out / "manifest.json").write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    except (RuntimeError, OSError, ValueError) as exc:
        print(f"fetch-dogior-plugins: {exc}", file=sys.stderr)
        return 1

    print(f"Prepared {len(packaged)} doGior plugins in {args.out} (source ref {BUILD_REF}).")
    print("Plugin bytes remain out of Git; manifest.json records source commit and hashes for IPK provenance.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
