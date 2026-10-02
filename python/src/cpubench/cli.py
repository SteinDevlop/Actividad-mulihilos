"""Interfaz de linea de comandos: argumentos, validacion y orquestacion."""

import argparse
import platform
import sys
from typing import List, Optional

from cpubench.executors import EXECUTORS, START_METHODS
from cpubench.report import CSV_HEADER, RunRecord, format_csv, format_text
from cpubench.timing import timed

DEFAULT_WORKLOAD = 20_000_000


def _positive_int(text: str) -> int:
    try:
        value = int(text.replace("_", ""))
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{text}' no es un entero valido")
    if value < 1:
        raise argparse.ArgumentTypeError(f"el valor debe ser >= 1 (recibido {value})")
    return value


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="cpubench",
        description="Carga CPU-bound en Python: secuencial, threading o multiprocessing.",
    )
    parser.add_argument("--mode", required=True, choices=sorted(EXECUTORS),
                        help="modalidad de ejecucion")
    parser.add_argument("--workload", type=_positive_int, default=DEFAULT_WORKLOAD,
                        help=f"numero de iteraciones (default {DEFAULT_WORKLOAD}; admite 1_000_000)")
    parser.add_argument("--workers", type=_positive_int, default=1,
                        help="threads o procesos (default 1; se ignora en sequential)")
    parser.add_argument("--repeat", type=_positive_int, default=1,
                        help="repeticiones; una salida por repeticion (default 1)")
    parser.add_argument("--csv", action="store_true",
                        help="una linea CSV por repeticion en lugar de texto")
    parser.add_argument("--csv-header", action="store_true",
                        help="imprime la cabecera CSV antes de los datos (implica --csv)")
    parser.add_argument("--start-method", choices=START_METHODS, default="spawn",
                        help="metodo de creacion de procesos; solo multiprocessing (default spawn)")
    parser.add_argument("--verbose", action="store_true",
                        help="al final del modo texto, imprime Python/SO/GIL/start method")
    return parser


def _environment_info(args: argparse.Namespace) -> str:
    gil_check = getattr(sys, "_is_gil_enabled", None)
    gil = "enabled" if gil_check is None or gil_check() else "DISABLED (free-threaded build)"
    start = args.start_method if args.mode == "multiprocessing" else "n/a"
    return "\n".join([
        f"Python version: {platform.python_version()} ({platform.python_implementation()})",
        f"Platform: {platform.platform()}",
        f"GIL: {gil}",
        f"Start method: {start}",
    ])


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    effective_workers = 1 if args.mode == "sequential" else args.workers
    if args.mode == "sequential" and args.workers != 1:
        print("Aviso: --workers se ignora en modo sequential (se reporta Workers: 1).",
              file=sys.stderr)

    executor = EXECUTORS[args.mode]
    use_csv = args.csv or args.csv_header
    if args.csv_header:
        print(CSV_HEADER)

    for run_index in range(args.repeat):
        result, seconds = timed(
            executor, args.workload, effective_workers, start_method=args.start_method
        )
        record = RunRecord(args.mode, effective_workers, args.workload, seconds, result)
        if use_csv:
            print(format_csv(record), flush=True)
        else:
            if run_index > 0:
                print()
            print(format_text(record), flush=True)
    if args.verbose and not use_csv:
        print(_environment_info(args))
    return 0
