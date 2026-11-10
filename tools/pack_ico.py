import struct
import sys
from pathlib import Path

root = Path(sys.argv[1])
sizes = [256, 128, 64, 48, 32, 24, 16]
images = []
for s in sizes:
    p = root / f"icon-{s}.png"
    images.append((s, p.read_bytes()))

header = struct.pack("<HHH", 0, 1, len(images))
offset = 6 + 16 * len(images)
entries = b""
payload = b""
for s, data in images:
    entries += struct.pack("<BBBBHHII", 0 if s == 256 else s, 0 if s == 256 else s, 0, 0, 1, 32,
                           len(data), offset)
    payload += data
    offset += len(data)

(root / "icon.ico").write_bytes(header + entries + payload)
print("wrote", root / "icon.ico", len(header + entries + payload), "bytes")
