# Card Expand Foundation Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Harden card-expand initialization and SDK contracts while preserving all existing TH18 card behavior.

**Architecture:** Keep the current single dispatcher DLL and `CE_CARD` source model. Move testable policy into platform-independent C helpers, make runtime setup consume those helpers, and introduce a public SDK header as a compatibility-preserving boundary around the TH18 implementation.

**Tech Stack:** C11, i686 MinGW GCC, GNU Make, Python 3, pytest, thcrap JSON/codecaves.

**Spec:** `docs/superpowers/specs/2026-09-07-card-expand-foundation-hardening-design.md`

## Global Constraints

- Target remains TH18 v1.00a; patch addresses and ABI constants do not change.
- Existing card IDs, values, assets, hooks, and gameplay behavior remain unchanged.
- Existing `native/cards/*.c` sources must continue to compile through `#include "sdk.h"`.
- Existing user changes, especially `patch/files.js`, must not be reverted.
- Each behavior change follows red-green-refactor and finishes with its focused test plus the broader host suite.

---

### Task 1: Behavior Registry Errors and O(1) Lookup

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/native/sdk_core.h`
- Modify: `mods/th18.v1.00a/card-expand/native/sdk_core.c`
- Modify: `mods/th18.v1.00a/card-expand/native/tests/test_sdk_core.c`

**Interfaces:**
- Produces: `int ce_sdk_registration_error(uint32_t *id, int *reason)` and direct `ce_sdk_find(uint32_t id)` lookup.
- Reason values: duplicate ID, out-of-range ID, or registry capacity.

- [ ] Add tests proving IDs below 58 and above 254 are rejected, a duplicate leaves a persistent error, reset clears it, and valid lookup returns the exact behavior pointer.
- [ ] Run `make test-host`; confirm the new assertions fail because range/error behavior is absent.
- [ ] Replace the linear-only registry with `behavior_by_id[255]`, retain ordered iteration for logs, and record the first registration error.
- [ ] Run `make test-host`; confirm all registry and existing tests pass.

### Task 2: SDK Cross-Layer Contract Validation

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/native/sdk_core.h`
- Modify: `mods/th18.v1.00a/card-expand/native/sdk_core.c`
- Modify: `mods/th18.v1.00a/card-expand/native/sdk.c`
- Modify: `mods/th18.v1.00a/card-expand/native/tests/test_sdk_core.c`

**Interfaces:**
- Produces: `ce_sdk_contract_check(json_ids, categories, count, bad_id, reason)`.
- Consumes: behavior hook metadata through an `is_active` field stored in `ce_behavior_t`, independent of `ce_hooks_t` layout.

- [ ] Add literal table tests for active JSON without active behavior, active behavior with non-active JSON, passive data-only cards, and valid active/passive cards.
- [ ] Run the focused host test and confirm the missing contract API fails compilation.
- [ ] Add `is_active` to behavior registration, populate it from `CE_CARD`, and call the pure contract check from `ce_sdk_setup()` after registration-error checking.
- [ ] Run `make test-host` and build the DLL to confirm the existing cards satisfy the contract.

### Task 3: Safe Give-Card Transaction

**Files:**
- Create: `mods/th18.v1.00a/card-expand/native/sdk_actions.h`
- Create: `mods/th18.v1.00a/card-expand/native/sdk_actions.c`
- Create: `mods/th18.v1.00a/card-expand/native/tests/test_sdk_actions.c`
- Modify: `mods/th18.v1.00a/card-expand/native/sdk.h`
- Modify: `mods/th18.v1.00a/card-expand/native/Makefile`

**Interfaces:**
- Produces: `int ce_give_card_transaction(uint32_t id, uint32_t mode, uint32_t notify, ce_allocate_card_cb, ce_mark_card_cb, void *ctx)` for host tests.
- `ce_give_card()` supplies real engine adapters and preserves its current signature.

- [ ] Add a host test whose fake allocator returns `-1` and assert the real transaction never invokes the marker; add a success case that marks exactly once with the same ID/notify.
- [ ] Run the focused test and confirm it fails because the transaction helper is absent.
- [ ] Implement the small transaction helper and route `ce_give_card()` through it.
- [ ] Run focused and full host tests.

### Task 4: Two-Phase Card Data Commit

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/native/cards_def.h`
- Modify: `mods/th18.v1.00a/card-expand/native/cards_def.c`
- Modify: `mods/th18.v1.00a/card-expand/native/cards.c`
- Modify: `mods/th18.v1.00a/card-expand/native/tests/test_cards_def.c`

**Interfaces:**
- Produces: fixed `ce_card_batch_t` storage and `ce_card_batch_add()` / `ce_card_batch_commit()` pure helpers.
- The runtime JSON adapter parses every object into the batch before table/text/registry publication.

- [ ] Add tests proving an invalid/duplicate late card leaves the destination table and published count unchanged, while a valid batch commits all rows in registration order.
- [ ] Run the focused test and confirm the missing batch API fails compilation.
- [ ] Implement fixed batch storage, including owned internal-name strings, and refactor `ce_cards_load()` into parse/validate then commit phases without per-card `malloc`.
- [ ] Run host tests and DLL compilation.

### Task 5: Unified Selfcheck Failure Exit

**Files:**
- Create: `mods/th18.v1.00a/card-expand/native/selfcheck_core.h`
- Create: `mods/th18.v1.00a/card-expand/native/selfcheck_core.c`
- Create: `mods/th18.v1.00a/card-expand/native/tests/test_selfcheck_core.c`
- Modify: `mods/th18.v1.00a/card-expand/native/selfcheck.c`
- Modify: `mods/th18.v1.00a/card-expand/native/Makefile`

**Interfaces:**
- Produces: a small cleanup guard that records whether expanded allocation may be active and invokes an injected rollback exactly once on every failure.

- [ ] Add tests covering failure before cave lookup, row derivation failure, late setup failure, success, and idempotent rollback.
- [ ] Run the focused test and confirm it fails because the guard API is absent.
- [ ] Refactor `ce_selfcheck()` to a single failure path using the guard; change `restore_alloc_bound()` to report restored/already-retail/protection-failed for logging.
- [ ] Run host tests, DLL build, and site verification.

### Task 6: Public SDK Compatibility Boundary

**Files:**
- Create: `mods/th18.v1.00a/card-expand/native/sdk/public.h`
- Modify: `mods/th18.v1.00a/card-expand/native/sdk.h`
- Modify: `mods/th18.v1.00a/card-expand/native/Makefile`

**Interfaces:**
- `sdk/public.h` owns `ce_card_t`, hook types, behavior declaration macros, state API, and public result constants.
- `sdk.h` remains the supported compatibility include and adds TH18 engine helpers after including `sdk/public.h`.

- [ ] Add a compile-only fixture that includes `sdk/public.h`, declares a minimal card behavior, and does not include `engine.h` directly.
- [ ] Run the compile fixture and confirm it fails because the public header does not exist.
- [ ] Move the public declarations/macros without changing generated symbols or vtable layout; keep `sdk.h` compatibility.
- [ ] Compile the fixture, all cards, and DLL; compare exports and x87 result.

### Task 7: Developer Commands and Test Collection

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/native/Makefile`
- Rename or refactor: `mods/th18.v1.00a/card-expand/native/tests/test_build_voice.py`
- Rename or refactor: `mods/th18.v1.00a/card-expand/native/tests/test_sound_emit.py`
- Rename or refactor: `mods/th18.v1.00a/card-expand/native/tests/test_sound_sites.py`

**Interfaces:**
- Produces: `make quick`, strengthened `make check`, `make package`, and pytest-safe collection.

- [ ] Run `python3 -m pytest -q` and record the current import-time `SystemExit` failure.
- [ ] Convert script entrypoints to `main()` guarded by `if __name__ == "__main__"`, exposing pytest test functions only where useful.
- [ ] Add toolchain discovery and make `dllverify` assert PE32, required exports, and allowed imports.
- [ ] Run `python3 -m pytest -q`, `make quick`, and `make check`.

### Task 8: Documentation and Final Verification

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/SDK.md`
- Modify: `mods/th18.v1.00a/card-expand/NEXT.md`
- Modify: `mods/th18.v1.00a/card-expand/MAP.md`
- Modify: `mods/th18.v1.00a/card-expand/native/sites.py`
- Modify: `mods/th18.v1.00a/card-expand/native/Makefile`

**Interfaces:**
- Documents the final public/platform boundary, contract errors, two-phase data load, and canonical developer commands.

- [ ] Remove stale claims about duplicate-ID link errors, post-init authority, active cards being future work, and obsolete test status.
- [ ] Run `python3 ../../../../tooling/check-docs.py` and `make check`.
- [ ] Run `make package`; inspect `git diff --check` and verify the pre-existing `patch/files.js` change was preserved.
- [ ] Commit only the files belonging to this implementation with a concrete validation summary.
