#!/usr/bin/env python3
"""
Script to convert PNG files from specified folders into a PDF document
with 4 images per page arranged in a 2x2 grid.
Uses Pillow (PIL) without ReportLab.
"""

import os
import glob
from PIL import Image

def get_png_files(folders):
    """Gather all PNG files from the specified folders."""
    png_files = []
    for folder in folders:
        if os.path.isdir(folder):
            # Use glob to find all PNG files in the folder
            folder_pngs = glob.glob(os.path.join(folder, "*.png"))
            png_files.extend(folder_pngs)
    return sorted(png_files)  # Sort files to maintain consistent order

def create_pdf(png_files, output_pdf, images_per_page=6):
    """Create a PDF with multiple PNG images arranged in a grid."""
    if not png_files:
        print("No PNG files found in the specified folders.")
        return
    
    # A4 size at higher DPI (300 DPI equivalent for better quality)
    page_width, page_height = 2480, 3508
    
    # Calculate layout - 2x3 grid (6 images per page) with minimal margins
    margin = 100
    
    # For 6 images per page: 2 columns, 3 rows
    cols = 2
    rows = 3
    
    available_width = page_width - (2 * margin)
    available_height = page_height - (2 * margin)
    
    # Size for each image cell
    cell_width = available_width // cols
    cell_height = available_height // rows
    
    # List to store page images
    pdf_pages = []
    temp_images = []
    
    # Process each PNG file
    current_page = Image.new('RGB', (page_width, page_height), 'white')
    
    for i, png_file in enumerate(png_files):
        try:
            # Calculate position on page for a 3x2 grid
            page_position = i % images_per_page
            row = page_position // cols  # Integer division for row index
            col = page_position % cols   # Modulo for column index
            
            # Calculate coordinates for pasting
            x = margin + (col * cell_width)
            y = margin + (row * cell_height)
            
            # Open and get dimensions of the image
            img = Image.open(png_file)
            
            # Convert to RGB if it's not already (e.g., if it's RGBA with transparency)
            if img.mode != 'RGB':
                img = img.convert('RGB')
                
            img_width, img_height = img.size
            
            # Calculate scaling to fit within cell while maintaining aspect ratio
            width_ratio = cell_width / img_width
            height_ratio = cell_height / img_height
            scale_factor = min(width_ratio, height_ratio) * 0.98  # Use 98% of available space
            
            # Calculate dimensions for the scaled image
            new_width = int(img_width * scale_factor)
            new_height = int(img_height * scale_factor)
            
            # Resize the image with high quality
            img_resized = img.resize((new_width, new_height), Image.LANCZOS if hasattr(Image, 'LANCZOS') else Image.ANTIALIAS)
            
            # Calculate centered position within cell
            x_centered = x + ((cell_width - new_width) // 2)
            y_centered = y + ((cell_height - new_height) // 2)
            
            # Paste the image onto the page
            current_page.paste(img_resized, (x_centered, y_centered))
            
            # Add a small caption (not easily done without using drawing functions, omitted)
            
            # Start a new page after every 4 images or when processing the last image
            if (page_position == images_per_page - 1) or (i == len(png_files) - 1):
                pdf_pages.append(current_page)
                if i < len(png_files) - 1:  # Not the last image
                    current_page = Image.new('RGB', (page_width, page_height), 'white')
            
        except Exception as e:
            print(f"Error processing {png_file}: {e}")
    
    # Save all pages to a PDF with high quality
    if pdf_pages:
        pdf_pages[0].save(
            output_pdf,
            save_all=True,
            append_images=pdf_pages[1:] if len(pdf_pages) > 1 else [],
            resolution=300.0,
            quality=95
        )
        print(f"PDF created successfully: {output_pdf}")
    else:
        print("No pages were created.")

def main():
    # Hard-coded folder paths
    folders = [
         "/fred/oz004/mbradley/sage-model/output/miniUchuu_Full_400_PSO_Weeklybest1/plots"
    ]
    
    # Hard-coded output PDF filename
    output_pdf = "miniUchuu_Full_400_plots.pdf"
    
    png_files = get_png_files(folders)
    print(f"Found {len(png_files)} PNG files.")
    
    create_pdf(png_files, output_pdf)

if __name__ == "__main__":
    main()