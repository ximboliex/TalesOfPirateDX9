# Native world map and shared discovery — implementation v1

Base: `46e9b4fb647dfe8a158d11667657ddee68027a0a` (`codex/server-x64`).
This feature branch does not complete or certify the earlier x64 migration.

## Delivered behavior

| Point | Default visibility | Discovery | Display |
|---|---|---|---|
| Configured boss | Public | None | Big Map; minimap when within its viewport |
| Configured miniboss | Public | None | Big Map; minimap when within its viewport |
| Configured chest | Hidden | A living, focused player approaches a living, visible target within its configured radius | Big Map only |
| Static or dynamic portal | Hidden | A living, focused player approaches within 2,000 cm | Big Map only |

Discovery is shared by players in the **same map copy**, not across unrelated copies.
Public means known by default, not drawn outside the minimap's physical viewport.
Special map markers are separate from the legacy nearby-entity dots on the minimap.
An inactive known spawn stays on the Big Map in gray; a destroyed dynamic portal is removed.
There is no promised respawn countdown: the server source does not expose a reliable unified schedule for all scripted spawns.

Alt+W opens the native 740×520 map. It has category filters, a known-points list,
selection/details, native geometric icons, zoom, drag to pan, right click to use
existing navigation, and a center/follow-player button. Minimum target resolution
is 800×600. The form uses the current GUI scaling; it is not a new web interface.
Zoom preserves the selected center. Map bounds come from the loaded terrain.
A shared projection defines screen positions and mouse-to-world conversion in cm.
The existing exploration-mask database path is not used to authorize these points.
The previous hardcoded region/monster-area hover text is replaced by point details.

## Classification and deployment

Deploy a matching rebuilt client and GameServer together, plus:

- `Client/scripts/lua/forms/main.clu` and its regenerated `main.bin`;
- `server/GameServer/resource/map_poi_types.txt`;
- `server/GameServer/resource/map_poi_worlds.txt`;
- changed `resource/*/entry.lua` files, which remove early portal-coordinate notices.

`main.bin` is the engine's byte transform `(source_byte + 23) mod 256`, not Lua bytecode.
New packet commands are `CMD_CM_MAP_POI=125` and `CMD_MC_MAP_POI=625`.
No GateServer protocol translation or SQL schema change is required.
GameServer strips exactly its two Gate routing LONGs for this request before strict
17-byte payload validation. Other command parsers retain their previous behavior.

The type file format is `character_template_id kind radius_cm`.
Kinds: `1=boss`, `2=miniboss`, `3=chest`; radius range 100–5,000 cm.
Portal classification follows static/dynamic map-entry events and transit NPCs.
The seed contains **16 bosses** explicitly identified by scripts: Wandering Soul 679
(`BossDW`) and Abaddon 974–988 (`Abaddon.Boss`); **four minibosses 690–693**
(`BossFC`, explicitly called mini bosses by the Forsaken City 2 entry gate);
and scripted chest templates 694, 705, 709–717, 859–861.
This is a source-backed seed, **not a complete game catalog**. The deployment owner
must explicitly classify the remaining boss/miniboss templates; ordinary monsters
are never promoted based on level or name heuristics.
Restart GameServer after editing either catalog. Startup logs report loaded counts.

Only copy 0 of the whitelisted permanent worlds (initially garner, magicsea,
darkblue, winterland) saves static discoveries to
`resource/map_poi_discoveries/<map>.txt`. These runtime files are git-ignored.
The server writes a temporary file, flushes it and atomically replaces the prior file.
Writes are coalesced to the one-second service tick and retried after failures.
A hard crash can lose discoveries made since the last successful flush.
Back up this directory with other server state. Map-name path components are restricted
by the explicit whitelist. The loader reads at most 16,384 rows per world.
Other map copies forget discovery when closed. Retained entities are registered
again with a new synchronization epoch. Dynamic portal openings always start hidden,
even when a new opening reuses the same entity or location.
Persistence is local to the process/deployment; there is no cross-GameServer discovery bus.

## Connections and limits

- `SubMap::Enter/GoOut/Run/Close`: register, remove, update, reset.
- `CCharacter::OnBeginSeen`, `CItem::OnBeginSeen`, player movement and periodic requests:
  server-owned discovery checks. Clients do not submit discovered IDs or coordinates.
- `Entity`: separate point ID and owner; bindings also verify existing life-generation
  and pool-handle validation before observing/updating an entity.
- `MapPOIService`: map-copy registry, 50 m hidden-point buckets, shared discoveries,
  persistence, full/delta synchronization. Invalid bindings become inactive/removed.
- `MapPOI.h`: fixed-width big-endian bounded codec, portable registry and projection.
  Hidden records do not enter either snapshots or deltas.
- `CMiniMapMgr`: polls every two seconds, validates map/context, maintains one authorized
  point store, rebuilds the list only on version/filter changes, renders both maps.
- `CLargerMap`: clipped terrain rendering and cached tiles; no exploration-bitmap scan
  on every pan. Reopening, zooming and reinitializing release/invalidate their textures.

Registry limit: **512 total live/inactive/tombstone points per map copy**, not 512 per player.
Destroyed dynamic-portal tombstones are compacted at capacity; the epoch changes and
clients receive a fresh snapshot. If a map genuinely has more than 512 retained points,
extra registrations are rejected and logged; add paging/spatial subscriptions before
raising this limit substantially. Max point name: 48 bytes; max map name: 31 bytes.
The bounded response remains below 64 KiB including the existing packet/routing overhead.
Positions update at most once per second; clients normally see changes within two
seconds. This is a map presentation system, not the authoritative combat/AI position feed.
The service assumes the existing single GameServer game-loop ownership; calls from
new worker threads would require an ownership/queue design.

Static point keys hash map, kind of spawn identity, template/event, initial position,
and a collision slot; no pointer or temporary world ID is persisted. Moving a spawn's
configured position can change its discovery identity. Distinct same-template chests
at different positions are independent. Identical overlapping static spawns use slots;
their identity across restarts depends on deterministic spawn order.
Discovery uses radius and the game's existing visibility flags, not terrain ray casting.

## Validation and remaining integration work

Run:

```sh
python tests/world_map/run_tests.py
python tests/mob_runtime/run_tests.py
python tests/server_x64/run_tests.py
```

World-map tests compile the production shared header, the complete service using
explicit game/Win32 adapters, and actual client tile update/show methods using
texture/scene adapters. They run with AddressSanitizer and UndefinedBehaviorSanitizer.
Coverage includes hidden/public separation, sharing/isolation, visibility and life
checks, moving spatial buckets, persistence/restart, 1,200 portal openings, stale
request contexts, 20,000 projection round trips, 20,000 malformed-frame mutations,
all frame truncations, capacity, removal, reopen/zoom/bounds and balanced tile releases.
Static checks verify command-ID uniqueness, project inclusion and binary-form parity.
Lua syntax was checked separately with the available Lua 5.4 parser; that is not a
LuaJIT execution test. Existing mob-runtime and server-x64 portable suites also pass.

**Not validated here:** a full MSVC build/link, LuaJIT GUI execution, actual DX9 rendering,
packet transport through live GateServer, real Windows persistence errors/crash recovery,
or multiplayer tests against the supplied game data. The container has no Windows/DX9
runtime or complete server SDK dependency setup. Treat the branch as an implementation
for integration testing, not as a tested production binary.

Before deploying: build the matching client/GameServer in Windows; check Alt+W at
800×600, 1024×768 and 1920×1080; test zoom/pan/follow/reopen, same-map teleport and
boat control; use two clients to discover different chests in the same copy; verify
another copy stays hidden; close/reopen a copy; destroy/reopen a random portal at the
same coordinates; restart a permanent world; and verify the completed boss/miniboss
catalog against the actual deployment's CharacterInfo and spawn scripts.
