# AI Prompt Cheat Sheet

## Essential Context Files (Always Read)

1. **CLAUDE.md** - Essential commands, key paths, development workflow (concise)
2. **docs/quick-reference.md** - Main documentation entry point with navigation hub and 8 architectural principles
3. **log/phase.md** - Current development phase and active work tracking

## Core Documentation Structure (Read Based on Task Type)

### **For any documentation or architectural work:**
- **docs/quick-reference.md** - Navigation hub and principle overview
- **docs/architectural-vision.md** - Detailed 8 architectural principles and implementation guidance
- **docs/documentation-guide.md** - Style standards and writing guidelines

### **For specific system work:**
- **Property system**: docs/code-generation-interface.md + docs/schema-reference.md
- **Testing issues**: docs/testing-framework.md
- **Performance concerns**: docs/benchmarking.md
- **Physics-agnostic core**: log/physics-agnostic-architecture-guide.md

### **For development context (log files):**
- **Current state**: log/architecture.md (technical system overview)
- **Past decisions**: log/decisions.md (architectural decision records)
- **Progress tracking**: log/progress.md (implementation milestones)
- **Active planning**: log/sage-master-plan.md (master implementation plan)

## Example Initial Prompts

**For implementation tasks:**
```
(1) Read @CLAUDE.md, @docs/quick-reference.md, @log/phase.md, @log/sage-master-plan.md. 

Today we will be implementing X. 

Before creating your plan, (2) read any additional docs identified in the quick reference needed for context, and (3) review the relevant code to ensure you clearly understand what needs to be done, where, and why. (4) For today's work, critical additional context can be found in @log/physics-agnostic-architecture-guide.md. (5) You should use the refactor branch of the code as a guide, where this work has already been implemented - see `sage-model-refactor-dir/src/**` - and also read the guide @docs/refactor-branch-architectural-analysis.md. Ultrathink!
```

**For architectural/design work:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @docs/architectural-vision.md, @log/phase.md.
Then design [feature] following the 8 architectural principles
```

**For debugging/fixes:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @log/phase.md, @log/architecture.md.
Then investigate and fix [specific issue]
```

**For documentation work:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @docs/documentation-guide.md.
Then create/update documentation for [topic] following style standards
```

**For major new features:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @docs/architectural-vision.md, @log/phase.md, @log/sage-master-plan.md.
Then design and implement [feature] ensuring compliance with all 8 architectural principles
```

**For refactor branch reference work:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @docs/refactor-branch-architectural-analysis.md.
Use refactor branch patterns to guide implementation while adapting for enhanced branch architecture
```

**For Phase 2A implementation work:**
```
Read @CLAUDE.md, @docs/quick-reference.md, @log/physics-agnostic-architecture-guide.md, @log/phase.md.
Then implement Task 2A.X following architectural compliance patterns and validation checklists
```

## Key Principles for AI Development

1. **Always start with docs/quick-reference.md** - It's the navigation hub for everything
2. **Reference architectural principles by number** - Use "Principle N: Name" format consistently  
3. **Follow documentation-guide.md standards** - For any documentation work
4. **Check current phase** - log/phase.md shows what work is currently prioritized
5. **Maintain architectural compliance** - All work must align with the 8 core principles

## Documentation Hierarchy Summary

```
docs/
├── quick-reference.md          # 🚀 START HERE - Main entry point
├── architectural-vision.md     # 📐 Detailed 8 principles reference
├── development-guide.md        # 🛠️ Implementation specifications
├── documentation-guide.md      # 📝 Style standards
├── testing-framework.md        # 🧪 Testing system
├── benchmarking.md            # ⚡ Performance measurement
├── code-generation-interface.md # ⚙️ Property system
└── schema-reference.md        # 📊 Property/parameter metadata

log/ (phase-specific)
├── physics-agnostic-architecture-guide.md # 🏗️ Phase 2A implementation guide
```
