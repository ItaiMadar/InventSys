"""Tkinter front end that composes configuration, grid, and mesh modules."""

from __future__ import annotations

import hashlib
import json
import queue
import secrets
import threading
import traceback
from pathlib import Path
import tkinter as tk
from tkinter import messagebox, scrolledtext, ttk

from PIL import Image, ImageDraw, ImageTk

from config import AppConfig, DEFAULT_CONFIG
import grid_generation as grid
import stl_mesh as mesh


class VisCryptoApp:
    def __init__(self, root: tk.Tk, config: AppConfig = DEFAULT_CONFIG) -> None:
        self.root = root
        self.config = config
        self.root.title("Printable Visual Cryptography")
        self.root.minsize(900, 760)
        self.preview_state: dict | None = None
        self.preview_photos: list[ImageTk.PhotoImage] = []
        self.worker_results: queue.Queue[tuple[bool, str]] = queue.Queue()
        self._build_variables()
        self._build_ui()
        self.root.after(100, self.update_preview)
        self.root.after(150, self._poll_worker_results)

    def _build_variables(self) -> None:
        defaults = self.config.grid
        self.plate_width = tk.StringVar(value=f"{defaults.plate_width_mm:g}")
        self.plate_height = tk.StringVar(value=f"{defaults.plate_height_mm:g}")
        self.dpi = tk.StringVar(value=f"{defaults.dpi:.3f}")
        self.key = tk.StringVar(value=secrets.token_hex(16))
        self.show_key = tk.BooleanVar(value=False)
        self.preview_scale = tk.DoubleVar(value=100.0)
        self.preview_scale_label = tk.StringVar(value="100%")
        self.status = tk.StringVar(value="Preparing preview…")

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=10)
        outer.grid(row=0, column=0, sticky="nsew")
        self.root.rowconfigure(0, weight=1)
        self.root.columnconfigure(0, weight=1)
        outer.rowconfigure(0, weight=1)
        outer.columnconfigure(1, weight=1)

        controls = ttk.Frame(outer)
        controls.grid(row=0, column=0, sticky="nsw", padx=(0, 12))
        previews = ttk.Frame(outer)
        previews.grid(row=0, column=1, sticky="nsew")
        previews.rowconfigure(2, weight=1)
        previews.columnconfigure(0, weight=1)

        source = ttk.LabelFrame(controls, text="Secret text", padding=8)
        source.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        self.text_widget = scrolledtext.ScrolledText(
            source, width=33, height=5, wrap="word"
        )
        self.text_widget.grid(row=0, column=0, sticky="ew")
        self.text_widget.insert("1.0", self.config.grid.default_text)

        dimensions = ttk.LabelFrame(controls, text="Plate", padding=8)
        dimensions.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        self._field(dimensions, 0, "Width (mm)", self.plate_width)
        self._field(dimensions, 1, "Height (mm)", self.plate_height)
        self._field(dimensions, 2, "Resolution (DPI)", self.dpi)

        security = ttk.LabelFrame(controls, text="Reusable key", padding=8)
        security.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        self.key_entry = ttk.Entry(security, textvariable=self.key, width=29, show="•")
        self.key_entry.grid(row=0, column=0, columnspan=2, sticky="ew")
        ttk.Checkbutton(
            security, text="Show", variable=self.show_key, command=self.toggle_key
        ).grid(row=1, column=0, sticky="w")
        ttk.Button(security, text="New random key", command=self.new_key).grid(
            row=1, column=1, sticky="e"
        )

        actions = ttk.Frame(controls)
        actions.grid(row=3, column=0, sticky="ew")
        self.preview_button = ttk.Button(
            actions, text="Preview", command=self.update_preview
        )
        self.preview_button.grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.generate_button = ttk.Button(
            actions, text="Generate STL", command=self.generate_stl
        )
        self.generate_button.grid(row=0, column=1, sticky="ew", padx=(4, 0))
        actions.columnconfigure((0, 1), weight=1)

        ttk.Label(previews, text="No files are written until Generate STL is clicked.").grid(
            row=0, column=0, sticky="w", pady=(0, 8)
        )
        scale_controls = ttk.Frame(previews)
        scale_controls.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        ttk.Label(scale_controls, text="Preview scale").grid(row=0, column=0, sticky="w")
        ttk.Scale(
            scale_controls,
            from_=10,
            to=100,
            variable=self.preview_scale,
            command=self._preview_scale_changed,
        ).grid(row=0, column=1, sticky="ew", padx=8)
        ttk.Label(scale_controls, textvariable=self.preview_scale_label, width=5).grid(
            row=0, column=2, sticky="e"
        )
        scale_controls.columnconfigure(1, weight=1)

        preview_grid = ttk.Frame(previews)
        preview_grid.grid(row=2, column=0, sticky="nsew")
        preview_grid.columnconfigure(0, weight=1)
        self.preview_labels: list[ttk.Label] = []
        for panel, title in enumerate(("Cipher plate", "Key plate", "Aligned overlay")):
            title_row = panel * 2
            image_row = title_row + 1
            ttk.Label(preview_grid, text=title, anchor="center").grid(
                row=title_row, column=0, sticky="ew", padx=4
            )
            label = ttk.Label(preview_grid, anchor="center", relief="solid")
            label.grid(row=image_row, column=0, sticky="nsew", padx=4, pady=(2, 6))
            preview_grid.rowconfigure(image_row, weight=1)
            self.preview_labels.append(label)
        ttk.Label(previews, textvariable=self.status, wraplength=850).grid(
            row=3, column=0, sticky="ew", pady=(10, 0)
        )

    @staticmethod
    def _field(parent, row: int, label: str, variable: tk.StringVar) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=2)
        ttk.Entry(parent, textvariable=variable, width=13).grid(
            row=row, column=1, sticky="ew", pady=2
        )

    def toggle_key(self) -> None:
        self.key_entry.configure(show="" if self.show_key.get() else "•")

    def new_key(self) -> None:
        self.key.set(secrets.token_hex(16))
        self.update_preview()

    def _preview_scale_changed(self, value: str) -> None:
        percent = round(float(value))
        self.preview_scale_label.set(f"{percent}%")
        if self.preview_state is not None:
            self._refresh_preview_images()

    def _layout(self) -> grid.GridLayout:
        try:
            width = float(self.plate_width.get())
            height = float(self.plate_height.get())
            dpi = float(self.dpi.get())
        except ValueError as exc:
            raise ValueError("Width, height, and DPI must be numbers.") from exc
        return grid.create_layout(width, height, dpi, self.config)

    def _plate_preview(
        self, pattern: list[list[bool]], layout: grid.GridLayout
    ) -> Image.Image:
        pitch = layout.subpixel_pitch_mm
        content_width = len(pattern[0]) * pitch
        content_height = len(pattern) * pitch
        border_x = (layout.plate_width_mm - content_width) / 2
        border_y = (layout.plate_height_mm - content_height) / 2
        display_scale = self.preview_scale.get() / 100.0
        pixels_per_mm = self.root.winfo_fpixels("1i") / 25.4 * display_scale
        output_width = max(1, round(layout.plate_width_mm * pixels_per_mm))
        output_height = max(1, round(layout.plate_height_mm * pixels_per_mm))
        image = Image.new("1", (output_width, output_height), 0)
        draw = ImageDraw.Draw(image)

        rib_cells = mesh.grid_cells(
            self.config.printer.nozzle_diameter_mm, layout.xy_step_mm
        )
        margin_low = rib_cells // 2 * layout.xy_step_mm
        margin_high = (rib_cells - rib_cells // 2) * layout.xy_step_mm
        for sy, row in enumerate(pattern):
            for sx, opaque in enumerate(row):
                if opaque:
                    continue
                x0 = border_x + sx * pitch + margin_low
                y0 = border_y + sy * pitch + margin_low
                x1 = border_x + (sx + 1) * pitch - margin_high
                y1 = border_y + (sy + 1) * pitch - margin_high
                draw.rectangle(
                    (
                        round(x0 * pixels_per_mm),
                        round(y0 * pixels_per_mm),
                        max(round(x0 * pixels_per_mm), round(x1 * pixels_per_mm) - 1),
                        max(round(y0 * pixels_per_mm), round(y1 * pixels_per_mm) - 1),
                    ),
                    fill=1,
                )
        notch = min(layout.orientation_notch_mm, border_x, border_y)
        notch_pixels = max(1, round(notch * pixels_per_mm))
        draw.rectangle((0, 0, notch_pixels - 1, notch_pixels - 1), fill=1)
        return image.convert("RGB")

    def _refresh_preview_images(self) -> None:
        if self.preview_state is None:
            return
        patterns = (
            self.preview_state["cipher"],
            self.preview_state["key_grid"],
            self.preview_state["overlay"],
        )
        layout = self.preview_state["layout"]
        images = [self._plate_preview(pattern, layout) for pattern in patterns]
        self.preview_photos = [ImageTk.PhotoImage(image) for image in images]
        for label, photo in zip(self.preview_labels, self.preview_photos):
            label.configure(image=photo)

    def update_preview(self) -> bool:
        try:
            layout = self._layout()
            secret = grid.render_text(self.text_widget.get("1.0", "end-1c"), layout)
            key = grid.key_bytes(self.key.get())
            cipher, key_grid = grid.construct_grids(secret, key)
            overlay = grid.aligned_overlay(cipher, key_grid)
            self.preview_state = {
                "layout": layout,
                "key": key,
                "cipher": cipher,
                "key_grid": key_grid,
                "overlay": overlay,
            }
            self._refresh_preview_images()
            self.status.set(
                f"2D preview ready — plate {layout.plate_width_mm:.2f} × "
                f"{layout.plate_height_mm:.2f} mm; {layout.logical_width} × "
                f"{layout.logical_height} pixels at {layout.actual_dpi:.3f} DPI."
            )
            return True
        except Exception as exc:
            self.preview_state = None
            self.status.set(f"Preview error: {exc}")
            messagebox.showerror("Preview error", str(exc), parent=self.root)
            return False

    def generate_stl(self) -> None:
        if not self.update_preview() or self.preview_state is None:
            return
        snapshot = dict(self.preview_state)
        self.preview_button.configure(state="disabled")
        self.generate_button.configure(state="disabled")
        self.status.set("Generating and validating STL meshes…")
        threading.Thread(target=self._stl_worker, args=(snapshot,), daemon=True).start()

    def _stl_worker(self, state: dict) -> None:
        try:
            output_directory = Path.cwd()
            prefix = self.config.mesh.output_prefix
            result = mesh.export_stl_pair(
                state["cipher"],
                state["key_grid"],
                state["layout"],
                output_directory,
                prefix,
                self.config,
            )
            images = {
                "cipher": grid.pattern_image(state["cipher"]),
                "key": grid.pattern_image(state["key_grid"]),
                "overlay": grid.pattern_image(state["overlay"]),
            }
            for name, image in images.items():
                image.save(output_directory / f"{prefix}_{name}.png")

            layout: grid.GridLayout = state["layout"]
            manifest = {
                "scheme": "2-of-2 visual cryptography; balanced 2x2 expansion",
                "dimensions_mm": [
                    layout.plate_width_mm,
                    layout.plate_height_mm,
                    result.cipher_geometry.thickness_mm,
                ],
                "logical_pixels": [layout.logical_width, layout.logical_height],
                "dpi": layout.actual_dpi,
                "key_sha256": hashlib.sha256(state["key"]).hexdigest(),
                "printer": {
                    "nozzle_mm": self.config.printer.nozzle_diameter_mm,
                    "layer_height_mm": self.config.printer.layer_height_mm,
                },
                "triangles": {
                    "cipher": result.cipher_triangles,
                    "key": result.key_triangles,
                },
                "solid_components": {"cipher": 1, "key": 1},
                "files": {
                    "cipher": str(result.cipher_path),
                    "key": str(result.key_path),
                },
            }
            manifest_path = output_directory / f"{prefix}_manifest.json"
            manifest_path.write_text(
                json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
            )
            self.worker_results.put(
                (True, f"Generated:\n{result.cipher_path}\n{result.key_path}")
            )
        except Exception as exc:
            self.worker_results.put((False, f"{exc}\n\n{traceback.format_exc()}"))

    def _poll_worker_results(self) -> None:
        try:
            while True:
                success, message = self.worker_results.get_nowait()
                self._generation_finished(success, message)
        except queue.Empty:
            pass
        self.root.after(150, self._poll_worker_results)

    def _generation_finished(self, success: bool, message: str) -> None:
        self.preview_button.configure(state="normal")
        self.generate_button.configure(state="normal")
        if success:
            self.status.set("STL generation complete.")
            messagebox.showinfo("STL generation complete", message, parent=self.root)
        else:
            self.status.set("STL generation failed.")
            messagebox.showerror("STL generation failed", message, parent=self.root)


def run() -> None:
    root = tk.Tk()
    VisCryptoApp(root)
    root.mainloop()


if __name__ == "__main__":
    run()
