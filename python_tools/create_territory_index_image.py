#!/usr/bin/env python3
"""
Create an 8-bit indexed image where each pixel value is the territory ID (1-60).
Pixels that don't belong to a territory (white background) are set to 0.
Also creates a CSV file with territory ID, size, and empty name column.
"""

import cv2
import numpy as np
from PIL import Image
import csv

# Define the palette
PALETTE = {
    'Red':     (255,   0,   0),
    'Green':   (  0, 255,   0),
    'Blue':    (  0,   0, 255),
    'Yellow':  (255, 255,   0),
    'Cyan':    (  0, 255, 255),
    'Magenta': (255,   0, 255),
    'White':   (255, 255, 255),
}

MIN_TERRITORY_SIZE = 1000


def snap_to_palette(img):
    """Snap each pixel to the nearest palette color."""
    palette_colors = np.array(list(PALETTE.values()), dtype=np.float32)
    img_float = img.astype(np.float32)

    img_expanded = img_float[:, :, np.newaxis, :]
    palette_expanded = palette_colors[np.newaxis, np.newaxis, :, :]
    distances = np.sum((img_expanded - palette_expanded) ** 2, axis=3)
    closest_idx = np.argmin(distances, axis=2)

    result = np.zeros_like(img)
    for i, (name, color) in enumerate(PALETTE.items()):
        mask = closest_idx == i
        result[mask] = color

    return result


def main():
    # Load the image
    img_path = "Images/Maps/territories.tif"
    print(f"Loading {img_path}...")

    pil_img = Image.open(img_path)
    img = np.array(pil_img)

    if img.shape[2] == 4:
        img = img[:, :, :3]

    h, w = img.shape[:2]
    print(f"Image size: {w} x {h}")

    # Snap to palette
    print("Quantizing to palette colors...")
    quantized = snap_to_palette(img)

    # Create the 8-bit index image (0 = background, 1-60 = territories)
    index_image = np.zeros((h, w), dtype=np.uint8)

    # List to store territory info for CSV
    territories = []
    territory_id = 0

    print("\nAssigning territory IDs...")

    for name, color in PALETTE.items():
        if name == 'White':
            continue

        color_mask = np.all(quantized == color, axis=2).astype(np.uint8) * 255
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(color_mask, connectivity=8)

        for i in range(1, num_labels):
            area = stats[i, cv2.CC_STAT_AREA]

            if area > MIN_TERRITORY_SIZE:
                territory_id += 1
                cx, cy = centroids[i]

                # Mark pixels with this territory ID
                region_mask = (labels == i)
                index_image[region_mask] = territory_id

                # Store info for CSV
                territories.append({
                    'id': territory_id,
                    'area': area,
                    'color': name,
                    'centroid_x': cx,
                    'centroid_y': cy,
                    'name': ''  # Empty for user to fill in
                })

                print(f"  Territory {territory_id:2d}: {area:>8,} px ({name})")

    print(f"\nTotal territories: {territory_id}")

    # Save the 8-bit index image
    index_path = "Images/Maps/territories_index.png"
    Image.fromarray(index_image).save(index_path)
    print(f"\nSaved 8-bit index image to: {index_path}")
    print("  Pixel values: 0 = background, 1-60 = territory IDs")

    # Also save a visualization where territories are more visible
    # (multiply by 4 so values 1-60 become 4-240, easier to see)
    viz_image = (index_image.astype(np.float32) * 4).clip(0, 255).astype(np.uint8)
    viz_path = "Images/Maps/territories_index_visible.png"
    Image.fromarray(viz_image).save(viz_path)
    print(f"Saved visible index image to: {viz_path}")
    print("  (Values multiplied by 4 for visibility)")

    # Save CSV file
    csv_path = "Images/Maps/territories.csv"
    with open(csv_path, 'w', newline='') as csvfile:
        writer = csv.writer(csvfile)
        # Header row
        writer.writerow(['ID', 'Area (pixels)', 'Color', 'Centroid X', 'Centroid Y', 'Name'])

        # Data rows
        for t in territories:
            writer.writerow([
                t['id'],
                t['area'],
                t['color'],
                f"{t['centroid_x']:.1f}",
                f"{t['centroid_y']:.1f}",
                t['name']
            ])

    print(f"\nSaved CSV file to: {csv_path}")
    print("  Columns: ID, Area, Color, Centroid X, Centroid Y, Name")
    print("  Open in Excel and fill in the 'Name' column for each territory")


if __name__ == "__main__":
    main()
