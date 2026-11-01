# Vitals HUD

Source code of Vitals HUD, a Skyrim SE/AE SKSE plugin that draws one compact HUD panel for the hidden numbers of the
player character - wetness, arousal, dirt, warmth, hunger, fatigue, cycle and cum craving - plus SL Survival's status,
its message panel and the diseases the player carries. Drawn with SKSE Menu Framework: no SkyUI widgets, no Papyrus
polling loop, no plugin file.

The released mod (DLL, stage pictures, disease icons, fonts, installer) is distributed separately; this repository
holds the code that builds it.

## How it works

- `Meters` reads every source natively four times a second on the main thread: WetFunction NG's
  `WFNG_GetPlayerGauge` export, SexLab Aroused NG's `SLA_GetArousalInt` or OSL Aroused's `GetArousalExt` exports
  (with the `sla_Arousal` faction as fallback), and the globals of Bathing in Skyrim, Creation Club Survival Mode,
  Beeing Female and SL Survival. A meter whose mod is missing never appears. `Trend` turns the readings into the
  calm trend arrows (one, two or three; a burst for sudden jumps).
- `Sls` asks SexLab Survival Framework's `SLSFW_GetHudStatus` export for licences, toll, curfew, curses and orgasm
  fatigue (`SLSFW_HudStatus.h` is the shared struct). `Feed` claims SL Survival's notifications through
  `SLSFW_ClaimMessages` / `SLSFW_GetMessages` and shows them in their own panel.
- `Diseases` scans the player's active effects and maps disease spells to icons using Disease Widget's definition
  format (`Data/SKSE/Plugins/VitalsHUD/Diseases.json`, then every `.json` in `VitalsHUD/Diseases` and
  `DiseaseWidget`); `DefaultDiseases.h` holds the built-in definitions.
- `Hud` draws the panel as an SKSE Menu Framework HUD element (compact tiles or detailed rows, SL Survival block,
  disease block, drag-to-move preview while the menu is open). `Typo` draws all text with the chosen font, weight
  and outline. `Input` toggles the detailed panel with the expand key.
- `Menu` builds the SKSE Menu Framework pages (Panel, Text, Meters, Diseases, SL Survival messages); `Settings` reads
  and writes `VitalsHUD.ini` (the defaults are in `data/`).

## Building

Requires [xmake](https://xmake.io) 3.x and Visual Studio 2022, plus two libraries placed in `lib/` (not included):

- [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng) (alandtse's `ng` branch) at
  `lib/CommonLibSSE-NG`
- [SKSE-Menu-Framework-3-API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API) at
  `lib/SKSE-Menu-Framework-3-API`

```
xmake f -m releasedbg --skyrim_vr=n
xmake build VitalsHUD
```

Before building, apply `commonlib-unk8c.patch` inside `lib/CommonLibSSE-NG` (`git apply ../../commonlib-unk8c.patch`).
Older revisions of the `ng` branch declare an extra `TESObjectREFR` virtual that Skyrim SE/AE do not have, so in an
SE/AE-only build every later virtual (`IsDead`, `IsInCombat`, ...) is called one slot too far.

`data/` holds the default `VitalsHUD.ini` and `Diseases.json` that ship next to the DLL. `tools/slice_sheets.py`
cuts the stage-picture sprite sheets into the icons the DLL loads (`Interface/VitalsHUD/<icon>.png`);
`tools/icons.py` is the older generator of vector stage icons.

## Credits

- `src/Diseases.cpp`, `src/Diseases.h` and `src/DefaultDiseases.h` are ported from **Disease Widget - SKSE** by
  **kuroishitam** and included with the author's permission (merging several spells into one icon: Julio005).
- Thiago099 and contributors - SKSE Menu Framework.
- CommonLibSSE-NG and its contributors; the SKSE team.

## License

MIT, see `LICENSE`.
