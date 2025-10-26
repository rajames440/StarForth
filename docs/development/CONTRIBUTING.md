# Contributing to StarshipOS

Welcome—glad you’re here. This guide tells you how to get set up, propose changes, and work with maintainers
efficiently. It’s opinionated on purpose so we ship fast and don’t break stuff (or, at least, not twice).

> TL;DR
>
> 1. Fork → branch → small PRs.
> 2. Follow Conventional Commits.
> 3. Run `make fmt lint test` before you push.
> 4. Include repro steps, docs, and tests.
> 5. Be kind. Be direct. Don’t ship junk.

---

## Code of Conduct

We follow the **Contributor Covenant**. Be respectful, assume good intent, and focus on the work. Violations can lead to
a ban from participation.

* Read: `CODE_OF_CONDUCT.md`
* Contact for incidents: [coc-contact@example.org](mailto:coc-contact@example.org)

---

## Scope & Architecture (two-minute tour)

* **What this repo is:** <one-sentence purpose>
* **What it isn’t:** <out-of-scope items>
* **Key directories:**

    * `cmd/` – CLI or entrypoints
    * `pkg/` or `src/` – libraries / core
    * `internal/` – private packages
    * `docs/` – docs, ADRs, diagrams
    * `test/` – integration/e2e tests
    * `.github/` – CI, workflows, templates

Add/update an **ADR** (Architecture Decision Record) for any non-trivial design choice: `docs/adrs/ADR-XXXX-title.md`.

---

## Getting Started

### Prerequisites

* OS: Linux, macOS, or WSL2
* Tooling: `git`, `make`, `docker` (optional), `<language toolchain>` (e.g., Go, Rust, Node, Python)
* Pre-commit hooks: `pipx install pre-commit` (or `brew install pre-commit`) → `pre-commit install`

### Quick Setup

```bash
# 1) Fork and clone
 git clone https://github.com/<you>/<PROJECT_NAME>.git
 cd <PROJECT_NAME>

# 2) Bootstrap toolchain (optional if repo provides it)
 make bootstrap   # installs dev deps, pre-commit hooks, etc.

# 3) Verify you can build and test
 make build
 make test
```

### Local Development Loop

```bash
make fmt      # auto-format
make lint     # static analysis
make test     # unit tests
make itest    # integration tests (if available)
make run      # run the app locally
```

If `make` isn’t your thing, check `CONTRIBUTING.md` for language-specific commands under **Appendix: Language Notes**.

---

## How to Contribute

We welcome:

* Bug fixes
* Small features
* Docs improvements (user or dev docs)
* Refactors that reduce complexity
* Test coverage and infra tooling

Open an issue first for large or controversial changes so we can align early.

### Issues

* **Bug report:** include *exact* repro steps, logs, and expected vs. actual behavior. Minimal repros beat novels.
* **Feature request:** explain the problem first; propose an API/UX; add a success metric.
* **Labels:** maintainers triage with `kind/*`, `area/*`, `priority/*`, `good-first-issue`, etc.

### Branching

* Base: `main` is the trunk; it must be green.
* Branch names: `feat/<slug>`, `fix/<slug>`, `docs/<slug>`, `chore/<slug>`.

### Commit Style (Conventional Commits)

We use **Conventional Commits** to automate changelogs and releases. Examples:

```
feat(runtime): add cgroups v2 isolation flag
fix(scheduler): prevent null deref on empty queue
docs: clarify quickstart on macOS
chore(ci): bump golangci-lint to 1.60
refactor(vm): split hypervisor interface
```

* Use the imperative mood.
* Keep the subject ≤ 72 chars; add a body if needed.
* Reference issues like `Fixes #123` or `Refs #456`.

### Signed-Off-By (DCO)

This project uses the **Developer Certificate of Origin (DCO)**.
Add a sign-off to every commit:

```
Signed-off-by: Your Name <you@example.com>
```

You can automate it:

```bash
git config --global commit.gpgsign true         # if you sign commits
git config --global user.name "Your Name"
git config --global user.email you@example.com
alias gcs="git commit -s"                        # always add DCO sign-off
```

> If your organization requires a CLA instead, link it here and state the rules.

---

## Pull Requests

* **Keep PRs small** (under \~300 lines changed when possible). Big bang PRs sit and rot.
* **Checklist:**

    * [ ] Issue linked (or rationale explained)
    * [ ] Tests added/updated
    * [ ] Docs updated (`README`, `docs/`, examples)
    * [ ] `make fmt lint test` passes locally
    * [ ] Backwards compatibility considered
    * [ ] Performance impact measured (if relevant)
* Draft PRs welcome early—mark as "Read

