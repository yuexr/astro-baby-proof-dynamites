# Baby-Proof Dynamites 1.2.2

Makes dynamite, hydrazine and gas explosions tiny with negligible blast force and crater. Equipment is protected from the game's direct explosion-destruction checks. EXO caches and supported debris are still breakable and will release the intended objects.

Dynamite must be directly attached to the item intended to break.


## Install

Requires UE4SS. Put the `BabyProofDynamites` folder in `Astro\Binaries\Win64\ue4ss\Mods`.

The mod writes `BabyProofDynamites-status.txt` in its `Scripts` folder. Look for **ACTIVE**. If it says INACTIVE, the message explains why it couldn't load.


## Vehicles

Vehicles are protected by default (i.e. railers, rovers, shuttles, VTOLs and rail vehicles).
To toggle this setting, go to `config.ini`:

```ini
[Explosions]
AllowAttachedVehicleDestruction=true
```
false (default): vehicles protected from all explosions
true: vehicles destruction allowed

Please ensure the explosive is directly attached to the vehicle and not storage.


## Something didn't work?

Send me `Scripts/BabyProofDynamites-events.txt` with what you tried, what you expected and what happened (screenshots optional but will help). The file is included and starts empty. The mod adds a line whenever it checks whether an explosion can destroy an object.
