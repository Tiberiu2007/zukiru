# ADR 0007 — License: MIT OR Apache-2.0 (dual)

- **Status:** Accepted
- **Date:** 2026-07-06
- **Deciders:** Zuki project
- **Supersedes:** —

## Context

Zuki is a game engine (see
[PROJECT_STRUCTURE.md](../../agents/PROJECT_STRUCTURE.md)) intended to be *built
on*: games, tools, and third parties will link its libraries, and many of those
downstream projects will be closed-source and commercial. An engine's license is
effectively permanent once outside contributions are accepted — relicensing later
requires the consent of every contributor — so it needs to be chosen deliberately.

Requirements:

1. **Permissive** — must not force games built with Zuki to be open-sourced.
   This rules out GPL, and makes LGPL/MPL awkward given Zuki links as static
   libraries (see [PROJECT_STRUCTURE.md](../../agents/PROJECT_STRUCTURE.md) §3).
2. **Patent protection** — an explicit patent grant is desirable to protect both
   contributors and users.
3. **Ecosystem-familiar** — should match what engine/library consumers already
   expect, to minimize legal friction on adoption.

## Decision

**Dual-license under `MIT OR Apache-2.0`**, at the user's option. Both license
texts live at the repo root as [`LICENSE-MIT`](../../LICENSE-MIT) and
[`LICENSE-APACHE`](../../LICENSE-APACHE); the [README](../../README.md) states
the terms and the inbound-contribution rule.

- Downstream users pick whichever license suits them.
- **MIT** provides maximum simplicity and universal familiarity (it is what
  Godot uses).
- **Apache-2.0** adds an explicit patent grant and contribution terms.
- Offering both is the well-trodden Rust/game-engine pattern (e.g. Bevy) and lets
  Zuki interoperate cleanly with code under either license.

Contributions are inbound under the same dual license (Apache-2.0 §5 inbound=outbound,
stated explicitly in the README's Contribution section), so no separate CLA is
required to keep the licensing coherent.

Copyright is attributed to "Zuki contributors" rather than a single individual,
so the notice does not need editing as the contributor set grows.

## Consequences

**Positive**
- Anyone can ship a game with Zuki — commercial or closed-source — with no
  copyleft obligations.
- The Apache-2.0 option carries an explicit patent grant; the MIT option keeps the
  simplest-possible path available.
- Familiar to the ecosystem; SPDX id `MIT OR Apache-2.0` is widely tooling-supported.

**Negative / trade-offs**
- Two license files and a dual-license notice to keep in sync (minor).
- Apache-2.0 requires preserving NOTICE/attribution on redistribution; acceptable
  and standard.
- The choice is effectively irreversible once external contributions land — this
  is intended.

## Alternatives considered

- **MIT only** — simplest, but no explicit patent grant. The dual license keeps
  MIT available while adding Apache-2.0's protections for those who want them.
- **Apache-2.0 only** — good protections, but more boilerplate and slightly less
  universal than MIT for small downstream projects.
- **zlib** — very simple and popular in C/C++ game-dev, but no patent grant and
  less common as a project-wide license than MIT/Apache.
- **GPL / LGPL / MPL** — copyleft; rejected because it would impose source
  obligations on games built with Zuki (fatal for engine adoption), with static
  linking making the weaker variants especially awkward.
