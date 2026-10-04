# SkyScape 1.2.0 to Skyrim Special Edition tools

These are the source tools used for the local SkyScape 1.2.0 port. They are specific to that release, not a universal Skyrim mod converter. They preserve form IDs, transform known LE record layouts, convert meshes through nifly, repair known texture paths, stage compatible scripts and audit asset references. This is a schema-based conversion, **not a Creation Kit resave**. Tutorial progression and every gameplay script have not been validated; the reported log/fire/fishing-net progression issue remains unresolved. SkyUI is still required.

No SkyScape, SkyUI, Skyrim, SKSE compiled scripts or converted assets are included. Obtain those separately under their respective terms. Do not publish the generated mod without the original authors' permission.

## Prerequisites and inputs

Use Windows, Python 3.10+, CMake 3.24+, a C++17 compiler, and `python -m pip install lz4`. Do not run Python with `-O`: conversion invariants use assertions.

From the repository root:

```powershell
git clone https://github.com/ousnius/nifly.git external/nifly
git -C external/nifly checkout cca0a770094bb962fb28ea1fec5ea903e68fda8e
cmake -S tools/skyscape-port -B build/skyscape-port-public
cmake --build build/skyscape-port-public --config Release
```

Populate the ignored `external/skyscape-port` directory:

- `original/`: extract the original SkyScape v1.2.0 Runecrafting Edition archive here, with `SkyScape.esm`, `meshes/`, `textures/`, `scripts/` directly inside. If assets are in a BSA, extract them first using `inspect_port.bsa(Path(archive), Path(destination))`.
- `skyui/`: put the separately downloaded SE SkyUI 6.11 `SkyUI_SE.esp` and `SkyUI_SE.bsa` here.
- `skse-script-reference/skse64_2_02_06/Data/Scripts/`: extracted official SKSE 2.2.6 compiled scripts and `Source/`.
- `external/skse64/scripts/modified/` (relative to repository): the 62 SKSE 2.3.1 source additions used for comparison.
- `external/asset-tools/texconv.exe`: Microsoft's DirectXTex texconv. It converts three unsupported DDS formats to SE-compatible formats.

The SKSE finalizer is intentionally version-specific: it checks source equivalence and updates only the verified SKSE release constant from 72 to 75 in its PEX. It does not install or replace SKSE native DLLs. Install the correct native SKSE for your game separately. This setup was used with Skyrim 1.7.104.0 and SKSE 2.3.1.

## Convert and audit

Set paths before running any commands (especially if Documents is redirected):

```powershell
$env:SKYSCAPE_GAME = 'D:\SteamLibrary\steamapps\common\Skyrim Special Edition'
$env:SKYSCAPE_DOCUMENTS = [Environment]::GetFolderPath('MyDocuments')
python tools/skyscape-port/prepare.py
build/skyscape-port-public/Release/port_meshes.exe external/skyscape-port/original/meshes external/skyscape-port/staged/meshes tools/skyscape-port/texture-fixes.tsv
python tools/skyscape-port/finalize_assets.py
python tools/skyscape-port/audit_assets.py
```

Start from a fresh staging directory when changing source versions; staging does not remove stale files. The audit writes `asset-audit.json`; both missing lists must be empty before installation. `conversion-report.json` records conversion details. Mesh conversion writes `staged/mesh-textures.tsv`, validates saved SSE versions and shape counts, and returns nonzero on failures. Stop if any command fails.

Optional environment overrides: `SKYSCAPE_PORT_ROOT` (working directory), `SKYSCAPE_SKSE_SOURCE` (modified PSC directory), and `SKYSCAPE_TEXCONV` (executable). See `port_config.py`. Keep these paths outside tracked source. CMake accepts `-DNIFLY_SOURCE_DIR=...`.

## Install and undo

Exit Skyrim yourself before installation. Install SkyrimScape first; the port installer requires and verifies that its DLL remains unchanged.

```powershell
python tools/skyscape-port/install_port.py
```

The installer copies staged assets, enables SkyScape and SkyUI in the current user's plugin lists, enables loose assets and Papyrus logging, and records file hashes and previous copies under `backups/skyscape-se-*`. It rejects a running Skyrim process. It does not manage Mod Organizer profiles. Use a separate new character; the original entry route is System > Mod Configuration > SkyScape > Teleport.

```powershell
python tools/skyscape-port/rollback.py          # verify only
python tools/skyscape-port/rollback.py --apply  # restore after exiting Skyrim
```

Rollback uses the same configured paths, validates destination boundaries and installed hashes, and refuses files changed since installation. Keep the backup and `last-install.txt`. Installation is not transactional; if interrupted, inspect the manifest before recovering. The tools do not change saves or fix tutorial scripts.
