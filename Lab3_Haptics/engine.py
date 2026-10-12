"""Evaluate chess positions with an official Stockfish UCI executable."""

from __future__ import annotations

import argparse
import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Sequence

import chess
import chess.engine


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENGINE_DIRECTORIES = (
    PROJECT_ROOT / "lab3_venv" / "engines" / "stockfish",
    PROJECT_ROOT / "tools" / "stockfish",
)


@dataclass(frozen=True)
class AnalysisConfig:
    """Stockfish resource and search limits."""

    top_n: int = 5
    time_ms: int = 250
    threads: int = 1
    hash_mb: int = 64
    mate_score_cp: int = 100_000

    def validate(self) -> None:
        if self.top_n < 1:
            raise ValueError("top_n must be at least 1")
        if not 10 <= self.time_ms <= 60_000:
            raise ValueError("time_ms must be between 10 and 60000")
        if not 1 <= self.threads <= 128:
            raise ValueError("threads must be between 1 and 128")
        if not 1 <= self.hash_mb <= 131_072:
            raise ValueError("hash_mb must be between 1 and 131072")
        if self.mate_score_cp < 10_000:
            raise ValueError("mate_score_cp must be at least 10000")


@dataclass(frozen=True)
class MoveEvaluation:
    """One candidate move, always scored for the player whose turn it is."""

    move: str
    san: str
    rank: int
    centipawns: int
    mate_in: int | None
    expected_score: float
    relative_quality: float
    principal_variation: tuple[str, ...]
    depth: int | None
    selective_depth: int | None
    nodes: int | None


def find_stockfish(explicit_path: str | os.PathLike[str] | None = None) -> Path:
    """Resolve Stockfish from an argument, environment variable, or tools/."""

    candidates: list[Path] = []
    if explicit_path:
        candidates.append(Path(explicit_path).expanduser())

    environment_path = os.environ.get("STOCKFISH_PATH")
    if environment_path:
        candidates.append(Path(environment_path).expanduser())

    for engine_directory in DEFAULT_ENGINE_DIRECTORIES:
        if engine_directory.exists():
            patterns = ("stockfish*.exe", "stockfish")
            for pattern in patterns:
                candidates.extend(engine_directory.rglob(pattern))

    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved.is_file():
            return resolved

    locations = ", ".join(str(path) for path in candidates) or "none"
    raise FileNotFoundError(
        "Stockfish executable not found. Pass --engine, set STOCKFISH_PATH, "
        "or extract it under lab3_venv/engines/stockfish. "
        f"Checked: {locations}"
    )


def parse_position(fen: str) -> chess.Board:
    """Parse and strictly validate a FEN, with 'startpos' as a convenience."""

    try:
        board = chess.Board() if fen.strip().lower() == "startpos" else chess.Board(fen)
    except ValueError as error:
        raise ValueError(f"invalid FEN: {error}") from error

    if not board.is_valid():
        raise ValueError(f"invalid chess position (status={board.status()})")
    if board.is_game_over():
        raise ValueError("position is already game over and has no move to evaluate")
    return board


def normalize_expected_scores(scores: Sequence[float]) -> list[float]:
    """Map best-to-worst expected scores to [1, 0].

    When all WDL values are effectively equal, rank is used as a deterministic
    fallback. This avoids division by zero while retaining useful ordering.
    """

    if not scores:
        return []
    if len(scores) == 1:
        return [1.0]

    best = max(scores)
    worst = min(scores)
    span = best - worst
    if span < 1e-9:
        denominator = len(scores) - 1
        return [1.0 - index / denominator for index in range(len(scores))]

    return [max(0.0, min(1.0, (score - worst) / span)) for score in scores]


class StockfishEvaluator:
    """Small synchronous adapter around python-chess's UCI support."""

    def __init__(
        self,
        engine_path: str | os.PathLike[str] | None = None,
        config: AnalysisConfig | None = None,
    ) -> None:
        self.config = config or AnalysisConfig()
        self.config.validate()
        self.engine_path = find_stockfish(engine_path)
        self._engine: chess.engine.SimpleEngine | None = None

    def __enter__(self) -> "StockfishEvaluator":
        self.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def start(self) -> None:
        if self._engine is not None:
            return
        self._engine = chess.engine.SimpleEngine.popen_uci(str(self.engine_path))
        self._engine.configure(
            {"Threads": self.config.threads, "Hash": self.config.hash_mb}
        )

    def close(self) -> None:
        if self._engine is not None:
            self._engine.quit()
            self._engine = None

    def evaluate(
        self,
        fen: str,
        *,
        top_n: int | None = None,
        time_ms: int | None = None,
    ) -> list[MoveEvaluation]:
        board = parse_position(fen)
        requested_top_n = self.config.top_n if top_n is None else top_n
        requested_time_ms = self.config.time_ms if time_ms is None else time_ms
        if requested_top_n < 1:
            raise ValueError("top_n must be at least 1")
        if not 10 <= requested_time_ms <= 60_000:
            raise ValueError("time_ms must be between 10 and 60000")
        if self._engine is None:
            self.start()
        assert self._engine is not None

        candidate_count = min(requested_top_n, board.legal_moves.count())
        analysis = self._engine.analyse(
            board,
            chess.engine.Limit(time=requested_time_ms / 1000.0),
            multipv=candidate_count,
        )
        if isinstance(analysis, dict):
            analysis = [analysis]

        prepared: list[dict[str, Any]] = []
        for info in analysis:
            principal_variation = info.get("pv", [])
            if not principal_variation:
                continue

            move = principal_variation[0]
            score = info["score"].pov(board.turn)
            mate_in = score.mate()
            centipawns = score.score(mate_score=self.config.mate_score_cp)
            if centipawns is None:
                raise RuntimeError("Stockfish returned an unscorable evaluation")

            wdl = score.wdl(model="sf16.1", ply=board.ply())
            expected_score = (wdl.wins + 0.5 * wdl.draws) / wdl.total()
            prepared.append(
                {
                    "move": move.uci(),
                    "san": board.san(move),
                    "centipawns": centipawns,
                    "mate_in": mate_in,
                    "expected_score": expected_score,
                    "principal_variation": tuple(
                        variation_move.uci() for variation_move in principal_variation
                    ),
                    "depth": info.get("depth"),
                    "selective_depth": info.get("seldepth"),
                    "nodes": info.get("nodes"),
                }
            )

        if not prepared:
            raise RuntimeError("Stockfish returned no candidate moves")

        # MultiPV is normally ordered, but explicitly sorting makes the adapter's
        # contract independent of engine output ordering.
        prepared.sort(key=lambda item: item["centipawns"], reverse=True)
        qualities = normalize_expected_scores(
            [item["expected_score"] for item in prepared]
        )

        return [
            MoveEvaluation(
                **item,
                rank=index + 1,
                relative_quality=qualities[index],
            )
            for index, item in enumerate(prepared)
        ]

    def choose_move(self, fen: str, *, time_ms: int, skill_level: int) -> str:
        """Choose a move at Stockfish skill 0-20 without weakening analysis."""

        board = parse_position(fen)
        if not 10 <= time_ms <= 60_000:
            raise ValueError("time_ms must be between 10 and 60000")
        if not 0 <= skill_level <= 20:
            raise ValueError("skill_level must be between 0 and 20")
        if self._engine is None:
            self.start()
        assert self._engine is not None

        result = self._engine.play(
            board,
            chess.engine.Limit(time=time_ms / 1000.0),
            options={"Skill Level": skill_level},
        )
        if result.move is None:
            raise RuntimeError("Stockfish returned no move")
        return result.move.uci()


def _build_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fen",
        default="startpos",
        help="FEN to evaluate, or 'startpos' (default: startpos)",
    )
    parser.add_argument("--engine", help="path to the Stockfish executable")
    parser.add_argument("--top-n", type=int, default=5)
    parser.add_argument("--time-ms", type=int, default=250)
    parser.add_argument("--threads", type=int, default=1)
    parser.add_argument("--hash-mb", type=int, default=64)
    parser.add_argument(
        "--json", action="store_true", help="emit JSON instead of a text table"
    )
    return parser


def main() -> int:
    arguments = _build_argument_parser().parse_args()
    config = AnalysisConfig(
        top_n=arguments.top_n,
        time_ms=arguments.time_ms,
        threads=arguments.threads,
        hash_mb=arguments.hash_mb,
    )

    try:
        with StockfishEvaluator(arguments.engine, config) as evaluator:
            results = evaluator.evaluate(arguments.fen)
    except (
        FileNotFoundError,
        ValueError,
        RuntimeError,
        OSError,
        chess.engine.EngineError,
    ) as error:
        raise SystemExit(f"error: {error}") from error

    if arguments.json:
        print(json.dumps([asdict(result) for result in results], indent=2))
    else:
        print("rank  move   SAN       cp       WDL exp.  relative  depth")
        for result in results:
            mate = f" mate {result.mate_in:+d}" if result.mate_in is not None else ""
            print(
                f"{result.rank:>4}  {result.move:<5}  {result.san:<8}  "
                f"{result.centipawns:>7}{mate:<9}  {result.expected_score:>7.3f}  "
                f"{result.relative_quality:>8.3f}  {result.depth or '-':>5}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
