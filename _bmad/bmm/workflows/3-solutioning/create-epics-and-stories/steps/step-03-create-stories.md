---
name: 'step-03-create-stories'
description: 'Generate all epics with their stories, writing one epic file per epic following the epic file template'

# Path Definitions
workflow_path: '{project-root}/_bmad/bmm/workflows/3-solutioning/create-epics-and-stories'

# File References
thisStepFile: './step-03-create-stories.md'
nextStepFile: './step-04-final-validation.md'
workflowFile: '{workflow_path}/workflow.md'
outputFile: '{planning_artifacts}/epics.md'
epicFilesFolder: '{epics_artifacts}'

# Task References
advancedElicitationTask: '{project-root}/_bmad/core/workflows/advanced-elicitation/workflow.xml'
partyModeWorkflow: '{project-root}/_bmad/core/workflows/party-mode/workflow.md'

# Template References
epicsTemplate: '{workflow_path}/templates/epics-template.md'
epicFileTemplate: '{workflow_path}/templates/epic-template.md'
---

# Step 3: Generate Epics and Stories

## STEP GOAL:

To generate all epics with their stories based on the approved epics_list, writing each epic to its own file (`{epicFilesFolder}/epic-{N}.md`) following the epic file template structure exactly. The epics index ({outputFile}) keeps only the epic list with file links - NO story details.

## MANDATORY EXECUTION RULES (READ FIRST):

### Universal Rules:

- 🛑 NEVER generate content without user input
- 📖 CRITICAL: Read the complete step file before taking any action
- 🔄 CRITICAL: Process epics sequentially
- 📋 YOU ARE A FACILITATOR, not a content generator
- ✅ YOU MUST ALWAYS SPEAK OUTPUT In your Agent communication style with the config `{communication_language}`

### Role Reinforcement:

- ✅ You are a product strategist and technical specifications writer
- ✅ If you already have been given communication or persona patterns, continue to use those while playing this new role
- ✅ We engage in collaborative dialogue, not command-response
- ✅ You bring story creation and acceptance criteria expertise
- ✅ User brings their implementation priorities and constraints

### Step-Specific Rules:

- 🎯 Generate stories for each epic following the template exactly
- 🚫 FORBIDDEN to deviate from template structure
- 💬 Each story must have clear acceptance criteria
- 🚪 ENSURE each story is completable by a single dev agent
- 🔗 **CRITICAL: Stories MUST NOT depend on future stories within the same epic**

## EXECUTION PROTOCOLS:

- 🎯 Generate stories collaboratively with user input
- 💾 Write each epic and its stories to its own file `{epicFilesFolder}/epic-{N}.md` following {epicFileTemplate}
- 🚫 FORBIDDEN to write epic details or stories into {outputFile} - the index keeps only the epic list with file links
- 📖 Process epics one at a time in sequence
- 🚫 FORBIDDEN to skip any epic or rush through stories

## STORY GENERATION PROCESS:

### 1. Load Approved Epic Structure

Load {outputFile} (the epics index) and review:

- Approved epics_list from Step 2 (including each epic's **File** path)
- FR coverage map
- All requirements (FRs, NFRs, additional)

Load {epicFileTemplate} - this is the structure each epic file must follow.

### 2. Explain Story Creation Approach

**STORY CREATION GUIDELINES:**

For each epic, create stories that:

- Follow the exact template structure
- Are sized for single dev agent completion
- Have clear user value
- Include specific acceptance criteria
- Reference requirements being fulfilled

**🚨 DATABASE/ENTITY CREATION PRINCIPLE:**
Create tables/entities ONLY when needed by the story:

- ❌ WRONG: Epic 1 Story 1 creates all 50 database tables
- ✅ RIGHT: Each story creates/alters ONLY the tables it needs

**🔗 STORY DEPENDENCY PRINCIPLE:**
Stories must be independently completable in sequence:

- ❌ WRONG: Story 1.2 requires Story 1.3 to be completed first
- ✅ RIGHT: Each story can be completed based only on previous stories
- ❌ WRONG: "Wait for Story 1.4 to be implemented before this works"
- ✅ RIGHT: "This story works independently and enables future stories"

**STORY FORMAT (from template):**

```
### Story {N}.{M}: {story_title}

As a {user_type},
I want {capability},
So that {value_benefit}.

**Acceptance Criteria:**

**Given** {precondition}
**When** {action}
**Then** {expected_outcome}
**And** {additional_criteria}
```

**✅ GOOD STORY EXAMPLES:**

_Epic 1: User Authentication_

- Story 1.1: User Registration with Email
- Story 1.2: User Login with Password
- Story 1.3: Password Reset via Email

_Epic 2: Content Creation_

- Story 2.1: Create New Blog Post
- Story 2.2: Edit Existing Blog Post
- Story 2.3: Publish Blog Post

**❌ BAD STORY EXAMPLES:**

- Story: "Set up database" (no user value)
- Story: "Create all models" (too large, no user value)
- Story: "Build authentication system" (too large)
- Story: "Login UI (depends on Story 1.3 API endpoint)" (future dependency!)
- Story: "Edit post (requires Story 1.4 to be implemented first)" (wrong order!)

### 3. Process Epics Sequentially

For each epic in the approved epics_list:

#### A. Epic Overview and File Creation

Display:

- Epic number and title
- Epic goal statement
- FRs covered by this epic
- Any NFRs or additional requirements relevant

Create the epic file `{epicFilesFolder}/epic-{N}.md` from {epicFileTemplate}:

1. Replace {{N}} with the epic number, {{epic_title}} with the epic title
2. Replace {{epic_goal}} with the epic goal statement
3. Replace {{epic_fr_list}} with the FRs covered by this epic
4. Verify the file path matches the **File** entry for this epic in the index epics_list

#### B. Story Breakdown

Work with user to break down the epic into stories:

- Identify distinct user capabilities
- Ensure logical flow within the epic
- Size stories appropriately

#### C. Generate Each Story

For each story in the epic:

1. **Story Title**: Clear, action-oriented
2. **User Story**: Complete the As a/I want/So that format
3. **Acceptance Criteria**: Write specific, testable criteria

**AC Writing Guidelines:**

- Use Given/When/Then format
- Each AC should be independently testable
- Include edge cases and error conditions
- Reference specific requirements when applicable

#### D. Collaborative Review

After writing each story:

- Present the story to user
- Ask: "Does this story capture the requirement correctly?"
- "Is the scope appropriate for a single dev session?"
- "Are the acceptance criteria complete and testable?"

#### E. Append to Epic File

When story is approved:

- Append it to the current epic file `{epicFilesFolder}/epic-{N}.md` following the epic file template structure
- Use correct numbering (Epic N, Story M)
- Maintain proper markdown formatting
- 🚫 NEVER append stories to {outputFile} (the index)

### 4. Epic Completion

After all stories for an epic are complete:

- Display epic summary
- Show count of stories created
- Verify all FRs for the epic are covered
- Confirm the epic file `{epicFilesFolder}/epic-{N}.md` is complete and matches its **File** entry in the index
- Get user confirmation to proceed to next epic

### 5. Repeat for All Epics

Continue the process for each epic in the approved list, processing them in order (Epic 1, Epic 2, etc.).

### 6. Final Document Completion

After all epics and stories are generated:

- Verify the index and every epic file follow their template structures exactly
- Ensure all placeholders are replaced
- Confirm all FRs are covered
- Check formatting consistency

## TEMPLATE STRUCTURE COMPLIANCE:

The final {outputFile} (epics index) must follow this structure exactly:

1. **Overview** section with project name
2. **Requirements Inventory** with all three subsections populated
3. **FR Coverage Map** showing requirement to epic mapping
4. **Epic List** with approved epic structure - each entry has title, goal, FRs covered, and **File** link
5. NO epic detail sections and NO stories in the index

Each epic file `{epicFilesFolder}/epic-{N}.md` must follow {epicFileTemplate} exactly:

1. **Epic header** `## Epic {N}: {title}` with goal and FRs covered
2. **All stories** for that epic (M = 1, 2, 3...)
   - Story header `### Story {N}.{M}: {title}` and user story
   - Acceptance Criteria using Given/When/Then format

### 7. Present FINAL MENU OPTIONS

After all epics and stories are complete:

Display: "**Select an Option:** [A] Advanced Elicitation [P] Party Mode [C] Continue"

#### Menu Handling Logic:

- IF A: Read fully and follow: {advancedElicitationTask}
- IF P: Read fully and follow: {partyModeWorkflow}
- IF C: Ensure all epic files are saved, update frontmatter of {outputFile}, then read fully and follow: {nextStepFile}
- IF Any other comments or queries: help user respond then [Redisplay Menu Options](#7-present-final-menu-options)

#### EXECUTION RULES:

- ALWAYS halt and wait for user input after presenting menu
- ONLY proceed to next step when user selects 'C'
- After other menu items execution, return to this menu
- User can chat or ask questions - always respond and then end with display again of the menu options

## CRITICAL STEP COMPLETION NOTE

ONLY WHEN [C continue option] is selected and [all epic files saved following the epic file template structure exactly], will you then read fully and follow: `{nextStepFile}` to begin final validation phase.

---

## 🚨 SYSTEM SUCCESS/FAILURE METRICS

### ✅ SUCCESS:

- All epics processed in sequence
- One epic file created per epic, matching the **File** links in the index
- Stories created for each epic in its epic file
- Template structures followed exactly (index and epic files)
- All FRs covered by stories
- Stories appropriately sized
- Acceptance criteria are specific and testable
- Index and epic files are complete and ready for development

### ❌ SYSTEM FAILURE:

- Deviating from template structure
- Writing epic details or stories into the index file
- Missing epic files, epics, or stories
- Epic file path not matching the index **File** link
- Stories too large or unclear
- Missing acceptance criteria
- Not following proper formatting

**Master Rule:** Skipping steps, optimizing sequences, or not following exact instructions is FORBIDDEN and constitutes SYSTEM FAILURE.
