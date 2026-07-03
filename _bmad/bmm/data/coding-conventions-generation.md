# Coding Rule Generation Guide

> Invoked via "Read fully and follow" by the **architect agent `CC` (Create Coding Rule)**
> command. Purpose: detect the project type and generate a comprehensive coding-rule
> document set under `docs/CodingRule/` so implementation agents (sm, dev, code-review,
> quick-flow) apply consistent rules.
> Expected to run AFTER PRD and Architecture are written.

## INPUT

- **Confirmed technology stack / language**:
  - Primary source: read `{planning_artifacts}/architecture.md` (tech stack decided in
    the architecture workflow step-04).
  - If `architecture.md` is absent or the stack is unclear: ask the user, or scan the
    codebase to infer the primary language/framework. Do not guess silently.
- **User-supplied references (optional)**: if the user provides additional documents,
  links, or an existing house style, treat them as authoritative input to incorporate.

## OUTPUT

- Target folder: **`{project-root}/docs/CodingRule/`** (create it if missing).
- Write the main rule document there (default name `coding-rule.md`; for the "Other"
  branch you may name it after the language, e.g. `coding-rule-python.md`).
- Additional rule documents may coexist in the same folder — consumers load **every
  document** in `docs/CodingRule/`, so keep this folder dedicated to coding rules only.

## PRE-CHECK (idempotency)

1. If `docs/CodingRule/` already contains rule document(s), DO NOT overwrite silently.
   Show the user what exists and ask: keep / add a new doc / regenerate. Default: keep.
2. Surface the detected project type and the branch you will use, and get a one-line
   confirmation before writing.

## STEP 1 — DETECT PROJECT TYPE

Classify the confirmed stack/language into exactly one branch:

| Branch | Trigger |
|--------|---------|
| **A. Unreal C++** | Unreal Engine present — `.uproject`, UE modules, `Build.cs`, `UCLASS/UPROPERTY`, or architecture states Unreal Engine |
| **B. General C++** | C/C++ is the primary language but NO Unreal Engine |
| **C. Other** | Any other primary stack (Python, TypeScript/JS, Go, Rust, C#/.NET, Java, …) |

If ambiguous, state the ambiguity and ask the user. Do not guess.

## STEP 2 — GENERATE (write comprehensive, detailed rules)

Produce the document body in the config `document_output_language` (the reference
template body is Korean — match it). Be **thorough and concrete with examples**, not a
terse summary.

### Branch A — Unreal C++

- Base: `{project-root}/_bmad/bmm/data/coding-conventions-template.md` (Epic C++ standard,
  18 sections), installed with B6G.
- Write a detailed `docs/CodingRule/coding-rule.md` based on it, essentially intact.
  Keep the Epic header/attribution. Fold in any user-supplied house rules.

### Branch B — General C++

- Base: the same template, but **remove/neutralize Unreal-only rules**:
  - Section 2 Epic Games copyright notice — drop or replace with the team's own
  - Section 3: Unreal type prefixes (`U`/`A`/`S`/`I`/`F`/`T` tied to UObject/UHT),
    the `F`/`U` typedef rules, the "UHT requires prefixes" note → replace with plain
    C++ naming (keep generic PascalCase/`b`-bool guidance, drop the UObject rationale)
  - Section 6: replace the UE-library policy with standard library (STL) usage
  - Section 9: drop `UPROPERTY()`, Blueprint enums, `TEnumAsByte<>`; `MoveTemp` →
    `std::move`; keep generic C++20 guidance (nullptr, enum class, override/final,
    range-based for, explicit lambda capture)
  - Section 10: drop `//@UE5` engine-edit tags (keep generic third-party marking)
  - Section 15/16/18: drop `TEXT()`, UE-specific `FORCEINLINE`, `ENUM_CLASS_FLAGS`,
    `FString`; keep the language-agnostic API/style principles
  - Replace UE containers/`FString` with `std` equivalents throughout
- Keep all language-agnostic parts and write them out in detail.
- Header attribution: `> Adapted from Epic C++ Coding Standard — Unreal-specific rules removed for general C++.`

### Branch C — Other

- Identify the primary language/framework. At generation time, **research the current
  official / widely-accepted industry standard via web search AND analyze any
  user-supplied documents**. Examples (verify the latest authoritative source, do not
  rely on memory):
  - Python → PEP 8 (+ PEP 257)
  - TypeScript/JS → official TS guidelines / the project's chosen community styleguide
  - Go → Effective Go + `gofmt`/`go vet`
  - Rust → Rust API Guidelines + `rustfmt`
  - C#/.NET → .NET runtime / Framework Design Guidelines
  - Java → Google/Oracle Java conventions (per project preference)
- Cite the authoritative source(s) and any user-supplied docs consulted at the top.
- Organize with the same section spirit as the reference template (naming, formatting,
  comments, error handling, structure, API design), adapted to the language, detailed.

## STEP 3 — WRITE & REPORT

1. Write the rule document(s) into `docs/CodingRule/`.
2. Report one line: detected project type, branch used, file(s) written, and
   (Branch C) the sources cited.
3. Return control to the architect agent.

## SUCCESS CRITERIA

- Exactly one branch applied, matching the confirmed stack.
- Output lives in `docs/CodingRule/` and is comprehensive/detailed (not a summary).
- Branch B contains NO Unreal-only rule; Branch C cites a real authoritative source
  (and any user-supplied docs).
- Existing rule docs not silently overwritten.
- Consumers (sm/dev/code-review/quick-flow) load every doc in `docs/CodingRule/`.
