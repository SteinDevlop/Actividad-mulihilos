#!/usr/bin/env python3
"""
test_equivalencia.py - Comprueba que Python da el MISMO resultado que C++ en todos los modos.

Los valores de REFERENCIA salen de un kernel C++ independiente
(g++ -std=c++17 -O0, acumulador volatile de 64 bits).
Si P1 cambia el kernel en main.cpp, hay que actualizar esta tabla con sus valores.

USO
    python3 test_equivalencia.py          # rapido (N hasta 1 000 000), unos segundos
    python3 test_equivalencia.py --full   # agrega los workloads de 10 M y 20 M
"""

import multiprocessing
import sys

import checksum

# N  ->  resultado esperado (calculado en C++)
REFERENCIA = {
    1: 0,
    7: 163,
    1_000: 4_032_535,
    1_000_000: 3_999_998_828_414,
    10_000_000: 399_999_987_067_243,
    20_000_000: 1_599_999_973_856_681,
}

TRABAJADORES = [1, 2, 4, 8, 16]


def comparar(nombre, obtenido, esperado, estado):
    estado["pruebas"] += 1
    if obtenido != esperado:
        estado["fallos"] += 1
        print(f"  FALLA {nombre}: obtuvo {obtenido}, esperaba {esperado}")


def main():
    completo = "--full" in sys.argv
    metodo = "fork" if sys.platform.startswith("linux") else "spawn"
    contexto = multiprocessing.get_context(metodo)
    estado = {"pruebas": 0, "fallos": 0}

    for n, esperado in REFERENCIA.items():
        if n >= 10_000_000 and not completo:
            continue
        print(f"N = {n:,}  (esperado {esperado})")

        resultado, _ = checksum.correr_secuencial(n, False)
        comparar(f"sequential N={n}", resultado, esperado, estado)

        for w in TRABAJADORES:
            if w > n:
                continue                                  # el programa exige N >= W
            if n >= 10_000_000 and w not in (4, 16):
                continue                                  # en N grande, pocas pruebas
            resultado, _ = checksum.correr_con_hilos(n, w, False)
            comparar(f"threading W={w} N={n}", resultado, esperado, estado)
            resultado, _ = checksum.correr_con_procesos(n, w, False, contexto)
            comparar(f"multiprocessing W={w} N={n}", resultado, esperado, estado)

    # El reparto debe cubrir [0, N) sin huecos ni solapes, aunque N no sea multiplo de W
    print("Reparto del rango sin huecos ni solapes")
    for n, w in [(1000, 16), (7, 4), (1_000_003, 8)]:
        bloques = checksum.repartir_rango(n, w)
        empieza_en_cero = bloques[0][0] == 0
        termina_en_n = bloques[-1][1] == n
        sin_huecos = all(bloques[k][1] == bloques[k + 1][0] for k in range(w - 1))
        comparar(f"reparto N={n} W={w}", empieza_en_cero and termina_en_n and sin_huecos,
                 True, estado)

    buenas = estado["pruebas"] - estado["fallos"]
    print(f"\n{buenas} de {estado['pruebas']} pruebas correctas")
    return 1 if estado["fallos"] else 0


if __name__ == "__main__":
    sys.exit(main())
