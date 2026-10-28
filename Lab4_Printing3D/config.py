"""Hardcoded defaults for the printable visual cryptography application."""

from __future__ import annotations

from dataclasses import dataclass, field


@dataclass(frozen=True)
class PrinterConfig:
    """Assumed FDM printer capabilities, in millimeters."""

    nozzle_diameter_mm: float = 0.4
    layer_height_mm: float = 0.2
    max_xy_mesh_step_mm: float = 0.2

    @property
    def maximum_dpi(self) -> float:
        # One logical pixel is two subpixels wide. Each subpixel needs a
        # nozzle-wide wall and a nozzle-wide aperture.
        return 25.4 / (4.0 * self.nozzle_diameter_mm)


@dataclass(frozen=True)
class GridConfig:
    """Defaults for the physical plate and encoded text grid."""

    plate_width_mm: float = 160
    plate_height_mm: float = 90
    dpi: float = 6.7
    minimum_border_mm: float = 3.0
    orientation_notch_mm: float = 3.0
    default_text: str = "TEXT"


@dataclass(frozen=True)
class MeshConfig:
    """Defaults for STL construction and output naming."""

    plate_thickness_mm: float = 1.0
    output_prefix: str = "visCrypt"


@dataclass(frozen=True)
class AppConfig:
    printer: PrinterConfig = field(default_factory=PrinterConfig)
    grid: GridConfig = field(default_factory=GridConfig)
    mesh: MeshConfig = field(default_factory=MeshConfig)


DEFAULT_CONFIG = AppConfig()
