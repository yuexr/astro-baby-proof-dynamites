from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

release = Path(__file__).resolve().parent.parent
source_mod = release / "BabyProofDynamites"
archive = release / "dist" / "Baby-Proof-Dynamites-1.3.0.zip"
archive.parent.mkdir(exist_ok=True)

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
    package.write(release / "README.md", "README.md")

print(f"Created {archive}")
