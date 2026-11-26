# SPDX-FileCopyrightText: 2026 Blender Authors
# SPDX-License-Identifier: GPL-2.0-or-later
# Minimal stub for mesh-only build.


class _StubPanel:
    bl_options = set()
    bl_label = ""

    @classmethod
    def poll(cls, _context):
        return False

    def draw(self, _context):
        pass


def __getattr__(name):
    if name[:1].isupper():
        return _StubPanel
    return lambda *args, **kwargs: None
