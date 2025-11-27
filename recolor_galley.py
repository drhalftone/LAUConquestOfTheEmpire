#!/usr/bin/env python3
"""
Recolor galley icon from white sail to player colors.
Takes the galley icon with white sail and creates versions for each player color.
"""

import os
import sys
from PIL import Image
import colorsys
import numpy as np

# Player colors (matching Qt code in gamemapwidget.cpp)
PLAYER_COLORS = {
    'red': (255, 0, 0),
    'blue': (0, 0, 255),
    'green': (0, 200, 0),
    'yellow': (255, 255, 0),
    'orange': (255, 165, 0),
    'black': (128, 128, 128),  # Gray for visibility
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

def is_white_or_light(r, g, b):
    """Check if a color is white or light (low saturation, high brightness)"""
    h, s, v = rgb_to_hsv(r, g, b)
    # White/light colors have low saturation and high value
    # Also catch light grays
    is_light = v > 0.7 and s < 0.3
    # Also catch near-white colors
    is_near_white = r > 200 and g > 200 and b > 200
    return is_light or is_near_white

def get_target_hsv(color_name):
    """Get the target HSV for a player color"""
    r, g, b = PLAYER_COLORS[color_name]
    h, s, v = rgb_to_hsv(r, g, b)
    return h, s, v

def recolor_image(input_path, output_path, target_color_name):
    """Recolor white/light pixels in an image to the target color"""
    img = Image.open(input_path).convert('RGBA')
    pixels = np.array(img)

    target_hue, target_sat, target_val = get_target_hsv(target_color_name)
    is_gray = (target_color_name == 'black')

    # Process each pixel
    height, width = pixels.shape[:2]
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[y, x]

            # Skip transparent pixels
            if a < 10:
                continue

            # Check if this pixel is white/light
            if is_white_or_light(r, g, b):
                h, s, v = rgb_to_hsv(r, g, b)

                if is_gray:
                    # For gray, desaturate and darken
                    gray = int(v * 180)
                    pixels[y, x] = [gray, gray, gray, a]
                else:
                    # Apply target hue, blend saturation, preserve relative brightness
                    # Use a moderate saturation for the colored sail
                    new_s = 0.6 + (1.0 - v) * 0.3  # More saturated for darker parts
                    new_v = v * 0.9  # Slightly darken to make color visible

                    new_r, new_g, new_b = hsv_to_rgb(target_hue, new_s, new_v)
                    pixels[y, x] = [new_r, new_g, new_b, a]

    # Save result
    result = Image.fromarray(pixels, 'RGBA')
    result.save(output_path)
    print(f"  Created: {output_path}")

def main():
    input_path = "images/galleyIcon.png"
    output_dir = "images/colored"

    if len(sys.argv) > 1:
        input_path = sys.argv[1]
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]

    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)

    basename = os.path.basename(input_path)
    name, ext = os.path.splitext(basename)

    print(f"Processing: {basename}")

    for color_name in PLAYER_COLORS:
        output_filename = f"{name}_{color_name}{ext}"
        output_path = os.path.join(output_dir, output_filename)
        recolor_image(input_path, output_path, color_name)

    print("\nDone!")

if __name__ == '__main__':
    main()
