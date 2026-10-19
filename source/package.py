from pathlib import Path
import re
from zipfile import ZIP_DEFLATED, ZipFile

release = Path(__file__).resolve().parent.parent
source_mod = release / "BabyProofDynamites"
archive = release / "Baby-Proof-Explosions-1.4.0.zip"

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
    readme = re.sub(r"!\[(?:\\.|[^\]\n])*\]\((?:\\.|[^()\n]|\([^()\n]*\))*\)", "", readme)
    readme = re.sub(r"!\[(?:\\.|[^\]\n])*\](?:\[[^\]\n]*\])?", "", readme)
    readme = re.sub(r"<picture\b[^>]*>.*?</picture\s*>", "", readme, flags=re.I | re.S)
    readme = re.sub(r"<img\b(?:[^>\"']|\"[^\"]*\"|'[^']*')*>", "", readme, flags=re.I)
    readme = re.sub(r"\n[ \t]*\n(?:[ \t]*\n)+", "\n\n", readme).lstrip("\r\n")
    package.writestr("README.md", readme)

print(f"Created {archive}")
