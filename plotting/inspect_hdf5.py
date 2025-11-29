import h5py

filename = './output/millennium_noffb/model_0.hdf5'

try:
    with h5py.File(filename, 'r') as f:
        print("Keys:", list(f.keys()))
        
        # Check attributes of the file or a snapshot group for redshift
        if 'Snap_63' in f:
            print("Snap_63 attributes:", list(f['Snap_63'].attrs.keys()))
            for key in f['Snap_63'].attrs.keys():
                print(f"{key}: {f['Snap_63'].attrs[key]}")
                
        # Check if there is a 'Redshift' array or similar
        # Sometimes it's in a separate group or attribute
except Exception as e:
    print(f"Error: {e}")
