"""Contiguous STL meshes from two-dimensional boolean grids."""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Sequence

from config import AppConfig, DEFAULT_CONFIG
from grid_generation import GridLayout


@dataclass(frozen=True)
class PlateGeometry:
    occupied: bytearray
    dims: tuple[int, int, int]
    axes: tuple[tuple[float, ...], tuple[float, ...], tuple[float, ...]]
    width_mm: float
    height_mm: float
    thickness_mm: float
    pitch_mm: float
    aperture_mm: float


@dataclass(frozen=True)
class ExportResult:
    cipher_path: Path
    key_path: Path
    cipher_triangles: int
    key_triangles: int
    cipher_geometry: PlateGeometry
    key_geometry: PlateGeometry


class BinaryStlWriter:
    def __init__(self, path: Path, label: str) -> None:
        self.file: BinaryIO = path.open("wb")
        self.file.write(label.encode("ascii", "replace")[:80].ljust(80, b"\0"))
        self.file.write(struct.pack("<I", 0))
        self.triangles = 0

    def triangle(
        self,
        normal: tuple[float, float, float],
        a: tuple[float, float, float],
        b: tuple[float, float, float],
        c: tuple[float, float, float],
    ) -> None:
        self.file.write(struct.pack("<12fH", *normal, *a, *b, *c, 0))
        self.triangles += 1

    def quad(
        self,
        normal: tuple[float, float, float],
        points: Sequence[tuple[float, float, float]],
    ) -> None:
        self.triangle(normal, points[0], points[1], points[2])
        self.triangle(normal, points[0], points[2], points[3])

    def close(self) -> int:
        self.file.seek(80)
        self.file.write(struct.pack("<I", self.triangles))
        self.file.close()
        return self.triangles


def grid_cells(length_mm: float, step_mm: float, minimum: int = 1) -> int:
    if length_mm <= 0 or step_mm <= 0:
        raise ValueError("Grid lengths and steps must be positive.")
    return max(minimum, math.ceil(length_mm / step_mm - 1e-9))


def build_plate(
    pattern: Sequence[Sequence[bool]],
    layout: GridLayout,
    config: AppConfig = DEFAULT_CONFIG,
) -> PlateGeometry:
    """Extrude a grid while enforcing one face-connected printable solid."""
    if not pattern or not pattern[0]:
        raise ValueError("Pattern cannot be empty.")
    if any(len(row) != len(pattern[0]) for row in pattern):
        raise ValueError("Pattern rows must have equal widths.")

    step = layout.xy_step_mm
    pitch_cells = grid_cells(layout.subpixel_pitch_mm, step, minimum=2)
    rib_cells = grid_cells(config.printer.nozzle_diameter_mm, step)
    if rib_cells >= pitch_cells:
        raise ValueError("The nozzle-wide rib leaves no printable aperture.")
    border_cells = grid_cells(layout.minimum_border_mm, step)
    layer_height = config.printer.layer_height_mm
    thickness_cells = grid_cells(config.mesh.plate_thickness_mm, layer_height)

    subpixel_height = len(pattern)
    subpixel_width = len(pattern[0])
    pitch = pitch_cells * step
    content_width = subpixel_width * pitch
    content_height = subpixel_height * pitch
    border_x = (layout.plate_width_mm - content_width) / 2
    border_y = (layout.plate_height_mm - content_height) / 2
    if (
        border_x + 1e-9 < layout.minimum_border_mm
        or border_y + 1e-9 < layout.minimum_border_mm
    ):
        raise ValueError("Plate dimensions leave insufficient border around the grid.")

    nx = border_cells * 2 + subpixel_width * pitch_cells
    ny = border_cells * 2 + subpixel_height * pitch_cells
    nz = thickness_cells
    occupied = bytearray(b"\x01") * (nx * ny * nz)

    margin_low = rib_cells // 2
    margin_high = rib_cells - margin_low
    for sy, row in enumerate(pattern):
        for sx, opaque in enumerate(row):
            if opaque:
                continue
            x0 = border_cells + sx * pitch_cells + margin_low
            x1 = border_cells + (sx + 1) * pitch_cells - margin_high
            y0 = border_cells + sy * pitch_cells + margin_low
            y1 = border_cells + (sy + 1) * pitch_cells - margin_high
            for z in range(nz):
                layer = nx * ny * z
                for y in range(y0, y1):
                    start = layer + nx * y + x0
                    occupied[start : start + (x1 - x0)] = b"\x00" * (x1 - x0)

    notch_x = min(
        border_cells,
        grid_cells(layout.orientation_notch_mm, border_x / border_cells),
    )
    notch_y = min(
        border_cells,
        grid_cells(layout.orientation_notch_mm, border_y / border_cells),
    )
    for z in range(nz):
        layer = nx * ny * z
        for y in range(ny - notch_y, ny):
            start = layer + nx * y
            occupied[start : start + notch_x] = b"\x00" * notch_x

    components = count_solid_components(occupied, (nx, ny, nz))
    if components != 1:
        raise ValueError(
            f"Plate must be one connected solid; found {components} components."
        )

    def axis_edges(
        border_mm: float, content_cells: int, total_mm: float
    ) -> tuple[float, ...]:
        edges = [0.0]
        border_step = border_mm / border_cells
        for _ in range(border_cells):
            edges.append(edges[-1] + border_step)
        for _ in range(content_cells):
            edges.append(edges[-1] + step)
        for _ in range(border_cells):
            edges.append(edges[-1] + border_step)
        edges[-1] = total_mm
        return tuple(edges)

    x_edges = axis_edges(
        border_x, subpixel_width * pitch_cells, layout.plate_width_mm
    )
    y_edges = axis_edges(
        border_y, subpixel_height * pitch_cells, layout.plate_height_mm
    )
    z_edges = tuple(index * layer_height for index in range(nz + 1))
    return PlateGeometry(
        occupied=occupied,
        dims=(nx, ny, nz),
        axes=(x_edges, y_edges, z_edges),
        width_mm=layout.plate_width_mm,
        height_mm=layout.plate_height_mm,
        thickness_mm=nz * layer_height,
        pitch_mm=pitch,
        aperture_mm=(pitch_cells - rib_cells) * step,
    )


def count_solid_components(occupied: bytearray, dims: tuple[int, int, int]) -> int:
    """Count six-neighbor components in the printable voxel grid."""
    nx, ny, nz = dims
    expected = nx * ny * nz
    if len(occupied) != expected:
        raise ValueError(f"Grid has {len(occupied)} cells; expected {expected}.")
    seen = bytearray(expected)
    components = 0
    layer_size = nx * ny
    for start, solid in enumerate(occupied):
        if not solid or seen[start]:
            continue
        components += 1
        stack = [start]
        seen[start] = 1
        while stack:
            index = stack.pop()
            z, within_layer = divmod(index, layer_size)
            y, x = divmod(within_layer, nx)
            neighbors = []
            if x > 0:
                neighbors.append(index - 1)
            if x + 1 < nx:
                neighbors.append(index + 1)
            if y > 0:
                neighbors.append(index - nx)
            if y + 1 < ny:
                neighbors.append(index + nx)
            if z > 0:
                neighbors.append(index - layer_size)
            if z + 1 < nz:
                neighbors.append(index + layer_size)
            for neighbor in neighbors:
                if occupied[neighbor] and not seen[neighbor]:
                    seen[neighbor] = 1
                    stack.append(neighbor)
    return components


def count_nonmanifold_edges(occupied: bytearray, dims: tuple[int, int, int]) -> int:
    nx, ny, nz = dims

    def sample(coord: Sequence[int]) -> int:
        x, y, z = coord
        if x < 0 or y < 0 or z < 0 or x >= nx or y >= ny or z >= nz:
            return 0
        return occupied[x + nx * (y + ny * z)]

    bad = 0
    for axis in range(3):
        u = (axis + 1) % 3
        v = (axis + 2) % 3
        for along in range(dims[axis]):
            for boundary_u in range(dims[u] + 1):
                for boundary_v in range(dims[v] + 1):
                    values = []
                    for offset_u, offset_v in ((-1, -1), (0, -1), (-1, 0), (0, 0)):
                        coordinate = [0, 0, 0]
                        coordinate[axis] = along
                        coordinate[u] = boundary_u + offset_u
                        coordinate[v] = boundary_v + offset_v
                        values.append(sample(coordinate))
                    a, b, c, d = values
                    if (a and d and not b and not c) or (b and c and not a and not d):
                        bad += 1
    return bad


def emit_voxel_mesh(geometry: PlateGeometry, writer: BinaryStlWriter) -> int:
    occupied, dims, axes = geometry.occupied, geometry.dims, geometry.axes
    nx, ny, nz = dims

    def sample(coord: Sequence[int]) -> int:
        x, y, z = coord
        if x < 0 or y < 0 or z < 0 or x >= nx or y >= ny or z >= nz:
            return 0
        return occupied[x + nx * (y + ny * z)]

    for axis in range(3):
        u = (axis + 1) % 3
        v = (axis + 2) % 3
        normal = [0.0, 0.0, 0.0]
        normal[axis] = 1.0
        for plane in range(dims[axis] + 1):
            for j in range(dims[v]):
                for i in range(dims[u]):
                    before = [0, 0, 0]
                    after = [0, 0, 0]
                    before[axis], after[axis] = plane - 1, plane
                    before[u] = after[u] = i
                    before[v] = after[v] = j
                    a, b = sample(before), sample(after)
                    if a == b:
                        continue
                    p = [0.0, 0.0, 0.0]
                    du = [0.0, 0.0, 0.0]
                    dv = [0.0, 0.0, 0.0]
                    p[axis] = axes[axis][plane]
                    p[u] = axes[u][i]
                    p[v] = axes[v][j]
                    du[u] = axes[u][i + 1] - axes[u][i]
                    dv[v] = axes[v][j + 1] - axes[v][j]
                    p0 = tuple(p)
                    p1 = tuple(p[k] + du[k] for k in range(3))
                    p2 = tuple(p[k] + du[k] + dv[k] for k in range(3))
                    p3 = tuple(p[k] + dv[k] for k in range(3))
                    if a:
                        writer.quad(tuple(normal), (p0, p1, p2, p3))
                    else:
                        writer.quad(tuple(-value for value in normal), (p0, p3, p2, p1))
    return writer.triangles


def _write_geometry(path: Path, geometry: PlateGeometry, label: str) -> int:
    writer = BinaryStlWriter(path, label)
    try:
        emit_voxel_mesh(geometry, writer)
        return writer.close()
    except Exception:
        writer.file.close()
        path.unlink(missing_ok=True)
        raise


def export_stl_pair(
    cipher: Sequence[Sequence[bool]],
    key_grid: Sequence[Sequence[bool]],
    layout: GridLayout,
    output_directory: Path,
    prefix: str,
    config: AppConfig = DEFAULT_CONFIG,
) -> ExportResult:
    """Build, validate, and write both STL plates."""
    output_directory.mkdir(parents=True, exist_ok=True)
    cipher_geometry = build_plate(cipher, layout, config)
    key_geometry = build_plate(key_grid, layout, config)
    for label, geometry in (("cipher", cipher_geometry), ("key", key_geometry)):
        bad = count_nonmanifold_edges(geometry.occupied, geometry.dims)
        if bad:
            raise ValueError(f"{label} plate contains {bad} non-manifold edges.")

    cipher_path = output_directory / f"{prefix}_cipher.stl"
    key_path = output_directory / f"{prefix}_key.stl"
    cipher_triangles = _write_geometry(
        cipher_path, cipher_geometry, "Visual cryptography cipher plate"
    )
    key_triangles = _write_geometry(
        key_path, key_geometry, "Visual cryptography key plate"
    )
    return ExportResult(
        cipher_path=cipher_path,
        key_path=key_path,
        cipher_triangles=cipher_triangles,
        key_triangles=key_triangles,
        cipher_geometry=cipher_geometry,
        key_geometry=key_geometry,
    )
