---
title: 'Enhanced Dev Story Definition of Done Checklist'
validation-target: 'Story markdown ({{story_path}})'
validation-criticality: 'HIGHEST'
required-inputs:
  - 'Story markdown file with enhanced Dev Notes containing comprehensive implementation context'
  - 'Completed Tasks/Subtasks section with all items marked [x]'
  - 'Updated File List section with all changed files'
  - 'Updated Dev Agent Record with implementation notes'
optional-inputs:
  - 'Test results output'
  - 'CI logs'
  - 'Linting reports'
validation-rules:
  - 'Only permitted story sections modified: Tasks/Subtasks checkboxes, Dev Agent Record, File List, Change Log, Status'
  - 'All implementation requirements from story Dev Notes must be satisfied'
  - 'Definition of Done checklist must pass completely'
  - 'Enhanced story context must contain sufficient technical guidance'
---

# 🎯 Enhanced Definition of Done Checklist

**Critical validation:** Story is truly ready for review only when ALL items below are satisfied

## 📋 Context & Requirements Validation

- [ ] **Story Context Completeness:** Dev Notes contains ALL necessary technical requirements, architecture patterns, and implementation guidance
- [ ] **Architecture Compliance:** Implementation follows all architectural requirements specified in Dev Notes
- [ ] **Technical Specifications:** All technical specifications (libraries, frameworks, versions) from Dev Notes are implemented correctly
- [ ] **Previous Story Learnings:** Previous story insights incorporated (if applicable) and build upon appropriately

## 🔍 Technical Specification Verification

- [ ] **Unverified Items Resolved:** All [unverified] items in story Dev Notes have been verified before implementation
- [ ] **Verification Method Documented:** Each verification includes method used (--help output, type definition check, file listing, grep result)
- [ ] **Verification Results Recorded:** Dev Agent Record contains verification results for all [unverified] items
- [ ] **Spec Corrections Applied:** Any story spec that did not match reality was corrected before implementation
- [ ] **No Unverified Implementations:** No code was written against an [unverified] spec without first verifying it

## ✅ Implementation Completion

- [ ] **All Tasks Complete:** Every task and subtask marked complete with [x]
- [ ] **Acceptance Criteria Satisfaction:** Implementation satisfies EVERY Acceptance Criterion in the story
- [ ] **No Ambiguous Implementation:** Clear, unambiguous implementation that meets story requirements
- [ ] **Edge Cases Handled:** Error conditions and edge cases appropriately addressed
- [ ] **Dependencies Within Scope:** Only uses dependencies specified in story or project-context.md

## 🧪 Testing & Quality Assurance

- [ ] **Unit Tests:** Unit tests added/updated for ALL core functionality introduced/changed by this story
- [ ] **Integration Tests:** Integration tests added/updated for component interactions when story requirements demand them
- [ ] **End-to-End Tests:** End-to-end tests created for critical user flows when story requirements specify them
- [ ] **Test Coverage:** Tests cover acceptance criteria and edge cases from story Dev Notes
- [ ] **Regression Prevention:** ALL existing tests pass (no regressions introduced)
- [ ] **Code Quality:** Linting and static checks pass when configured in project
- [ ] **Test Framework Compliance:** Tests use project's testing frameworks and patterns from Dev Notes

## 📝 Documentation & Tracking

- [ ] **File List Complete:** File List includes EVERY new, modified, or deleted file (paths relative to repo root)
- [ ] **Dev Agent Record Updated:** Contains relevant Implementation Notes and/or Debug Log for this work
- [ ] **Change Log Updated:** Change Log includes clear summary of what changed and why
- [ ] **Review Follow-ups:** All review follow-up tasks (marked [AI-Review]) completed and corresponding review items marked resolved (if applicable)
- [ ] **Story Structure Compliance:** Only permitted sections of story file were modified

## 🎯 수동 테스트 안내 범위 (사용자 보고용)

- [ ] **본 작업 한정:** 사용자에게 안내한 수동 테스트 항목이 본 스토리 AC 에 직접 매핑되는 새 기능 검증으로만 구성되었는가
- [ ] **회귀 자동 포함 금지:** 스토리 문서의 "회귀 시나리오" / "Regression Tests" / "기존 동작 보존 테스트" 류 표·Task 를 수동 테스트 목록에 자동 포함하지 않았는가
- [ ] **회귀 커버리지 명시:** 회귀 위험은 "빌드 통과 + 코드 리뷰 + 자동 테스트 / 정적 검사로 커버됨" 을 보고서에 명시했는가
- [ ] **최소 검증 세트:** AC 가 N 개면 보통 N 개 또는 그 이하의 최소 검증 세트만 제시했는가 (자산 부담 적은 항목 우선)
- [ ] **명시적 요청 시에만 회귀 포함:** 사용자가 명시적으로 회귀 검증을 요청한 경우에만 회귀 항목을 수동 테스트에 추가했는가

## 🔚 Final Status Verification

- [ ] **Story Status Updated:** Story Status set to "review"
- [ ] **Sprint Status Updated:** Sprint status updated to "review" (when sprint tracking is used)
- [ ] **Quality Gates Passed:** All quality checks and validations completed successfully
- [ ] **No HALT Conditions:** No blocking issues or incomplete work remaining
- [ ] **User Communication Ready:** Implementation summary prepared for user review

## 🎯 Final Validation Output

```
Definition of Done: {{PASS/FAIL}}

✅ **Story Ready for Review:** {{story_key}}
📊 **Completion Score:** {{completed_items}}/{{total_items}} items passed
🔍 **Quality Gates:** {{quality_gates_status}}
📋 **Test Results:** {{test_results_summary}}
📝 **Documentation:** {{documentation_status}}
```

**If FAIL:** List specific failures and required actions before story can be marked Ready for Review

**If PASS:** Story is fully ready for code review and production consideration
