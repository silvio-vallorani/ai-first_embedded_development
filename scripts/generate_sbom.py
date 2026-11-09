#!/usr/bin/env python3
"""Generate the SPDX 2.3 SBOM of the RT1180 firmware from its build (ADR-0011).

    python scripts/generate_sbom.py                 # after scripts/rt1180_mcuboot.py build
    python scripts/generate_sbom.py selftest        # check the generator on a synthetic tree

The SDK components are the ones whose sources were compiled, or whose headers were on the include
path, in the build directories' compile_commands.json: each file is mapped to the nearest
SBOM.spdx.json that NXP ships at the root of every SDK repository.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK_ROOT = Path(os.environ.get("MCUXPRESSO_SDK", ROOT.parent / "mcuxpresso-sdk")).resolve() / "mcuxsdk"
# Bootloader plus the v1 release image of the update-test set (scripts/rt1180_mcuboot.py).
DEFAULT_BUILDS = [ROOT / "build" / "rt1180-mcuboot", ROOT / "build" / "rt1180-ut-v1"]
DEFAULT_OUTPUT = ROOT / "build" / "sbom" / "aied-rt1180-firmware.spdx.json"
REPO_URL = "https://github.com/silvio-vallorani/ai-first_embedded_development"
TOOLCHAIN = ("Arm GNU Toolchain", "14.2.rel1")  # also provides the statically linked newlib and libgcc

# NXP repositories whose SBOM lists many third-party packages that are not linked into this firmware:
# only the repository package itself is reported.
CONTAINER_REPOS = {".", "components", "examples", "devices/RT"}
# Third-party packages actually linked, per SDK repository (names as in NXP's SBOM).
LINKED_THIRD_PARTY = {
    "arch/arm/CMSIS": ["ARM-software/CMSIS_6"],
    "middleware/littlefs": ["littlefs-project/littlefs"],
    "middleware/mbedtls3x": ["mbed TLS"],
    "middleware/mcuboot_opensource": ["mcu-tools/mcuboot"],
    "middleware/multicore/mcmgr": [],
}
# NXP's SBOM versions can lag the sources (it lists littlefs 2.9.1 and mbed TLS 3.6.4 for 2.11 and 3.6.7):
# the version is read from the source where it is reliable. (file in the repository, regex, format)
VERSION_PROBES = {
    "littlefs-project/littlefs": ("lfs.h", r"#define LFS_VERSION 0x([0-9a-fA-F]{4})([0-9a-fA-F]{4})",
                                  lambda m: f"{int(m[1], 16)}.{int(m[2], 16)}"),
    "mbed TLS": ("include/mbedtls/build_info.h", r'MBEDTLS_VERSION_STRING\s+"([^"]+)"', lambda m: m[1]),
    "mcu-tools/mcuboot": ("docs/release-notes.md", r"^## Version (\S+)", lambda m: m[1]),
}
# Security references written for the actual version, so scanners can match known vulnerabilities.
SECURITY_REFS = {
    "ARM-software/CMSIS_6": [("PACKAGE-MANAGER", "purl", "pkg:github/ARM-software/CMSIS_6@v{v}")],
    "littlefs-project/littlefs": [("PACKAGE-MANAGER", "purl", "pkg:github/littlefs-project/littlefs@v{v}")],
    "mbed TLS": [("SECURITY", "cpe23Type", "cpe:2.3:a:arm:mbed_tls:{v}:*:*:*:*:*:*:*"),
                 ("PACKAGE-MANAGER", "purl", "pkg:github/Mbed-TLS/mbedtls@v{v}")],
    "mcu-tools/mcuboot": [("SECURITY", "cpe23Type", "cpe:2.3:a:mcu-tools:mcuboot:{v}:*:*:*:*:*:*:*"),
                          ("PACKAGE-MANAGER", "purl", "pkg:github/mcu-tools/mcuboot@v{v}")],
}


def compile_entries(build_dirs: list[Path]) -> list[dict]:
    entries = []
    for build in build_dirs:
        databases = sorted(build.rglob("compile_commands.json"))
        if not databases:
            sys.exit(f"Error: no compile_commands.json under {build}; build the firmware first.")
        for database in databases:
            entries += json.loads(database.read_text(encoding="utf-8"))
    return entries


def used_paths(entries: list[dict]) -> set[Path]:
    """Compiled source files and include directories of every compile command."""
    paths = set()
    for entry in entries:
        directory = Path(entry.get("directory", "."))
        paths.add((directory / entry["file"]).resolve())
        args = entry["arguments"] if "arguments" in entry else shlex.split(entry["command"], posix=os.name != "nt")
        for arg in args:
            if arg.startswith("-I") and len(arg) > 2:
                paths.add((directory / arg[2:].strip('"')).resolve())
    return paths


def sdk_repos(paths: set[Path], sdk: Path) -> set[str]:
    """SDK repository folders (relative to the SDK root, "." for the core) that own a used path."""
    repos = set()
    for path in paths:
        if not path.is_relative_to(sdk):
            continue
        folder = path if path.is_dir() else path.parent
        while folder != sdk.parent and not (folder / "SBOM.spdx.json").is_file():
            folder = folder.parent
        if folder != sdk.parent:
            repos.add(folder.relative_to(sdk).as_posix() or ".")
    return repos


def git(args: list[str], cwd: Path) -> str:
    result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else ""


def spdx_id(name: str) -> str:
    return "SPDXRef-" + re.sub(r"[^A-Za-z0-9.-]+", "-", name).strip("-")


def package(name: str, version: str, **fields) -> dict:
    entry = {"SPDXID": spdx_id(f"{name}-{version}"), "name": name, "versionInfo": version,
             "downloadLocation": "NOASSERTION", "filesAnalyzed": False,
             "licenseConcluded": "NOASSERTION", "licenseDeclared": "NOASSERTION", "supplier": "NOASSERTION"}
    entry.update(fields)
    return entry


def from_nxp(source: dict, comment: str, repo: Path) -> dict:
    keep = {k: source[k] for k in ("licenseConcluded", "licenseDeclared", "downloadLocation", "homepage",
                                   "originator") if source.get(k)}
    name, version = source["name"], source["versionInfo"]
    if name in VERSION_PROBES:
        file, pattern, render = VERSION_PROBES[name]
        match = re.search(pattern, (repo / file).read_text(encoding="utf-8", errors="replace"), re.MULTILINE)
        if not match:
            sys.exit(f"Error: cannot read the {name} version from {repo / file}.")
        if render(match) != version.lstrip("v"):
            comment += f"; version read from {file} (NXP SBOM states {version})"
            version = render(match)
    if name in SECURITY_REFS:
        keep["externalRefs"] = [{"referenceCategory": c, "referenceType": t, "referenceLocator": l.format(v=version.lstrip("v"))}
                                for c, t, l in SECURITY_REFS[name]]
    return package(name, version, comment=comment, **keep)


def sdk_packages(repos: set[str], sdk: Path) -> tuple[list[dict], dict[str, dict]]:
    """Packages of the used SDK repositories and the LicenseRef texts they reference."""
    packages, licenses = [], {}
    for repo in sorted(repos):
        if repo not in CONTAINER_REPOS and repo not in LINKED_THIRD_PARTY:
            sys.exit(f"Error: SDK repository {repo} is linked but not classified in generate_sbom.py; "
                     "add it to CONTAINER_REPOS or LINKED_THIRD_PARTY.")
        document = json.loads((sdk / repo / "SBOM.spdx.json").read_text(encoding="utf-8"))
        described = {r["relatedSpdxElement"] for r in document["relationships"] if r["relationshipType"] == "DESCRIBES"}
        commit = git(["rev-parse", "HEAD"], sdk / repo)
        location = f"MCUXpresso SDK repository {repo}" + (f", commit {commit}" if commit else "")
        wanted = set(LINKED_THIRD_PARTY.get(repo, []))
        for source in document["packages"]:
            if source["SPDXID"] in described:
                packages.append(from_nxp(source, location, sdk / repo))
            elif source["name"] in wanted:
                packages.append(from_nxp(source, f"Third-party code in {location}", sdk / repo))
                wanted.discard(source["name"])
        if wanted:
            sys.exit(f"Error: {sorted(wanted)} not found in {repo}/SBOM.spdx.json.")
        used = set(re.findall(r"LicenseRef-[A-Za-z0-9.-]+", json.dumps(packages)))
        licenses.update({info["licenseId"]: info for info in document.get("hasExtractedLicensingInfos", [])
                         if info["licenseId"] in used})
    return packages, licenses


def build_tools() -> list[dict]:
    tools = [package(*TOOLCHAIN, supplier="Organization: Arm",
                     downloadLocation="https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads",
                     comment="Compiler; its newlib C library and libgcc are statically linked into the images")]
    for line in (ROOT / "requirements.txt").read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Za-z0-9_.-]+)==([^\s#]+)", line)
        if match:
            name, version = match.groups()
            purl = f"pkg:pypi/{name.lower()}@{version}"
            tools.append(package(name, version, downloadLocation=f"https://pypi.org/project/{name}/{version}/",
                                 externalRefs=[{"referenceCategory": "PACKAGE-MANAGER", "referenceType": "purl",
                                                "referenceLocator": purl}]))
    return tools


def generate(build_dirs: list[Path], sdk: Path, version: str) -> dict:
    sdk = sdk.resolve()
    repos = sdk_repos(used_paths(compile_entries(build_dirs)), sdk)
    product = package("aied-rt1180-firmware", version, supplier="Person: Silvio Vallorani",
                      downloadLocation=f"git+{REPO_URL}", homepage=REPO_URL,
                      licenseConcluded="BSD-3-Clause", licenseDeclared="BSD-3-Clause",
                      comment="MCUboot bootloader and CM33 + CM7 application image for the MIMXRT1180-EVK")
    contained, licenses = sdk_packages(repos, sdk)
    tools = build_tools()
    relationships = [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES",
                      "relatedSpdxElement": product["SPDXID"]}]
    relationships += [{"spdxElementId": product["SPDXID"], "relationshipType": "CONTAINS",
                       "relatedSpdxElement": p["SPDXID"]} for p in contained]
    relationships += [{"spdxElementId": p["SPDXID"], "relationshipType": "BUILD_TOOL_OF",
                       "relatedSpdxElement": product["SPDXID"]} for p in tools]
    created = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return {"spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
            "name": f"aied-rt1180-firmware-{version}",
            "documentNamespace": f"{REPO_URL}/sbom/{version}-{uuid.uuid4()}",
            "creationInfo": {"created": created, "creators": ["Tool: aied-generate-sbom"]},
            "hasExtractedLicensingInfos": list(licenses.values()),
            "packages": [product, *contained, *tools], "relationships": relationships}


def command_generate(args: argparse.Namespace) -> None:
    version = git(["describe", "--tags", "--always", "--dirty"], ROOT) or "unknown"
    document = generate(args.build or DEFAULT_BUILDS, args.sdk, version)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(f"{len(document['packages'])} packages: {args.output}")
    for p in document["packages"]:
        print(f"  {p['name']} {p['versionInfo']}")


def command_selftest(_: argparse.Namespace) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        sdk, build = Path(tmp) / "mcuxsdk", Path(tmp) / "build"

        def repo(path: str, *names: str) -> None:
            (sdk / path / "src").mkdir(parents=True)
            packages = [{"SPDXID": f"SPDXRef-{i}", "name": n, "versionInfo": "v1.0"} for i, n in enumerate(names)]
            describes = [{"spdxElementId": "SPDXRef-DOCUMENT", "relationshipType": "DESCRIBES",
                          "relatedSpdxElement": "SPDXRef-0"}]
            (sdk / path / "SBOM.spdx.json").write_text(json.dumps({"packages": packages, "relationships": describes}))

        repo(".", "mcuxsdk-core", "Ruby")
        repo("middleware/mbedtls3x", "nxp_mbedtls", "mbed TLS")
        repo("middleware/lwip", "lwip")
        repo("arch/arm/CMSIS", "mcu-sdk-cmsis", "ARM-software/CMSIS_6", "ARM-software/CMSIS-NN")
        (sdk / "middleware/mbedtls3x/include/mbedtls").mkdir(parents=True)
        (sdk / "middleware/mbedtls3x/include/mbedtls/build_info.h").write_text('#define MBEDTLS_VERSION_STRING  "1.2.3"')
        build.mkdir()
        entries = [{"directory": str(build), "file": str(sdk / "middleware/mbedtls3x/src/aes.c"),
                    "command": f"gcc -I{sdk / 'arch/arm/CMSIS/src'} -c aes.c"},
                   {"directory": str(build), "file": str(sdk / "src/fsl_common.c"), "arguments": ["gcc", "-c"]}]
        (build / "compile_commands.json").write_text(json.dumps(entries))
        names = [p["name"] for p in generate([build], sdk, "test")["packages"]]
        assert names[:6] == ["aied-rt1180-firmware", "mcuxsdk-core", "mcu-sdk-cmsis", "ARM-software/CMSIS_6",
                             "nxp_mbedtls", "mbed TLS"], names
        assert "Ruby" not in names and "ARM-software/CMSIS-NN" not in names and "lwip" not in names, names
        # The version comes from the source, not from the (possibly stale) NXP SBOM.
        mbedtls = next(p for p in generate([build], sdk, "test")["packages"] if p["name"] == "mbed TLS")
        assert mbedtls["versionInfo"] == "1.2.3" and "NXP SBOM states v1.0" in mbedtls["comment"], mbedtls
        assert {"referenceCategory": "SECURITY", "referenceType": "cpe23Type",
                "referenceLocator": "cpe:2.3:a:arm:mbed_tls:1.2.3:*:*:*:*:*:*:*"} in mbedtls["externalRefs"], mbedtls
        # An unclassified SDK repository must stop the generator instead of being silently dropped.
        entries.append({"directory": str(build), "file": str(sdk / "middleware/lwip/src/tcp.c"), "arguments": []})
        (build / "compile_commands.json").write_text(json.dumps(entries))
        try:
            generate([build], sdk, "test")
        except SystemExit as error:
            assert "middleware/lwip" in str(error), error
        else:
            raise AssertionError("unclassified repository accepted")
    print("SBOM generator: OK")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--build", type=Path, action="append", help="build directory (repeatable)")
    parser.add_argument("--sdk", type=Path, default=SDK_ROOT, help="mcuxsdk folder of the MCUXpresso SDK")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.set_defaults(func=command_generate)
    sub = parser.add_subparsers(dest="command")
    sub.add_parser("selftest", help="check the generator without an SDK").set_defaults(func=command_selftest)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
