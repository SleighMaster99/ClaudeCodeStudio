---
name: onboarding
description: 'Generate a comprehensive onboarding document for new developers joining this project.'
---

# Developer Onboarding Document Generator

**Goal:** Automatically scan the project and generate a single, comprehensive onboarding guide for new developers joining the team.

**Your Role:** You are a technical writer creating a human-readable onboarding document. Scan project metadata, structure, and configuration — do NOT deep-dive into source code. Produce a clear, actionable guide that helps a new developer get productive quickly.

---

## INITIALIZATION

### Configuration Loading

Load config from `{project-root}/_bmad/bmm/config.yaml` and resolve:

- `project_name`, `user_name`, `communication_language`, `document_output_language`
- `output_folder`, `planning_artifacts`, `implementation_artifacts`, `epics_artifacts`
- `date` = current date

### Path Variables

- `installed_path` = `{project-root}/_bmad/bmm/workflows/onboarding`
- `template_path` = `{installed_path}/onboarding-template.md`
- `output_path` = `{output_folder}/onboarding-guide.md`
- `bmad_config_path` = `{project-root}/_bmad/_config`

### Critical Rules

- **ALL output text** (section headings, descriptions, explanations) MUST be written in `{document_output_language}`
- Translate template section titles and placeholder text to `{document_output_language}`
- If `onboarding-guide.md` already exists at `output_path`, overwrite it (re-run = refresh)
- Do NOT deep-scan source code files — focus on metadata, config, and structure

---

## PHASE 1: PROJECT FUNDAMENTALS

### 1.1 Repository Basics

Scan and collect:

- **README.md** — extract project name, description, purpose (first ~50 lines)
- **package.json** / **pyproject.toml** / **Cargo.toml** / equivalent — name, version, description, scripts
- **LICENSE** — license type
- **Git info** — default branch, remote URL

### 1.2 Tech Stack Detection

From dependency files (package.json, requirements.txt, go.mod, etc.):

- **Languages** — primary language(s) and versions
- **Frameworks** — web framework, UI library, test framework
- **Key Dependencies** — database, ORM, auth, API client, etc.
- **Dev Tools** — linter, formatter, bundler, task runner

### 1.3 Project Structure

Generate a **2-level directory tree** of the project root with brief descriptions:

```
project-root/
├── src/           — Source code
├── tests/         — Test suites
├── docs/          — Documentation
├── _bmad/         — BMAD Method configuration
└── ...
```

### 1.4 Development Environment Setup

Extract from README, package.json scripts, Makefile, docker-compose, etc.:

- **Prerequisites** — Node.js version, Python version, Docker, etc.
- **Install command** — `npm install`, `pip install`, etc.
- **Run command** — `npm run dev`, `python manage.py runserver`, etc.
- **Test command** — `npm test`, `pytest`, etc.
- **Build command** — if applicable

---

## PHASE 2: DEVELOPMENT WORKFLOW

### 2.1 Git Workflow

Scan for:

- **Git hooks** — `.husky/`, `_bmad/hooks/`, `.git/hooks/` — list active hooks and what they do
- **Branch strategy** — from CONTRIBUTING.md, CLAUDE.md, or PR templates
- **PR templates** — `.github/pull_request_template.md`
- **CI/CD** — `.github/workflows/`, `.gitlab-ci.yml`, `Jenkinsfile` — list pipelines and their purpose

### 2.2 Code Standards

Scan for:

- **Linter config** — `.eslintrc`, `ruff.toml`, `.golangci.yml`, etc.
- **Formatter config** — `.prettierrc`, `black`, `gofmt`, etc.
- **CLAUDE.md rules** — extract key development rules from CLAUDE.md if it exists
- **EditorConfig** — `.editorconfig` settings

### 2.3 Test Strategy

Detect:

- **Test framework** — Jest, Vitest, Pytest, Go test, etc.
- **Test directory** — where tests live
- **Test run command** — how to execute tests
- **Coverage** — coverage tool and thresholds if configured

### 2.4 Build & Deploy

Detect:

- **Build system** — webpack, vite, turbo, make, etc.
- **Build command** — primary build command
- **Environments** — dev, staging, production indicators
- **Deployment** — Dockerfile, Vercel config, AWS config, etc.

---

## PHASE 3: BMAD WORKFLOW (Conditional)

> **If `{project-root}/_bmad/` directory does NOT exist, skip this entire phase and note in the output that BMAD is not installed.**

### 3.1 BMAD Installation Detection

- Confirm `_bmad/` exists
- Load `_bmad/bmm/config.yaml` if present (already loaded in INIT)
- Detect installed modules: check for `_bmad/core/`, `_bmad/bmm/`

### 3.2 Available Commands

Scan `{project-root}/.claude/commands/bmad-*.md`:

- List each command with its name and description (from frontmatter)
- Group by category if possible (analysis, planning, implementation, anytime)

### 3.3 Workflow Guide

Load `{bmad_config_path}/bmad-help.csv`:

- Extract the **phase-ordered workflow sequence** (phase 1 → 2 → 3 → 4)
- For the `anytime` group, highlight the **top daily-use commands**:
  - `/bmad-help` — Get unstuck
  - `/bmad-bmm-quick-dev` — Quick tasks
  - `/bmad-bmm-sprint-status` — Sprint overview
  - `/bmad-bmm-document-project` — Generate docs
  - `/bmad-bmm-onboarding` — Refresh this guide
- Create a simple "your first day" command sequence recommendation

---

## PHASE 4: PROJECT STATUS

### 4.1 Planning Artifacts

Check for existence of key documents in `{planning_artifacts}`:

- PRD (`*prd*`, `*product-requirements*`)
- Architecture doc (`*architecture*`, `*arch*`)
- UX design (`*ux*`, `*design*`)
- Product brief (`*brief*`)
- List found documents with relative paths

### 4.2 Epic & Story Status

If `{epics_artifacts}` exists:

- List epic files found (`epic-*.md`)
- If `sprint-status.yaml` exists in `{implementation_artifacts}`, parse and summarize:
  - Total epics, stories
  - Completion status (done / in-progress / pending counts)
  - Current sprint focus

### 4.3 Existing Documentation

Check `{output_folder}` for document-project outputs:

- `project-overview.md`
- `source-tree.md`
- `*-deep-dive.md`
- Any `index.md` files
- List found documents as reference links (do NOT duplicate content)

---

## PHASE 5: DOCUMENT GENERATION

### 5.1 Load Template

Read the full template from `{template_path}`.

### 5.2 Fill Template

Replace all placeholder sections with the data collected in Phases 1-4:

- Every `<!-- PLACEHOLDER: ... -->` must be replaced with actual content
- Remove any placeholder comments that have no data (section stays but shows "Not detected" or equivalent in `{document_output_language}`)
- Translate ALL fixed text (section titles, labels, descriptions) to `{document_output_language}`
- Keep code blocks, file paths, and command names in their original form

### 5.3 Write Output

Save the completed document to `{output_path}`.

### 5.4 Completion Summary

Output to the user:

```
✅ Onboarding guide generated: {output_path}

Sections included:
- Project Overview
- Project Structure
- Dev Environment Setup
- Development Workflow
- [BMAD Workflow — if applicable]
- Project Status
- Reference Documents
- Quick Start Checklist
```
