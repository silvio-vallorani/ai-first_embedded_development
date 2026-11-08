#!/usr/bin/env python3
"""Generate GitHub Wiki pages from the repository's Markdown documentation."""

from __future__ import annotations

import re
import shutil
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "wiki"

SPECIAL_SLUGS = {
    Path("README.md"): "Project-Overview",
    Path("docs/index.md"): "Documentation-Index",
    Path("docs/architecture/ARCHITECTURE.md"): "System-Architecture",
    Path("docs/architecture/MEMORY_MAP.md"): "Memory-Architecture",
    Path("docs/architecture/BOOT_FLOW.md"): "Boot-and-Recovery",
    Path("docs/architecture/FIRMWARE_UPDATE.md"): "Firmware-Update",
    Path("docs/architecture/decisions/README.md"): "Architecture-Decisions",
    Path("docs/toolchains/MCUXPRESSO_SDK.md"): "Build-System-MCUXpresso-SDK",
    Path("docs/engineering/UNIT_TESTING.md"): "Testing-and-Validation",
    Path("examples/dual_core_led_toggle/README.md"): "Example-Dual-Core-LED-Toggle",
}

LINK_PATTERN = re.compile(r"(?<!!)\[([^\]]+)\]\(([^)]+)\)")


def collect_sources() -> list[Path]:
    """Collect Markdown files that are part of the published documentation."""
    sources = [ROOT / "README.md"]
    sources.extend(sorted((ROOT / "docs").rglob("*.md")))
    sources.extend(sorted((ROOT / "examples").glob("*/README.md")))
    return [path for path in sources if path.is_file()]


def page_slug(source: Path) -> str:
    """Return a stable Wiki page slug for a source document."""
    relative = source.relative_to(ROOT)
    if relative in SPECIAL_SLUGS:
        return SPECIAL_SLUGS[relative]

    stem = source.stem
    if stem.startswith("ADR-"):
        return stem
    return re.sub(r"[^A-Za-z0-9-]+", "-", stem).strip("-")


def rewrite_links(content: str, source: Path, slug_by_path: dict[Path, str]) -> str:
    """Rewrite relative Markdown links to their generated Wiki page names."""
    def replace(match: re.Match[str]) -> str:
        label, raw_target = match.groups()
        target = raw_target.strip()
        if not target or target.startswith(("http://", "https://", "mailto:", "#")):
            return match.group(0)

        parsed = urlsplit(target)
        target_path = parsed.path
        if not target_path:
            return match.group(0)

        resolved = (source.parent / target_path).resolve()
        try:
            resolved.relative_to(ROOT.resolve())
        except ValueError:
            return match.group(0)

        if resolved in slug_by_path:
            destination = slug_by_path[resolved]
            if parsed.fragment:
                destination += f"#{parsed.fragment}"
            return f"[{label}]({destination})"

        return match.group(0)

    return LINK_PATTERN.sub(replace, content)


def main() -> None:
    """Generate the Wiki home page and one page for every source document."""
    sources = collect_sources()
    slug_by_path = {path.resolve(): page_slug(path) for path in sources}

    if len(set(slug_by_path.values())) != len(slug_by_path):
        raise SystemExit("Wiki page slug collision detected; assign explicit unique slugs.")

    if OUTPUT.exists():
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir(parents=True)

    page_entries: list[tuple[str, str]] = []
    for source in sources:
        slug = slug_by_path[source.resolve()]
        content = source.read_text(encoding="utf-8")
        content = rewrite_links(content, source, slug_by_path)
        (OUTPUT / f"{slug}.md").write_text(content.rstrip() + "\n", encoding="utf-8")
        title_match = re.search(r"^#\s+(.+)$", content, flags=re.MULTILINE)
        title = title_match.group(1).strip() if title_match else slug
        page_entries.append((title, slug))

    sections = {
        "Project": ["Project-Overview", "Documentation-Index"],
        "Architecture": [
            "System-Architecture",
            "Memory-Architecture",
            "Boot-and-Recovery",
            "Firmware-Update",
            "Architecture-Decisions",
            "WIKI-PUBLISHING",
        ],
        "Build and quality": [
            "Build-System-MCUXpresso-SDK",
            "Testing-and-Validation",
        ],
        "Examples": ["Example-Dual-Core-LED-Toggle"],
    }

    home = [
        "# AI-First Embedded Development",
        "",
        "This Wiki is generated automatically from the Markdown sources in the main repository.",
        "Edit the source documents in the main repository; do not edit generated Wiki pages directly.",
        "",
    ]
    available = {slug: title for title, slug in page_entries}
    for section, slugs in sections.items():
        links = [(available[slug], slug) for slug in slugs if slug in available]
        if links:
            home.extend([f"## {section}", ""])
            home.extend(f"- [{title}]({slug})" for title, slug in links)
            home.append("")

    adr_pages = sorted(
        ((title, slug) for title, slug in page_entries if slug.startswith("ADR-")),
        key=lambda item: item[1],
    )
    if adr_pages:
        home.extend(["## Architecture Decision Records", ""])
        home.extend(f"- [{title}]({slug})" for title, slug in adr_pages)
        home.append("")

    (OUTPUT / "Home.md").write_text("\n".join(home).rstrip() + "\n", encoding="utf-8")
    print(f"Generated {len(page_entries) + 1} Wiki pages in {OUTPUT}")


if __name__ == "__main__":
    main()
