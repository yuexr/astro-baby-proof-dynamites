# Baby-Proof Dynamites 1.3.0

Makes dynamite, hydrazine and gas explosions tiny with negligible blast force and crater. Equipment is protected from the game's direct explosion-destruction checks. EXO caches and supported debris are still breakable and will release the intended objects.

Dynamite must be directly attached to the item intended to break.


## Install

- **(RECOMMENDED)** Install with [Nexus Mods Vortex](https://www.nexusmods.com/vortex), which automatically installs required extensions for Astroneer mods.
- Manual installation:
    1. Download the latest [UE4SS experimental .zip file](https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/experimental-latest) (scroll to the bottom under assets).
    2. In Steam, right-click Astroneer → Manage → Browse local files. Open `Astro\Binaries\Win64`.
    3. Extract the UE4SS .zip content into `Astro\Binaries\Win64`.
    4. Extract the mod's zip `Baby-Proof-Dynamites-#.#.#.zip` into `Astro\Binaries\Win64\ue4ss\Mods`.
    5. It should look something like this: `ASTRONEER\Astro\Binaries\Win64\ue4ss\Mods\BabyProofDynamites`.

The mod writes `BabyProofDynamites-status.txt` in its `Scripts` folder. Look for **ACTIVE**. If it says INACTIVE, the message explains why it couldn't load.


## Vehicles

Vehicles are protected by default; it can be toggled manually in config file or using the UE4SS in-game console.

**Manual toggle:** (requires a game restart)

Go to `\Astro\Binaries\Win64\ue4ss\Mods\BabyProofDynamites\config.ini`:
```ini
[Explosions]
AllowAttachedVehicleDestruction=false
```
- `false` (default): vehicles protected from all explosions
- `true`: vehicle destruction allowed

**In-game console toggle:** (works immediately in game)

Press F10 once to open the console input field or twice to see the full console.

Available commands:
```
babyproof vehicles on
babyproof vehicles off
babyproof vehicles status
```
`v` works as an alias to `vehicles`, as in `babyproof v on`. Changes apply immediately and are saved to `config.ini`.

> [!IMPORTANT]
> Please ensure the explosive is directly attached to the vehicle and not storage.
> Due to an asset indestructible flag, rail cars and rail engines must be packaged with a packager first to be destroyed (same as vanilla). Attach dynamite directly to package with vehicle destruction on.


## Something didn't work?

Send me `Scripts/BabyProofDynamites-events.txt` with what you tried, what you expected and what happened (screenshots optional but will help). The file is included and starts empty. The mod logs each check for explosion destruction.


---


Inspired by [Anti Grief Mod by Colson](https://atenfyr.com/ams-archive/mod?mod=WeakDynamiteMod.Colson).