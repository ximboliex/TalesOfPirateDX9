# Mob runtime: first P0 implementation batch

Base: `5e9521409bab5a81226a5d5114b8e3d676a77ad1`.

Run from the repository root:

```sh
python3 tests/mob_runtime/run_tests.py
```

Requires Python 3 and g++ with C++14, AddressSanitizer and UndefinedBehaviorSanitizer.
The runner compiles the current production declarations and function bodies against
small deterministic doubles. It does not copy the implementations into the tests.
The doubles emulate entity resolution and map operations; they do not emulate the
full allocator, networking, Lua VM, collision grid or Windows server.

LeakSanitizer is disabled because the execution container prevents its `/proc`
thread inspection. Entity retirement is checked logically; allocator leak freedom
must also be verified in the real server.

## Implemented contracts

- Respawn deadlines use a 64-bit monotonic Windows clock from each death event.
  Compatibility policy: reappearance deadline = death + corpse delay + respawn
  delay. A negative corpse delay retains the corpse; a negative respawn delay
  retires the entity. Zero delays may complete both transitions in one update.
- A min-heap visits due respawns instead of scanning all pending entries. At most
  256 transitions execute in one update; overdue work remains pending.
- Temporaries are freed after corpse removal. Failed reinsertion cannot orphan
  an entity. Reset draining preserves permanent map population and rejects stale
  entity references. `Drop()` is a validated reset drain, not a destruction API.
- Internal life generations advance on pool initialization and permanent respawn.
  Pending actions, query results, threat records, combat/movement targets and
  deferred casts validate the relevant identity/generation and map instance.
- Actions execute in insertion order. Death, reset and reuse clear pending
  actions. Parameter decoding uses memcpy into aligned locals, validates lengths,
  and resolves entity targets by identity instead of queued raw pointers.
  This is not a combo sequencer or a skill-completion acknowledgement API.
- Harm/threat arithmetic uses signed 64-bit intermediates and saturates at the
  exact Lua-number integer limit (2^53-1). Negative threat clamps at zero.
  Invalid record indices return no result. Equal threat has a valid comparison.
- Spatial query consumers own candidate snapshots across callbacks. Candidates
  use exact center-in-shape tests; radius/body intersection is not the policy in
  this batch. Circle/rectangle intersection includes side intersections.
- IsPosValid checks destination terrain instead of the entity's current area.
  It is not a full obstacle, body-clearance or reachability test.
- Deferred casts carry launch skill level and source/map generations. They are
  rejected after source respawn, transfer, invalidation or map reset. Their
  existing 1-second polling interval is retained for the later timing phase.

## Coverage and limits

50 explicit scenarios across respawn, actions, threat, spatial queries and deferred
casts; 10,000 repeated temporary lifecycles and 78,141 circle point comparisons.
Full Windows/MSVC compilation and in-server integration/load testing remain required.
All new generation fields are internal; no packet layout has intentionally changed.
A complete rebuild is required because internal C++ structures changed.

## Remaining work, in dependency order

1. Validate this batch with the real Windows build and test-map scenarios. In
   particular, test death callbacks, area skills, instance closure and summons.
2. Finish lifecycle reference migration for pet-owner references and all periodic
   state sources; avoid presenting these as fully migrated.
3. Separate contribution/reward accounting from temporary threat. Keep the current
   five-entry limit until Lua consumers and EventCalculate's 25-person local array
   have been migrated together. This batch deliberately does not raise that limit.
4. Fix periodic-state elapsed-time narrowing and define dormancy catch-up policy;
   migrate remaining timer users and the deferred-skill scheduler.
5. Integrate bounded server navigation, collision/clearance checks, progress
   detection and controlled replanning. No pathfinding algorithm is integrated by
   this patch. Review cone/rectangle geometry before promising precise telegraphs.
6. Measure p95/p99 update cost, allocation/candidate counts, overdue events, path
   expansions and packets before tuning cadence or parallelizing work.
