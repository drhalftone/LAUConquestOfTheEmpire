#!/usr/bin/env python3
"""
Extract territory neighbors by tracing boundary contours clockwise.
Starting from 12 o'clock position, record neighbors in order as they appear.
Only count a neighbor if it appears for at least MIN_CONSECUTIVE pixels.
Same neighbor can appear multiple times if boundary touches it in separate sections.
"""

import cv2
import numpy as np
from PIL import Image
import csv
import math

# Minimum consecutive pixels to count as a valid neighbor segment
MIN_CONSECUTIVE = 10


def find_12_oclock_index(contour, centroid):
    """
    Find the index of the contour point closest to 12 o'clock.
    12 o'clock = directly above the centroid (minimum y, closest to centroid x).
    """
    cx, cy = centroid

    # Find the topmost points (minimum y)
    min_y = min(pt[0][1] for pt in contour)

    # Among topmost points, find the one closest to centroid x
    best_idx = 0
    best_dist = float('inf')

    for i, pt in enumerate(contour):
        x, y = pt[0]
        if y == min_y:
            dist = abs(x - cx)
            if dist < best_dist:
                best_dist = dist
                best_idx = i

    return best_idx


def get_neighbor_at_boundary(index_img, x, y, territory_id):
    """
    For a boundary pixel at (x,y) belonging to territory_id,
    find which territory is on the outside (the neighbor).
    Returns the neighbor territory ID, or 0 if only background.
    """
    h, w = index_img.shape

    # Check 8-connected neighbors, return first non-self, non-background neighbor
    for dy in [-1, 0, 1]:
        for dx in [-1, 0, 1]:
            if dy == 0 and dx == 0:
                continue
            nx, ny = x + dx, y + dy
            if 0 <= nx < w and 0 <= ny < h:
                neighbor_val = index_img[ny, nx]
                if neighbor_val != territory_id and neighbor_val != 0:
                    return neighbor_val

    return 0  # No neighbor (only background)


def trace_boundary_neighbors(index_img, territory_id, contour, centroid):
    """
    Trace the boundary contour clockwise from 12 o'clock.
    For each pixel, record the neighbor.
    Return list of (neighbor_id, count) for each segment.
    """
    if len(contour) == 0:
        return []

    # Find starting index (12 o'clock)
    start_idx = find_12_oclock_index(contour, centroid)

    # Reorder contour to start from 12 o'clock
    n = len(contour)
    ordered_contour = [contour[(start_idx + i) % n] for i in range(n)]

    # OpenCV contours are counter-clockwise by default, reverse for clockwise
    ordered_contour = ordered_contour[::-1]

    # Walk the contour and record neighbor for each pixel
    pixel_neighbors = []
    for pt in ordered_contour:
        x, y = pt[0]
        neighbor = get_neighbor_at_boundary(index_img, x, y, territory_id)
        pixel_neighbors.append(neighbor)

    # Now group consecutive identical neighbors and count them
    segments = []  # List of (neighbor_id, count)
    if not pixel_neighbors:
        return []

    current_neighbor = pixel_neighbors[0]
    current_count = 1

    for i in range(1, len(pixel_neighbors)):
        if pixel_neighbors[i] == current_neighbor:
            current_count += 1
        else:
            segments.append((current_neighbor, current_count))
            current_neighbor = pixel_neighbors[i]
            current_count = 1

    # Don't forget the last segment
    segments.append((current_neighbor, current_count))

    # Handle wrap-around: if first and last segments have same neighbor, merge them
    if len(segments) > 1 and segments[0][0] == segments[-1][0]:
        merged_count = segments[0][1] + segments[-1][1]
        segments = segments[1:-1]  # Remove first and last
        segments.append((segments[-1][0] if segments else pixel_neighbors[0], merged_count))
        # Actually, let's be more careful - put merged segment at the start
        segments = [(pixel_neighbors[0], merged_count)] + segments[:-1] if segments else [(pixel_neighbors[0], merged_count)]

    return segments


def filter_segments(segments, min_count):
    """
    Filter out segments with fewer than min_count consecutive pixels.
    Return list of neighbor IDs in order.
    """
    filtered = []
    for neighbor_id, count in segments:
        if neighbor_id != 0 and count >= min_count:
            filtered.append(neighbor_id)
    return filtered


def main():
    # Load the index image
    index_path = "Images/Maps/territories_index.png"
    print(f"Loading {index_path}...")

    index_img = np.array(Image.open(index_path))
    h, w = index_img.shape
    print(f"Image size: {w} x {h}")
    print(f"Minimum consecutive pixels for valid neighbor: {MIN_CONSECUTIVE}")

    # Find all territory IDs
    territory_ids = sorted([v for v in np.unique(index_img) if v > 0])
    print(f"Found {len(territory_ids)} territories")

    # Process each territory
    results = []

    for tid in territory_ids:
        print(f"\nProcessing territory {tid}...")

        # Create binary mask for this territory
        mask = (index_img == tid).astype(np.uint8) * 255

        # Find contours
        contours, hierarchy = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)

        if not contours:
            print(f"  WARNING: No contour found for territory {tid}")
            results.append({'id': tid, 'neighbors': [], 'segments': []})
            continue

        # Use the largest contour (in case of multiple)
        contour = max(contours, key=cv2.contourArea)

        # Calculate centroid
        M = cv2.moments(contour)
        if M['m00'] > 0:
            cx = M['m10'] / M['m00']
            cy = M['m01'] / M['m00']
        else:
            cx, cy = contour[0][0]

        print(f"  Contour points: {len(contour)}, centroid: ({cx:.1f}, {cy:.1f})")

        # Trace boundary and get neighbor segments with counts
        segments = trace_boundary_neighbors(index_img, tid, contour, (cx, cy))

        # Filter to only keep segments with enough consecutive pixels
        neighbors = filter_segments(segments, MIN_CONSECUTIVE)

        # Remove consecutive duplicates (shouldn't happen after filtering, but just in case)
        cleaned_neighbors = []
        for n in neighbors:
            if not cleaned_neighbors or cleaned_neighbors[-1] != n:
                cleaned_neighbors.append(n)

        # Check if first and last are same (wrap-around duplicate)
        if len(cleaned_neighbors) > 1 and cleaned_neighbors[0] == cleaned_neighbors[-1]:
            cleaned_neighbors = cleaned_neighbors[:-1]

        results.append({'id': tid, 'neighbors': cleaned_neighbors, 'segments': segments})

        # Show raw segments for debugging
        valid_segs = [(n, c) for n, c in segments if n != 0]
        print(f"  Raw segments: {valid_segs}")
        print(f"  Filtered neighbors (>={MIN_CONSECUTIVE}px): {cleaned_neighbors}")

    # Save to CSV
    csv_path = "Images/Maps/territory_neighbors.csv"
    with open(csv_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Territory ID', 'Neighbors (clockwise from 12 o\'clock)'])

        for r in results:
            # Join neighbors with semicolons
            neighbor_str = ';'.join(map(str, r['neighbors']))
            writer.writerow([r['id'], neighbor_str])

    print(f"\n\nSaved neighbor data to: {csv_path}")

    # Also print a summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    for r in results:
        n_list = r['neighbors']
        unique_count = len(set(n_list))
        print(f"Territory {r['id']:2d}: {len(n_list)} neighbor segments, {unique_count} unique neighbors -> {n_list}")


if __name__ == "__main__":
    main()
