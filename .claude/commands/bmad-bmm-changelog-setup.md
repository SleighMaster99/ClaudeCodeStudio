---
name: 'changelog-setup'
description: '프로젝트에 CHANGELOG.md 자동 생성 기능 설정'
---

<steps CRITICAL="TRUE">
1. Always LOAD the FULL @{project-root}/_bmad/core/tasks/workflow.xml
2. READ its entire contents
3. Pass the yaml path as 'workflow-config' parameter to workflow.xml: {project-root}/_bmad/bmm/workflows/changelog-setup/workflow.yaml
4. Follow workflow.xml instructions EXACTLY as written
5. Save outputs after EACH section
</steps>
