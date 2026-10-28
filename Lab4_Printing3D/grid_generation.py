"""Constructs the secret image, then generates the cipher and key grids"""

from __future__ import annotations

import hashlib
import hmac
import math
from dataclasses import dataclass
from typing import Sequence

from PIL import Image, ImageDraw, ImageFont

from config import AppConfig, DEFAULT_CONFIG


BALANCED_PATTERNS: tuple[tuple[bool, bool, bool, bool], ...] = (
    (True, True, False, False),
    (False, False, True, True),
    (True, False, True, False),
    (False, True, False, True),
    (True, False, False, True),
    (False, True, True, False),
)


@dataclass(frozen=True)
class GridLayout:
    plate_width_mm: float
    plate_height_mm: float
    dpi: float
    logical_width: int
    logical_height: int
    subpixel_pitch_mm: float
    xy_step_mm: float
    minimum_border_mm: float
    orientation_notch_mm: float

    @property
    def actual_dpi(self) -> float:
        return 25.4 / (2.0 * self.subpixel_pitch_mm)


class HmacStream:
    """Deterministic cryptographic byte stream derived from a key."""

    def __init__(self, key: bytes) -> None:
        self.key = key
        self.counter = 0
        self.buffer = bytearray()

    def byte(self) -> int:
        if not self.buffer:
            block = hmac.new(
                self.key, self.counter.to_bytes(8, "big"), hashlib.sha256
            ).digest()
            self.counter += 1
            self.buffer.extend(block)
        value = self.buffer[0]
        del self.buffer[0]
        return value

    def choice_six(self) -> int:
        while True:
            value = self.byte()
            if value < 252:
                return value % 6


def _grid_cells(length_mm: float, step_mm: float, minimum: int = 1) -> int:
    return max(minimum, math.ceil(length_mm / step_mm - 1e-9))


def create_layout(
    width_mm: float,
    height_mm: float,
    dpi: float,
    config: AppConfig = DEFAULT_CONFIG,
) -> GridLayout:
    """Resolve physical dimensions and DPI into a printable logical grid."""
    if width_mm <= 0 or height_mm <= 0 or dpi <= 0:
        raise ValueError("Width, height, and DPI must be positive.")
    maximum = config.printer.maximum_dpi
    if dpi > maximum + 1e-9:
        raise ValueError(
            f"DPI cannot exceed {maximum:.3f} with a "
            f"{config.printer.nozzle_diameter_mm:.1f} mm nozzle."
        )

    border = config.grid.minimum_border_mm
    usable_width = width_mm - 2 * border
    usable_height = height_mm - 2 * border
    logical_width = math.floor(usable_width * dpi / 25.4 + 1e-9)
    logical_height = math.floor(usable_height * dpi / 25.4 + 1e-9)
    if logical_width < 1 or logical_height < 1:
        minimum = 25.4 / dpi + 2 * border
        raise ValueError(
            f"Width and height must each be at least {minimum:.2f} mm at {dpi:g} DPI."
        )

    target_pitch = 12.7 / dpi
    pitch_cells = _grid_cells(
        target_pitch, config.printer.max_xy_mesh_step_mm, minimum=2
    )
    xy_step = target_pitch / pitch_cells
    return GridLayout(
        plate_width_mm=width_mm,
        plate_height_mm=height_mm,
        dpi=dpi,
        logical_width=logical_width,
        logical_height=logical_height,
        subpixel_pitch_mm=target_pitch,
        xy_step_mm=xy_step,
        minimum_border_mm=border,
        orientation_notch_mm=config.grid.orientation_notch_mm,
    )


def _load_font(size: int):
    for name in ("DejaVuSans-Bold.ttf", "arialbd.ttf", "Arial Bold.ttf"):
        try:
            return ImageFont.truetype(name, size=size)
        except OSError:
            continue
    return ImageFont.load_default()


def render_text(text: str, layout: GridLayout) -> list[list[bool]]:
    """Rasterize centered text into the logical secret grid."""
    width, height = layout.logical_width, layout.logical_height
    canvas = Image.new("L", (width, height), 255)
    draw = ImageDraw.Draw(canvas)
    text = text or " "
    chosen_font = _load_font(max(1, height))
    for size in range(max(1, height * 2), 3, -1):
        font = _load_font(size)
        box = draw.multiline_textbbox((0, 0), text, font=font, spacing=1, align="center")
        if box[2] - box[0] <= width - 2 and box[3] - box[1] <= height - 2:
            chosen_font = font
            break
    box = draw.multiline_textbbox(
        (0, 0), text, font=chosen_font, spacing=1, align="center"
    )
    x = (width - (box[2] - box[0])) / 2 - box[0]
    y = (height - (box[3] - box[1])) / 2 - box[1]
    draw.multiline_text(
        (x, y), text, fill=0, font=chosen_font, spacing=1, align="center"
    )
    return [
        [canvas.getpixel((x, y)) < 128 for x in range(width)]
        for y in range(height)
    ]


def construct_grids(
    secret_pixels: Sequence[Sequence[bool]], key: bytes
) -> tuple[list[list[bool]], list[list[bool]]]:
    """Return ``(cipher_grid, key_grid)``.

    The key grid depends only on the key and grid dimensions, which allows one
    physical key plate to be reused with multiple same-sized cipher plates.
    """
    logical_height = len(secret_pixels)
    logical_width = len(secret_pixels[0])
    cipher = [[False] * (logical_width * 2) for _ in range(logical_height * 2)]
    key_grid = [[False] * (logical_width * 2) for _ in range(logical_height * 2)]
    stream = HmacStream(key)

    for y in range(logical_height):
        for x in range(logical_width):
            pattern = BALANCED_PATTERNS[stream.choice_six()]
            cipher_pattern = (
                tuple(not value for value in pattern)
                if secret_pixels[y][x]
                else pattern
            )
            for dy in range(2):
                for dx in range(2):
                    index = dy * 2 + dx
                    key_grid[y * 2 + dy][x * 2 + dx] = pattern[index]
                    cipher[y * 2 + dy][x * 2 + dx] = cipher_pattern[index]
    return cipher, key_grid


def aligned_overlay(
    cipher: Sequence[Sequence[bool]], key_grid: Sequence[Sequence[bool]]
) -> list[list[bool]]:
    return [
        [cipher[y][x] or key_grid[y][x] for x in range(len(cipher[0]))]
        for y in range(len(cipher))
    ]


def key_bytes(passphrase: str) -> bytes:
    return hashlib.sha256(passphrase.encode("utf-8")).digest()


def pattern_image(pattern: Sequence[Sequence[bool]], scale: int = 8) -> Image.Image:
    height, width = len(pattern), len(pattern[0])
    image = Image.new("1", (width, height), 1)
    pixels = image.load()
    for y, row in enumerate(pattern):
        for x, opaque in enumerate(row):
            pixels[x, y] = 0 if opaque else 1
    return image.resize((width * scale, height * scale), Image.Resampling.NEAREST)
