# Human-Friendly Recipe System Integration Plan

## Executive Summary

This document outlines the integration of a **human-friendly recipe system** into the Enhanced SAGE Refactoring Plan. The recipe system transforms SAGE from a technical configuration interface into an astronomer-centric scientific instrument, while leveraging the metadata-driven architecture already established in Phases 1-5.2.

**Core Concept**: Astronomers write natural, recipe-like configuration files that automatically generate all technical configuration files needed by SAGE, including optimized property sets, parameter values, and module configurations.

## Vision Overview

### Current State (Phase 5.2.G)
```
Astronomer → .par file + config.json → SAGE
             (technical)   (technical)
```

### Target State (Recipe System)
```
Astronomer → recipe.par → Auto-Generator → {optimized .yaml files, config.json} → SAGE
             (human)       (intelligent)     (computer-optimized)
```

## System Architecture

### File Flow and Auto-Generation Pipeline

#### 1. Human Input: Enhanced Recipe File
```bash
# millennium_experiment.par - Human-friendly recipe
%------------------------------------------
%----- Experiment Definition --------------
%------------------------------------------
ExperimentName              Millennium Run Analysis
ExperimentDescription       Study galaxy formation with AGN feedback

%------------------------------------------
%----- Simulation Data -------------------  
%------------------------------------------
TreeFiles                   /data/millennium/trees_063
TreeFormat                  lhalo_binary
OutputLocation              /results/my_experiment/
OutputSnapshots             63 37 32 27 23 20 18 16

%------------------------------------------
%----- Physics Configuration -------------
%------------------------------------------
# Module selections (human-friendly names)
StarFormationModule         croton2006_standard
    SfrEfficiency           0.05
    
AGNFeedbackModule           bondi_hoyle  
    RadioModeEfficiency     0.08
    QuasarModeEfficiency    0.005

SupernovaFeedbackModule     enabled
    ReheatingEfficiency     3.0
    EjectionEfficiency      0.3

GasCoolingModule            sutherland_dopita
    MetalDependence         enabled

MergerModule                standard
    MajorMergerThreshold    0.3

%------------------------------------------
%----- Output Configuration --------------
%------------------------------------------
# What properties to track and output (human-readable names)
GalaxyProperties            StellarMass BulgeMass DiskMass BlackHoleMass HotGas ColdGas MetalsStellarMass
AnalysisOutputs             mass_functions merger_trees gas_fractions black_hole_relations

%------------------------------------------
%----- Performance Settings --------------
%------------------------------------------
ParallelCores               8
MemoryOptimization          balanced    # minimal, balanced, full
```

#### 2. Auto-Generated Technical Files

**The recipe system generates all technical configuration files automatically:**

```bash
# Auto-generated from recipe
millennium_experiment_properties.yaml    # Optimized property subset
millennium_experiment_parameters.yaml    # Complete parameter values  
millennium_experiment_modules.json       # Module pipeline configuration
millennium_experiment_build.config       # Build optimization flags
```

### Key Integration Points

#### A. Module Configuration Parameters → config.json Generation

**Yes, you're exactly right!** The module configuration parameters in `parameters.yaml` replace and auto-generate the current `config.json` file:

```yaml
# Extension to parameters.yaml - Module Selection Parameters
parameters:
  modules:
    physics:
      - name: "StarFormationModule"
        type: string
        category: physics
        enum_values: ["croton2006_standard", "kennicutt_schmidt", "disabled"]
        default_value: "disabled"
        struct_field: "modules.star_formation_type"
        
      - name: "AGNFeedbackModule" 
        type: string
        category: physics
        enum_values: ["bondi_hoyle", "empirical", "croton2006", "disabled"]
        default_value: "disabled"
        struct_field: "modules.agn_feedback_type"
        
      - name: "SupernovaFeedbackModule"
        type: string
        category: physics
        enum_values: ["enabled", "disabled"]
        default_value: "disabled"
        struct_field: "modules.supernova_feedback_enabled"

  output:
    - name: "GalaxyProperties"
      type: string[]
      category: core
      description: "List of galaxy properties to include in output"
      struct_field: "output.requested_properties"
      
    - name: "AnalysisOutputs"
      type: string[]
      category: core  
      description: "List of analysis outputs to generate"
      struct_field: "output.analysis_types"
```

#### B. Translation Logic: Human Names → Technical Implementation

```python
# Physics module mapping (part of recipe processor)
PHYSICS_MODULE_MAPPING = {
    # Star Formation
    "croton2006_standard": {
        "modules": ["star_formation", "feedback"],
        "properties": ["StellarMass", "ColdGas", "SfrDisk", "SfrBulge"],
        "parameters": ["SfrEfficiency"]
    },
    
    # AGN Feedback
    "bondi_hoyle": {
        "modules": ["agn_feedback", "black_hole_growth"],
        "properties": ["BlackHoleMass", "QuasarAccretionRate", "RadioModeAccretionRate"],
        "parameters": ["RadioModeEfficiency", "QuasarModeEfficiency", "BlackHoleGrowthRate"]
    },
    
    # Analysis outputs
    "mass_functions": {
        "properties": ["StellarMass", "BulgeMass", "DiskMass", "HaloMvir"],
        "transformers": ["stellar_mass_function_transformer"]
    }
}
```

## Integration with Enhanced Refactoring Plan

### Modified Phase 5.2.G: Physics Module Migration + Recipe System Foundation

```markdown
##### 5.2.G.3: Recipe System Foundation ⏰ **NEW**
**Goal:** Implement the foundation for human-friendly recipe processing
**Timeline:** 2-3 weeks (during current 5.2.G work)

Components:
- **5.2.G.3.1 Extend Parameter System**: Add module selection and output configuration parameters to parameters.yaml
- **5.2.G.3.2 Create Physics Module Registry**: Implement mapping from human-friendly names to technical module implementations  
- **5.2.G.3.3 Enhance .par Parser**: Add support for module configuration syntax in existing parameter file parser
- **5.2.G.3.4 Property List Parser**: Add capability to parse and validate "GalaxyProperties" and "AnalysisOutputs" lists
- **5.2.G.3.5 Validation Framework**: Implement validation for module combinations and property dependencies
```

### Enhanced Phase 5.3: Recipe System Implementation

```markdown  
#### 5.3.9: Recipe System Integration ⏰ **NEW**
**Goal:** Complete end-to-end recipe system with auto-generation pipeline
**Timeline:** 3-4 weeks (extends 5.3 timeline by ~1 month)

Components:
- **5.3.9.1 Recipe Processor**: Implement sage-prepare command that processes recipe files
- **5.3.9.2 Auto-Generation Pipeline**: Create system to generate optimized properties.yaml, complete parameters.yaml, and module config.json
- **5.3.9.3 Build Integration**: Integrate recipe system with existing build system (make custom-physics CONFIG=...)
- **5.3.9.4 Validation Testing**: Test complex module combinations and property optimizations
- **5.3.9.5 Legacy Compatibility**: Ensure existing .par files continue to work unchanged
- **5.3.9.6 Documentation**: Create user guide for recipe system
```

### New Phase 5.4: Recipe System Refinement

```markdown
#### Phase 5.4: Advanced Recipe Features ⏰ **NEW**
**Goal:** Add sophisticated recipe system capabilities
**Timeline:** 1-2 months (optional, can be deferred)

Components:
- **5.4.1 Experiment Templates**: Create pre-defined templates for common experiments
- **5.4.2 Interactive Configuration**: Add wizard-style configuration for complex setups
- **5.4.3 Validation and Suggestions**: Implement intelligent validation with suggested fixes
- **5.4.4 Performance Optimization**: Auto-tune memory and parallelization settings
- **5.4.5 Analysis Pipeline Integration**: Connect recipe system to plotting/analysis workflows
```

## Implementation Details

### Recipe Processor Architecture

```python
# sage-prepare command implementation
class RecipeProcessor:
    def __init__(self):
        self.physics_registry = PhysicsModuleRegistry()
        self.property_optimizer = PropertyOptimizer()
        self.parameter_resolver = ParameterResolver()
    
    def process_recipe(self, recipe_file: str) -> Dict[str, str]:
        """Process recipe file and return paths to generated files"""
        
        # 1. Parse human-friendly recipe
        recipe_config = self.parse_recipe_file(recipe_file)
        
        # 2. Resolve physics modules
        active_modules = self.physics_registry.resolve_modules(
            recipe_config['physics_modules']
        )
        
        # 3. Generate optimized property set
        properties_file = self.property_optimizer.generate_minimal_properties(
            active_modules, 
            recipe_config['output_properties']
        )
        
        # 4. Generate complete parameter file
        parameters_file = self.parameter_resolver.generate_parameters(
            recipe_config['parameters'],
            active_modules
        )
        
        # 5. Generate module configuration (replaces config.json)
        config_file = self.generate_module_config(
            active_modules,
            recipe_config['module_parameters']
        )
        
        # 6. Generate build configuration
        build_config = self.generate_build_config(
            properties_file,
            recipe_config['performance_settings']
        )
        
        return {
            'properties': properties_file,
            'parameters': parameters_file, 
            'modules': config_file,
            'build': build_config
        }
```

### Module Configuration Auto-Generation

**This directly answers your question about config.json replacement:**

```python
def generate_module_config(self, active_modules: List[str], module_params: Dict) -> str:
    """Generate module configuration JSON from recipe selections"""
    
    config = {
        "pipeline": {
            "phases": {
                "HALO": [],
                "GALAXY": [],
                "POST": [],
                "FINAL": ["output_preparation"]
            }
        },
        "modules": {}
    }
    
    # Map human module selections to technical module configurations
    for human_name, module_info in active_modules.items():
        for tech_module in module_info['modules']:
            config['modules'][tech_module] = {
                "enabled": True,
                "parameters": module_params.get(human_name, {}),
                "phase": module_info['default_phase']
            }
            
            # Add to appropriate pipeline phase
            config['pipeline']['phases'][module_info['default_phase']].append(tech_module)
    
    # Write generated config.json
    config_file = f"{recipe_base_name}_modules.json"
    with open(config_file, 'w') as f:
        json.dump(config, f, indent=2)
    
    return config_file
```

## User Workflow

### For Astronomers

```bash
# 1. Create experiment recipe (human-friendly)
cp templates/galaxy_formation_experiment.par my_experiment.par
# Edit my_experiment.par with preferred settings

# 2. Prepare optimized SAGE configuration (automatic)
./sage-prepare my_experiment.par

# 3. Build and run (automatic optimization)
# sage-prepare automatically runs:
# make custom-physics CONFIG=my_experiment_build.config
# ./sage my_experiment.par my_experiment_modules.json

# 4. Results ready for analysis
ls output/my_experiment/
```

### For Developers

```bash
# Add new physics module to registry
echo "my_new_module" >> physics_registry.yaml

# Define module properties and parameters  
# (system automatically detects and integrates)

# Test with recipe system
./sage-prepare test_new_module.par
```

## Benefits and Integration Advantages

### ✅ **Leverages Existing Infrastructure**
- **Properties System**: Uses existing metadata-driven property system
- **Parameter System**: Extends current parameter.yaml system
- **Module System**: Uses Phase 5.2 module infrastructure
- **Build System**: Enhances existing make targets

### ✅ **Timeline Integration**
- **Phase 5.2.G**: Add recipe foundation during current module migration work
- **Phase 5.3**: Complete recipe system during validation phase  
- **No Delays**: Enhances rather than replaces current work

### ✅ **Backward Compatibility**
- **Existing .par Files**: Continue to work unchanged
- **Current Workflows**: No disruption to current development
- **Gradual Adoption**: Recipe features can be adopted incrementally

### ✅ **Future Flexibility** 
- **Extensible Design**: Easy to add new physics modules and analysis types
- **Template System**: Can evolve toward more sophisticated interfaces
- **Analysis Integration**: Natural path to connect with plotting/analysis tools

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Recipe complexity overwhelming simple cases | Medium | Medium | Provide simple templates; maintain .par compatibility |
| Auto-generation bugs | Medium | High | Comprehensive validation; fallback to manual config |
| Physics module registry maintenance | Low | Medium | Automated module discovery; registry validation tools |
| Performance overhead from optimization | Low | Medium | Benchmark auto-generated vs manual configurations |

## Timeline Integration

### **Immediate (Phase 5.2.G - Current)**
- Extend parameters.yaml with module configuration parameters
- Create basic physics module registry
- Enhance .par parser for recipe syntax

### **Near-term (Phase 5.3)**  
- Implement sage-prepare command
- Complete auto-generation pipeline
- Comprehensive testing and validation

### **Future (Phase 5.4+)**
- Advanced recipe features
- Interactive configuration tools
- Integration with analysis workflows

## Conclusion

The recipe system represents the **natural evolution** of SAGE's metadata-driven architecture toward true astronomer-centric usability. By building on the solid foundation of properties.yaml, parameters.yaml, and the module system, it transforms technical complexity into scientific simplicity while maintaining all the power and optimization of the underlying system.

**Key Answer**: Yes, the module configuration parameters in parameters.yaml enable complete auto-generation of config.json files from human recipe inputs, eliminating the need for astronomers to understand technical module configuration details.