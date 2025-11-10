#!/usr/bin/env python3
"""
Download CGM observation papers from NASA ADS
"""

import urllib.request
import urllib.error
import os
import time
from pathlib import Path

# Paper database with ADS bibcodes
PAPERS = {
    # Figure 1: M_CGM vs M_vir (z~0)
    'werk2014': {
        'bibcode': '2014ApJ...792....8W',
        'title': 'Werk_2014_COS-Halos',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014ApJ...792....8W'
    },
    'anderson2013': {
        'bibcode': '2013MNRAS.431.3269A',
        'title': 'Anderson_2013_COS-Dwarfs',
        'url': 'https://ui.adsabs.harvard.edu/abs/2013MNRAS.431.3269A'
    },
    'faerman2020': {
        'bibcode': '2020ApJ...893...82F',
        'title': 'Faerman_2020_MW-CGM',
        'url': 'https://ui.adsabs.harvard.edu/abs/2020ApJ...893...82F'
    },
    'liang2014': {
        'bibcode': '2014MNRAS.445.2061L',
        'title': 'Liang_Chen_2014',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014MNRAS.445.2061L'
    },
    'stocke2013': {
        'bibcode': '2013ApJ...763..148S',
        'title': 'Stocke_2013',
        'url': 'https://ui.adsabs.harvard.edu/abs/2013ApJ...763..148S'
    },
    'berg2019': {
        'bibcode': '2019ApJ...883..133B',
        'title': 'Berg_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019ApJ...883..133B'
    },
    'nicastro2018': {
        'bibcode': '2018Natur.558..406N',
        'title': 'Nicastro_2018_Nature',
        'url': 'https://ui.adsabs.harvard.edu/abs/2018Natur.558..406N'
    },
    'burchett2019': {
        'bibcode': '2019ApJ...877L...3B',
        'title': 'Burchett_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019ApJ...877L...3B'
    },
    'chen2019': {
        'bibcode': '2019MNRAS.484.2924C',
        'title': 'Chen_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019MNRAS.484.2924C'
    },
    'dai2020': {
        'bibcode': '2020ApJ...896...15D',
        'title': 'Dai_2020',
        'url': 'https://ui.adsabs.harvard.edu/abs/2020ApJ...896...15D'
    },
    'tumlinson2017': {
        'bibcode': '2017ARA&A..55..389T',
        'title': 'Tumlinson_2017_ARAA',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017ARA%26A..55..389T'
    },
    'johnson2015': {
        'bibcode': '2015ApJ...802...60J',
        'title': 'Johnson_2015',
        'url': 'https://ui.adsabs.harvard.edu/abs/2015ApJ...802...60J'
    },
    'thom2012': {
        'bibcode': '2012ApJ...758L..41T',
        'title': 'Thom_2012',
        'url': 'https://ui.adsabs.harvard.edu/abs/2012ApJ...758L..41T'
    },
    'keeney2017': {
        'bibcode': '2017ApJS..230....6K',
        'title': 'Keeney_2017',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017ApJS..230....6K'
    },
    
    # Figure 1: M_CGM vs M_vir (z~2)
    'prochaska2017': {
        'bibcode': '2017ApJ...837..169P',
        'title': 'Prochaska_2017_KODIAQ',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017ApJ...837..169P'
    },
    
    # Figure 1: f_CGM vs M_vir (z~0)
    'stern2018': {
        'bibcode': '2018ApJ...865...91S',
        'title': 'Stern_2018',
        'url': 'https://ui.adsabs.harvard.edu/abs/2018ApJ...865...91S'
    },
    'bregman2018': {
        'bibcode': '2018ApJ...862....3B',
        'title': 'Bregman_2018',
        'url': 'https://ui.adsabs.harvard.edu/abs/2018ApJ...862....3B'
    },
    'mathews2017': {
        'bibcode': '2017ApJ...846L..24M',
        'title': 'Mathews_Prochaska_2017',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017ApJ...846L..24M'
    },
    'gupta2012': {
        'bibcode': '2012ApJ...756L...8G',
        'title': 'Gupta_2012',
        'url': 'https://ui.adsabs.harvard.edu/abs/2012ApJ...756L...8G'
    },
    'hafen2019': {
        'bibcode': '2019MNRAS.488.1248H',
        'title': 'Hafen_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019MNRAS.488.1248H'
    },
    'peeples2019': {
        'bibcode': '2019ApJ...873..129P',
        'title': 'Peeples_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019ApJ...873..129P'
    },
    'anderson2010': {
        'bibcode': '2010ApJ...714..320A',
        'title': 'Anderson_Bregman_2010',
        'url': 'https://ui.adsabs.harvard.edu/abs/2010ApJ...714..320A'
    },
    'dai2012': {
        'bibcode': '2012ApJ...755..107D',
        'title': 'Dai_2012',
        'url': 'https://ui.adsabs.harvard.edu/abs/2012ApJ...755..107D'
    },
    'savage2014': {
        'bibcode': '2014ApJS..212....8S',
        'title': 'Savage_2014',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014ApJS..212....8S'
    },
    
    # Figure 3: CGM Metallicity (z~0)
    'peeples2014': {
        'bibcode': '2014ApJ...786...54P',
        'title': 'Peeples_2014_COS-Halos',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014ApJ...786...54P'
    },
    'tumlinson2011': {
        'bibcode': '2011Sci...334..948T',
        'title': 'Tumlinson_2011_Science',
        'url': 'https://ui.adsabs.harvard.edu/abs/2011Sci...334..948T'
    },
    'lehner2013': {
        'bibcode': '2013ApJ...770..138L',
        'title': 'Lehner_2013',
        'url': 'https://ui.adsabs.harvard.edu/abs/2013ApJ...770..138L'
    },
    'zahedy2019': {
        'bibcode': '2019MNRAS.484.2257Z',
        'title': 'Zahedy_2019_MusE-QuBES',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019MNRAS.484.2257Z'
    },
    'werk2016': {
        'bibcode': '2016ApJ...833..354W',
        'title': 'Werk_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016ApJ...833..354W'
    },
    'burchett2016': {
        'bibcode': '2016ApJ...832..124B',
        'title': 'Burchett_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016ApJ...832..124B'
    },
    'wotta2016': {
        'bibcode': '2016ApJ...831...95W',
        'title': 'Wotta_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016ApJ...831...95W'
    },
    'prochaska2011': {
        'bibcode': '2011Sci...333.1258P',
        'title': 'Prochaska_2011_Science',
        'url': 'https://ui.adsabs.harvard.edu/abs/2011Sci...333.1258P'
    },
    'qu2018': {
        'bibcode': '2018ApJ...856....5Q',
        'title': 'Qu_Bregman_2018',
        'url': 'https://ui.adsabs.harvard.edu/abs/2018ApJ...856....5Q'
    },
    'peroux2016': {
        'bibcode': '2016MNRAS.457..903P',
        'title': 'Peroux_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016MNRAS.457..903P'
    },
    'christensen2014': {
        'bibcode': '2014ApJ...787..142C',
        'title': 'Christensen_2014',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014ApJ...787..142C'
    },
    'peroux2020': {
        'bibcode': '2020MNRAS.499.2462P',
        'title': 'Peroux_2020',
        'url': 'https://ui.adsabs.harvard.edu/abs/2020MNRAS.499.2462P'
    },
    'cooper2015': {
        'bibcode': '2015ApJ...812...58C',
        'title': 'Cooper_2015',
        'url': 'https://ui.adsabs.harvard.edu/abs/2015ApJ...812...58C'
    },
    'shen2013': {
        'bibcode': '2013ApJ...765...89S',
        'title': 'Shen_2013',
        'url': 'https://ui.adsabs.harvard.edu/abs/2013ApJ...765...89S'
    },
    'johnson2017': {
        'bibcode': '2017ApJ...850L..10J',
        'title': 'Johnson_2017',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017ApJ...850L..10J'
    },
    'kacprzak2019': {
        'bibcode': '2019MNRAS.485.1027K',
        'title': 'Kacprzak_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019MNRAS.485.1027K'
    },
    'tripp2011': {
        'bibcode': '2011Sci...334..952T',
        'title': 'Tripp_2011_Science',
        'url': 'https://ui.adsabs.harvard.edu/abs/2011Sci...334..952T'
    },
    'churchill2013': {
        'bibcode': '2013ApJ...763L..42C',
        'title': 'Churchill_2013',
        'url': 'https://ui.adsabs.harvard.edu/abs/2013ApJ...763L..42C'
    },
    'wotta2019': {
        'bibcode': '2019ApJ...872...81W',
        'title': 'Wotta_2019',
        'url': 'https://ui.adsabs.harvard.edu/abs/2019ApJ...872...81W'
    },
    'lehner2016': {
        'bibcode': '2016ApJ...833..283L',
        'title': 'Lehner_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016ApJ...833..283L'
    },
    'som2015': {
        'bibcode': '2015ApJ...806...25S',
        'title': 'Som_2015',
        'url': 'https://ui.adsabs.harvard.edu/abs/2015ApJ...806...25S'
    },
    
    # Figure 3: CGM Metallicity (z~2)
    'rudie2012': {
        'bibcode': '2012ApJ...750...67R',
        'title': 'Rudie_2012_KBSS',
        'url': 'https://ui.adsabs.harvard.edu/abs/2012ApJ...750...67R'
    },
    'bordoloi2014': {
        'bibcode': '2014ApJ...784..108B',
        'title': 'Bordoloi_2014',
        'url': 'https://ui.adsabs.harvard.edu/abs/2014ApJ...784..108B'
    },
    'fumagalli2016': {
        'bibcode': '2016MNRAS.455.4100F',
        'title': 'Fumagalli_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016MNRAS.455.4100F'
    },
    'turner2017': {
        'bibcode': '2017MNRAS.471..690T',
        'title': 'Turner_2017',
        'url': 'https://ui.adsabs.harvard.edu/abs/2017MNRAS.471..690T'
    },
    'steidel2010': {
        'bibcode': '2010ApJ...717..289S',
        'title': 'Steidel_2010',
        'url': 'https://ui.adsabs.harvard.edu/abs/2010ApJ...717..289S'
    },
    'crighton2015': {
        'bibcode': '2015MNRAS.446...18C',
        'title': 'Crighton_2015',
        'url': 'https://ui.adsabs.harvard.edu/abs/2015MNRAS.446...18C'
    },
    'rauch2016': {
        'bibcode': '2016MNRAS.455.3991R',
        'title': 'Rauch_2016',
        'url': 'https://ui.adsabs.harvard.edu/abs/2016MNRAS.455.3991R'
    },
}


def create_output_directory():
    """Create directory for downloaded papers"""
    output_dir = Path("cgm_papers")
    output_dir.mkdir(exist_ok=True)
    return output_dir


def download_paper_pdf(bibcode, title, output_dir):
    """
    Attempt to download paper PDF from ADS
    Note: This requires direct PDF links which may not always be available
    """
    # ArXiv PDF link construction (for papers with ArXiv versions)
    # ADS provides links but direct download requires authentication or ArXiv fallback
    
    print(f"  Checking: {title}")
    
    # Try to construct arXiv link if possible
    # Most of these papers have arXiv versions, but we'd need to look up the arXiv IDs
    
    # For now, just save the reference information
    return False


def create_bibliography_file(output_dir):
    """Create a bibliography file with all paper references"""
    bib_file = output_dir / "CGM_Papers_Bibliography.txt"
    
    with open(bib_file, 'w') as f:
        f.write("="*80 + "\n")
        f.write("CGM OBSERVATIONS - PAPER BIBLIOGRAPHY\n")
        f.write("="*80 + "\n\n")
        
        f.write("Total Papers: {}\n\n".format(len(PAPERS)))
        
        # Group by figure
        f.write("\n" + "="*80 + "\n")
        f.write("FIGURE 1: CGM Mass and Fraction vs Halo Mass\n")
        f.write("="*80 + "\n\n")
        
        fig1_papers = [
            'werk2014', 'anderson2013', 'faerman2020', 'liang2014', 'stocke2013',
            'berg2019', 'nicastro2018', 'burchett2019', 'chen2019', 'dai2020',
            'tumlinson2017', 'johnson2015', 'thom2012', 'keeney2017', 'prochaska2017',
            'stern2018', 'bregman2018', 'mathews2017', 'gupta2012', 'hafen2019',
            'peeples2019', 'anderson2010', 'dai2012', 'savage2014'
        ]
        
        for i, key in enumerate(fig1_papers, 1):
            if key in PAPERS:
                paper = PAPERS[key]
                f.write(f"{i}. {paper['title']}\n")
                f.write(f"   Bibcode: {paper['bibcode']}\n")
                f.write(f"   URL: {paper['url']}\n\n")
        
        f.write("\n" + "="*80 + "\n")
        f.write("FIGURE 3: CGM Metallicity\n")
        f.write("="*80 + "\n\n")
        
        fig3_papers = [
            'peeples2014', 'tumlinson2011', 'lehner2013', 'zahedy2019', 'werk2016',
            'burchett2016', 'wotta2016', 'prochaska2011', 'qu2018', 'peroux2016',
            'christensen2014', 'peroux2020', 'cooper2015', 'shen2013', 'johnson2017',
            'kacprzak2019', 'tripp2011', 'churchill2013', 'wotta2019', 'lehner2016',
            'som2015', 'rudie2012', 'bordoloi2014', 'fumagalli2016', 'turner2017',
            'steidel2010', 'crighton2015', 'rauch2016'
        ]
        
        for i, key in enumerate(fig3_papers, 1):
            if key in PAPERS:
                paper = PAPERS[key]
                f.write(f"{i}. {paper['title']}\n")
                f.write(f"   Bibcode: {paper['bibcode']}\n")
                f.write(f"   URL: {paper['url']}\n\n")
    
    print(f"\n✓ Bibliography saved to: {bib_file}")


def create_bibtex_file(output_dir):
    """Create a BibTeX file with ADS bibcodes for easy import"""
    bib_file = output_dir / "cgm_papers.bib"
    
    with open(bib_file, 'w') as f:
        f.write("% CGM Observations Bibliography\n")
        f.write("% Download citations from ADS using these bibcodes:\n")
        f.write("% https://ui.adsabs.harvard.edu/abs/<bibcode>/exportcitation\n\n")
        
        for key, paper in sorted(PAPERS.items()):
            f.write(f"% {paper['title']}\n")
            f.write(f"% ADS Bibcode: {paper['bibcode']}\n")
            f.write(f"% URL: {paper['url']}\n\n")
    
    print(f"✓ BibTeX reference file saved to: {bib_file}")


def create_download_script(output_dir):
    """Create a bash script to manually download papers"""
    script_file = output_dir / "download_papers.sh"
    
    with open(script_file, 'w') as f:
        f.write("#!/bin/bash\n")
        f.write("# Script to open all paper URLs in browser\n")
        f.write("# You can then manually download PDFs from each page\n\n")
        
        for key, paper in sorted(PAPERS.items()):
            f.write(f"# {paper['title']}\n")
            f.write(f"open '{paper['url']}'\n")
            f.write("sleep 2  # Delay between opening tabs\n\n")
    
    os.chmod(script_file, 0o755)
    print(f"✓ Download script saved to: {script_file}")
    print(f"  Run with: bash {script_file}")


def create_ads_query_url(output_dir):
    """Create ADS library URL with all papers"""
    query_file = output_dir / "ADS_Library_Query.txt"
    
    # Create a list of all bibcodes
    bibcodes = [paper['bibcode'] for paper in PAPERS.values()]
    
    with open(query_file, 'w') as f:
        f.write("ADS Library Creation Instructions\n")
        f.write("="*80 + "\n\n")
        f.write("1. Go to: https://ui.adsabs.harvard.edu/\n")
        f.write("2. Click 'Libraries' (top right)\n")
        f.write("3. Click 'Create Library'\n")
        f.write("4. Name it: 'CGM Observations'\n")
        f.write("5. Use the following bibcodes to add papers:\n\n")
        
        for bibcode in sorted(bibcodes):
            f.write(f"{bibcode}\n")
        
        f.write("\n\nOr use this bulk query:\n")
        f.write("="*80 + "\n")
        bibcode_query = " OR ".join([f'bibcode:"{b}"' for b in bibcodes])
        f.write(f"\n{bibcode_query}\n")
    
    print(f"✓ ADS library instructions saved to: {query_file}")


def main():
    print("="*80)
    print("CGM OBSERVATIONS - PAPER DOWNLOAD UTILITY")
    print("="*80)
    print(f"\nTotal papers to process: {len(PAPERS)}")
    
    # Create output directory
    output_dir = create_output_directory()
    print(f"\nOutput directory: {output_dir}/")
    
    # Create reference files
    print("\nCreating reference files...")
    create_bibliography_file(output_dir)
    create_bibtex_file(output_dir)
    create_download_script(output_dir)
    create_ads_query_url(output_dir)
    
    print("\n" + "="*80)
    print("DOWNLOAD INSTRUCTIONS")
    print("="*80)
    print("\nOption 1: Manual Download")
    print("  - Run: bash cgm_papers/download_papers.sh")
    print("  - This will open all papers in your browser")
    print("  - Download PDFs manually from each page")
    
    print("\nOption 2: Create ADS Library")
    print("  - Follow instructions in: cgm_papers/ADS_Library_Query.txt")
    print("  - This creates a library you can bulk export")
    
    print("\nOption 3: Use ADS API (requires ADS API token)")
    print("  - Get token from: https://ui.adsabs.harvard.edu/user/settings/token")
    print("  - Use ads Python package: pip install ads")
    print("  - See: https://ads.readthedocs.io/")
    
    print("\n" + "="*80)
    print("FILES CREATED:")
    print("="*80)
    print(f"  1. cgm_papers/CGM_Papers_Bibliography.txt")
    print(f"  2. cgm_papers/cgm_papers.bib")
    print(f"  3. cgm_papers/download_papers.sh")
    print(f"  4. cgm_papers/ADS_Library_Query.txt")
    print("="*80 + "\n")


if __name__ == '__main__':
    main()
