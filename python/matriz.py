#!/usr/bin/env python3
"""
matriz.py - Ejecuta TODA la matriz de Python de la Fase 0 y guarda un CSV v2.

Matriz (por sistema operativo):
    sequential                          1 configuracion   x runs
    threading        1, 2, 4, 8 hilos   4 configuraciones  x runs
    multiprocessing  1, 2, 4, 8 procesos 4 configuraciones x runs
Con --runs 5 son 45 corridas medidas (5 + 20 + 20); las 40 de la matriz de la guia
mas las 5 secuenciales de la linea base.

Cada configuracion se lanza en un proceso nuevo de checksum.py con 1 corrida de
calentamiento que se descarta (protocolo de la Fase 0).

USO
    python3 matriz.py --workload 10000000 --runs 5 --out resultados_linux.csv
    taskset -c 0 python3 matriz.py --workload 10000000 --out resultados_linux_1lp.csv
    (en Windows:  python matriz.py --workload 10000000 --out resultados_windows.csv)
"""

import argparse
import os
import subprocess
import sys

import checksum
import test_equivalencia

CARPETA = os.path.dirname(os.path.abspath(__file__))
SCRIPT = os.path.join(CARPETA, "checksum.py")
NIVELES = [1, 2, 4, 8]


def main():
    ap = argparse.ArgumentParser(description="Ejecuta la matriz completa de Python.")
    ap.add_argument("--workload", "-n", type=int, default=10_000_000)
    ap.add_argument("--runs", type=int, default=5, help="repeticiones por configuracion")
    ap.add_argument("--out", default="resultados_python.csv", help="archivo CSV de salida")
    ap.add_argument("--levels", type=int, nargs="+", default=NIVELES,
                    help="niveles de hilos/procesos (por defecto 1 2 4 8)")
    args = ap.parse_args()

    # Si conocemos el resultado correcto para este workload, cada corrida se valida.
    esperado = test_equivalencia.REFERENCIA.get(args.workload)

    # Lista de configuraciones: (modo, trabajadores)
    configuraciones = [("sequential", 1)]
    for nivel in args.levels:
        configuraciones.append(("threading", nivel))
    for nivel in args.levels:
        configuraciones.append(("multiprocessing", nivel))

    with open(args.out, "w") as salida:
        salida.write(checksum.CABECERA_CSV + "\n")
        for modo, w in configuraciones:
            print(f"-> {modo:16s} W={w}  workload={args.workload}  runs={args.runs}", flush=True)
            comando = [sys.executable, SCRIPT, "--mode", modo, "--workers", str(w),
                       "--workload", str(args.workload), "--runs", str(args.runs),
                       "--warmup", "1", "--csv"]
            if esperado is not None:
                comando += ["--expect", str(esperado)]
            proceso = subprocess.run(comando, capture_output=True, text=True)
            if proceso.returncode != 0:
                print(proceso.stderr)
                print("ERROR: la configuracion fallo; se detiene la matriz.")
                return 1
            salida.write(proceso.stdout)
            salida.flush()

    print(f"Listo. Resultados en {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
