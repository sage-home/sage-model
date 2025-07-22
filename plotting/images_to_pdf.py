#!/usr/bin/env python3
"""
Script to convert PDF files from specified folders into a new PDF document
with 6 pages (rendered as images) per output page in a 2x3 grid.
"""

import os
import glob
from PIL import Image
from pdf2image import convert_from_path

def get_pdf_files(folders):
    """Gather all PDF files from the specified folders."""
    pdf_files = []
    for folder in folders:
        if os.path.isdir(folder):
            folder_pdfs = glob.glob(os.path.join(folder, "*.pdf"))
            pdf_files.extend(folder_pdfs)
    return sorted(pdf_files)

def convert_pdfs_to_images(pdf_files, dpi=150):
    """Convert all pages from all PDF files to images."""
    images = []
    for pdf_file in pdf_files:
        try:
            pages = convert_from_path(pdf_file, dpi=dpi)
            images.extend(pages)
        except Exception as e:
            print(f"Error processing {pdf_file}: {e}")
    return images

def create_pdf(images, output_pdf, images_per_page=6):
    """Create a PDF with multiple images arranged in a 2x3 grid per page."""
    if not images:
        print("No images were extracted from PDFs.")
        return

    # A4 size at 300 DPI
    page_width, page_height = 2480, 3508
    margin = 10
    cols, rows = 2, 3

    available_width = page_width - (2 * margin)
    available_height = page_height - (2 * margin)

    cell_width = available_width // cols
    cell_height = available_height // rows

    pdf_pages = []
    current_page = Image.new('RGB', (page_width, page_height), 'white')

    for i, img in enumerate(images):
        page_position = i % images_per_page
        row = page_position // cols
        col = page_position % cols

        x = margin + (col * cell_width)
        y = margin + (row * cell_height)

        if img.mode != 'RGB':
            img = img.convert('RGB')

        img_width, img_height = img.size

        width_ratio = cell_width / img_width
        height_ratio = cell_height / img_height
        scale_factor = min(width_ratio, height_ratio) * 0.98

        new_width = int(img_width * scale_factor)
        new_height = int(img_height * scale_factor)

        img_resized = img.resize((new_width, new_height), Image.LANCZOS)

        x_centered = x + ((cell_width - new_width) // 2)
        y_centered = y + ((cell_height - new_height) // 2)

        current_page.paste(img_resized, (x_centered, y_centered))

        if (page_position == images_per_page - 1) or (i == len(images) - 1):
            pdf_pages.append(current_page)
            if i < len(images) - 1:
                current_page = Image.new('RGB', (page_width, page_height), 'white')

    if pdf_pages:
        os.makedirs(os.path.dirname(output_pdf), exist_ok=True)
        pdf_pages[0].save(
            output_pdf,
            save_all=True,
            append_images=pdf_pages[1:],
            resolution=300.0,
            quality=95
        )
        print(f"PDF created successfully: {output_pdf}")
    else:
        print("No pages were created.")

def main():
    folders = [
        "/Users/mbradley/Downloads/millennium_plots"
    ]
    output_pdf = "./output/millennium_big.pdf"

    pdf_files = get_pdf_files(folders)
    print(f"Found {len(pdf_files)} PDF files.")

    images = convert_pdfs_to_images(pdf_files)
    print(f"Converted to {len(images)} image pages.")

    create_pdf(images, output_pdf)

if __name__ == "__main__":
    main()
