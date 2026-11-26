#!/usr/bin/env bash
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

# This snapshot references assets stored in Blender's official LFS repository.
git lfs install --local --skip-smudge --skip-repo
git -c lfs.url=https://projects.blender.org/blender/blender.git/info/lfs lfs fetch origin HEAD
git lfs fsck --objects --pointers HEAD
git lfs checkout

python3 - <<'PY'
import pathlib
import subprocess

paths = subprocess.check_output(["git", "lfs", "ls-files", "--name-only"]).decode().splitlines()
for name in paths:
    with pathlib.Path(name).open("rb") as asset:
        if asset.read(43).startswith(b"version https://git-lfs.github.com/spec/v1"):
            raise SystemExit(f"Unmaterialized Git LFS asset: {name}")
print(f"Verified checkout of {len(paths)} Git LFS paths")
PY
