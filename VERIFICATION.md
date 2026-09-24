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
