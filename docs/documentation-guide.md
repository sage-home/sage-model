# SAGE Documentation Guide

**Purpose**: Standards and guidelines for writing consistent, professional SAGE documentation  
**Audience**: Contributors creating or updating SAGE documentation  
**Status**: Active style guide for all documentation work  

---

## Documentation Philosophy

SAGE documentation serves multiple audiences with different needs, organized in a clear hierarchy that enables quick navigation while providing comprehensive detail when needed. All documentation should be:

- **Self-contained**: Explains concepts without requiring external references
- **Use case-oriented**: Organized around what developers and users actually need to do
- **Consistent**: Follows standardized structure and style across all documents
- **Current**: Accurately reflects implemented capabilities, clearly distinguishes future plans

---

## Documentation Hierarchy

### 1. Entry Point: [docs/quick-reference.md](quick-reference.md)
**Purpose**: Fast navigation hub and architectural overview  
**Audience**: All users - first document people should read  
**Content**: 
- Brief SAGE overview with 8 architectural principles (numbered, abbreviated)
- Use case-based navigation to detailed documentation
- Essential commands and workflows
- Clear current vs. future capability organization

### 2. Architectural Foundation: [docs/architectural-vision.md](architectural-vision.md)  
**Purpose**: Comprehensive architectural guidance and principle reference  
**Audience**: Developers, contributors, system architects  
**Content**:
- Detailed description of all 8 architectural principles
- Implementation philosophy and design patterns  
- Target architecture and success criteria
- Current implementation status

### 3. Specialized Guides: Feature-Specific Documentation
**Purpose**: Detailed guidance for specific SAGE systems  
**Audience**: Developers working with specific components  
**Examples**:
- [testing-framework.md](testing-framework.md) - CMake testing system
- [benchmarking.md](benchmarking.md) - Performance measurement
- [code-generation-interface.md](code-generation-interface.md) - Property system
- [schema-reference.md](schema-reference.md) - Property/parameter metadata

### 4. Development Context: [../CLAUDE.md](../CLAUDE.md)
**Purpose**: Essential commands and development workflow  
**Audience**: Developers (especially AI assistants) needing quick command reference  
**Content**: Build commands, file locations, development guidelines

---

## Writing Standards

### Document Structure

Every document should follow this template:

```markdown
# Document Title

**Purpose**: Clear statement of what this document covers  
**Audience**: Who should read this document  
**Status**: Current status (Active, Draft, Planned, etc.)  

---

## Overview
Brief description of the topic and its importance to SAGE

**ARCHITECTURAL PRINCIPLE ALIGNMENT**: (if applicable)
List relevant principles with brief descriptions:
- **Principle N: Name** - Brief description of what the principle means

## [Main Content Sections]
Organized by user workflow and use cases

## [Current vs Future]
Clear separation of implemented capabilities vs planned features

## [Cross-References]
Links to related documentation using consistent format
```

### Architectural Principle References

**Consistent Format**: Always reference principles as:
**"Principle N: Brief Name"** - Short explanation of what it means in this context

**Examples**:
- **Principle 1: Physics-Agnostic Core Infrastructure** - Ensures core systems operate independently of physics implementations
- **Principle 3: Metadata-Driven Architecture** - System structure defined by metadata rather than hardcoded implementations

**Link to Details**: Include link to [architectural-vision.md](architectural-vision.md) for comprehensive principle descriptions.

### Current vs Future Content

**Clear Status Indicators**:
- ✅ **Current Capabilities** - Features that are implemented and working
- 🚧 **In Progress** - Features currently being implemented  
- 📋 **Planned** - Features planned for future implementation

**Explicit Separation**:
```markdown
## Current Capabilities ✅
Describe what actually works today

## Future Capabilities 📋  
Describe planned features with references to Master Plan phases
```

### Writing Style

**Tone**: Professional, technical, but accessible
**Perspective**: Direct, active voice ("The system provides..." not "The system is designed to provide...")
**Length**: Comprehensive but concise - include necessary detail without unnecessary verbosity
**Code Examples**: Include practical examples showing actual usage patterns

### Cross-References

**Standard Link Format**:
- Internal links: `[Document Name](filename.md)` 
- Sections: `[Section Name](filename.md#section-anchor)`
- External: `[External Reference](full-url)` with brief description

**Navigation**:
- Always link back to [quick-reference.md](quick-reference.md) from specialized documents
- Link to [architectural-vision.md](architectural-vision.md) when referencing principles
- Cross-link related documents where relevant

---

## Content Organization Patterns

### Use Case-Based Organization

Organize content around what users actually need to do:

**Good**:
```markdown
## Getting Started
## Testing Your Changes  
## Adding New Properties
## Debugging Performance Issues
```

**Avoid**:
```markdown
## System Architecture
## Implementation Details
## Technical Specifications
```

### Feature Documentation Template

For documenting specific SAGE features:

```markdown
# Feature Name Documentation

**Purpose**: What this feature does and why it exists  
**Audience**: Who needs to use this feature  
**Status**: Implementation status  

## Overview
Brief description and importance

**ARCHITECTURAL PRINCIPLE ALIGNMENT**: 
- **Principle N: Name** - How this feature supports the principle

## Current Capabilities ✅
What works today

## Usage Examples
Practical examples showing real usage

## Configuration
How to configure and customize  

## Troubleshooting
Common issues and solutions

## Future Enhancements 📋
Planned improvements (with Master Plan references)

## Related Documentation
Links to other relevant docs
```

### API/Interface Documentation

For documenting programming interfaces:

```markdown
## Function/Interface Name

**Purpose**: What it does  
**Parameters**: Description of inputs  
**Returns**: Description of outputs  
**Example**: Code example showing usage  
**Notes**: Important implementation details or constraints
```

---

## Review Criteria

Before publishing documentation, verify:

### ✅ Content Quality
- [ ] **Self-contained**: Document explains all concepts it references
- [ ] **Accurate**: Content reflects actual implementation, not aspirational goals
- [ ] **Complete**: Covers all essential aspects of the topic
- [ ] **Current**: Clearly separates implemented from planned features

### ✅ Structure Compliance  
- [ ] **Template followed**: Uses standard document structure
- [ ] **Principle references**: Architectural principles referenced consistently
- [ ] **Navigation**: Proper cross-references and links to quick-reference
- [ ] **Organization**: Content organized by use cases, not internal structure

### ✅ Style Consistency
- [ ] **Writing style**: Professional, active voice, appropriate technical level
- [ ] **Formatting**: Consistent use of headings, lists, code blocks
- [ ] **Links**: Proper internal and external link formatting
- [ ] **Status indicators**: Clear current vs future capability marking

### ✅ User Experience
- [ ] **Audience appropriate**: Content matches intended audience needs
- [ ] **Workflow aligned**: Organization follows actual user workflows  
- [ ] **Examples included**: Practical examples for key concepts
- [ ] **Troubleshooting**: Common issues and solutions provided

---

## Special Documentation Types

### Generated Documentation

Some documentation is generated from code or metadata:
- Property system documentation from YAML schemas
- API documentation from code comments
- Test coverage reports from test suite

**Standards for Generated Docs**:
- Include generation timestamp and source
- Provide context linking to relevant design documentation  
- Maintain consistency with manual documentation style
- Include regeneration instructions

### Legacy Migration Documentation

When documenting transitions from legacy systems:
- Clearly identify what's being replaced and why
- Provide migration paths for existing users
- Include compatibility information
- Reference relevant architectural principles driving the change

### Future Planning Documentation

For documenting planned but not implemented features:
- Use clear future tense and status indicators
- Reference Master Plan phases and timelines
- Explain architectural motivation
- Include placeholders in quick-reference navigation

---

## Maintenance Guidelines

### Regular Reviews
- **Quarterly**: Review all documentation for accuracy
- **Per Release**: Update status indicators and current capabilities
- **Per Major Change**: Update architectural alignment and cross-references

### Update Triggers
Update documentation when:
- New features are implemented
- Architectural changes affect existing systems
- User feedback indicates confusion or gaps
- Dependencies or external tools change

### Consistency Checks
Maintain consistency across:
- Architectural principle descriptions
- Command examples and file paths
- Cross-reference links and navigation
- Status indicators and capability descriptions

---

## Tools and Automation

### Recommended Tools
- **Markdown linting**: Ensure consistent formatting
- **Link checking**: Verify all links work correctly  
- **Spell checking**: Professional presentation
- **Generated content**: Automate where possible

### Documentation Testing
- Verify all code examples work as shown
- Test command sequences in clean environments
- Validate links and cross-references regularly
- Check generated documentation integration

---

## Examples

### Good Documentation Example

```markdown
# Galaxy Property Access

**Purpose**: Guide to accessing galaxy properties in SAGE using the type-safe property system  
**Audience**: Developers working with galaxy data structures  
**Status**: Current system (implemented and active)  

## Overview

SAGE provides type-safe access to galaxy properties through generated macros that ensure compile-time validation and runtime efficiency.

**ARCHITECTURAL PRINCIPLE ALIGNMENT**:
- **Principle 3: Metadata-Driven Architecture** - Property access generated from YAML metadata  
- **Principle 4: Single Source of Truth** - Unified property access eliminating synchronization bugs
- **Principle 8: Type Safety and Validation** - Compile-time type checking and bounds validation

## Current Capabilities ✅

### Basic Property Access
```c
// Safe property access - generated from metadata
float mass = GALAXY_GET_STELLARMASS(galaxy);
GALAXY_SET_STELLARMASS(galaxy, new_mass);
```

[... rest of example ...]
```

### Poor Documentation Example (Avoid)

```markdown
# Property System

The property system is designed to provide access to galaxy properties.

## Architecture
The system uses a metadata-driven approach with YAML files...

## Implementation  
Properties are implemented using macros...
```

**Problems**:
- Vague purpose and audience
- No architectural principle alignment  
- Technical focus instead of use case focus
- No practical examples
- Missing status and current capabilities

---

## Conclusion

Following these documentation standards ensures SAGE documentation remains professional, consistent, and useful for its diverse audience. The hierarchical organization with quick-reference entry point makes the system navigable, while the architectural principle alignment maintains consistency with SAGE's design philosophy.

When in doubt, prioritize clarity and usefulness over completeness, and always consider the documentation from the perspective of someone encountering SAGE for the first time.