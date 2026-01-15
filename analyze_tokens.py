#!/usr/bin/env python3
"""
Analyze token data from grid simulations.
Computes higher order entropy for Von Neumann neighborhoods (r=10) around each cell.
"""

import csv
import brotli
import math
from pathlib import Path
import glob
from collections import defaultdict


def shannon_entropy(byte_string):
    """Compute Shannon entropy of a byte string."""
    if len(byte_string) == 0:
        return 0.0

    entropy = 0
    for byte in range(256):
        frequency = byte_string.count(byte) / len(byte_string)
        if frequency > 0:
            entropy += frequency * math.log(frequency, 2)
    return -entropy


def kolmogorov_complexity_estimate(byte_string):
    """
    Estimate Kolmogorov complexity using Brotli compression.
    Complexity of 8.0 means incompressible (0.0 bits saved per byte).
    Complexity of 0.0 means fully compressible (8.0 bits saved per byte).
    """
    if len(byte_string) == 0:
        return 0.0

    compressed = brotli.compress(byte_string)
    return len(compressed) / len(byte_string) * 8.0


def higher_order_entropy(byte_string):
    """Compute higher order entropy: Shannon entropy - Kolmogorov complexity estimate."""
    return shannon_entropy(byte_string) - kolmogorov_complexity_estimate(byte_string)


def get_von_neumann_neighbors(x, y, width, height, radius):
    """
    Get Von Neumann neighborhood (Manhattan distance <= radius) for a cell.

    Args:
        x, y: Cell coordinates
        width, height: Grid dimensions
        radius: Manhattan distance radius

    Returns:
        List of (x, y) tuples for neighbors within radius
    """
    neighbors = []

    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            # Calculate Manhattan distance
            manhattan_dist = abs(dx) + abs(dy)

            # Skip if outside radius or is the cell itself
            if manhattan_dist == 0 or manhattan_dist > radius:
                continue

            nx = x + dx
            ny = y + dy

            # Check bounds
            if 0 <= nx < width and 0 <= ny < height:
                neighbors.append((nx, ny))

    return neighbors


def read_token_csv(csv_path):
    """
    Read token CSV file and organize by grid position.

    Returns:
        (epoch, width, height, grid_programs)
        where grid_programs is dict: (x, y) -> list of char bytes
    """
    with open(csv_path, 'r') as f:
        reader = csv.DictReader(f)

        # Organize data by grid position
        grid_data = defaultdict(list)
        epoch = None
        max_x = 0
        max_y = 0

        for row in reader:
            if epoch is None:
                epoch = int(row['epoch_snapshot'])

            x = int(row['grid_x'])
            y = int(row['grid_y'])
            pos = int(row['pos_in_program'])
            char = int(row['char'])

            grid_data[(x, y)].append((pos, char))

            max_x = max(max_x, x)
            max_y = max(max_y, y)

    # Sort each program by position
    grid_programs = {}
    for (x, y), data in grid_data.items():
        data.sort(key=lambda item: item[0])
        grid_programs[(x, y)] = [char for pos, char in data]

    width = max_x + 1
    height = max_y + 1

    return epoch, width, height, grid_programs


def analyze_epoch(csv_path, radius=10):
    """
    Analyze one epoch's token data.

    Args:
        csv_path: Path to CSV file
        radius: Von Neumann neighborhood radius

    Returns:
        List of result dictionaries
    """
    print(f"Analyzing {csv_path}...")

    # Read CSV
    epoch, width, height, grid_programs = read_token_csv(csv_path)

    print(f"  Grid size: {width}x{height}, Epoch: {epoch}")

    # Results storage
    results = []

    # For each cell in the grid
    for y in range(height):
        for x in range(width):
            # Get Von Neumann neighbors
            neighbors = get_von_neumann_neighbors(x, y, width, height, radius)

            # Include the cell itself
            cells_to_analyze = [(x, y)] + neighbors

            # Collect all bytes from these cells
            neighborhood_bytes = bytearray()

            for cx, cy in cells_to_analyze:
                if (cx, cy) in grid_programs:
                    char_bytes = grid_programs[(cx, cy)]
                    neighborhood_bytes.extend(char_bytes)

            # Compute HOE for this neighborhood
            neighborhood_bytes = bytes(neighborhood_bytes)
            hoe = higher_order_entropy(neighborhood_bytes)

            results.append({
                'epoch': epoch,
                'grid_x': x,
                'grid_y': y,
                'hoe': hoe,
                'neighborhood_size': len(cells_to_analyze),
                'total_bytes': len(neighborhood_bytes)
            })

    print(f"  Computed HOE for {len(results)} cells")

    # Compute statistics
    hoe_values = [r['hoe'] for r in results]
    print(f"  HOE range: [{min(hoe_values):.4f}, {max(hoe_values):.4f}]")
    print(f"  HOE mean: {sum(hoe_values)/len(hoe_values):.4f}")

    return results


def save_results(all_results, output_path):
    """Save results to CSV."""
    with open(output_path, 'w', newline='') as f:
        fieldnames = ['epoch', 'grid_x', 'grid_y', 'hoe', 'neighborhood_size', 'total_bytes']
        writer = csv.DictWriter(f, fieldnames=fieldnames)

        writer.writeheader()
        for result in all_results:
            writer.writerow(result)


def main():
    """Main analysis function."""
    # Find all token CSV files
    token_dir = Path("data/tokens")
    csv_files = sorted(glob.glob(str(token_dir / "tokens_epoch_*.csv")))

    if not csv_files:
        print("No token CSV files found in data/tokens/")
        return

    print(f"Found {len(csv_files)} token files")
    print(f"Using Von Neumann neighborhood radius: 10")
    print()

    # Analyze each epoch
    all_results = []

    for csv_path in csv_files:
        results = analyze_epoch(csv_path, radius=10)
        all_results.extend(results)
        print()

    # Save results
    output_path = token_dir / "neighborhood_hoe_analysis.csv"
    save_results(all_results, output_path)
    print(f"Saved results to {output_path}")

    # Print summary statistics by epoch
    print("\n=== Summary Statistics by Epoch ===")
    epochs = sorted(set(r['epoch'] for r in all_results))

    for epoch in epochs:
        epoch_results = [r for r in all_results if r['epoch'] == epoch]
        hoe_values = [r['hoe'] for r in epoch_results]

        print(f"\nEpoch {epoch}:")
        print(f"  Count: {len(hoe_values)}")
        print(f"  Mean:  {sum(hoe_values)/len(hoe_values):.6f}")
        print(f"  Min:   {min(hoe_values):.6f}")
        print(f"  Max:   {max(hoe_values):.6f}")


if __name__ == "__main__":
    main()
