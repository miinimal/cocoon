#!/usr/bin/env python3
"""cocoon-launch: a missing TEE image file gives a clear error instead of a traceback (#27)."""
import importlib.machinery
import importlib.util
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
loader = importlib.machinery.SourceFileLoader("cocoon_launch", str(HERE.parent / "scripts" / "cocoon-launch"))
spec = importlib.util.spec_from_loader("cocoon_launch", loader)
cl = importlib.util.module_from_spec(spec)
loader.exec_module(cl)

failures = 0


def expect(name, ok):
    global failures
    print(("ok   " if ok else "FAIL ") + name)
    failures += 0 if ok else 1


d = pathlib.Path(tempfile.mkdtemp())
for f in ("OVMF.fd", "image.vmlinuz", "image.initrd", "image.img"):
    (d / f).write_text("x")

try:
    cl.check_image_dir(str(d), True)
    expect("missing image.cmdline exits", False)
except SystemExit as e:
    msg = str(e)
    expect("missing image.cmdline exits", True)
    expect("message names the missing file", "image.cmdline" in msg)
    expect("test mode explains how to build a test image", "build-image test" in msg)

try:
    cl.check_image_dir(str(d), False)
except SystemExit as e:
    expect("prod mode points to images/prod", "images/prod" in str(e))

(d / "image.cmdline").write_text("roothash=0")
try:
    cl.check_image_dir(str(d), True)
    expect("complete image dir passes", True)
except SystemExit:
    expect("complete image dir passes", False)

print("all tests passed" if not failures else f"{failures} test(s) FAILED")
sys.exit(1 if failures else 0)
