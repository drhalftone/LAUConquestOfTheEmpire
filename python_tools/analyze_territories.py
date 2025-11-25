#!/usr/bin/env python3
"""
Analyze territories.tif to find all territory regions.
- Snaps all pixels to nearest palette color (handles anti-aliasing)
- White pixels are background (ignored)
- Each connected component of a non-white color is a separate territory
"""

import cv2
import numpy as np
from PIL import Image

# Define the palette - the pure colors you used
PALETTE = {
    'Red':     (255,   0,   0),
    'Green':   (  0, 255,   0),
    'Blue':    (  0,   0, 255),
    'Yellow':  (255, 255,   0),
    'Cyan':    (  0, 255, 255),
    'Magenta': (255,   0, 255),
    'White':   (255, 255, 255),
}

def snap_to_palette(img):
    """Snap each pixel to the nearest palette color."""
    h, w = img.shape[:2]
    result = np.zeros_like(img)
    color_names = np.empty((h, w), dtype=object)

    # Convert palette to numpy array for vectorized distance calculation
    palette_colors = np.array(list(PALETTE.values()), dtype=np.float32)
    palette_names = list(PALETTE.keys())

    # Reshape image for broadcasting
    img_float = img.astype(np.float32)

    # Calculate distance to each palette color
    # img_float shape: (h, w, 3)
    # palette_colors shape: (7, 3)
    # We want distances shape: (h, w, 7)

    # Expand dimensions for broadcasting
    img_expanded = img_float[:, :, np.newaxis, :]  # (h, w, 1, 3)
    palette_expanded = palette_colors[np.newaxis, np.newaxis, :, :]  # (1, 1, 7, 3)

    # Calculate squared Euclidean distance
    distances = np.sum((img_expanded - palette_expanded) ** 2, axis=3)  # (h, w, 7)

    # Find index of closest palette color for each pixel
    closest_idx = np.argmin(distances, axis=2)  # (h, w)

    # Map back to colors
    for i, (name, color) in enumerate(PALETTE.items()):
        mask = closest_idx == i
        result[mask] = color
        color_names[mask] = name

    return result, color_names


def main():
    # Load the image
    img_path = "Images/Maps/territories.tif"
    print(f"Loading {img_path}...")

    pil_img = Image.open(img_path)
    img = np.array(pil_img)

    print(f"Image shape: {img.shape}")
    print(f"Image dtype: {img.dtype}")

    if len(img.shape) == 2:
        print("ERROR: Grayscale image detected, need RGB")
        return

    if img.shape[2] == 4:
        print("RGBA image - using RGB channels only")
        img = img[:, :, :3]

    # Count original unique colors
    pixels = img.reshape(-1, 3)
    original_unique = len(np.unique(pixels, axis=0))
    print(f"\nOriginal image has {original_unique} unique colors (due to anti-aliasing)")

    # Snap to palette
    print("\nSnapping all pixels to nearest palette color...")
    quantized, color_names = snap_to_palette(img)

    # Verify quantization
    quant_pixels = quantized.reshape(-1, 3)
    quant_unique = np.unique(quant_pixels, axis=0)
    print(f"After quantization: {len(quant_unique)} unique colors")

    # Count pixels per color
    print("\nPixel counts per palette color:")
    for name, color in PALETTE.items():
        mask = np.all(quantized == color, axis=2)
        count = np.sum(mask)
        pct = 100.0 * count / (img.shape[0] * img.shape[1])
        print(f"  {name:8s}: {count:>10,} pixels ({pct:.1f}%)")

    # Save quantized image for verification
    quantized_path = "Images/Maps/territories_quantized.png"
    Image.fromarray(quantized).save(quantized_path)
    print(f"\nSaved quantized image to: {quantized_path}")

    # Find connected components for each non-white color
    print("\n" + "="*60)
    print("Finding connected components (territories)...")
    print("="*60)

    all_territories = []
    territory_id = 0

    for name, color in PALETTE.items():
        if name == 'White':
            continue  # Skip background

        # Create binary mask for this color
        mask = np.all(quantized == color, axis=2).astype(np.uint8) * 255

        # Find connected components
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(mask, connectivity=8)

        num_regions = num_labels - 1  # Label 0 is background
        if num_regions > 0:
            print(f"\n{name}: {num_regions} territory/territories")

        for i in range(1, num_labels):
            territory_id += 1
            area = stats[i, cv2.CC_STAT_AREA]
            x = stats[i, cv2.CC_STAT_LEFT]
            y = stats[i, cv2.CC_STAT_TOP]
            w = stats[i, cv2.CC_STAT_WIDTH]
            h = stats[i, cv2.CC_STAT_HEIGHT]
            cx, cy = centroids[i]

            territory_info = {
                'id': territory_id,
                'color_name': name,
                'color_rgb': color,
                'area': area,
                'bbox': (x, y, w, h),
                'centroid': (cx, cy),
            }
            all_territories.append(territory_info)

            # Only print details for significant regions (filter out tiny noise)
            if area > 100:
                print(f"  #{territory_id:2d}: {area:>8,} px, centroid=({cx:7.1f}, {cy:7.1f}), bbox=({x},{y},{w}x{h})")
            else:
                print(f"  #{territory_id:2d}: {area:>8,} px (NOISE - very small)")

    # Summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)

    # Filter out noise (very small regions)
    significant = [t for t in all_territories if t['area'] > 1000]
    noise = [t for t in all_territories if t['area'] <= 1000]

    print(f"Total regions found: {len(all_territories)}")
    print(f"Significant territories (>1000 px): {len(significant)}")
    print(f"Noise regions (<=1000 px): {len(noise)}")

    # Sort by area
    significant.sort(key=lambda t: t['area'], reverse=True)

    print(f"\nTop 10 largest territories:")
    for i, t in enumerate(significant[:10]):
        print(f"  {i+1:2d}. #{t['id']:2d} {t['color_name']:8s}: {t['area']:>10,} px at ({t['centroid'][0]:.0f}, {t['centroid'][1]:.0f})")

    if noise:
        print(f"\nSmallest noise regions (may need cleanup):")
        noise.sort(key=lambda t: t['area'])
        for t in noise[:5]:
            print(f"  #{t['id']:2d} {t['color_name']:8s}: {t['area']:>6,} px")


if __name__ == "__main__":
    main()
