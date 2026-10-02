"""Medicion de tiempo consistente para todas las modalidades."""

import time
from typing import Callable, Tuple, TypeVar

T = TypeVar("T")


def timed(func: Callable[..., T], *args, **kwargs) -> Tuple[T, float]:
    """Ejecuta func(*args, **kwargs) y devuelve (resultado, segundos).

    Usa time.perf_counter() (monotonico, alta resolucion, Windows y Linux).
    Solo mide la llamada: nada de parseo de argumentos ni impresion.
    """
    begin = time.perf_counter()
    value = func(*args, **kwargs)
    elapsed = time.perf_counter() - begin
    return value, elapsed
