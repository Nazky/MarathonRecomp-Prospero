#!/usr/bin/env python3
"""Generate the ignored version.cpp/version.h files for the manual PS5 build."""

from __future__ import annotations

import os
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
source = root / "MarathonRecomp"
version_file = source / "res" / "version.txt"
values: dict[str, str] = {}
for raw_line in version_file.read_text(encoding="utf-8").splitlines():
    line = raw_line.strip()
    if not line or "=" not in line:
        continue
    key, value = line.split("=", 1)
    values[key.strip()] = value.strip().strip('"')

required = {"VERSION_MILESTONE", "VERSION_MAJOR", "VERSION_MINOR", "VERSION_REVISION"}
missing = required - values.keys()
if missing:
    raise SystemExit(f"missing version fields in {version_file}: {', '.join(sorted(missing))}")

milestone = values["VERSION_MILESTONE"]
version_string = f"v{values['VERSION_MAJOR']}.{values['VERSION_MINOR']}.{values['VERSION_REVISION']}"
if milestone:
    version_string = f"{milestone} {version_string}"
version_string += "-PS5"

# Release configuration in MarathonRecomp/CMakeLists.txt suppresses VCS/build
# suffixes. The archive workspace may not have Git metadata, so keep these empty.
values.update(
    BUILD_TYPE=os.environ.get("BUILD_TYPE", "Release"),
    BRANCH_NAME="",
    COMMIT_HASH="",
    COMMIT_HASH_SHORT="",
    VERSION_STRING=version_string,
)

for template_name, output_name in (
    ("version.h.template", "version.h"),
    ("version.cpp.template", "version.cpp"),
):
    template = source / "res" / template_name
    output = source / output_name
    text = template.read_text(encoding="utf-8")
    for key, value in values.items():
        text = text.replace(f"@{key}@", value)
    unresolved = sorted(set(re.findall(r"@[A-Z_]+@", text)))
    if unresolved:
        raise SystemExit(f"unresolved template values in {template}: {', '.join(unresolved)}")
    if not output.exists() or output.read_text(encoding="utf-8") != text:
        output.write_text(text, encoding="utf-8")
        print(f"Generated {output.relative_to(root)}")
