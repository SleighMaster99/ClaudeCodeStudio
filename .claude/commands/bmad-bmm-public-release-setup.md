---
name: 'public-release-setup'
description: 'Private 저장소에서 Public 릴리스 저장소로 자동 배포 설정'
---

<steps CRITICAL="TRUE">
1. Always LOAD the FULL @{project-root}/_bmad/core/tasks/workflow.xml
2. READ its entire contents
3. Pass the yaml path as 'workflow-config' parameter to workflow.xml: {project-root}/_bmad/bmm/workflows/public-release-setup/workflow.yaml
4. Follow workflow.xml instructions EXACTLY as written
5. Save outputs after EACH section
</steps>
