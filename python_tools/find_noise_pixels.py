#!/usr/bin/env python3
"""
Create a binary image showing pixels that are NOT part of significant territories.
- White pixels = noise (small regions that were filtered out)
- Black pixels = significant territories or background
"""

import cv2
import numpy as np
from PIL import Image

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


def main():
    # Load the image
    img_path = "Images/Maps/territories.tif"
    print(f"Loading {img_path}...")

    pil_img = Image.open(img_path)
    img = np.array(pil_img)

    if img.shape[2] == 4:
        img = img[:, :, :3]

    print(f"Image shape: {img.shape}")

    # Snap to palette
    print("Quantizing to palette colors...")
    quantized = snap_to_palette(img)

    h, w = quantized.shape[:2]

    # Create output images
    noise_mask = np.zeros((h, w), dtype=np.uint8)  # Binary: white = noise
    noise_highlighted = quantized.copy()  # Color image with noise highlighted

    total_noise_pixels = 0

    # Process each non-white color
    for name, color in PALETTE.items():
        if name == 'White':
            continue

        # Create binary mask for this color
        color_mask = np.all(quantized == color, axis=2).astype(np.uint8) * 255

        # Find connected components
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(color_mask, connectivity=8)

        # Mark small regions as noise
        for i in range(1, num_labels):
            area = stats[i, cv2.CC_STAT_AREA]
            if area <= MIN_TERRITORY_SIZE:
                # This is a noise region - mark it
                region_mask = (labels == i)
                noise_mask[region_mask] = 255
                noise_highlighted[region_mask] = [255, 255, 255]  # White overlay
                total_noise_pixels += area

    print(f"\nTotal noise pixels: {total_noise_pixels:,}")
    print(f"Percentage of image: {100.0 * total_noise_pixels / (h * w):.3f}%")

    # Save the binary noise mask
    noise_binary_path = "Images/Maps/noise_pixels_binary.png"
    Image.fromarray(noise_mask).save(noise_binary_path)
    print(f"\nSaved binary noise mask to: {noise_binary_path}")
    print("  (White = noise pixels, Black = valid territories or background)")

    # Save the highlighted version (noise shown as white on the quantized image)
    noise_highlighted_path = "Images/Maps/noise_pixels_highlighted.png"
    Image.fromarray(noise_highlighted).save(noise_highlighted_path)
    print(f"Saved highlighted version to: {noise_highlighted_path}")
    print("  (White overlay = noise pixels on top of quantized territories)")

    # Also create an inverted version showing VALID territories
    valid_mask = np.zeros((h, w, 3), dtype=np.uint8)

    for name, color in PALETTE.items():
        if name == 'White':
            continue

        color_mask = np.all(quantized == color, axis=2).astype(np.uint8) * 255
        num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(color_mask, connectivity=8)

        for i in range(1, num_labels):
            area = stats[i, cv2.CC_STAT_AREA]
            if area > MIN_TERRITORY_SIZE:
                # Valid territory - paint it with its color
                region_mask = (labels == i)
                valid_mask[region_mask] = color

    valid_territories_path = "Images/Maps/valid_territories_only.png"
    Image.fromarray(valid_mask).save(valid_territories_path)
    print(f"Saved valid territories only to: {valid_territories_path}")
    print("  (Only significant territories, noise removed)")


if __name__ == "__main__":
    main()
