# Verification

## Live evidence

The 0.1 runtime log confirmed SKSE loaded the DLL, installed its input/camera/HUD hooks, received F8 toggles, and recorded ground, interaction and hostile-actor click orders. An order being recorded does not prove that walking, activation or combat completed.

The user reported that clicking can cause backwards walking. This remains an open live regression until version 0.2 is tested. The user requested RuneScape-style click feedback, zoom, proper routing, fast travel on foot and jumping over terrain.

## Version 0.2 checks

- Release x64 DLL builds with the expected SKSE exports.
- Control tests cover camera basis/focus, zoom limits/direction/release handling, camera-relative direction roundtrips, height-aware arrival, click-pulse lifetime, and bounded jump landing distances/heights.
- Navigation tests cover a wall detour, sampled route-segment containment after smoothing, disconnected areas/floors, same-triangle travel, corrupt portal links, and non-walkable degenerate/vertical triangles.
- Offline disassembly of the installed 1.7.104 executable confirms ThirdPersonState slots 4/5 read the rotation/translation fields, PlayerControls slot 1 processes the input data, and MovementHandler's forward event writes positive Y input. These are static observations; they do not prove native movement/camera integration is correct at runtime.

## User playtest still needed

Live 0.3.0 logging captured 162 click rays at the recorded checkpoint; 56 hit reference 000B98DA. Reading Skyrim.esm identified its base 0002F245 as ACTI editor ID FXcameraAttachEffectsACT. Those hits changed around the screen but generated a fixed interaction destination at 14626.9, -49668.2, 710.8, explaining the repeated no-destination-cell failures. Other grid orders completed, including a logged traversal landing. 0.3.1 filters that specific confirmed effects form and the player using a Havok all-hit fallback after an ignored nearest hit. Build and three core suites pass, but multi-hit runtime behavior remains pending installation/restart and live verification. This finding does not explain every start-clearance or disconnected-grid failure.

0.3.0 replaces triangle-graph path search with a 32-unit multi-height grid. Tests cover routing without triangle adjacency, smooth open-floor paths, reuse/invalidation of 16x16-cell groups, circular clearance rejecting narrow corridors, directed calculated drops, physics vetoes and separate stacked floors. The recorded mesh benchmark generated 26,591 cells in 131 groups in 12.37 ms, planned in 3.57 ms with a permissive geometry-only validator, and refreshed in 2.89 ms while reusing all 131 groups. Those numbers exclude native collision checks and HUD rendering. Native performance, picking, routes, jumps and drops still need live validation. Every click now logs its input/ray hit before acceptance or rejection; route waypoints and grid reasons are logged explicitly.

0.2.7 addresses reported poor routes by removing centroid waypoints and centroid-based travel costs, extending bounded corridor smoothing, adding a traversal action cost, and rejecting ground clicks without a valid vertical surface projection. A new broad-triangle regression requires the resulting walking route to match straight-line length even when the journey exceeds one smoothing segment. Existing wall containment and directed drop tests must still pass. The HUD now draws the active route and destination in white; click logs distinguish the original collision hit from the selected endpoint. These are targeted corrections, not proof that all click-selection or routing issues are resolved.

0.2.6 addresses the user's screenshot feedback with generated directed drop links, 48-unit boundary samples, spatially indexed lower-surface lookup, physics validation and explicit overlay arrows. Regression tests prove that a small landing away from a broad edge's midpoint can connect, A-star uses the calculated link, the link is not traversable uphill, blocked validation prevents insertion, and excessive drops are rejected. A geometry-only benchmark on the latest live accepted-route snapshot found 96 candidate drops in 1.94 ms; this does not include physics or prove any candidate is valid in game. Native rendering of the legend/arrows, actual fall execution, fall damage and runtime generation cost still need live validation.

0.2.5 adds the requested terrain overlay using the validated planner mesh. Pure tests verify walking versus jump-link connectivity, disconnected/corrupt portals, and viewport polygon clipping. HUD rendering, legend positioning, alignment with terrain, and rendering cost still require a native visual playtest; compilation cannot establish those. Green indicates graph connectivity, not collision-ray visibility through foreground objects. Overlay data is refreshed every two seconds.

0.2.4 adds vertical surface normalization and native ledge-link traversal. New synthetic tests cover bounded jump-up routes, directed drops, smoothing preservation of takeoff/landing pairs, rejecting tall jumps and deep drops, and normalization without distant lateral movement or snapping to overhead floors. The captured live ordinary route still replays with matching waypoints and contained walking segments. This is offline evidence only: native ledge ABI interpretation, collision probes, actual jumping/landing and drop behavior remain unverified. The user's requested arbitrary terrain traversal is not complete merely because authored-link tests pass.

0.2.2 live logs show actor heading matching desired route headings modulo 2pi, forward input, and completed walks. Route planning with the asynchronous recorder measured about 1.3–1.7 ms in the observed session. This supports the facing/travel correction, not completion of combat or terrain traversal.

0.2.3 adds native line-of-sight gating for attacks/activation, accepts already-reachable action targets without forcing a navmesh route, resets the movement-stall timer while acting, and discards old chase routes at melee arrival so departing targets get a fresh plan. It logs activation return values and target health at melee input submission. These are staged changes requiring native runtime validation; health observations do not by themselves attribute damage to the player's attack.

The user reported 0.2.1 was better, then explicitly requested reversing the removal of actor facing. Version 0.2.2 restores route-facing and synchronizes native movement yaw for forward travel. Orbit rendering remains independent. Full diagnostic CSV writes now run on a worker owning plain data only. Replaying the captured rejected route reproduced rejection: the nearest walkable point was 536.3 units from the clicked collision point, beyond the 160-unit endpoint limit. This explains that captured rejection, not every reported pathfinding failure.

The 0.2 session started on 2026-09-25 at 08:00:34. Live logs record wheel distance changes, route planning around 1.2–1.5 ms, changing player coordinates along route legs, and walk arrivals at 08:01:08 and 08:01:14. Actor heading remained 5.96 radians through multiple differently directed legs, then changed at arrival. This demonstrates travel and some arrivals, but exposes missing facing during travel. Several clicks reported no connected route; their cause is not yet diagnosed. No successful jump or attack is proven by this trace.

The user confirmed travel direction was fine, but reported broken pathfinding, spurious jumps, no visible wheel zoom, and an oversized marker. Version 0.2.1 preserves the original camera-relative steering and disables automatic jumping. It handles small waypoint overshoots, brakes on final approach, clears both movement vectors on arrival, and restores running. Marker radius is reduced to 3–8 HUD units. Offline disassembly at RVA 0x8fd590 confirms ThirdPersonState::Update copies translation fields directly to the camera node without calling GetTranslation. A new slot-3 post-update hook applies the orbit transform to that node; zoom limits are 90–4000. Route rejection logs distinguish missing endpoints from disconnected routes; accepted and rejected CSV snapshots preserve exact mesh and waypoint data for reproduction. These corrections require live validation.

1. Verify zoom directions and the gold/red click pulse.
2. Click destinations around the player after different camera rotations. Check both travel direction and facing/animation.
3. Travel around a wall, over slopes, and across an exterior-cell boundary; inspect route planning time and arrival records in the log.
4. Test low-obstacle jumping, obstacles too tall to jump, headroom and landing restrictions.
5. Verify native melee attacks actually hit, target chasing, interaction, menus and save/load handoff.

The assistant may launch Skyrim when requested but must not operate the user's mouse or keyboard. Never replace the installed DLL during a running Skyrim process.


## 0.3.2 click, combat and spike investigation

The 0.3.1 playtest log at 12:35 contains repeated `grid-to-destination-blocked` rejections for moving actors, followed by cancellation of their orders. At 12:35:18.253 click 68 starts a search that finishes at 12:35:19.942 with 14,708 expansions and 113,491 collision checks (about 1.69 seconds). Accepted ground routes also contain a first grid anchor behind the actual starting position.

Changes: smooth with the actual start included; search for an 80-unit target approach region with visibility checks; retain action orders through failed/pending chase plans; resume A-star state across input updates with a soft 4 ms / 128-expansion slice; invalidate dynamic edge results for each new search; split F7 outlines into 256-cell shapes and add drawing/snapshot diagnostics. The old log does not establish the exact cause of intermittent F7 disappearance, so the rendering change remains a candidate fix pending user playtest.

Regression coverage adds backward-anchor avoidance, occupied-target approach, target visibility, incremental search completion, pending-search replacement and dynamic collision invalidation. Native gameplay and overlay behavior for this build remain unverified until the user plays it.


## 0.3.3 combat text

Added a bounded pool of 48 floating labels, using nearby actor health deltas and TESHitEvent kHitBlocked. Gold/red distinguish other actors/player, cyan marks BLOCK. Text fades after 1.25 seconds and clears when leaving gameplay or loading a save. No attack-input damage estimates or invented blocked amounts. Native build succeeds; existing regression suites remain required. Visual placement, font availability, blocked-event timing and displayed health deltas need user gameplay validation.


## 0.3.4 shared HUD fonts

User reported missing fonts after combat testing. The 0.3.3 log confirms BLOCK and damage events for both player and enemy, so event generation is functioning. Custom labels requested device Arial/_sans with embedFonts=false. Changed combat labels to $EverywhereMediumFont and the grid legend to $EverywhereFont with embedFonts=true, using Skyrim shared font mappings (also documented in SkyUI build/fontconfig.txt). Removed the unneeded bold style request to avoid requiring a separate glyph face. No font archives or global font configuration replaced. Visual confirmation remains pending.


## 0.3.5 click-ray correction and font-independent labels

Clicks 28, 32 and 35 in the 0.3.4 session hit geometry with negative normalZ, then projected 291-399 units downward to road points behind the player. Ground-only continuation now selects an upward-facing hit on the same camera ray. Actor and interaction targets retain normal picking. Downward ground projection is restricted to 64 units. Added a physically checked, radius-supported direct-walk fast path; immediate rejected ground clicks restore the prior walk.

User confirmed mapped font labels still failed. All custom text now uses original 5x7 vector glyphs rendered with the existing working GFx drawing API. Combat geometry is cached per displayed text/color. Native build and all three regression suites pass; live appearance and picking still require user validation.


## 0.3.6 Windows font and stamina-aware combat

Added Windows Segoe UI semibold glyph extraction using GDI gray coverage and cached HUD geometry. A native test verifies this machine resolves Segoe UI, provides all required glyphs with nonzero advances, and returns anti-aliased coverage. Added combat policy tests for defense thresholds/hysteresis, no-threat/equipment gating, heavy-attack cadence/cooldown, threat priority and stamina reserve. All five suites pass. Native control integration compiles; no automated gameplay input was used. User validation remains necessary for block/power animation execution, stamina consumption and font appearance.


## 0.3.9 ramp and step walking

Replaced cell-centre height classification and straight interpolated height checks with sampled ground profiles. Walking allows 24-unit step changes and slopes up to 45 degrees, with eight-unit floor samples and physical checks along the floor. New regressions cover walking upstairs/downstairs without jumps, tall-riser rejection, walkable ramps, steep-slope rejection and hill crests. All five suites pass. Limits are conservative planner defaults; the SDK does not expose a confirmed usable step-height field in the common character-controller layout. Native stair/ramp behavior and performance require user playtesting.

## 0.3.8 continuous route replacement

Forward clicks preserve active movement while a replacement search runs. New tests cover forward/reverse click classification, joining ahead of a moving start, preserving jump takeoffs and rejecting blocked joins. All five suites pass. Unchanged grid geometry now retains cached footprint results. HUD source inspection confirmed SetCrosshairEnabled and bCrosshairEnabled; the plugin saves/restores that setting on the HUD movie. In-game smoothness, timing improvements and crosshair behavior still need user validation.

## 0.3.7 blank system-font glyph crash

User reported a crash on save load in 0.3.6. No matching SkyrimSE crash dump was available. Native font probing reproduced a definite out-of-bounds read: Segoe UI space has a GDI 1x1 bounding box, stride 4, but zero bitmap bytes. The HUD legend drawn on load indexed that empty vector. Blank glyph dimensions are now normalized to zero while preserving advance; drawing also explicitly skips empty buffers. Nonempty bitmap dimensions are validated against byte size. The system-font regression now exercises renderer row/column addresses for the complete legend and verifies space semantics. Live save-load confirmation remains pending.
