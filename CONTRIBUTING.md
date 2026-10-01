# Contributing

How work is organised in this repository. Code conventions are in [docs/CODE_STYLE.md](docs/CODE_STYLE.md).

## Where things are

| What | Where |
|---|---|
| Vision, requirements, decision log, roadmap | [docs/PROJECT_PLAN.md](docs/PROJECT_PLAN.md) |
| Target structure and who owns what | [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) |
| Code conventions | [docs/CODE_STYLE.md](docs/CODE_STYLE.md) |
| Work packages and their status | [Project board](https://github.com/users/SvenSZim/projects/3) |

## 1. Every change belongs to an issue

- Planned work is a **work package**: an issue titled `WP <phase>.<n> — <title>` with goal, scope, done-when conditions and dependencies.
- Anything else gets an issue from one of the templates (bug, decision) before work starts.
- A package is only started when everything it depends on is done.

Board columns:

| Column | Meaning |
|---|---|
| Backlog | Planned, not yet startable |
| Ready | All dependencies are done |
| In progress | Someone is working on it; it has a branch |
| Done | Merged, issue closed |

Move the card when the state changes. When a package is finished, move the packages it unblocked to Ready.

## 2. Branches

- `main` is always green: it builds and its tests pass.
- One branch per issue, branched from `main`:

  | Kind | Name | Example |
  |---|---|---|
  | Work package | `wp/<id>-<short-name>` | `wp/0.1-directory-tree` |
  | Bug fix | `fix/<issue>-<short-name>` | `fix/57-font-path` |
  | Docs only | `docs/<short-name>` | `docs/code-style` |

- Branches are short-lived. Delete them after merging.

## 3. Commits

Format:

```
[area] short summary in the imperative

Optional body: why the change was made, not what the diff already shows.
Refs #<issue>
```

- `area` is one of the area labels without the prefix: `build`, `core`, `ui`, `render`, `layout`, `input`, `theme`, `widgets`, `app`, `docs`.
- Summary: imperative, lower case, no full stop, at most 72 characters. "add panel batch", not "added panel batch".
- One logical change per commit. Formatting-only changes go in their own commit.
- Every commit builds.

Example:

```
[render] rebuild a panel batch only when the panel is dirty

Refs #17
```

## 4. Pull requests

- One pull request per issue, into `main`. Title is the issue title; the description starts with `Closes #<issue>`.
- Fill in the pull request template.
- CI must be green. It runs:

  | Job | What it checks | Blocks |
  |---|---|---|
  | Format | `tools/format.sh --check` | yes |
  | Linux (GCC, release) | build with warnings as errors, all tests including the window smoke test on a virtual display | yes |
  | Linux (Clang, debug, sanitizers) | build with warnings as errors, tests with AddressSanitizer and UndefinedBehaviorSanitizer | yes |
  | Windows (MSVC) | build and tests, as a portability check | no |
- Merge by squashing, so `main` has one commit per work package. The squash commit follows the commit format above.

## 5. Definition of done

A package is done when all of these are true:

- Every "Done when" item in the issue is met and ticked.
- It builds without new warnings and all tests pass, locally and in CI.
- New behaviour that needs no window has tests.
- The code is formatted and follows [docs/CODE_STYLE.md](docs/CODE_STYLE.md).
- The layer and ownership rules hold.
- Docs are updated if the public API or the structure changed.
- The pull request is merged, the issue closed, the card in Done.

## 6. Decisions

Design decisions are not made in code review or in commit messages.

- A question that affects the public API, the structure or the responsibilities gets a **decision** issue.
- Once decided, it is recorded in the decision log (`docs/PROJECT_PLAN.md` §3) with an ID, date and status, in the same pull request that acts on it.
- If a decision changes the structure, `docs/ARCHITECTURE.md` is updated in that pull request too.
- An earlier decision is changed by a new entry that names the one it replaces, never by editing history.

## 7. Adding third-party material

Fonts, images and libraries are listed in [THIRD_PARTY.md](THIRD_PARTY.md) with their license, and the license text is stored next to them. New library dependencies need a decision first.
