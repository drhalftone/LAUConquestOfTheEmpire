#!/usr/bin/env python3
"""
Recolor unit icons from red to player colors.
Takes icons with red hue and creates versions for each player color.
"""

import os
import sys
from PIL import Image
import colorsys
import numpy as np

# Player colors (matching Qt code in gamemapwidget.cpp and player.cpp)
PLAYER_COLORS = {
    'red': (255, 0, 0),      # Player A
    'green': (0, 255, 0),    # Player B
    'blue': (0, 0, 255),     # Player C
    'yellow': (255, 255, 0), # Player D
    'gray': (128, 128, 128), # Player E (Black would be invisible)
    'orange': (255, 165, 0), # Player F
}

def rgb_to_hsv(r, g, b):
    """Convert RGB (0-255) to HSV (0-360, 0-1, 0-1)"""
    r, g, b = r / 255.0, g / 255.0, b / 255.0
    h, s, v = colorsys.rgb_to_hsv(r, g, b)
    return h * 360, s, v

def hsv_to_rgb(h, s, v):
    """Convert HSV (0-360, 0-1, 0-1) to RGB (0-255)"""
    h = h / 360.0
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return int(r * 255), int(g * 255), int(b * 255)

def is_red_hue(h, s, v):
    """Check if a color has a red hue (with sufficient saturation and brightness)"""
    # Red hue is around 0° or 360° (wraps around)
    # Allow range of roughly 0-30° and 330-360°
    is_red = (h <= 30 or h >= 330)
    has_saturation = s > 0.2  # Must have some color
    has_brightness = v > 0.1  # Must not be too dark
    return is_red and has_saturation and has_brightness

def get_target_hue(color_name):
    """Get the target hue for a player color"""
    r, g, b = PLAYER_COLORS[color_name]
    h, s, v = rgb_to_hsv(r, g, b)
    return h, s

def recolor_image(input_path, output_path, target_color_name):
    """Recolor red pixels in an image to the target color"""
    img = Image.open(input_path).convert('RGBA')
    pixels = np.array(img)

    target_hue, target_base_sat = get_target_hue(target_color_name)
    is_black = (target_color_name == 'black')

    # Process each pixel
    height, width = pixels.shape[:2]
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[y, x]

            # Skip transparent pixels
            if a < 10:
                continue

            h, s, v = rgb_to_hsv(r, g, b)

            # Check if this pixel has a red hue
            if is_red_hue(h, s, v):
                if is_black:
                    # For black/gray, desaturate completely and use value for gray level
                    gray = int(v * 180)  # Scale to max 180 for dark gray look
                    pixels[y, x] = [gray, gray, gray, a]
                else:
                    # Shift hue to target color while preserving saturation and value
                    # Blend the saturation slightly toward the target color's saturation
                    new_s = s * 0.7 + target_base_sat * 0.3
                    new_s = min(1.0, new_s)

                    new_r, new_g, new_b = hsv_to_rgb(target_hue, new_s, v)
                    pixels[y, x] = [new_r, new_g, new_b, a]

    # Save result
    result = Image.fromarray(pixels, 'RGBA')
    result.save(output_path)
    print(f"  Created: {output_path}")

def process_icon(input_path, output_dir):
    """Process a single icon, creating versions for all player colors"""
    basename = os.path.basename(input_path)
    name, ext = os.path.splitext(basename)

    print(f"Processing: {basename}")

    for color_name in PLAYER_COLORS:
        output_filename = f"{name}_{color_name}{ext}"
        output_path = os.path.join(output_dir, output_filename)
        recolor_image(input_path, output_path, color_name)

def main():
    if len(sys.argv) < 2:
        print("Usage: python recolor_icons.py <icon_file_or_directory> [output_directory]")
        print("\nExample:")
        print("  python recolor_icons.py images/infantryIcon.png")
        print("  python recolor_icons.py images/ images/colored/")
        sys.exit(1)

    input_path = sys.argv[1]
    output_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(input_path) or '.'

    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)

    if os.path.isfile(input_path):
        # Process single file
        process_icon(input_path, output_dir)
    elif os.path.isdir(input_path):
        # Process all PNG files in directory
        for filename in os.listdir(input_path):
            if filename.lower().endswith('.png') and 'Icon' in filename:
                filepath = os.path.join(input_path, filename)
                process_icon(filepath, output_dir)
    else:
        print(f"Error: {input_path} is not a valid file or directory")
        sys.exit(1)

    print("\nDone!")

if __name__ == '__main__':
    main()
