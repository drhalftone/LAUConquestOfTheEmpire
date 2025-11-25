#!/usr/bin/env python3
"""
Re-analyze territories.tif and clean up any remaining noise by
filling small regions with neighboring pixel colors.
"""

import cv2
import numpy as np
from PIL import Image
from collections import Counter

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

MIN_TERRITORY_SIZE = 1000  # Minimum pixels to be considered a valid territory


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


def get_neighbor_colors(img, y, x, radius=3):
    """Get colors of neighboring pixels within radius."""
    h, w = img.shape[:2]
    colors = []

    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            if dy == 0 and dx == 0:
                continue
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w:
                color = tuple(img[ny, nx])
                colors.append(color)

    return colors


def fill_noise_with_neighbors(quantized, noise_mask):
    """Fill noise pixels with the most common neighboring color."""
    result = quantized.copy()
    h, w = quantized.shape[:2]

    # Get coordinates of noise pixels
    noise_coords = np.where(noise_mask > 0)
    noise_pixels = list(zip(noise_coords[0], noise_coords[1]))

    print(f"Filling {len(noise_pixels)} noise pixels...")

    filled_count = 0
    for y, x in noise_pixels:
        neighbor_colors = get_neighbor_colors(quantized, y, x, radius=5)

        # Filter out the current pixel's color and white (background)
        current_color = tuple(quantized[y, x])
        valid_colors = [c for c in neighbor_colors
                       if c != current_color and c != (255, 255, 255)]

        if valid_colors:
            # Use the most common neighbor color
            most_common = Counter(valid_colors).most_common(1)[0][0]
            result[y, x] = most_common
            filled_count += 1
        else:
            # No valid neighbors, try white as fallback
            result[y, x] = (255, 255, 255)

    print(f"Filled {filled_count} pixels with neighbor colors")
    return result


def main():
    # Load the image
    img_path = "Images/Maps/territories.tif"
    print(f"Loading {img_path}...")

    pil_img = Image.open(img_path)
    img = np.array(pil_img)

    if img.shape[2] == 4:
        img = img[:, :, :3]

    print(f"Image shape: {img.shape}")

    # Count original unique colors
    pixels = img.reshape(-1, 3)
    original_unique = len(np.unique(pixels, axis=0))
    print(f"Original unique colors: {original_unique}")

    # Snap to palette
    print("\nQuantizing to palette colors...")
    quantized = snap_to_palette(img)

    # First pass: identify noise regions
    print("\nIdentifying noise regions...")
    h, w = quantized.shape[:2]
    noise_mask = np.zeros((h, w), dtype=np.uint8)

    noise_count = 0
    for name, color in PALETTE.items():
        if name == 'White':
            continue

        color_mask = np.all(quantized == color, axis=2).astype(np.uint8) * 255
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(color_mask, connectivity=8)

        for i in range(1, num_labels):
            area = stats[i, cv2.CC_STAT_AREA]
            if area <= MIN_TERRITORY_SIZE:
                region_mask = (labels == i)
                noise_mask[region_mask] = 255
                noise_count += 1

    total_noise_pixels = np.sum(noise_mask > 0)
    print(f"Found {noise_count} noise regions ({total_noise_pixels} pixels)")

    if total_noise_pixels > 0:
        # Fill noise with neighboring colors
        print("\nFilling noise with neighboring colors...")
        cleaned = fill_noise_with_neighbors(quantized, noise_mask)
    else:
        print("\nNo noise to clean!")
        cleaned = quantized

    # Save cleaned image
    cleaned_path = "Images/Maps/territories_cleaned.png"
    Image.fromarray(cleaned).save(cleaned_path)
    print(f"\nSaved cleaned image to: {cleaned_path}")

    # Re-analyze the cleaned image
    print("\n" + "="*60)
    print("Re-analyzing cleaned image...")
    print("="*60)

    all_territories = []
    territory_id = 0

    print("\nPixel counts per palette color:")
    for name, color in PALETTE.items():
        mask = np.all(cleaned == color, axis=2)
        count = np.sum(mask)
        pct = 100.0 * count / (h * w)
        print(f"  {name:8s}: {count:>10,} pixels ({pct:.1f}%)")

    for name, color in PALETTE.items():
        if name == 'White':
            continue

        color_mask = np.all(cleaned == color, axis=2).astype(np.uint8) * 255
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(color_mask, connectivity=8)

        num_regions = num_labels - 1
        if num_regions > 0:
            print(f"\n{name}: {num_regions} territory/territories")

        for i in range(1, num_labels):
            territory_id += 1
            area = stats[i, cv2.CC_STAT_AREA]
            x = stats[i, cv2.CC_STAT_LEFT]
            y = stats[i, cv2.CC_STAT_TOP]
            w_box = stats[i, cv2.CC_STAT_WIDTH]
            h_box = stats[i, cv2.CC_STAT_HEIGHT]
            cx, cy = centroids[i]

            territory_info = {
                'id': territory_id,
                'color_name': name,
                'color_rgb': color,
                'area': area,
                'bbox': (x, y, w_box, h_box),
                'centroid': (cx, cy),
            }
            all_territories.append(territory_info)

            if area > MIN_TERRITORY_SIZE:
                print(f"  #{territory_id:2d}: {area:>8,} px, centroid=({cx:7.1f}, {cy:7.1f})")
            else:
                print(f"  #{territory_id:2d}: {area:>8,} px (STILL NOISE)")

    # Summary
    print("\n" + "="*60)
    print("FINAL SUMMARY")
    print("="*60)

    significant = [t for t in all_territories if t['area'] > MIN_TERRITORY_SIZE]
    remaining_noise = [t for t in all_territories if t['area'] <= MIN_TERRITORY_SIZE]

    print(f"Total regions: {len(all_territories)}")
    print(f"Valid territories: {len(significant)}")
    print(f"Remaining noise regions: {len(remaining_noise)}")

    if remaining_noise:
        print("\nRemaining noise (may need manual cleanup):")
        for t in remaining_noise[:10]:
            print(f"  #{t['id']} {t['color_name']}: {t['area']} px at ({t['centroid'][0]:.0f}, {t['centroid'][1]:.0f})")


if __name__ == "__main__":
    main()
