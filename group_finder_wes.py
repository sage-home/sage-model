def disp_gap(data):
    N = len(data)
    if N < 2:
        return np.nan
    data_sort = np.sort(data)
    # compute gaps and weights
    gaps = data_sort[1:] - data_sort[:-1]
    weights = np.arange(1, N) * np.arange(N - 1, 0, -1)
    # Gapper estimate of dispersion
    sigma_gap = np.sqrt(np.pi) / (N * (N - 1)) * np.sum(weights * gaps)
    sigma = np.sqrt((N / (N - 1)) * sigma_gap**2)
    return sigma

def Group_Catalog(data):
    group_catalog = []
    for group_id, group in data.groupby('group_id'):
        central_galaxy_group = group[group['Galaxy_ID'] == group['Central_Galaxy_ID']]
        if central_galaxy_group.empty:
            continue
        central_galaxy = central_galaxy_group.iloc[0]
        ra_median = group['Right_Ascension'].median()
        dec_median = group['Declination'].median()
        z_obs_median = group['Redshift_Observed'].median()
        z_cos_median = group['Redshift_Cosmological'].median()
        M_200 = central_galaxy['Central_Galaxy_Mvir']
        vdisp = group['vel'].std()
        vdisp_gap = disp_gap(group['vel'])
        # c1 = SkyCoord(ra=group['Right_Ascension'].values * u.degree, dec=group['Declination'].values * u.degree, frame='icrs')
        # c2 = SkyCoord(ra=group['Right_Ascension'].median() * u.degree, dec=group['Declination'].median() * u.degree, frame='icrs')
        # Calculate the separation of each galaxy from the group's central position
        group['sep'] = ((2 * cosmo.comoving_distance(z_obs_median).value * 
              np.sin(c1.separation(c2).to(u.radian)/2))).value         
        radius = np.max(group['sep'])
        R_200 = central_galaxy['Rvir']
        Sig_M = np.log10(np.sum(10**group['Total_Stellar_Mass']))
        with np.errstate(divide='ignore', invalid='ignore'):
            Sig_M11 = np.log10(np.sum(10**group[group['Total_Stellar_Mass'] > 11]['Total_Stellar_Mass']))
            Sig_M11 = np.nan if np.isinf(Sig_M11) else Sig_M11
            Sig_M105 = np.log10(np.sum(10**group[group['Total_Stellar_Mass'] > 10.5]['Total_Stellar_Mass']))
            Sig_M105 = np.nan if np.isinf(Sig_M105) else Sig_M105
            Sig_M10 = np.log10(np.sum(10**group[group['Total_Stellar_Mass'] > 10]['Total_Stellar_Mass']))
            Sig_M10 = np.nan if np.isinf(Sig_M10) else Sig_M10
            Sig_M95 = np.log10(np.sum(10**group[group['Total_Stellar_Mass'] > 9.5]['Total_Stellar_Mass']))
            Sig_M95 = np.nan if np.isinf(Sig_M95) else Sig_M95
            Sig_M9 = np.log10(np.sum(10**group[group['Total_Stellar_Mass'] > 9]['Total_Stellar_Mass']))
            Sig_M9 = np.nan if np.isinf(Sig_M9) else Sig_M9
        M_Cen = central_galaxy['Total_Stellar_Mass']
        M_Cen3 = np.log10(np.sum(10**group.nlargest(3, 'Total_Stellar_Mass')['Total_Stellar_Mass']))
        M_Cen5 = np.log10(np.sum(10**group.nlargest(5, 'Total_Stellar_Mass')['Total_Stellar_Mass']))
        M_200_Disp = np.log10( 5/3 * vdisp_gap**2 * radius / G)
        M_200_Disp_Other = np.log10( 1.2e15 * (vdisp_gap / 1000)**3 * (1/ np.sqrt(0.7 + 0.3 * (1 + z_obs_median)**3)) / h)
        M_200_CM = S2HM(M_Cen, 39.788, 10.611, 0.926, -0.609)
        M_200_C3M = S3HM(M_Cen3, 46.944, 10.483, 0.249, -0.601)
        CID = central_galaxy['Galaxy_ID'].astype(int)
                
        group_catalog.append({
            'group_id': group_id,
            'Nm': len(group),
            'ra': ra_median,
            'dec': dec_median,
            'z_obs': z_obs_median,
            'z_cos': z_cos_median,
            'vdisp': vdisp,
            'vdisp_gap': vdisp_gap,
            'radius': radius,
            'M_200': M_200,
            'Mhalo_VT': M_200_Disp,
            'M_200_Disp_Other': M_200_Disp_Other,
            'Mhalo_CVT': M_200_Disp_Correction,
            'Mhalo_PM': M_200_CM,
            'Mhalo_P3M': M_200_C3M,
            'R_200': R_200,
            'Sig_M': Sig_M,
            'Sig_M11': Sig_M11,
            'Sig_M105': Sig_M105,
            'Sig_M10': Sig_M10,
            'Sig_M95': Sig_M95,
            'Sig_M9': Sig_M9,
            'M_Cen': M_Cen,
            'M_Cen3': M_Cen3,
            'M_Cen5': M_Cen5,
            'CID': CID
        })
    group_catalog = pd.DataFrame(group_catalog)
    # Merge group_catalog columns back into the original data (Gdata) based on group_id
    # Select columns to merge
    merge_cols = ['group_id', 'vdisp_gap', 'radius', 'Mhalo_P3M', 'Mhalo_VT', 'Mhalo_CVT']
    # Merge on group_id
    data = data.merge(group_catalog[merge_cols], on='group_id', how='left')
    return data, group_catalog














