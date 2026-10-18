"""Inventory original assets without decompressing or modifying game files."""
import csv
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "data" / "game"
OUT = ROOT / "data" / "textures"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    records = []
    for path in sorted(GAME.rglob("*")):
        if not path.is_file():
            continue
        content = path.read_bytes()
        magic = content[:4].hex().upper()
        if content.startswith(b"\x89PNG\r\n\x1a\n"):
            storage = "PNG image; directly editable"
            target = OUT / "original" / "system" / path.relative_to(GAME)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        elif magic == "0FF512ED":
            storage = "XMem LZX TD wrapper; inner data not decoded"
        elif content[:4] in (b"FArc", b"FArC", b"FARC"):
            storage = "FARC archive signature; entries not extracted"
        elif content.startswith(b"AFS2"):
            storage = "AFS2 audio bank; encoded entries not extracted"
        elif content.startswith(b"@UTF"):
            storage = "CRI UTF table/container; entries not extracted"
        else:
            storage = "Not decoded; extension is a hint only"
        records.append(dict(path=path.relative_to(GAME).as_posix(),
                            bytes=len(content), signature=magic,
                            sha256=hashlib.sha256(content).hexdigest(), storage=storage))
    (OUT / "asset_inventory.json").write_text(json.dumps(records, indent=2), encoding="utf-8")
    with (OUT / "asset_inventory.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    print(f"Inventoried {len(records)} files. Exported existing PNGs only.")
    print("Packed arena backgrounds and replacement loading are not implemented.")


if __name__ == "__main__":
    main()
