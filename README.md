# SkyrimScape

An experimental SKSE plugin that turns Skyrim Special Edition into a RuneScape-style click adventure, with an elevated orbit camera, click-to-walk, interactions and hostile-target melee orders.

**Version 0.3.29. Windows x64 Skyrim runtime 1.7.104.0 only.** The plugin refuses other runtimes. This is a source repository, not a complete mod-manager-ready release.

Version 0.3.29 filters the verified WhiterunBraithLarsFightTrigger base 00092A6A from plugin ray queries. Exact walk goals with blocked nearest grid anchors try up to 16 nearby validated alternatives. Melee arrival uses 3D distance and a 64-unit height limit, with matching goal validation; walk waypoints cannot be consumed on a different height layer. Nearby pursuit refreshes a ground-profile/collision-validated approach every 100 ms, with A* retained for obstructed paths and explicit traversal preserved. Live stair pursuit remains to be tested.

Version 0.3.28 uses native Actor::GetAttackReach for melee/unarmed combat instead of a fixed 110-unit range. Strike distance is reduced by 15 percent (maximum 18 units); route goals stop a further 20 units inside that range. Chase prediction and incoming melee threat distance use weapon-aware ranges. Weapon/reach changes invalidate the combat route; logs record weapon, strike and approach distances. The native function was inspected in runtime 1.7.104. Bow/non-melee behavior remains unchanged. Actual hits still depend on native animation, target movement and collision.

Version 0.3.27 replaces one-frame normal attack taps with press/held/release input (up to 120 ms, below the configured power threshold). Only acknowledged native attack animations count toward power cadence. Missing animation acknowledgement triggers a bounded retry delay and diagnostic, rather than 180 ms input spam. Logs distinguish acknowledgement and release for live two-handed weapon testing.

Version 0.3.26 filters verified Skyrim.esm defaultSetStageTRIG volumes (base 00033F50) from plugin ray queries. The Whiterun obstruction 000C07F0 is C02KickerTrigger1, not a gate; local master data confirms its quest-trigger script. Native trigger events remain unchanged. Static interaction routes now finish within 24 units of their approach point and activate within 48, replacing the previous 80/130 stopping ranges. Failed native activation produces rejected-click feedback. Live gate validation remains required.

Version 0.3.25 retains picked surfaces for non-actor interactions instead of routing to large object pivots. Context menu entries retain each door hit position. Approach goals are projected onto nearby ground on the player-facing side; activation checks distance to that approach and a collision ray to the picked face. Actor pursuit is unchanged. Missing ground and blocked line of sight remain rejected; gate scripts and collision are not bypassed. Whiterun gate coverage needs a live test.

Version 0.3.24 adds a player-relative interior cutaway: unskinned lighting geometry entirely above player feet + 180 units is hidden while F5/F8 roof hiding is active. The threshold rises continuously with stair movement, preserving lower floors and revealing upper pieces as the player climbs. This is whole-piece culling, not per-triangle clipping: combined wall/ceiling pieces straddling the cutoff remain. Named camera-obstructing roof handling is retained outdoors. Right-click performs an all-hit collision ray query, deduplicates doors, orders them near-to-far and provides a separately targeted Open entry for each. Mouse wheel scrolls long context menus instead of zooming; selection revalidates the loaded door and world/cell. Only doors with collision intersecting the cursor ray are included. Live stair/building coverage and door transitions still need validation.

Version 0.3.23 gates HUD scene traversal across loading transitions. Suspending a scene immediately invalidates HUD/roof readiness; an input update overlapping a transition cannot republish its earlier active state. Combat text no longer searches camera nodes while inactive. The last observed door crash followed loading-menu closure, with no crash dump available; this closes an unsafe access path but still requires live transition verification.

Version 0.3.22 rejects F8 activation until a character has loaded, character creation and loading menus have closed, the player has 3D in an attached cell, normal movement controls are enabled and the camera is in first/third person. Rejected presses do not queue activation. Menu checks happen before accessing player scene data. F8 can still turn an already enabled mode off.

Version 0.3.21 removes game-reference/parent traversal and static-model lookups from the roof render hook after a reported indoor F8 crash. Detection uses only the current geometry name and bound diffuse texture name; the separately mutable texture set is no longer read. Roof hiding remains enabled during F8 and toggled with F5. Geometry must obstruct the player-to-camera segment above head height and be named roof/ceiling/rafter. No scene nodes or collision objects are changed. Hidden-roof click-through is temporarily removed until a safe game-thread mapping exists; roof collision can still block screen picks. Unnamed or combined building meshes may remain visible. No crash dump was available to prove the faulting function, so indoor F8 and door transitions still require live retesting.

Version 0.3.19 guards door/loading transitions. Before native door activation it suspends scene-dependent automation/render overrides and clears the displayed route/grid; loading-menu events keep scene work gated and add a short settling period after closing. Gameplay requires an attached player cell. Input rechecks gameplay after the native input handler and again after tick, so a door activation cannot leave the same update refreshing navigation against a transitioning scene. The reported crash ended immediately after door activation, but no crash dump was available to prove its faulting function; live transition testing is still required. F6 does not implement roof removal.

Version 0.3.18 adds arrow/bolt reactions during active attack orders. Native projectile hits from a hostile bow/crossbow user provide an eight-second threat window. Distance, health/stamina fractions and proximity to the current target select continued melee, chasing the shooter, or searching for cover. Cover search samples 16 nearby positions (96/192-unit rings), at most one every 50 ms. Candidates require supported walking clearance and four torso/head sightline checks blocked by terrain or static/tree geometry. Cover is revalidated every 150 ms against the shooter's current position, movement times out after three seconds, and cover holds last only 1.2–2.5 seconds. Healthy players may close on a shooter when no nearby cover is found; depleted players retain their current order. Cancellation/menus clear the response. No projectile-flight prediction, invulnerability, automatic idle retaliation or routing around obstacles to distant cover is implemented. Live bow/crossbow combat testing remains necessary. Mesh replacement remains disabled as in 0.3.17.

Version 0.3.17 disables runtime mesh replacement following visible stretched sheets/spikes during the 0.3.16 playtest. Texture coverage remains enabled, but F6 currently preserves original geometry. The exact engine layout or buffer-lifetime mismatch is not yet established; standalone simplifier tests did not validate integration with actual Skyrim draws. Mesh replacement must remain disabled until that integration is verified. The descriptions below of mesh simplification document the experimental implementation, not an enabled feature in 0.3.17.

Version 0.3.16 expands F6 texture coverage to grass, distant trees, effect materials and lighting glow maps. Palette/greyscale lookup textures are preserved. Compatible opaque environment-map, glow, parallax and HD LOD-object materials also become eligible for static mesh simplification and neutral tangent normals. Inactive alpha properties no longer exclude an opaque mesh. Float positions can be identified from a 16-byte position/UV layout as well as FULLPREC, and geometry counts exclude GPU allocation padding from simplification input. Bounded texture/mesh caches release old entries to admit new assets rather than permanently skipping them when full. Foliage silhouettes, skinning, shared terrain geometry, water and textures without usable mip chains remain outside mesh or texture conversion as appropriate. Expanded coverage requires live visual testing.

Version 0.3.15 addresses repeated `start-to-grid-blocked` rejections. Validated direct walking is tried before connecting to a grid centre. If the nearest start centre is obstructed, up to 16 nearby alternatives are tried with full footprint and collision checks. Initial nav-surface alignment allows 48 units rather than 32, while steps between samples remain limited to 24 units. Regression tests cover an obstructed nearest centre, modest physical-floor/navmesh height mismatch, and rejection outside the bounded height tolerance. Live verification at the reported stuck position remains necessary.

Version 0.3.14 replaces the ineffective runtime-style COM-vtable hook with three byte-verified triangle submission call sites in 1.7.104, covering ordinary and instanced indexed draws. The earlier diagnostic run reached input and lighting material hooks but never the indexed-draw hook. All call sites are checked before patching; a mismatch disables visual-hook installation and logs the reason. Calls preserve instance arguments and resolve the live driver/overlay interface when forwarding. F6 remains opt-in, and visual validation still requires an in-game test.

Version 0.3.13 makes player combat automation reassess an active attack order continuously. Native animation state gates attacks with a 180 ms input debounce instead of a fixed one-second light-attack wait. Incoming melee windups suppress new attacks; blocking retains stamina thresholds. When blocking is unavailable and the player is grounded and free of a committed animation, a short ordinary sidestep can use a footprint/clearance-checked walking segment. This grants no invulnerability and never forces a jump or cancels native animations.

Chasing samples target motion, predicts up to 0.45 seconds/120 units ahead, validates the predicted ground path at most every 100 ms, and considers a new route every 200 ms after 40 units of goal movement. Teleports, stale samples and vertical traversal discard the velocity estimate. Incremental searches keep their start and goal stable while the existing route continues; reaching actual attack range ends a pending chase. Hit events provide a 1.2-second reaction window for facing/defending against another hostile attacker. A hostile attacker within 200 units can replace a chase target farther than 220 units, except during jump/drop traversal. Walking, interaction, menus and cancellation do not trigger automatic retaliation. These changes automate the player, not every NPC's AI, and require live combat testing.

Version 0.3.12 adds an experimental **runtime Old School visual mode**, toggled with F6 during F8 gameplay (initially off). Diffuse textures on supported lighting-shader draws use existing mips nearest to a maximum 64-pixel edge and point filtering. Compatible default materials use a neutral tangent normal with zero specular alpha. World texture formats, colour space and alpha are preserved; no converted game files are created. This reduces detail but does not repaint Skyrim into a complete OSRS art style.

Eligible opaque, static, float-position triangle meshes are read back without waiting for the GPU, simplified by meshoptimizer on a background worker, then drawn with cached replacement index buffers. The target is 35% of original triangles with a 1.5% relative error limit and locked boundaries; the error and boundary constraints may prevent reaching that target. Original vertices, UV seams, collision, navigation, saves and archives remain intact. There is one pending mesh job, at most 20 submissions per second, up to 64 MiB of retained mesh buffers, and a conservative 256 MiB limit for retained texture resources. This is not a VRAM-saving texture pack: original resources remain loaded by Skyrim.

Coverage includes ordinary lighting materials, exposed landscape diffuse layers, grass, distant trees and effect source textures. Mesh simplification excludes skinning, dynamic meshes, transparency, shared terrain sections, nonzero draw offsets and unsupported layouts. Water shaders and some special materials are not transformed. F6 restores original drawing immediately; F8 off or paused gameplay also suspends the overrides. HUD/font rendering does not use these world passes. Runtime style logs report first texture override, additional shader categories and simplified mesh counts. WARP resource tests and simplifier tests are automated; visual coverage and performance in Skyrim still need playtesting.

Right-click opens a contextual action panel for the nearest picked target and the ground under that screen point. Actions include Walk Here, Attack, Talk (only for non-hostile actors that allow dialogue), Search on bodies, Open on doors/containers, Take on items, and Use/Activate on furniture/activators. Friendly actors without dialogue use Interact. Opening the menu stops the current order. Left-click selects; Escape, right-click again or an outside click dismisses without selecting. Middle-button orbit also closes it. Target handles are revalidated on selection, and Talk is cancelled if the actor dies or becomes hostile before arrival. Native activation still handles dialogue, locks, ownership and scripted interactions; the menu does not enumerate hidden objects behind the nearest target.

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
| Right-click | Stop the current order and open contextual actions; right-click again to dismiss |
| Hold middle mouse and move | Orbit and tilt |
| Scroll down / up | Zoom out / in |
| F7 | Toggle terrain overlay |
| F5 | Toggle camera-obstructing roof/ceiling hiding (on by default in F8) |
| F6 | Toggle experimental runtime Old School visuals (requires F8; initially off) |
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
