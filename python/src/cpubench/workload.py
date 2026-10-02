"""Carga CPU-bound pura y particion del trabajo.

Este modulo no conoce hilos ni procesos. Solo define:
  * kernel(start, stop): el calculo para el rango [start, stop).
  * partition(total, parts): como se reparte [0, total) en rangos contiguos.
  * combine(partials): como se agregan los resultados parciales.

El resultado total es una suma modulo 2**64. Como la suma modular es asociativa
y conmutativa, el resultado final es IDENTICO sin importar como se parta el
rango ni en que orden terminen los workers.
"""

from typing import Iterable, List, Tuple

MASK64 = (1 << 64) - 1
MULT_A = 0x9E3779B97F4A7C15
MULT_B = 0xBF58476D1CE4E5B9

Chunk = Tuple[int, int]


def kernel(start: int, stop: int) -> int:
    """Suma modulo 2**64 de f(i) para i en [start, stop).

    f(i) = (((i * A) mod 2**64) XOR (i >> 7)) * B
    Aritmetica entera pura: sin I/O, sin sleep, sin memoria significativa.
    """
    acc = 0
    a, b, mask = MULT_A, MULT_B, MASK64
    for i in range(start, stop):
        acc = (acc + ((((i * a) & mask) ^ (i >> 7)) * b)) & mask
    return acc


def partition(total: int, parts: int) -> List[Chunk]:
    """Divide [0, total) en `parts` rangos contiguos de tamano casi igual.

    Los primeros `total % parts` rangos reciben una unidad extra. Siempre
    devuelve exactamente `parts` rangos (algunos pueden estar vacios si
    parts > total). Depende solo de (total, parts), nunca de la modalidad.
    """
    if total < 0 or parts < 1:
        raise ValueError("total debe ser >= 0 y parts >= 1")
    base, extra = divmod(total, parts)
    chunks: List[Chunk] = []
    start = 0
    for index in range(parts):
        size = base + (1 if index < extra else 0)
        chunks.append((start, start + size))
        start += size
    return chunks


def combine(partials: Iterable[int]) -> int:
    """Agrega resultados parciales (suma modulo 2**64)."""
    return sum(partials) & MASK64
