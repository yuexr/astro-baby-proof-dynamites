from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

release = Path(__file__).resolve().parent.parent
source_mod = release / "BabyProofDynamites"
archive = release / "Baby-Proof-Explosions-1.3.1.zip"

files = (
    "config.ini",
    "enabled.txt",
    "Scripts/main.lua",
    "Scripts/BabyProofDynamitesNative.dll",
)

with ZipFile(archive, "w", compression=ZIP_DEFLATED) as package:
    for name in files:
        package.write(source_mod / name, f"BabyProofDynamites/{name}")
    package.writestr("BabyProofDynamites/Scripts/BabyProofDynamites-events.txt", b"")
    readme = (release / "README.md").read_text(encoding="utf-8")
    lines = readme.splitlines(keepends=True)
    if lines and lines[0].startswith("![") and "./img/babyproof-banner.png" in lines[0]:
        readme = "".join(lines[1:]).lstrip("\r\n")
    package.writestr("README.md", readme)

print(f"Created {archive}")
