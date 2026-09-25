# SkyrimScape

An experimental SKSE plugin that turns Skyrim Special Edition into a RuneScape-style click adventure, with an elevated orbit camera, click-to-walk, interactions and hostile-target melee orders.

**Version 0.3.10. Windows x64 Skyrim runtime 1.7.104.0 only.** The plugin refuses other runtimes. This is a source repository, not a complete mod-manager-ready release.

Combat numbers and BLOCK use an anti-aliased black outline, approximately 1.5 HUD pixels thick, behind their existing coloured Segoe UI glyphs. Outline masks and rendered label shapes are cached.

Version 0.3.9 samples floor profiles every eight horizontal units for walking. Conservative explicit limits are 24-unit steps and 45-degree slopes; these are planner defaults, not values extracted from the native character controller. Collision segments follow the sampled floor, with lengths bounded to 32 horizontal units and step transitions kept separate. Routing, smoothing and look-ahead share this check. Small stairs remain walks; tall risers and unsupported gaps require separate validated traversal actions. Loaded navigation surfaces are still required, so arbitrary unmeshed geometry is not discovered by this change.

Version 0.3.8 preserves the active walk for ground clicks within 60 degrees of current travel. Replacement searches keep a stable origin while movement continues; completed routes require a clearance-checked join from the moving player. Failed replacements retain the old route. Movement stops at the old validated endpoint if the replacement is still pending. A bounded look-ahead skips walking waypoints only when terrain support and collision checks pass, and preserves jump/drop transitions. Unchanged grid geometry retains columns and cached footprint results. The native centre crosshair is hidden in F8 gameplay and its previous enabled setting is restored when leaving the mode.

## Features

- A-star routing on a 32-unit terrain grid, any-angle smoothing, running and character facing toward travel. Native navmesh provides surface heights; its triangle adjacency is no longer used for route search.
- Independent elevated camera with middle-button orbit and mouse-wheel zoom.
- Ground-click projection that preserves slopes and separate floor heights.
- Experimental jump/drop transitions between grid cells, with landing and clearance checks.
- Click-to-approach interactions and hostile-target melee input, with line-of-sight checks.
- Floating health-loss numbers over nearby actors (gold) and the player (red), plus blue BLOCK indicators from native blocked-hit events. Available in F8 mode independently of F7.
- Terrain grid overlay, route drawing, click diagnostics and offline route replay.

Completed walks, corrected facing, the camera and the terrain overlay have been observed in live playtests. **Combat, calculated drops, jump execution and all interaction cases remain unverified end to end.** See [VERIFICATION.md](VERIFICATION.md) for detailed evidence and remaining checks.

## Controls

| Input | Action |
| --- | --- |
| F8 | Enable/disable after loading a character |
| Left-click ground | Walk to the destination |
| Left-click hostile actor | Approach and issue melee input |
| Left-click eligible actor/object | Approach and activate |
| Right-click | Cancel the order |
| Hold middle mouse and move | Orbit and tilt |
| Scroll down / up | Zoom out / in |
| F7 | Toggle terrain overlay |
| WASD | Cancel the order and use native movement |

The mode starts disabled and resets on save loading. Menus and scripted control restrictions cancel orders.

The overlay is an **X-ray grid diagnostic**. Green squares show supported cells, not guaranteed connectivity. White shows the chosen route and destination. Foreground objects can still intercept clicks over green areas.

The grid caches 16x16-cell groups and rebuilds groups when their surface geometry signature changes. Height layers remain separate. Movement checks a 20-unit circular footprint, prevents diagonal corner cutting, and tests body-width collision rays before accepting route edges. Jumps/drops are inferred between suitable grid cells and retained as explicit actions during smoothing. The cache refreshes after two seconds, movement of 256 units, or a world/interior change. This new native integration still needs live playtesting.

Every click is logged before planning, including rejected clicks: sequence number, screen coordinates, collision hit/reference and normal, GRID acceptance/rejection reason, cache statistics, collision-check count, and the selected route points. The existing triangle CSV replay remains a tool for older recordings; new grid routes are recorded in the live text log.

Live 0.3.0 tracing identified the invisible FXcameraAttachEffectsACT volume intercepting terrain clicks and producing orders toward its unrelated object origin. Version 0.3.1 filters this confirmed effects form and the player out of ray results, selecting the nearest remaining hit. Other activators remain pickable. Later logs confirm that camera/self hits are skipped.

Version 0.3.2 smooths from the actual player position so the nearest grid anchor does not force an initial backward step. Actor/object routes search for a visible approach cell within 80 units rather than walking into the target collider. Failed chase plans retain the order and retry; clicking another target replaces the previous search.

Native A-star searches resume across input updates, with a soft 4 ms / 128-expansion budget per call. Grid refresh is deferred during a pending search. Initialization, an individual expansion and final smoothing can exceed the soft budget; slow slices are logged. Completed-plan logs include slice count and maximum slice time. The F7 renderer separates outlines into 256-cell Flash shapes and logs snapshot/draw timings and visible cell counts. These 0.3.2 changes pass portable regression tests and still need a live gameplay check.

## Build on Windows

Requirements: Git, CMake 3.24+, Visual Studio C++ tools and a Windows SDK. The native build was tested with Visual Studio 2026. Dependency revisions are pinned in [scripts/bootstrap.ps1](scripts/bootstrap.ps1).

From PowerShell in a fresh checkout:

    ./scripts/bootstrap.ps1
    $toolchain = Join-Path $PWD 'external/vcpkg/scripts/buildsystems/vcpkg.cmake'
    cmake -S . -B build/plugin -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain" -DVCPKG_TARGET_TRIPLET=x64-windows-static
    cmake --build build/plugin --config Release --parallel 8
    ctest --test-dir build/plugin -C Release --output-on-failure

Output: build/plugin/Release/SkyrimScape.dll. Dependencies stay in the ignored external directory. Bootstrap rejects mismatched existing dependency revisions instead of replacing them.

### Local core tests without Skyrim or the SKSE SDK

    cmake -S . -B build/tests -DSKYRIMSCAPE_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
    cmake --build build/tests --config Release
    ctest --test-dir build/tests -C Release --output-on-failure

These tests cover control mathematics, route containment, disconnected surfaces, directed terrain links and overlay connectivity/clipping. They do not validate native gameplay.

## Install a locally built plugin

Supply your own Skyrim installation, matching SKSE runtime and address library for **1.7.104.0**. Development uses SKSE 2.3.1 built from [upstream SKSE source](https://github.com/ianpatt/skse64), and a format-5 address library re-encoded from the published [address mapping](https://github.com/alandtse/skyrim_vr_address_library). Dependencies and game assets are excluded from this repository.

Close Skyrim, then run the installer with your game path:

    ./scripts/install.ps1 -Game 'D:\SteamLibrary\steamapps\common\Skyrim Special Edition'

The installer checks the game version and required runtime files, backs up an existing SkyrimScape DLL, refuses to install while Skyrim is running, and verifies the installed hash. Start skse64_loader.exe, load a character and press F8.

Install SkyrimScape.cmd uses the default path above. The advanced scripts/prepare-runtime.ps1 helper expects already-built SKSE binaries and the address CSV; it is **not a complete SKSE installer** and does not install SKSE Papyrus scripts. The native plugin currently does not use Papyrus.

## Known limitations

- Loaded navigation surfaces are required. Arbitrary unmeshed rock tops and automatic jump-up discovery remain incomplete. Dynamic obstacles can stall orders.
- Calculated drops are local to the player and require existing landing mesh. Their height bound uses a margin below the game's fall-damage setting, capped at 512 units. Actual fall behavior remains experimental.
- Attack orders submit right-hand melee input. Ranged aiming, spell charging and complete combat timing are unfinished.
- Camera obstruction handling, persistent world destination markers, target labels and a right-click action menu are unfinished.
- Green overlay cells indicate terrain support, not verified connectivity or click visibility through foreground objects.

## Diagnostics

Logs and accepted/rejected route CSVs are saved beside SKSE logs under Skyrim's Documents folder: My Games/Skyrim Special Edition/SKSE/.

    ./build/plugin/Release/SkyrimScapeRouteReplay.exe 'path/to/SkyrimScape-route-accepted.csv'

| Source | Purpose |
| --- | --- |
| src/plugin.cpp | Native input, camera, movement, actions and HUD |
| src/navigation.hpp | Portable pathfinding and surface geometry |
| src/terrain_links.hpp | Calculated boundary/drop connections |
| src/native_navigation.hpp | Loaded Skyrim navmesh integration |
| src/terrain_overlay.hpp | Connectivity classification and screen clipping |
| tests/ | Core regression tests |

## License and credits

Original SkyrimScape code is **GPL-3.0-or-later**; see [LICENSE](LICENSE). [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), SKSE and other dependencies retain their own licenses. No Bethesda executables, game assets, saves, local logs or runtime binaries are distributed here.

SkyrimScape is an independent experimental project, not affiliated with Bethesda or Jagex.

Combat numbers use observed health loss, including mitigation and other health-draining effects. Multiple hits between updates are combined; they are not attributed to a particular attacker. Healing does not produce damage numbers. Actors are sampled within 3000 units; newly observed actors establish a baseline first. BLOCK indicates a blocked hit, not a fabricated prevented-damage amount. Text rendering and event timing still require a live playtest.

Version 0.3.5 draws combat numbers, BLOCK and the grid legend with original vector glyphs, removing the external/shared-font dependency. Ground clicks on steep or downward-facing geometry continue along the screen ray to an upward-facing surface. Ground projection is limited to 64 units downward, preventing distant overhead collisions from projecting onto the road behind the player. Rejected immediate walk requests preserve the previous walk; clear supported straight routes bypass A-star. Obstructed routes may still pause while planning.


Version 0.3.6 uses the installed Windows Segoe UI font at semibold weight. GDI supplies anti-aliased glyph coverage, cached and drawn in the HUD without depending on Flash font loading. Original vector glyphs remain a fallback if Windows font extraction fails; no Windows font files are distributed.

While an attack order is active, automatic defense observes the selected target's melee attack phase, facing, distance (under 180 units) and line of sight. A shield or a block-capable melee weapon with no separate offhand weapon/spell is required. Blocking starts above max(20 stamina, 20% of permanent maximum), can continue down to max(10, 10%), and releases when the threat ends, stamina is low or a 1.2-second hold expires. Cancel, menus and target changes release held controls. It does not guarantee every incoming hit is blocked.

After at least three light attack attempts, a melee power attack may be attempted when no incoming attack is detected, at least max(60 stamina, 60% of permanent maximum) remains and the six-second power cooldown has elapsed. It uses native held attack input and the game's power-attack delay. Skyrim determines actual stamina costs and whether the animation succeeds; the plugin does not refill stamina or force damage. AUTO BLOCK and AUTO POWER decisions are logged. Live combat timing still needs playtesting.

Version 0.3.7 fixes a load-time HUD crash in 0.3.6: GDI returns no pixel data for spaces despite a nonzero bounding box. Empty glyphs now retain spacing without attempting to draw pixels; bitmap dimensions are checked against buffer size.
