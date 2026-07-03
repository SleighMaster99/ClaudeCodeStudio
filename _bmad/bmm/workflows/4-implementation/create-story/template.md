# Story {{epic_num}}.{{story_num}}: {{story_title}}

Status: ready-for-dev
Story-Type: {{story_type}}

<!-- Note: Validation is optional. Run validate-create-story for quality check before dev-story. -->

<!--
🎯 STORY 문서 작성 구조 원칙 (반드시 준수):
- Story / Acceptance Criteria / Tasks·Subtasks 섹션은 "요점"만 간결하게 작성한다.
- 배경, 기술적 결정, 코드 예시, API 시그니처, 파일 경로, 제약, 참조 등 모든 상세 내용은
  Dev Notes 섹션(및 하위 섹션)에 모은다.
- Tasks에는 "무엇을 할지"만 적고, "어떻게 할지"의 상세는 Dev Notes에서 참조하도록 한다.
- 동일 정보를 Story/AC/Tasks와 Dev Notes에 중복 기재하지 않는다. 상세는 Dev Notes에만.

🛡️ 회귀 시나리오 vs 수동 테스트 분리 원칙 (반드시 준수):
- AC 는 "새 기능에 대한 직접 검증 가능한 결과" 만 포함한다. 기존 동작 보존(회귀) 항목은 AC 에 섞지 않는다.
- 회귀 시나리오 / Regression Tests / "기존 동작 보존 검증" 을 스토리에 포함해야 한다면 반드시:
  · Dev Notes 내 별도 하위 섹션(예: "Regression Risk & Automated Coverage")에 분리하여 작성.
  · 해당 섹션에 "자동 테스트 / 코드 리뷰 / 정적 검사로 커버되는 회귀 위험이며,
     Dev Agent 의 사용자 대상 수동 테스트 안내에 자동 포함되지 않음" 을 명시.
  · Tasks 에 회귀 관련 작업이 필요하면 "(자동 테스트 / CI 영역, 수동 테스트 자동 포함 대상 아님)" 주석을 붙임.
- Why: Dev Agent 가 회귀 시나리오를 새 기능 수동 테스트로 오해해 본 작업 검증 효율이 떨어지는 사고를 방지한다.
-->

## GitHub Tracking

| 항목 | 값 |
|------|-----|
| Issue | |
| Issue URL | |
| Branch | |
| PR | |
| PR URL | |

## Story

<!-- 요점만: 역할/원하는 행동/얻을 가치를 각 한 줄로. 배경·동기·상세 설명은 Dev Notes에. -->

As a {{role}},
I want {{action}},
So that {{benefit}}.

## Acceptance Criteria

<!-- 요점만: 검증 가능한 결과를 한 줄씩 나열. 구현 방법/기술 상세/예외 처리 디테일은 Dev Notes에. -->

1. [Add acceptance criteria from epics/PRD]

## Tasks / Subtasks

<!-- 요점만: 작업 항목을 짧은 동사구로. 코드 예시, API 시그니처, 파일 경로 상세, 라이브러리 사용법은
     Dev Notes 해당 항목에 작성하고 여기서는 "Dev Notes의 X 참조" 식으로 연결한다. -->

- [ ] Task 1 (AC: #)
  - [ ] Subtask 1.1
- [ ] Task 2 (AC: #)
  - [ ] Subtask 2.1

## Dev Notes

<!-- 상세 내용 집결지: 위 섹션의 모든 "왜/어떻게/무엇으로"가 여기에 모인다.
     - 아키텍처 패턴/제약, 영향 받는 소스트리, 테스트 표준
     - 라이브러리·프레임워크 버전 및 선택 이유
     - 코드 예시, API 시그니처, 데이터 모델, 파일 경로 상세
     - 외부 기술 주장은 [verified] / [unverified] 태그 필수
     - 모든 기술 세부는 출처 인용 [Source: docs/<file>.md#Section]
     - 회귀 시나리오 / 기존 동작 보존 검증이 필요하면 아래 "Regression Risk & Automated Coverage"
       하위 섹션에만 작성한다(AC / Tasks 본문에 섞지 않음). -->

- Relevant architecture patterns and constraints
- Source tree components to touch
- Testing standards summary

### Regression Risk & Automated Coverage

<!-- 이 섹션은 "기존 동작 보존(회귀)" 위험과 그에 대한 자동 커버리지를 분리해서 기록한다.
     - 여기에 적힌 항목은 자동 테스트 / 코드 리뷰 / 정적 검사로 커버되는 회귀 위험이다.
     - Dev Agent 의 사용자 대상 수동 테스트 안내에는 자동 포함되지 않는다
       (사용자가 명시적으로 회귀 검증을 요청한 경우에만 포함).
     - 회귀 관련 항목을 AC / Tasks 본문에 섞어 쓰지 말고 반드시 이 섹션에 정리한다. -->

- 회귀 위험 항목: (없으면 "해당 없음")
- 커버 수단: 자동 테스트 / 코드 리뷰 / 정적 검사 등으로 어떻게 보장하는지 한 줄씩

### Technical Claims Verification Status

<!-- Each external technical claim MUST be tagged with verification status.
     [verified] = confirmed at story-creation time via --help, type definitions, file read, or grep
     [unverified] = could not be verified at story-creation; Dev Agent MUST verify before implementing -->


### Retrospective Learnings

<!-- Auto-populated from previous epic retrospectives when available -->

- Relevant action items from previous retrospectives
- Recurring code review patterns to watch for
- Team agreements applicable to this story

### Project Structure Notes

- Alignment with unified project structure (paths, modules, naming)
- Detected conflicts or variances (with rationale)

### References

- Cite all technical details with source paths and sections, e.g. [Source: docs/<file>.md#Section]

## Dev Agent Record

### Agent Model Used

{{agent_model_name_version}}

### Debug Log References

### Completion Notes List

### File List
