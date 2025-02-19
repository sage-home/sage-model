# redshift_utils.py
def get_redshift_info(z=None, z_str=None, filename=None):
    """
    Universal redshift conversion function. Provide one of:
    - z: float redshift value 
    - z_str: string redshift value
    - filename: filename containing redshift
    
    Returns tuple of (float_value, string_value)
    """
    # Redshift mapping 
    REDSHIFT_MAP = {
        0.0: '0',
        0.2: '02', 
        0.5: '05',
        0.8: '08',
        1.0: '10',
        1.1: '11', 
        1.5: '15',
        2.0: '20',
        2.4: '24',
        3.0: '30',
        3.1: '31',
        3.6: '36', 
        4.0: '40',
        4.6: '46',
        5.0: '50',
        5.7: '57',
        6.0: '60',
        6.3: '63',
        7.0: '70',
        7.7: '77',
        8.0: '80',
        8.5: '85',
        10.0: '100',
        10.4: '104'
    }
    
    # Convert float to string
    if z is not None:
        return z, REDSHIFT_MAP.get(z)
        
    # Convert string to float
    if z_str is not None:
        for z_val, s in REDSHIFT_MAP.items():
            if s == z_str:
                return z_val, z_str
                
    # Extract from filename
    if filename is not None:
        import re
        match = re.search(r'z(\d+)(?:_|\.)', filename)
        if match:
            z_str = match.group(1)
            for z_val, s in REDSHIFT_MAP.items():
                if s == z_str:
                    return z_val, z_str
                    
    return None, None

def get_all_redshifts():
    """Return sorted list of all supported redshifts"""
    for z in sorted([0.0, 0.2, 0.5, 0.8, 1.0, 1.1, 1.5, 2.0, 2.4, 3.0, 3.1, 3.6, 4.0, 
                    4.6, 5.0, 5.7, 6.0, 6.3, 7.0, 7.7, 8.0, 8.5, 10.0, 10.4]):
        yield z