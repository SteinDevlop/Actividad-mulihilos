"""Estrategias de ejecucion. Todas tienen la misma firma:

    run_xxx(workload, workers, **opciones) -> int

y todas usan workload.kernel / partition / combine, de modo que hacen
exactamente el mismo trabajo.
"""

import multiprocessing
import threading
from typing import Callable, Dict, List

from cpubench import workload as wl

START_METHODS = ("spawn", "fork", "forkserver")


def run_sequential(workload: int, workers: int = 1, **_options) -> int:
    """Una unica ejecucion logica en el hilo principal. `workers` se ignora."""
    return wl.kernel(0, workload)


def _thread_worker(index: int, start: int, stop: int, results: List[int]) -> None:
    results[index] = wl.kernel(start, stop)


def run_threading(workload: int, workers: int, **_options) -> int:
    """Un thread por chunk. En CPython con GIL no hay paralelismo CPU real."""
    chunks = wl.partition(workload, workers)
    results = [0] * len(chunks)
    threads = [
        threading.Thread(target=_thread_worker, args=(i, start, stop, results))
        for i, (start, stop) in enumerate(chunks)
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    return wl.combine(results)


def run_multiprocessing(
    workload: int, workers: int, start_method: str = "spawn", **_options
) -> int:
    """Un proceso por chunk mediante un Pool. Paralelismo CPU real."""
    chunks = wl.partition(workload, workers)
    context = multiprocessing.get_context(start_method)
    with context.Pool(processes=workers) as pool:
        partials = pool.starmap(wl.kernel, chunks)
    return wl.combine(partials)


EXECUTORS: Dict[str, Callable[..., int]] = {
    "sequential": run_sequential,
    "threading": run_threading,
    "multiprocessing": run_multiprocessing,
}
