#!/usr/bin/env python3
"""PreToolUse hook: ask before editing files that define memory layout, boot chain or update.

CLAUDE.md: any change to memory layout, linker placement, boot sequence or security
boundaries requires an ADR update. This hook turns that rule into a confirmation prompt.
"""

import fnmatch
import json
import os
import sys

GUARDED = (
    "*.ld", "*.icf", "*.scf",
    "*flash_partitioning*",
    "firmware/boot/*",
    "examples/*/mcuboot/*",
    "examples/*/log/primary.conf",
    "platform/log/mcux/aied_log_port.c",
    "scripts/rt1180_mcuboot.py",
    "scripts/rt1180_logs.py",
    "docs/architecture/MEMORY_MAP.md",
    "docs/architecture/BOOT_FLOW.md",
)


def main() -> None:
    try:
        payload = json.load(sys.stdin)
    except ValueError:
        return  # unreadable payload: leave the decision to the normal permission rules
    path = (payload.get("tool_input") or {}).get("file_path", "")
    root = os.environ.get("CLAUDE_PROJECT_DIR", os.getcwd())
    try:
        relative = os.path.relpath(path, root).replace("\\", "/")
    except ValueError:  # different drive on Windows: outside the project
        return
    if relative.startswith("../") or "/decisions/ADR-" in f"/{relative}":
        return
    if any(fnmatch.fnmatch(relative, pattern) for pattern in GUARDED):
        print(json.dumps({"hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "ask",
            "permissionDecisionReason": (
                f"{relative} defines memory layout, boot chain or update behaviour. "
                "Per CLAUDE.md the matching ADR (docs/architecture/decisions) and "
                "MEMORY_MAP/BOOT_FLOW must be updated in the same change."),
        }}))


if __name__ == "__main__":
    main()
