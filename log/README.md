# SAGE Logging System Guide

**Streamlined logging for efficient development** - Current state information without historical overload.

## Current Session Logs (4 files)

**Read these at the start of each coding session** for current project context:

- **`decision-log.md`** - Recent architectural decisions with rationale (append new decisions here)
- **`phase-tracker-log.md`** - Current phase objectives and progress checklist  
- **`project-state-log.md`** - Current codebase architecture snapshot and UML-like diagrams
- **`recent-progress-log.md`** - Recent completed milestones with file changes (append new progress here)

## Historical Reference

- **`HISTORICAL_ARCHIVE.md`** - Consolidated past decisions, changes, and phase progression for lookup
- **`completed/`** - Archived logs from completed phases (decision-log.md and recent-progress-log.md only)

## Workflow for Developers

### **Starting a Coding Session**
1. Read the 4 current log files for context
2. Check `HISTORICAL_ARCHIVE.md` if you need background on past decisions
3. Update `todays-edits.md` with planned work to avoid duplication

### **During Development**
- Update `recent-progress-log.md` when completing milestones (append, 100-word limit per entry)
- Update `decision-log.md` when making architectural decisions (append, 100-word limit per entry)

### **At Phase Completion**
- Archive valuable logs (decision and recent-progress) to `completed/` directory
- Reset `phase-tracker-log.md` and `project-state-log.md` for new phase
- Update `HISTORICAL_ARCHIVE.md` with consolidated information from completed phase

## Log File Rules

### **Current Logs (lightweight, current focus)**
- **Word limits enforced** to prevent overload
- **Append-only** for decisions and progress (maintains chronology)
- **Overwrite** for phase tracker and project state (keeps current)

### **Historical Archive (comprehensive reference)**
- **Searchable** using keywords and timeline structure
- **Consolidated** from past logs to prevent information loss
- **Non-overwhelming** - reference only, not required reading

## Information Flow

```
Daily Work → Current Logs → Phase Completion → Historical Archive
     ↓           ↓              ↓                    ↓
todays-edits → decisions → completed/ archive → HISTORICAL_ARCHIVE.md
             → progress
```

## Benefits of This System

✅ **Current logs stay lightweight** - no historical overload  
✅ **Historical information preserved** - accessible but not overwhelming  
✅ **Quick context** - 4 files give complete current state  
✅ **Easy lookup** - historical archive is searchable and organized  
✅ **Prevents duplication** - daily edit tracking avoids redundant work  

---

*This system separates "what I need to know now" from "what I might need to reference later"*