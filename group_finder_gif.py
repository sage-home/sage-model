import os
from PIL import Image

def png_to_gif(folder_path=".", output_name="output.gif", duration=500):
    # Get all PNG files and sort them
    png_files = sorted([f for f in os.listdir(folder_path) if f.endswith('.png')])
    
    if not png_files:
        print("No PNG files found!")
        return
    
    # Load images
    images = []
    for png_file in png_files:
        img = Image.open(os.path.join(folder_path, png_file))
        images.append(img)
    
    # Save as GIF
    images[0].save(
        output_name,
        save_all=True,
        append_images=images[1:],
        duration=duration,
        loop=0
    )
    
    print(f"Created {output_name} from {len(images)} PNG files")

# Run the conversion
png_to_gif('/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/groups_SAGE_images/', 'all.gif', 1000)