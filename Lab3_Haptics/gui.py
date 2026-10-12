"""Local chess GUI with live hover-to-vibration feedback."""

from __future__ import annotations

import argparse
from concurrent.futures import Future, ThreadPoolExecutor
from dataclasses import dataclass
import tkinter as tk
from tkinter import messagebox, ttk

import chess

from engine import AnalysisConfig, MoveEvaluation, StockfishEvaluator
from haptics import (
    HapticSettings,
    HapticSignal,
    build_move_signals,
    build_piece_signals,
)
from serial_transport import SerialVibrationController, available_ports


BOARD_PIXELS = 640
LIGHT_SQUARE = "#f0d9b5"
DARK_SQUARE = "#b58863"
SELECTED_SQUARE = "#6fa8dc"
LAST_MOVE_SQUARE = "#d6cf65"
LEGAL_TARGET = "#417d48"

PIECE_GLYPHS = {
    "P": "♙",
    "N": "♘",
    "B": "♗",
    "R": "♖",
    "Q": "♕",
    "K": "♔",
    "p": "♟",
    "n": "♞",
    "b": "♝",
    "r": "♜",
    "q": "♛",
    "k": "♚",
}


@dataclass
class EngineTask:
    future: Future
    kind: str
    fen: str
    generation: int


class ChessVibrationGui:
    def __init__(self, root: tk.Tk, engine_path: str | None = None) -> None:
        self.root = root
        self.root.title("Lab3 Haptics V1")
        self.root.resizable(False, False)

        self.board = chess.Board()
        self.human_color = chess.WHITE
        self.selected_square: chess.Square | None = None
        self.last_move: chess.Move | None = None
        self.hover_square: chess.Square | None = None
        self.position_generation = 0
        self.evaluations: list[MoveEvaluation] = []
        self.evaluation_by_move: dict[str, MoveEvaluation] = {}
        self.move_signals: dict[str, HapticSignal] = {}
        self.piece_signals: dict[str, HapticSignal] = {}
        self.analysis_ready = False
        self.closing = False

        self.engine_executor = ThreadPoolExecutor(
            max_workers=1, thread_name_prefix="lab3-stockfish"
        )
        self.serial_executor = ThreadPoolExecutor(
            max_workers=1, thread_name_prefix="lab3-serial"
        )
        self.evaluator = StockfishEvaluator(
            engine_path,
            AnalysisConfig(top_n=5, time_ms=300, threads=1, hash_mb=64),
        )
        self.engine_task: EngineTask | None = None
        self.serial_task: Future[None] | None = None
        self.vibration = SerialVibrationController()
        self.active_signal: HapticSignal | None = None

        self.settings = HapticSettings()
        self.analysis_time_ms = 300
        self.engine_time_ms = 400
        self.bot_skill = 10
        self.port_lookup: dict[str, str] = {}

        self._create_variables()
        self._build_layout()
        self._refresh_ports()
        self._draw_board()

        self.root.protocol("WM_DELETE_WINDOW", self._close)
        self.root.after(50, self._poll_background_tasks)
        self.root.after(750, self._refresh_haptic_watchdog)
        self._begin_turn()

    def _create_variables(self) -> None:
        self.color_var = tk.StringVar(value="White")
        self.top_n_var = tk.StringVar(value="5")
        self.analysis_ms_var = tk.StringVar(value="300")
        self.engine_ms_var = tk.StringVar(value="400")
        self.bot_skill_var = tk.StringVar(value="10")
        self.frequency_weak_var = tk.StringVar(value="2.0")
        self.frequency_strong_var = tk.StringVar(value="10.0")
        self.amplitude_weak_var = tk.StringVar(value="0.15")
        self.amplitude_strong_var = tk.StringVar(value="1.0")
        self.port_var = tk.StringVar()
        self.game_status_var = tk.StringVar(value="Starting Stockfish…")
        self.connection_status_var = tk.StringVar(value="Motor: disconnected")
        self.haptic_status_var = tk.StringVar(value="Haptic: off")

    def _build_layout(self) -> None:
        outer = ttk.Frame(self.root, padding=10)
        outer.grid(sticky="nsew")

        self.canvas = tk.Canvas(
            outer,
            width=BOARD_PIXELS,
            height=BOARD_PIXELS,
            highlightthickness=0,
        )
        self.canvas.grid(row=0, column=0, rowspan=2, sticky="n")
        self.canvas.bind("<Button-1>", self._on_board_click)
        self.canvas.bind("<Motion>", self._on_board_motion)
        self.canvas.bind("<Leave>", self._on_board_leave)

        controls = ttk.Frame(outer, padding=(14, 0, 0, 0))
        controls.grid(row=0, column=1, sticky="nsew")

        game_box = ttk.LabelFrame(controls, text="Game", padding=8)
        game_box.grid(row=0, column=0, sticky="ew")
        ttk.Label(game_box, text="Play as").grid(row=0, column=0, sticky="w")
        ttk.Combobox(
            game_box,
            textvariable=self.color_var,
            values=("White", "Black"),
            width=10,
            state="readonly",
        ).grid(row=0, column=1, sticky="ew")
        ttk.Label(game_box, text="Top N moves").grid(row=1, column=0, sticky="w")
        ttk.Spinbox(game_box, from_=1, to=20, textvariable=self.top_n_var, width=8).grid(
            row=1, column=1, sticky="ew"
        )
        ttk.Label(game_box, text="Haptic analysis ms").grid(row=2, column=0, sticky="w")
        ttk.Entry(game_box, textvariable=self.analysis_ms_var, width=10).grid(
            row=2, column=1, sticky="ew"
        )
        ttk.Label(game_box, text="Engine move ms").grid(row=3, column=0, sticky="w")
        ttk.Entry(game_box, textvariable=self.engine_ms_var, width=10).grid(
            row=3, column=1, sticky="ew"
        )
        ttk.Label(game_box, text="Bot skill (0–20)").grid(row=4, column=0, sticky="w")
        ttk.Spinbox(
            game_box, from_=0, to=20, textvariable=self.bot_skill_var, width=8
        ).grid(row=4, column=1, sticky="ew")
        ttk.Button(game_box, text="New game / Apply", command=self._new_game).grid(
            row=5, column=0, columnspan=2, sticky="ew", pady=(6, 0)
        )
        ttk.Button(game_box, text="Undo full move", command=self._undo).grid(
            row=6, column=0, columnspan=2, sticky="ew", pady=(4, 0)
        )

        haptic_box = ttk.LabelFrame(controls, text="Haptic range", padding=8)
        haptic_box.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        fields = (
            ("Weak frequency (Hz)", self.frequency_weak_var),
            ("Strong frequency (Hz)", self.frequency_strong_var),
            ("Weak amplitude", self.amplitude_weak_var),
            ("Strong amplitude", self.amplitude_strong_var),
        )
        for row, (label, variable) in enumerate(fields):
            ttk.Label(haptic_box, text=label).grid(row=row, column=0, sticky="w")
            ttk.Entry(haptic_box, textvariable=variable, width=10).grid(
                row=row, column=1, sticky="ew"
            )

        motor_box = ttk.LabelFrame(controls, text="Motor", padding=8)
        motor_box.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        self.port_combo = ttk.Combobox(
            motor_box, textvariable=self.port_var, width=29, state="readonly"
        )
        self.port_combo.grid(row=0, column=0, columnspan=2, sticky="ew")
        ttk.Button(motor_box, text="Refresh", command=self._refresh_ports).grid(
            row=1, column=0, sticky="ew", pady=(5, 0)
        )
        self.connect_button = ttk.Button(
            motor_box, text="Connect", command=self._toggle_serial_connection
        )
        self.connect_button.grid(row=1, column=1, sticky="ew", pady=(5, 0))
        ttk.Label(motor_box, textvariable=self.connection_status_var, wraplength=260).grid(
            row=2, column=0, columnspan=2, sticky="w", pady=(5, 0)
        )

        moves_box = ttk.LabelFrame(controls, text="Current top moves", padding=8)
        moves_box.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        self.moves_list = tk.Listbox(moves_box, width=40, height=8)
        self.moves_list.grid(sticky="ew")

        ttk.Label(
            controls,
            textvariable=self.haptic_status_var,
            wraplength=290,
            foreground="#6a2f00",
        ).grid(row=4, column=0, sticky="w", pady=(10, 0))
        ttk.Label(
            controls, textvariable=self.game_status_var, wraplength=290
        ).grid(row=5, column=0, sticky="w", pady=(8, 0))
        ttk.Label(
            controls,
            text="Click a piece, then hover or click a legal destination. "
            "Promotion currently defaults to a queen.",
            wraplength=290,
        ).grid(row=6, column=0, sticky="w", pady=(10, 0))

    def _read_settings(self) -> None:
        settings = HapticSettings(
            top_n=int(self.top_n_var.get()),
            frequency_weak_hz=float(self.frequency_weak_var.get()),
            frequency_strong_hz=float(self.frequency_strong_var.get()),
            amplitude_weak=float(self.amplitude_weak_var.get()),
            amplitude_strong=float(self.amplitude_strong_var.get()),
        )
        settings.validate()
        analysis_time = int(self.analysis_ms_var.get())
        engine_time = int(self.engine_ms_var.get())
        bot_skill = int(self.bot_skill_var.get())
        if not 50 <= analysis_time <= 5000:
            raise ValueError("haptic analysis time must be between 50 and 5000 ms")
        if not 50 <= engine_time <= 5000:
            raise ValueError("engine move time must be between 50 and 5000 ms")
        if not 0 <= bot_skill <= 20:
            raise ValueError("bot skill must be between 0 and 20")
        self.settings = settings
        self.analysis_time_ms = analysis_time
        self.engine_time_ms = engine_time
        self.bot_skill = bot_skill

    def _new_game(self) -> None:
        try:
            self._read_settings()
        except ValueError as error:
            messagebox.showerror("Invalid settings", str(error), parent=self.root)
            return

        self._stop_haptic()
        self.board.reset()
        self.human_color = chess.WHITE if self.color_var.get() == "White" else chess.BLACK
        self.selected_square = None
        self.last_move = None
        self.position_generation += 1
        self._clear_analysis()
        self._draw_board()
        self._begin_turn()

    def _undo(self) -> None:
        if not self.board.move_stack:
            return
        self._stop_haptic()
        # On the human turn, undo Stockfish's reply and the preceding human move.
        # While Stockfish is thinking, only the human move has been made.
        plies = 2 if self.board.turn == self.human_color and len(self.board.move_stack) >= 2 else 1
        for _ in range(plies):
            if self.board.move_stack:
                self.board.pop()
        self.last_move = self.board.peek() if self.board.move_stack else None
        self.selected_square = None
        self.position_generation += 1
        self._clear_analysis()
        self._draw_board()
        self._begin_turn()

    def _clear_analysis(self) -> None:
        self.analysis_ready = False
        self.evaluations = []
        self.evaluation_by_move = {}
        self.move_signals = {}
        self.piece_signals = {}
        self.moves_list.delete(0, tk.END)

    def _begin_turn(self) -> None:
        if self.closing or self.engine_task is not None:
            return
        if self.board.is_game_over():
            self._show_game_over()
        elif self.board.turn == self.human_color:
            self._request_player_analysis()
        else:
            self._request_engine_move()

    def _request_player_analysis(self) -> None:
        fen = self.board.fen()
        legal_count = self.board.legal_moves.count()
        self.game_status_var.set("Analyzing your legal moves…")
        future = self.engine_executor.submit(
            self.evaluator.evaluate,
            fen,
            top_n=legal_count,
            time_ms=self.analysis_time_ms,
        )
        self.engine_task = EngineTask(
            future=future,
            kind="analysis",
            fen=fen,
            generation=self.position_generation,
        )

    def _request_engine_move(self) -> None:
        fen = self.board.fen()
        self.game_status_var.set("Stockfish is thinking…")
        future = self.engine_executor.submit(
            self.evaluator.choose_move,
            fen,
            time_ms=self.engine_time_ms,
            skill_level=self.bot_skill,
        )
        self.engine_task = EngineTask(
            future=future,
            kind="engine_move",
            fen=fen,
            generation=self.position_generation,
        )

    def _poll_background_tasks(self) -> None:
        if self.closing:
            return
        if self.engine_task is not None and self.engine_task.future.done():
            task = self.engine_task
            self.engine_task = None
            try:
                results = task.future.result()
            except Exception as error:
                self.game_status_var.set(f"Engine error: {error}")
            else:
                current = (
                    task.generation == self.position_generation
                    and task.fen == self.board.fen()
                )
                if current and task.kind == "analysis":
                    self._apply_analysis(results)
                elif current and task.kind == "engine_move":
                    self._apply_engine_move(results)
                else:
                    self._begin_turn()

        if self.serial_task is not None and self.serial_task.done():
            task = self.serial_task
            self.serial_task = None
            try:
                task.result()
            except Exception as error:
                self.connection_status_var.set(f"Motor connection failed: {error}")
                self.connect_button.configure(text="Connect")
            else:
                self.connection_status_var.set("Motor: connected")
                self.connect_button.configure(text="Disconnect")

        self.root.after(50, self._poll_background_tasks)

    def _apply_analysis(self, results: list[MoveEvaluation]) -> None:
        self.evaluations = results
        self.evaluation_by_move = {result.move: result for result in results}
        self.move_signals = build_move_signals(results, self.settings)
        self.piece_signals = build_piece_signals(results, self.settings)
        self.analysis_ready = True
        self.moves_list.delete(0, tk.END)
        for result in results[: self.settings.top_n]:
            score = (
                f"M{result.mate_in:+d}"
                if result.mate_in is not None
                else f"{result.centipawns / 100:+.2f}"
            )
            signal = self.move_signals[result.move]
            self.moves_list.insert(
                tk.END,
                f"{result.rank}. {result.san:<7} {score:>7}  "
                f"{signal.frequency_hz:.1f} Hz / {signal.amplitude:.2f}",
            )
        side = "White" if self.board.turn == chess.WHITE else "Black"
        self.game_status_var.set(f"Your turn ({side}). Select a piece.")

    def _apply_engine_move(self, move_uci: str) -> None:
        move = chess.Move.from_uci(move_uci)
        if move not in self.board.legal_moves:
            self.game_status_var.set(f"Engine returned illegal move {move_uci}")
            return
        san = self.board.san(move)
        self.board.push(move)
        self.last_move = move
        self.selected_square = None
        self.position_generation += 1
        self._clear_analysis()
        self._draw_board()
        self.game_status_var.set(f"Stockfish played {san}")
        self._begin_turn()

    def _show_game_over(self) -> None:
        self._stop_haptic()
        outcome = self.board.outcome()
        if outcome is None:
            text = "Game over"
        elif outcome.winner is None:
            text = f"Draw — {outcome.termination.name.replace('_', ' ').title()}"
        else:
            winner = "White" if outcome.winner == chess.WHITE else "Black"
            text = f"{winner} wins — {outcome.termination.name.replace('_', ' ').title()}"
        self.game_status_var.set(text)

    def _square_from_xy(self, x: int, y: int) -> chess.Square | None:
        if not 0 <= x < BOARD_PIXELS or not 0 <= y < BOARD_PIXELS:
            return None
        size = BOARD_PIXELS // 8
        column = x // size
        row = y // size
        if self.human_color == chess.WHITE:
            file_index = column
            rank_index = 7 - row
        else:
            file_index = 7 - column
            rank_index = row
        return chess.square(file_index, rank_index)

    def _xy_from_square(self, square: chess.Square) -> tuple[int, int]:
        file_index = chess.square_file(square)
        rank_index = chess.square_rank(square)
        if self.human_color == chess.WHITE:
            return file_index, 7 - rank_index
        return 7 - file_index, rank_index

    def _draw_board(self) -> None:
        self.canvas.delete("all")
        size = BOARD_PIXELS // 8
        legal_targets = set()
        if self.selected_square is not None:
            legal_targets = {
                move.to_square
                for move in self.board.legal_moves
                if move.from_square == self.selected_square
            }

        for square in chess.SQUARES:
            column, row = self._xy_from_square(square)
            file_index = chess.square_file(square)
            rank_index = chess.square_rank(square)
            fill = LIGHT_SQUARE if (file_index + rank_index) % 2 else DARK_SQUARE
            if self.last_move is not None and square in (
                self.last_move.from_square,
                self.last_move.to_square,
            ):
                fill = LAST_MOVE_SQUARE
            if square == self.selected_square:
                fill = SELECTED_SQUARE

            x1, y1 = column * size, row * size
            self.canvas.create_rectangle(
                x1, y1, x1 + size, y1 + size, fill=fill, outline=fill
            )

            if square in legal_targets:
                radius = 8
                center_x, center_y = x1 + size / 2, y1 + size / 2
                self.canvas.create_oval(
                    center_x - radius,
                    center_y - radius,
                    center_x + radius,
                    center_y + radius,
                    fill=LEGAL_TARGET,
                    outline="",
                )

            piece = self.board.piece_at(square)
            if piece is not None:
                self.canvas.create_text(
                    x1 + size / 2,
                    y1 + size / 2,
                    text=PIECE_GLYPHS[piece.symbol()],
                    font=("Segoe UI Symbol", 43),
                    fill="#111111",
                )

            if row == 7:
                self.canvas.create_text(
                    x1 + size - 5,
                    y1 + size - 4,
                    text=chess.FILE_NAMES[file_index],
                    anchor="se",
                    font=("Segoe UI", 9, "bold"),
                    fill="#333333",
                )
            if column == 0:
                self.canvas.create_text(
                    x1 + 4,
                    y1 + 4,
                    text=str(rank_index + 1),
                    anchor="nw",
                    font=("Segoe UI", 9, "bold"),
                    fill="#333333",
                )

    def _candidate_move(
        self, origin: chess.Square, destination: chess.Square
    ) -> chess.Move | None:
        candidates = [
            move
            for move in self.board.legal_moves
            if move.from_square == origin and move.to_square == destination
        ]
        if not candidates:
            return None
        return next(
            (move for move in candidates if move.promotion == chess.QUEEN),
            candidates[0],
        )

    def _on_board_click(self, event: tk.Event) -> None:
        square = self._square_from_xy(event.x, event.y)
        if square is None or self.board.is_game_over():
            return
        if self.board.turn != self.human_color or not self.analysis_ready:
            self.game_status_var.set("Please wait for Stockfish analysis.")
            return

        piece = self.board.piece_at(square)
        if self.selected_square is not None:
            move = self._candidate_move(self.selected_square, square)
            if move is not None:
                san = self.board.san(move)
                self._stop_haptic()
                self.board.push(move)
                self.last_move = move
                self.selected_square = None
                self.position_generation += 1
                self._clear_analysis()
                self._draw_board()
                self.game_status_var.set(f"You played {san}")
                self._begin_turn()
                return

        if piece is not None and piece.color == self.human_color:
            self.selected_square = None if square == self.selected_square else square
        else:
            self.selected_square = None
        self.hover_square = None
        self._stop_haptic()
        self._draw_board()

    def _on_board_motion(self, event: tk.Event) -> None:
        square = self._square_from_xy(event.x, event.y)
        if square == self.hover_square:
            return
        self.hover_square = square
        if (
            square is None
            or not self.analysis_ready
            or self.board.turn != self.human_color
            or self.board.is_game_over()
        ):
            self._stop_haptic()
            return

        if self.selected_square is not None:
            if square == self.selected_square:
                self._preview_piece(square)
                return
            move = self._candidate_move(self.selected_square, square)
            if move is None:
                self._stop_haptic("Haptic: illegal destination")
                return
            result = self.evaluation_by_move.get(move.uci())
            signal = self.move_signals.get(move.uci())
            if signal is None:
                rank = result.rank if result is not None else "?"
                self._stop_haptic(f"Move {self.board.san(move)}: rank {rank}, outside top {self.settings.top_n}")
                return
            self._activate_haptic(
                signal,
                f"Move {self.board.san(move)}: rank {result.rank if result else '?'}",
            )
            return

        self._preview_piece(square)

    def _preview_piece(self, square: chess.Square) -> None:
        piece = self.board.piece_at(square)
        if piece is None or piece.color != self.human_color:
            self._stop_haptic()
            return
        name = chess.piece_name(piece.piece_type).title()
        signal = self.piece_signals.get(chess.square_name(square))
        if signal is None:
            self._stop_haptic(f"{name} on {chess.square_name(square)} has no legal move")
            return
        self._activate_haptic(
            signal, f"{name} on {chess.square_name(square)} aggregate"
        )

    def _on_board_leave(self, _event: tk.Event) -> None:
        self.hover_square = None
        self._stop_haptic()

    def _activate_haptic(self, signal: HapticSignal, description: str) -> None:
        changed = signal != self.active_signal
        self.active_signal = signal
        connection = "motor" if self.vibration.connected else "virtual"
        self.haptic_status_var.set(
            f"Haptic ({connection}) — {description}: "
            f"{signal.frequency_hz:.2f} Hz, amplitude {signal.amplitude:.2f}"
        )
        if changed and self.vibration.connected:
            try:
                self.vibration.vibrate(signal)
            except Exception as error:
                self.connection_status_var.set(f"Motor write failed: {error}")

    def _stop_haptic(self, message: str = "Haptic: off") -> None:
        was_active = self.active_signal is not None
        self.active_signal = None
        self.haptic_status_var.set(message)
        if was_active and self.vibration.connected:
            try:
                self.vibration.stop()
            except Exception as error:
                self.connection_status_var.set(f"Motor stop failed: {error}")

    def _refresh_haptic_watchdog(self) -> None:
        if self.closing:
            return
        if self.active_signal is not None and self.vibration.connected:
            try:
                self.vibration.vibrate(self.active_signal)
            except Exception as error:
                self.connection_status_var.set(f"Motor write failed: {error}")
        self.root.after(750, self._refresh_haptic_watchdog)

    def _refresh_ports(self) -> None:
        ports = available_ports()
        self.port_lookup = {port.label: port.device for port in ports}
        labels = list(self.port_lookup)
        self.port_combo.configure(values=labels)
        if labels and self.port_var.get() not in self.port_lookup:
            self.port_var.set(labels[0])
        if not labels:
            self.port_var.set("")

    def _toggle_serial_connection(self) -> None:
        if self.serial_task is not None:
            return
        if self.vibration.connected:
            self._stop_haptic()
            self.vibration.close()
            self.connection_status_var.set("Motor: disconnected")
            self.connect_button.configure(text="Connect")
            return

        port = self.port_lookup.get(self.port_var.get())
        if port is None:
            messagebox.showerror("No serial port", "Select a serial port first.", parent=self.root)
            return
        self.connection_status_var.set(f"Connecting to {port}; waiting for board reset…")
        self.connect_button.configure(text="Connecting…")
        self.serial_task = self.serial_executor.submit(self.vibration.connect, port)

    def _close(self) -> None:
        if self.closing:
            return
        self.closing = True
        self._stop_haptic()
        self.vibration.close()
        self.root.withdraw()
        try:
            close_future = self.engine_executor.submit(self.evaluator.close)
            close_future.result(timeout=6)
        except Exception:
            pass
        self.engine_executor.shutdown(wait=False, cancel_futures=True)
        self.serial_executor.shutdown(wait=False, cancel_futures=True)
        self.root.destroy()


def _argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", help="optional path to the Stockfish executable")
    return parser


def main() -> int:
    arguments = _argument_parser().parse_args()
    root = tk.Tk()
    try:
        ChessVibrationGui(root, arguments.engine)
    except Exception as error:
        root.withdraw()
        messagebox.showerror("Startup failed", str(error), parent=root)
        root.destroy()
        return 1
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
