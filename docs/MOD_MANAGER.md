# Marathon managed mods

Marathon has a managed-folder mod manager built on its existing file-overlay loader. The manager does not reject mods based on platform folders: mods with `xenon/`, `win32/`, both, or other layouts can be enabled and are passed to the file-overlay loader. The startup notification counts detected managed mod folders, and the title-screen **Square** shortcut is hidden when none are found; **Options → Mods** remains available either way.

> **Compatibility note:** A mod showing as enabled means the manager is attempting to load it; it is not a test or guarantee that the mod works in this recompiled game. Some mods may be incompatible, may only affect paths used by a different platform or game version, may conflict with other mods, or may rely on features that are not implemented. In particular, archive merging is inactive, and PS5 registration of brand-new custom archives remains disabled.

## Install location

- **PS5:** `/data/MarathonRecomp/mods/`
- **Desktop:** the game's user-data directory, under `mods/`

Put each mod in its own immediate subfolder. The expected PS5 layout for the example mod is:

```text
/data/MarathonRecomp/mods/
└── anim rework/
    ├── mod.ini
    ├── xenon/archives/player_sonic.arc
    └── win32/archives/player_sonic.arc
```

Loose replacement files can mirror the game's asset paths directly. A root-level `mod.ini` is recognized as a managed mod; HMM-configured include folders and root-level `xenon/` or `win32/` trees are made available to the full-path resolver in load-priority order. The resolver checks exact paths first and accepts singular `archive/` as a fallback for `archives/` in both trees. HMM-style `[Main] IncludeDirCount` / `IncludeDirN` entries and UMM-style `[Details]` / `[Filesystem]` settings are read. In UMM metadata, `CustomFilesystem=True` is separate from `Merge`. On PS5, generated `system.arc` overlays are disabled pending validation against a retail archive, so a new `.arc` absent from the base `archive.pkg` is not registered there; direct file lookups from mod folders remain available. Non-PS5 builds prepare overlays under the user-data `modcache/custom-filesystem/` folder. The `#` filename marker is accepted for custom-filesystem mods and stripped for lookup, matching Sonic '06 Mod Manager's custom-file convention; custom archive names and package paths must be ASCII. `CustomFilesystem=False` does not register archives, even when `Merge=True`. The manager also reads optional `[Desc]` title, author, and description fields. The dedicated Mods window reads optional per-mod settings from an HMM-style `[Main] ConfigSchemaFile` entry; details are below.

Marathon scans those folders at startup and creates `mods.json` if it does not exist. Newly discovered folders are appended and enabled by default; a folder added while the game is running appears after the next launch. The manifest stores the manager order and enabled state:

```json
{
  "version": 1,
  "mods": [
    { "folder": "Example Mod", "enabled": true },
    { "folder": "Another Mod", "enabled": false }
  ]
}
```

Entries can also be plain folder-name strings, which default to enabled. The array order is the load priority: the first enabled mod wins when multiple mods replace the same asset.

## In-game controls

When at least one managed mod folder is present, press **Square** on the title screen (shown as the X face-button icon) to open the dedicated Mods window. Otherwise, open the manager through **Options → Mods**.

- **Up / Down:** select a mod or option.
- **A:** enable or disable the selected mod.
- **Left / Right:** change load priority; while editing options, cycle the selected value.
- **Square / X icon:** open the selected mod's options; in the options panel, change the selected value.
- **B:** go back or close the window. While the Mods window or its restart confirmation is active, B/Circle is reserved for that UI and cannot trigger the title task's back/quit behavior.

The title-screen window stages changes. On close, confirm to save the managed mod list and configuration files and restart (desktop) or close and relaunch the game (PS5); declining discards the staged changes. Mod changes are not hot-reloaded.

The existing **Options → Mods** page remains available for quick enable/disable and load-order adjustments. Those edits are saved to `mods.json` immediately and use the Options menu's restart prompt.

## Per-mod options

A mod can declare its configuration schema in `mod.ini`, for example:

```ini
[Main]
ConfigSchemaFile = "config.schema.json"
```

The schema file path and the schema's optional `IniFile` path are relative to the mod folder. The schema follows Unleashed Recompiled's `Groups` / `Elements` / `Enums` structure. Enum options use `Type` to refer to an array in `Enums`; Boolean/Bool options get On/Off choices automatically. Each element may provide `Name`, `DisplayName`, `Description`, `Type`, and `DefaultValue`; enum choices provide `Value`, `DisplayName`, and an optional `Description`. For example:

```json
{
  "IniFile": "mod.ini",
  "Groups": [
    {
      "Name": "Graphics",
      "DisplayName": "Graphics",
      "Elements": [
        { "Name": "EnableFeature", "DisplayName": "Feature", "Type": "Bool", "DefaultValue": true },
        { "Name": "Quality", "DisplayName": "Quality", "Type": "Quality", "DefaultValue": "high" }
      ]
    }
  ],
  "Enums": {
    "Quality": [
      { "Value": "low", "DisplayName": "Low" },
      { "Value": "high", "DisplayName": "High" }
    ]
  }
}
```

The menu writes changed values into the configured INI file while preserving unrelated entries. Options without choices are shown read-only.

Only managed-folder mods under the root are exposed in this window. UMM's `Merge` setting remains distinct from custom-filesystem archive registration; the game's archive-merge hooks are currently inactive, so merge behavior may not work. Non-PS5 builds register new custom archives through the generated `system.arc` overlay; PS5 overlay redirection is temporarily disabled pending validation against a retail archive. Whole-file lookups can use paths from either platform tree, but a mod being enabled does not guarantee that those files will be requested or work correctly in-game. External Xenia patch files are not loaded, and Marathon's built-in `Codes` toggles and legacy ModsDB INI behavior remain on their separate loader path.
