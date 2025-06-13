#!/usr/bin/env python3
"""
HDF5 Output Verification Script for SAGE Model
Verifies that all core properties from properties.yaml are present in HDF5 output
and checks for reasonable values given their units.
"""

import h5py
import numpy as np
import sys
import os
from pathlib import Path

# Core properties from properties.yaml (is_core: true)
CORE_PROPERTIES = {
    'SnapNum': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (0, 100)},
    'Type': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (0, 1)},
    'GalaxyNr': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (0, None)},
    'CentralGal': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'HaloNr': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'MostBoundID': {'type': 'int64', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'GalaxyIndex': {'type': 'uint64', 'units': 'dimensionless', 'expected_range': (0, None)},
    'CentralGalaxyIndex': {'type': 'uint64', 'units': 'dimensionless', 'expected_range': (0, None)},
    'SAGEHaloIndex': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'SAGETreeIndex': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'SimulationHaloIndex': {'type': 'int64', 'units': 'dimensionless', 'expected_range': (-1, None)},
    'dT': {'type': 'float32', 'units': 'Gyr', 'expected_range': (0.0, 15.0)},
    # Position components - stored as individual fields
    'Posx': {'type': 'float32', 'units': 'Mpc/h', 'expected_range': (0.0, 500.0)},
    'Posy': {'type': 'float32', 'units': 'Mpc/h', 'expected_range': (0.0, 500.0)},
    'Posz': {'type': 'float32', 'units': 'Mpc/h', 'expected_range': (0.0, 500.0)},
    # Velocity components - stored as individual fields
    'Velx': {'type': 'float32', 'units': 'km/s', 'expected_range': (-5000.0, 5000.0)},
    'Vely': {'type': 'float32', 'units': 'km/s', 'expected_range': (-5000.0, 5000.0)},
    'Velz': {'type': 'float32', 'units': 'km/s', 'expected_range': (-5000.0, 5000.0)},
    # Spin components - stored as individual fields  
    'Spinx': {'type': 'float32', 'units': 'dimensionless', 'expected_range': (-1.0, 1.0)},
    'Spiny': {'type': 'float32', 'units': 'dimensionless', 'expected_range': (-1.0, 1.0)},
    'Spinz': {'type': 'float32', 'units': 'dimensionless', 'expected_range': (-1.0, 1.0)},
    'Len': {'type': 'int32', 'units': 'dimensionless', 'expected_range': (0, None)},
    'Mvir': {'type': 'float32', 'units': '1e10 Msun/h', 'expected_range': (0.0, 1e6)},
    'deltaMvir': {'type': 'float32', 'units': '1e10 Msun/h', 'expected_range': (-1e6, 1e6)},
    'CentralMvir': {'type': 'float32', 'units': '1e10 Msun/h', 'expected_range': (0.0, 1e6)},
    'Rvir': {'type': 'float32', 'units': 'Mpc/h', 'expected_range': (0.0, 10.0)},
    'Vvir': {'type': 'float32', 'units': 'km/s', 'expected_range': (0.0, 3000.0)},
    'Vmax': {'type': 'float32', 'units': 'km/s', 'expected_range': (0.0, 1000.0)},
    'VelDisp': {'type': 'float32', 'units': 'km/s', 'expected_range': (0.0, 1000.0)},
    'MergTime': {'type': 'float32', 'units': 'Gyr', 'expected_range': (0.0, 999.9)},
    'infallMvir': {'type': 'float32', 'units': '1e10 Msun/h', 'expected_range': (0.0, 1e6)},
    'infallVvir': {'type': 'float32', 'units': 'km/s', 'expected_range': (0.0, 3000.0)},
    'infallVmax': {'type': 'float32', 'units': 'km/s', 'expected_range': (0.0, 1000.0)}
}

def check_dataset_properties(dataset, prop_name, prop_info):
    """Check a single dataset for correct properties and reasonable values."""
    issues = []
    
    # Check data type
    expected_dtype = prop_info['type']
    actual_dtype = str(dataset.dtype)
    
    dtype_map = {
        'int32': ['int32', '<i4', '>i4', '=i4'],
        'int64': ['int64', '<i8', '>i8', '=i8'],  
        'uint64': ['uint64', '<u8', '>u8', '=u8'],
        'float32': ['float32', '<f4', '>f4', '=f4']
    }
    
    if expected_dtype in dtype_map:
        if actual_dtype not in dtype_map[expected_dtype]:
            issues.append(f"Type mismatch: expected {expected_dtype}, got {actual_dtype}")
    
    # All properties should be 1D arrays (one value per galaxy)
    if len(dataset.shape) != 1:
        issues.append(f"Unexpected shape: expected 1D array, got {dataset.shape}")
    
    # Check value ranges
    data = dataset[...]
    if 'expected_range' in prop_info:
        min_val, max_val = prop_info['expected_range']
        
        # For arrays, check each component
        if len(data.shape) > 1:
            data_flat = data.flatten()
        else:
            data_flat = data
            
        actual_min = np.min(data_flat)
        actual_max = np.max(data_flat)
        
        if min_val is not None and actual_min < min_val:
            issues.append(f"Values below expected minimum: min={actual_min:.6g}, expected_min={min_val}")
        if max_val is not None and actual_max > max_val:
            issues.append(f"Values above expected maximum: max={actual_max:.6g}, expected_max={max_val}")
            
        # Check for suspicious patterns
        if prop_info['units'] != 'dimensionless':
            zero_fraction = np.sum(data_flat == 0) / len(data_flat)
            if zero_fraction > 0.95:
                issues.append(f"Suspicious: {zero_fraction*100:.1f}% of values are exactly zero")
                
        # Check for NaN or inf values
        if np.any(~np.isfinite(data_flat)):
            nan_count = np.sum(np.isnan(data_flat))
            inf_count = np.sum(np.isinf(data_flat))
            if nan_count > 0:
                issues.append(f"Contains {nan_count} NaN values")
            if inf_count > 0:
                issues.append(f"Contains {inf_count} infinite values")
    
    return issues

def analyze_hdf5_file(filepath):
    """Analyze a single HDF5 file."""
    print(f"\n=== Analyzing {filepath} ===")
    
    if not os.path.exists(filepath):
        print(f"ERROR: File {filepath} does not exist")
        return
        
    try:
        with h5py.File(filepath, 'r') as f:
            print(f"File structure:")
            def print_structure(name, obj):
                print(f"  {name}: {type(obj).__name__}")
                if isinstance(obj, h5py.Dataset):
                    print(f"    Shape: {obj.shape}, Type: {obj.dtype}")
            f.visititems(print_structure)
            
            # Check for snapshot groups
            snap_groups = [key for key in f.keys() if key.startswith('Snap_')]
            if snap_groups:
                print(f"\nFound {len(snap_groups)} snapshot groups: {snap_groups}")
                
                # Analyze first snapshot group for detailed property analysis
                first_snap = snap_groups[0]
                gal_group = f[first_snap]
                print(f"\nAnalyzing snapshot {first_snap} (contains {len(gal_group.keys())} datasets)")
                
                # Check each core property
                missing_properties = []
                present_properties = []
                property_issues = {}
                
                for prop_name, prop_info in CORE_PROPERTIES.items():
                    if prop_name in gal_group:
                        present_properties.append(prop_name)
                        dataset = gal_group[prop_name]
                        issues = check_dataset_properties(dataset, prop_name, prop_info)
                        if issues:
                            property_issues[prop_name] = issues
                    else:
                        missing_properties.append(prop_name)
                
                # Report results
                print(f"\n=== PROPERTY ANALYSIS (for {first_snap}) ===")
                print(f"Core properties expected: {len(CORE_PROPERTIES)}")
                print(f"Core properties found: {len(present_properties)}")
                print(f"Core properties missing: {len(missing_properties)}")
                
                if missing_properties:
                    print(f"\nMISSING PROPERTIES:")
                    for prop in missing_properties:
                        print(f"  - {prop} ({CORE_PROPERTIES[prop]['units']})")
                
                if property_issues:
                    print(f"\nPROPERTY ISSUES:")
                    for prop, issues in property_issues.items():
                        print(f"  {prop}:")
                        for issue in issues:
                            print(f"    - {issue}")
                
                # Check for additional properties (non-core)
                additional_props = []
                for prop in gal_group.keys():
                    if prop not in CORE_PROPERTIES:
                        additional_props.append(prop)
                
                if additional_props:
                    print(f"\nADDITIONAL PROPERTIES (non-core):")
                    for prop in additional_props:
                        dataset = gal_group[prop]
                        print(f"  - {prop}: {dataset.shape}, {dataset.dtype}")
                        
                        # Basic checks for additional properties
                        data = dataset[...]
                        if len(data.shape) > 1:
                            data_flat = data.flatten()
                        else:
                            data_flat = data
                            
                        if np.any(~np.isfinite(data_flat)):
                            nan_count = np.sum(np.isnan(data_flat))
                            inf_count = np.sum(np.isinf(data_flat))
                            print(f"    WARNING: Contains {nan_count} NaN and {inf_count} inf values")
                
                # Sample some data for inspection
                print(f"\n=== SAMPLE DATA (from {first_snap}) ===")
                sample_size = min(5, gal_group[list(gal_group.keys())[0]].shape[0])
                print(f"Showing first {sample_size} galaxies:")
                
                for prop_name in ['SnapNum', 'Type', 'GalaxyNr', 'Mvir', 'Rvir', 'VelDisp', 'Posx', 'Vely', 'Spinz'][:9]:
                    if prop_name in gal_group:
                        data = gal_group[prop_name][...]
                        print(f"  {prop_name}: {data[:sample_size]}")
                
                # Check consistency across all snapshots
                print(f"\n=== SNAPSHOT CONSISTENCY CHECK ===")
                for snap_group in snap_groups:
                    snap_data = f[snap_group]
                    snap_props = set(snap_data.keys())
                    first_snap_props = set(gal_group.keys())
                    
                    missing_in_snap = first_snap_props - snap_props
                    extra_in_snap = snap_props - first_snap_props
                    
                    if missing_in_snap or extra_in_snap:
                        print(f"  {snap_group}: inconsistent properties")
                        if missing_in_snap:
                            print(f"    Missing: {missing_in_snap}")
                        if extra_in_snap:
                            print(f"    Extra: {extra_in_snap}")
                    else:
                        print(f"  {snap_group}: consistent ({len(snap_data.keys())} properties, {len(snap_data[list(snap_data.keys())[0]])} galaxies)")
                        
            else:
                print("ERROR: No snapshot groups found in HDF5 file")
                print("Available groups:", list(f.keys()))
                
    except Exception as e:
        print(f"ERROR reading {filepath}: {e}")

def main():
    """Main analysis function."""
    print("SAGE Model HDF5 Output Verification")
    print("=" * 50)
    
    # Analyze both output files
    base_path = "/Volumes/Internal/results/sage-model/millennium"
    files_to_check = ["model.hdf5", "model_0.hdf5"]
    
    for filename in files_to_check:
        filepath = os.path.join(base_path, filename)
        analyze_hdf5_file(filepath)
    
    print(f"\n=== SUMMARY ===")
    print("Analysis complete. Check the detailed output above for any issues.")
    print("Focus on:")
    print("- Missing core properties")
    print("- Data type mismatches") 
    print("- Values outside expected ranges")
    print("- High percentage of zero values")
    print("- NaN or infinite values")

if __name__ == "__main__":
    main()