---
name: 'step-04-final-validation'
description: 'Validate complete coverage of all requirements by epics and ensure epic structure readiness'

# Path Definitions
workflow_path: '{project-root}/_bmad/bmm/workflows/3-solutioning/create-epics-and-stories'

# File References
thisStepFile: './step-04-final-validation.md'
workflowFile: '{workflow_path}/workflow.md'
outputFile: '{planning_artifacts}/epics.md'
epicFilesFolder: '{epics_artifacts}'

# Task References
advancedElicitationTask: '{project-root}/_bmad/core/workflows/advanced-elicitation/workflow.xml'
partyModeWorkflow: '{project-root}/_bmad/core/workflows/party-mode/workflow.md'

# Template References
epicsTemplate: '{workflow_path}/templates/epics-template.md'
---

# Step 4: Final Validation

## STEP GOAL:

To validate complete coverage of all requirements by epics and ensure epic structure is ready for story creation.

## MANDATORY EXECUTION RULES (READ FIRST):

### Universal Rules:

- 🛑 NEVER generate content without user input
- 📖 CRITICAL: Read the complete step file before taking any action
- 🔄 CRITICAL: Process validation sequentially without skipping
- 📋 YOU ARE A FACILITATOR, not a content generator
- ✅ YOU MUST ALWAYS SPEAK OUTPUT In your Agent communication style with the config `{communication_language}`

### Role Reinforcement:

- ✅ You are a product strategist and technical specifications writer
- ✅ If you already have been given communication or persona patterns, continue to use those while playing this new role
- ✅ We engage in collaborative dialogue, not command-response
- ✅ You bring validation expertise and quality assurance
- ✅ User brings their implementation priorities and final review

### Step-Specific Rules:

- 🎯 Focus ONLY on validating complete requirements coverage by epics
- 🚫 FORBIDDEN to skip any validation checks
- 💬 Validate FR coverage and epic structure
- 🚪 ENSURE all epics are well-defined for subsequent story creation

## EXECUTION PROTOCOLS:

- 🎯 Validate every requirement is covered by at least one epic
- 💾 Check epic dependencies and flow
- 📖 Verify architecture compliance
- 🚫 FORBIDDEN to approve incomplete coverage

## CONTEXT BOUNDARIES:

- Available context: Complete epic and story breakdown from previous steps
- Focus: Final validation of requirements coverage, story completeness, and epic readiness
- Limits: Validation only, no new content creation
- Dependencies: Completed epic design from Step 2 and stories from Step 3

## VALIDATION PROCESS:

### 0. Load Index and Epic Files

- Load {outputFile} (the epics index)
- For each epic in the Epic List, load its epic file from the **File** link (`{epicFilesFolder}/epic-{N}.md`)

**CRITICAL CHECK:**

- Every epic in the Epic List has a **File** link
- Every linked epic file exists - a missing file is a validation failure
- No epic details or stories remain in the index itself

### 1. FR Coverage Validation

Review the epic breakdown to ensure EVERY FR is covered:

**CRITICAL CHECK:**

- Go through each FR from the Requirements Inventory
- Verify it appears in at least one epic's FR coverage
- No FRs should be left uncovered

### 2. Story Completeness Validation

**CRITICAL CHECK:**

- Verify EVERY epic file has at least one story defined
- Check that story numbering follows `Story {N}.{M}` format (Epic.Story)
- Verify each epic's stories cover the FRs assigned to that epic
- Count stories per epic and display summary (e.g., "Epic 1: 5 stories, Epic 2: 3 stories")

### 3. Architecture Implementation Validation

**Check for Starter Template Setup:**

- Does Architecture document specify a starter template?
- If YES: Note that Epic 1 should include project setup from the starter template

### 4. Epic Structure Validation

**Check that:**

- Epics deliver user value, not technical milestones
- Dependencies flow naturally
- Each epic has a clear goal statement
- No big upfront technical work without user value

### 5. Dependency Validation (CRITICAL)

**Epic Independence Check:**

- Does each epic deliver COMPLETE functionality for its domain?
- Can Epic 2 function without Epic 3 being implemented?
- Can Epic 3 function standalone using Epic 1 & 2 outputs?
- ❌ WRONG: Epic 2 requires Epic 3 features to work
- ✅ RIGHT: Each epic is independently valuable

### 6. Parallel Development Validation (병렬 개발 검증)

**독립 실행 검증:**
- 각 에픽이 다른 기능 에픽 없이 테스트 가능한가?
- Epic 1 (Foundation) 의존성만 허용

**머지 충돌 위험 검증:**
- 여러 에픽이 같은 파일의 같은 부분을 수정하는가?
- ❌ WRONG: Epic 2와 Epic 3가 같은 함수 수정
- ✅ RIGHT: 같은 파일이라도 다른 함수/섹션 수정
- ✅ RIGHT: 인터페이스로 분리하여 각자 구현

**인터페이스 정의 검증:**
- 에픽 간 협력이 인터페이스로 정의되었는가?
- 모킹/스텁 전략이 명시되었는가?

**병렬 개발 체크리스트:**
- [ ] 각 에픽이 독립적으로 테스트 가능
- [ ] 머지 충돌 위험 영역 식별됨
- [ ] 필요시 인터페이스로 분리됨

### 7. Complete and Save

If all validations pass:

- Update any remaining placeholders in the index and epic files
- Ensure proper formatting
- Save the final epics.md index and all `{epicFilesFolder}/epic-{N}.md` files

**Present Final Menu:**
**All validations complete!** [C] Complete Workflow

When C is selected, the workflow is complete and the epics.md index plus all epic files are ready.

Epics creation complete. Read fully and follow: `_bmad/core/tasks/bmad-help.md` with argument `Create Epics`.

Upon Completion of task output:

1. Inform the user: **"에픽 및 스토리 생성이 완료되었습니다."**
2. Display story count summary per epic (e.g., "Epic 1: 5 stories, Epic 2: 3 stories, ...")
3. Offer to answer any questions about the epics and stories.

---

> **🛑 PHASE BOUNDARY - MANDATORY HALT**
>
> Solutioning 워크플로우가 완료되었습니다.
> create-story, dev-story, sprint-planning 등 구현 워크플로우를 자동으로 시작하지 마세요.
> 사용자가 명시적으로 다음 작업을 선택해야 합니다. HALT하고 사용자 입력을 기다리세요.
