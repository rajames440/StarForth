# capsules/

FORTH personality files loaded by the VM at boot. A capsule is an immutable,
content-addressed payload; its XXHash64 hash is its identity. Any mutation
changes the hash and the birth protocol rejects the image.

## Key files

| File | Type | Purpose |
|------|------|---------|
| `init.4th` | `(m) MAMA_INIT` | Default Mama VM personality — loaded at LBN 2048 |
| `ACL.4th` | user | Word-level ACL system; self-activating at boot |
| `zuse.4th` | user | Bootstrap superuser; loaded by `ACL.4th` |
| `doe.4th` | user | DoE workload words (`EXEC-DOE`) — opt-in |
| `init-0.4th` … `init-9.4th` | `(p)` | Numbered personality variants |
| `init-l8-*.4th` | `(p)` | L8 Jacquard mode variants (stable/volatile/diverse/temporal/transition/omni) |
| `hermes/init.4th` | `(p)` | Hermes baby VM personality |
| `artemis/init.4th` | `(p)` | Artemis baby VM personality |

## Block namespace

Block ranges are shared across all loaded capsules — collisions cause silent
word-definition overwrites.

| Range | Owner |
|-------|-------|
| 2048–2099 | `init.4th` |
| 2100–2199 | `doe.4th` |
| 3000–3999 | workload capsules |
| 4000+ | user-defined (ACL.4th, zuse.4th, …) |

Each block is limited to 1024 bytes. Verify with `wc -c` before committing.

## See also

- [`experiments/bare_metal/README.md`](../experiments/bare_metal/README.md) — DoE protocols and block namespace rules
- [`docs/03-architecture/word-acl/DESIGN.md`](../docs/03-architecture/word-acl/DESIGN.md) — ACL system design
- [`tools/mkcapsule.c`](../tools/mkcapsule.c) — assembles capsules into `capsule_generated.c`
- [Project root](../README.md)
