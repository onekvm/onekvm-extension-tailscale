#!/usr/bin/env python3
"""Check and optionally update the pinned Tailscale release.

The extension package version is intentionally the same as the Tailscale
release it contains.  Keeping this check in a small standard-library script
lets the scheduled workflow use the same rules as local verification.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
BBAPPEND_DIR = ROOT / "oe-recipes/recipes-connectivity/tailscale"
EXTENSION_DIR = ROOT / "oe-recipes/recipes-onekvm/tailscale"
README = ROOT / "oe-recipes/README.md"
UPSTREAM_REPO = "https://github.com/tailscale/tailscale.git"
VERSION_RE = re.compile(r"^1\.\d+\.\d+$")


def one_file(paths: list[Path], description: str) -> Path:
    if len(paths) != 1:
        names = ", ".join(str(path.relative_to(ROOT)) for path in paths)
        raise RuntimeError(f"expected one {description}, found {len(paths)}: {names}")
    return paths[0]


def release_version() -> str:
    override = os.environ.get("TAILSCALE_VERSION")
    if override:
        return override.removeprefix("v")

    api_url = os.environ.get(
        "TAILSCALE_RELEASE_API_URL",
        "https://api.github.com/repos/tailscale/tailscale/releases/latest",
    )
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "onekvm-tailscale-version-check"}
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = Request(api_url, headers=headers)
    with urlopen(request, timeout=30) as response:
        payload = json.load(response)
    tag = payload.get("tag_name", "")
    version = tag.removeprefix("v")
    if not version:
        raise RuntimeError(f"GitHub release response has no tag_name: {api_url}")
    return version


def release_commit(version: str) -> str:
    override = os.environ.get("TAILSCALE_SRCREV")
    if override:
        return override

    tag = f"refs/tags/v{version}"
    result = subprocess.run(
        ["git", "ls-remote", UPSTREAM_REPO, tag, f"{tag}^{{}}"],
        check=True,
        capture_output=True,
        text=True,
    )
    commits: dict[str, str] = {}
    for line in result.stdout.splitlines():
        commit, ref = line.split(maxsplit=1)
        commits[ref] = commit
    # Annotated tags have a peeled commit at the ^{} ref. Lightweight tags
    # only have the plain ref.
    try:
        return commits[f"{tag}^{{}}"]
    except KeyError:
        try:
            return commits[tag]
        except KeyError as error:
            raise RuntimeError(f"Tailscale release tag v{version} was not found") from error


def current_files() -> tuple[Path, Path]:
    bbappend = one_file(sorted(BBAPPEND_DIR.glob("tailscale_*.bbappend")), "Tailscale bbappend")
    recipe = one_file(
        sorted(EXTENSION_DIR.glob("onekvm-extension-tailscale_*.bb")),
        "Tailscale extension recipe",
    )
    return bbappend, recipe


def current_state() -> dict[str, str | Path]:
    bbappend, recipe = current_files()
    bbappend_text = bbappend.read_text()
    recipe_text = recipe.read_text()

    pv_match = re.search(r'(?m)^PV\s*=\s*"([^"]+)"\s*$', bbappend_text)
    rev_match = re.search(r'(?m)^SRCREV\s*=\s*"([0-9a-f]{40})"\s*$', bbappend_text)
    recipe_version = recipe.stem.rsplit("_", maxsplit=1)[-1]
    dependency_match = re.search(
        r'(?m)^RDEPENDS:\$\{PN\}.*\btailscale\s+\(>=\s*([^)]+)\)',
        recipe_text,
    )
    if not pv_match or not rev_match or not dependency_match:
        raise RuntimeError("could not parse the Tailscale version fields from the recipes")

    return {
        "bbappend": bbappend,
        "recipe": recipe,
        "version": pv_match.group(1),
        "recipe_version": recipe_version,
        "dependency_version": dependency_match.group(1),
        "srcrev": rev_match.group(1),
    }


def validate_version(version: str, source: str) -> None:
    if not VERSION_RE.fullmatch(version):
        raise RuntimeError(f"{source} has unsupported version {version!r}; expected 1.x.y")


def mismatches(state: dict[str, str | Path], latest: str, commit: str) -> list[str]:
    problems: list[str] = []
    for field in ("version", "recipe_version", "dependency_version"):
        value = str(state[field])
        if value != latest:
            problems.append(f"{field} is {value}, expected {latest}")
    if str(state["srcrev"]) != commit:
        problems.append(f"SRCREV is {state['srcrev']}, expected {commit}")
    return problems


def replace_once(text: str, pattern: str, replacement: str, description: str) -> str:
    updated, count = re.subn(pattern, replacement, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise RuntimeError(f"could not update {description}")
    return updated


def update_files(state: dict[str, str | Path], version: str, commit: str) -> None:
    bbappend = Path(state["bbappend"])
    recipe = Path(state["recipe"])
    bbappend_text = bbappend.read_text()
    bbappend_text = replace_once(
        bbappend_text,
        r'^(PV\s*=\s*")[^"]+("\s*)$',
        rf"\g<1>{version}\g<2>",
        "Tailscale PV",
    )
    bbappend_text = replace_once(
        bbappend_text,
        r'^(SRCREV\s*=\s*")[0-9a-f]{40}("\s*)$',
        rf"\g<1>{commit}\g<2>",
        "Tailscale SRCREV",
    )
    bbappend_text = re.sub(r"Tailscale 1\.\d+\.\d+", f"Tailscale {version}", bbappend_text)
    bbappend.write_text(bbappend_text)

    recipe_text = recipe.read_text()
    recipe_text = replace_once(
        recipe_text,
        r'(RDEPENDS:\$\{PN\}.*\btailscale\s+\(>=\s*)[^)]+(\))',
        rf"\g<1>{version}\g<2>",
        "extension runtime dependency",
    )
    recipe.write_text(recipe_text)

    destination = recipe.with_name(f"onekvm-extension-tailscale_{version}.bb")
    if destination != recipe:
        if destination.exists():
            raise RuntimeError(f"cannot rename recipe; destination exists: {destination}")
        recipe.rename(destination)

    readme_text = README.read_text()
    readme_text = re.sub(r"Tailscale 1\.\d+\.\d+", f"Tailscale {version}", readme_text)
    readme_text = re.sub(r"tailscale >= 1\.\d+\.\d+", f"tailscale >= {version}", readme_text)
    README.write_text(readme_text)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update",
        action="store_true",
        help="update the recipes to the latest stable release before checking",
    )
    args = parser.parse_args()

    latest = release_version()
    validate_version(latest, "latest Tailscale release")
    commit = release_commit(latest)
    state = current_state()
    for field in ("version", "recipe_version", "dependency_version"):
        validate_version(str(state[field]), field)

    problems = mismatches(state, latest, commit)
    current = str(state["version"])
    if problems and args.update:
        if tuple(map(int, current.split("."))) > tuple(map(int, latest.split("."))):
            raise RuntimeError(
                f"the pinned version {current} is newer than the latest release {latest}; refusing to downgrade"
            )
        update_files(state, latest, commit)
        state = current_state()
        problems = mismatches(state, latest, commit)

    print(f"Tailscale release: {latest} ({commit})")
    print(f"Pinned extension: {state['version']} ({state['srcrev']})")
    if problems:
        for problem in problems:
            print(f"ERROR: {problem}", file=sys.stderr)
        if args.update:
            print(
                "Version fields were updated, but the generated recipe metadata still needs review.",
                file=sys.stderr,
            )
        return 1
    print("Tailscale and extension versions are in sync.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
