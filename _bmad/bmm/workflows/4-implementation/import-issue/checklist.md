# Import Issue 검증 체크리스트

Import Issue 워크플로우 완료 후 품질을 검증하는 체크리스트입니다.

## 필수 검증 항목

### 1. 이슈 처리
- [ ] 이슈 번호가 유효하고 존재함
- [ ] 이슈 상태가 조회됨 (open/closed)
- [ ] 이슈 타입이 올바르게 분류됨 (feat/fix/refactor/docs/test/chore)

### 2. 에픽 매칭
- [ ] 에픽 매칭이 적절함 (또는 새 에픽이 올바르게 생성됨)
- [ ] 스토리 번호가 순차적으로 부여됨
- [ ] 에픽 파일이 필요한 경우 업데이트됨

### 3. 스토리 파일
- [ ] 스토리 파일이 올바른 위치에 생성됨
  - 경로: `{implementation_artifacts}/{epic_num}-{story_num}-{title}.md`
- [ ] 스토리 파일에 필수 섹션이 모두 포함됨:
  - [ ] Story 섹션 (As a... I want... so that...)
  - [ ] Acceptance Criteria (최소 1개 이상)
  - [ ] GitHub Tracking 테이블 (Issue #, URL, Branch)
  - [ ] Tasks/Subtasks 섹션 (빈 상태라도 존재)
  - [ ] Dev Notes 섹션

### 4. GitHub Tracking
- [ ] Issue 번호가 기존 이슈와 일치
- [ ] Issue URL이 유효함
- [ ] Branch 이름이 올바른 형식: `{story_type}/{story_key}`
- [ ] PR 필드는 비어있음 (개발 전이므로)

### 5. Sprint Status
- [ ] sprint-status.yaml이 업데이트됨 (파일이 있는 경우)
- [ ] 새 스토리가 `ready-for-dev` 상태로 추가됨
- [ ] 에픽 항목이 있음

### 6. 원본 이슈 업데이트
- [ ] 원본 이슈에 링크 코멘트가 추가됨
- [ ] 코멘트에 스토리 정보가 포함됨 (ID, 파일 경로, 브랜치)

## 품질 검증 항목

### User Story 품질
- [ ] User Story가 명확하고 이해 가능함
- [ ] 역할(role)이 적절함 (user, developer, admin 등)
- [ ] 행동(action)이 구체적임
- [ ] 이점(benefit)이 명시됨

### Acceptance Criteria 품질
- [ ] AC가 테스트 가능함 (검증할 수 있는 조건)
- [ ] AC가 이슈 본문의 요구사항을 반영함
- [ ] AC가 모호하지 않음

### 일관성
- [ ] 스토리 타입과 브랜치명이 일관됨
- [ ] 이슈 본문의 중요 정보가 스토리에 포함됨
- [ ] 기술 요구사항이 Dev Notes에 반영됨

## 에러 상황 체크

### 복구 가능한 상황
- [ ] 이슈 정보 부족 → 사용자에게 추가 컨텍스트 요청
- [ ] 에픽 없음 → 새 에픽 생성 옵션 제공
- [ ] Sprint Status 없음 → 경고 후 계속 진행

### 치명적 상황 (워크플로우 중단)
- [ ] Git 저장소 아님 → 중단
- [ ] gh CLI 미인증 → 중단 및 인증 안내
- [ ] 유효한 이슈 없음 → 중단

## 완료 후 권장 사항

1. **스토리 파일 보완**
   - Tasks/Subtasks 섹션 작성
   - Dev Notes에 구현 힌트 추가
   - 관련 파일 경로 명시

2. **개발 시작 전**
   - 스토리 내용이 원본 이슈와 일치하는지 확인
   - AC가 충분히 구체적인지 검토

3. **이슈 상태 관리**
   - 작업 시작 시 이슈에 assignee 설정
   - 필요시 이슈에 추가 라벨 적용
