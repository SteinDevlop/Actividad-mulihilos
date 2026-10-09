#!/usr/bin/env python3
"""
checksum.py  -  Laboratorio de Procesos e Hilos (version Python)

QUE HACE
    Calcula una suma de verificacion de 64 bits sobre los numeros i = 0, 1, ..., N-1:

        f(i)      = ((i mod 97) * (i mod 53))  XOR  (i << 3)
        resultado = f(0) + f(1) + ... + f(N-1)      (modulo 2^64)

    Es el MISMO calculo que hace el programa en C++. El resultado debe ser identico en
    todos los modos; si no lo es, el reparto del trabajo esta mal.

COMO SE REPARTE EL TRABAJO (igual que en C++)
    El rango [0, N) se corta en W bloques seguidos del mismo tamano (N // W).
    El ultimo bloque se queda con lo que sobre. Cada trabajador suma su bloque y
    al final se suman los parciales.

MODOS
    sequential       1 proceso, 1 hilo        (escenario E0)
    threading        1 proceso, W hilos       (escenarios E1 y E1-C)
    multiprocessing  W procesos, 1 hilo c/u   (escenario E2)

EJEMPLOS
    python3 checksum.py --mode sequential --workload 10000000
    python3 checksum.py --mode threading --workers 4 --workload 10000000 --runs 5
    python3 checksum.py --mode multiprocessing -w 8 -n 10000000 --csv --header
    taskset -c 0 python3 checksum.py --mode threading -w 4 --csv --log-lp
"""

import argparse
import ctypes
import multiprocessing
import os
import sys
import threading
import time

MASCARA_64 = (1 << 64) - 1

CABECERA_CSV = ("language,os,mode,processes,threads_per_process,total_workers,"
                "lp_affinity,workload,run,time_seconds,cpu_total_pct,result")


# =====================================================================================
# PASO 1. El calculo y el reparto del rango
# =====================================================================================

def sumar_bloque(inicio, fin):
    """Suma f(i) para i desde 'inicio' hasta 'fin - 1' (modulo 2^64)."""
    suma = 0
    for i in range(inicio, fin):
        suma += ((i % 97) * (i % 53)) ^ ((i << 3) & MASCARA_64)
    return suma & MASCARA_64


def repartir_rango(n, trabajadores):
    """Devuelve una lista [(inicio, fin), ...] con un bloque por trabajador."""
    tamano = n // trabajadores
    bloques = []
    for k in range(trabajadores):
        inicio = k * tamano
        fin = inicio + tamano
        if k == trabajadores - 1:
            fin = n                      # el ultimo se queda con el resto
        bloques.append((inicio, fin))
    return bloques


# =====================================================================================
# PASO 2. Datos del sistema (SO, procesador logico, afinidad)
# =====================================================================================

def nombre_so():
    if sys.platform.startswith("linux"):
        return "linux"
    if sys.platform.startswith("win"):
        return "windows"
    return sys.platform


def procesador_logico_actual():
    """Numero del procesador logico (LP) donde corre este hilo. -1 si no se puede saber."""
    try:
        if sys.platform.startswith("linux"):
            return ctypes.CDLL(None).sched_getcpu()
        if sys.platform.startswith("win"):
            return ctypes.windll.kernel32.GetCurrentProcessorNumber()
    except Exception:
        pass
    return -1


def etiqueta_afinidad():
    """'all' si el proceso puede usar todos los LP; si no, cuantos LP tiene permitidos."""
    try:
        if hasattr(os, "sched_getaffinity"):                 # Linux
            permitidos = len(os.sched_getaffinity(0))
            if permitidos < os.cpu_count():
                return str(permitidos)
        elif sys.platform.startswith("win"):                 # Windows
            mascara_proceso = ctypes.c_size_t()
            mascara_sistema = ctypes.c_size_t()
            kernel32 = ctypes.windll.kernel32
            kernel32.GetCurrentProcess.restype = ctypes.c_void_p
            kernel32.GetProcessAffinityMask(ctypes.c_void_p(kernel32.GetCurrentProcess()),
                                            ctypes.byref(mascara_proceso),
                                            ctypes.byref(mascara_sistema))
            permitidos = bin(mascara_proceso.value).count("1")
            total = bin(mascara_sistema.value).count("1")
            if permitidos < total:
                return str(permitidos)
    except Exception:
        pass
    return "all"


def anotar_lp(activo, modo, k, momento):
    """Con --log-lp imprime en que LP esta el trabajador k. Devuelve el LP."""
    lp = procesador_logico_actual()
    if activo:
        texto = (f"[lp] modo={modo} trabajador={k} {momento} "
                 f"pid={os.getpid()} tid={threading.get_native_id()} lp={lp}\n")
        os.write(2, texto.encode())      # una sola escritura: las lineas no se mezclan
    return lp


# =====================================================================================
# PASO 3. Lo que hace cada trabajador
# =====================================================================================

def trabajo_de_hilo(k, inicio, fin, parciales, log_lp):
    """Un hilo suma su bloque y guarda el parcial en parciales[k]."""
    anotar_lp(log_lp, "threading", k, "inicio")
    parciales[k] = sumar_bloque(inicio, fin)
    anotar_lp(log_lp, "threading", k, "fin")


def trabajo_de_proceso(k, inicio, fin, cola, log_lp):
    """Un proceso hijo suma su bloque y envia (k, parcial, CPU usado) al padre."""
    cpu_inicial = time.process_time()
    anotar_lp(log_lp, "multiprocessing", k, "inicio")
    parcial = sumar_bloque(inicio, fin)
    anotar_lp(log_lp, "multiprocessing", k, "fin")
    cola.put((k, parcial, time.process_time() - cpu_inicial))


# =====================================================================================
# PASO 4. Una corrida completa por modo. Devuelven (resultado, segundos de CPU)
# =====================================================================================

def correr_secuencial(n, log_lp):
    cpu_inicial = time.process_time()
    anotar_lp(log_lp, "sequential", 0, "inicio")
    resultado = sumar_bloque(0, n)
    anotar_lp(log_lp, "sequential", 0, "fin")
    return resultado, time.process_time() - cpu_inicial


def correr_con_hilos(n, hilos, log_lp):
    cpu_inicial = time.process_time()          # cuenta el CPU de todos los hilos
    parciales = [0] * hilos
    lista_de_hilos = []

    for k, (inicio, fin) in enumerate(repartir_rango(n, hilos)):
        h = threading.Thread(target=trabajo_de_hilo,
                             args=(k, inicio, fin, parciales, log_lp))
        h.start()
        lista_de_hilos.append(h)

    for h in lista_de_hilos:
        h.join()

    resultado = sum(parciales) & MASCARA_64
    return resultado, time.process_time() - cpu_inicial


def correr_con_procesos(n, procesos, log_lp, contexto):
    cpu_inicial = time.process_time()          # CPU del padre
    cola = contexto.Queue()
    lista_de_procesos = []

    for k, (inicio, fin) in enumerate(repartir_rango(n, procesos)):
        p = contexto.Process(target=trabajo_de_proceso,
                             args=(k, inicio, fin, cola, log_lp))
        p.start()
        lista_de_procesos.append(p)

    respuestas = [cola.get() for _ in lista_de_procesos]   # primero leer, luego join
    for p in lista_de_procesos:
        p.join()

    resultado = sum(r[1] for r in respuestas) & MASCARA_64
    cpu_de_hijos = sum(r[2] for r in respuestas)           # CPU medido dentro de cada hijo
    return resultado, (time.process_time() - cpu_inicial) + cpu_de_hijos


# =====================================================================================
# PASO 5. Programa principal
# =====================================================================================

def leer_argumentos():
    ap = argparse.ArgumentParser(description="Checksum de 64 bits (version Python).")
    ap.add_argument("--mode", choices=["sequential", "threading", "multiprocessing"],
                    default="sequential", help="modo de ejecucion")
    ap.add_argument("--workers", "-w", type=int, default=1,
                    help="hilos o procesos (1, 2, 4, 8, 16)")
    ap.add_argument("--workload", "-n", type=int, default=10_000_000,
                    help="N: tamano del rango [0, N)")
    ap.add_argument("--runs", type=int, default=1, help="repeticiones medidas")
    ap.add_argument("--warmup", type=int, default=0,
                    help="corridas de calentamiento que se descartan")
    ap.add_argument("--csv", action="store_true", help="una linea CSV v2 por corrida")
    ap.add_argument("--header", action="store_true", help="con --csv, imprime la cabecera")
    ap.add_argument("--log-lp", action="store_true",
                    help="cada trabajador imprime su LP al inicio y al fin (stderr)")
    ap.add_argument("--expect", type=int, default=None,
                    help="resultado esperado; si no coincide el programa termina con error")
    ap.add_argument("--affinity-label", default=None,
                    help="fuerza el valor de lp_affinity (por defecto se detecta)")
    args = ap.parse_args()

    if args.workers < 1 or args.runs < 1 or args.warmup < 0:
        ap.error("workers y runs deben ser >= 1, y warmup >= 0")
    if args.workload < args.workers:
        ap.error("workload debe ser mayor o igual que workers")
    return args


def main():
    args = leer_argumentos()

    # ---- Que escenario es: cuantos procesos y cuantos hilos por proceso -------------
    if args.mode == "sequential":
        trabajadores, procesos, hilos_por_proceso = 1, 1, 1
    elif args.mode == "threading":
        trabajadores, procesos, hilos_por_proceso = args.workers, 1, args.workers
    else:  # multiprocessing
        trabajadores, procesos, hilos_por_proceso = args.workers, args.workers, 1

    # En Linux se usa fork (igual que C++); Windows solo tiene spawn (como CreateProcess).
    metodo = "fork" if sys.platform.startswith("linux") else "spawn"
    contexto = multiprocessing.get_context(metodo)

    afinidad = args.affinity_label or etiqueta_afinidad()

    # ---- Encabezado legible (solo cuando NO se pide CSV) ----------------------------
    if args.csv:
        if args.header:
            print(CABECERA_CSV)
    else:
        print("Language: Python")
        print(f"SO: {nombre_so()} | Python {sys.version.split()[0]} | "
              f"LP visibles: {os.cpu_count()} | afinidad: {afinidad}")
        print(f"Modo: {args.mode} | procesos: {procesos} | "
              f"hilos por proceso: {hilos_por_proceso} | workload: {args.workload}")

    # ---- Calentamiento: se ejecuta y se descarta ------------------------------------
    for _ in range(args.warmup):
        if args.mode == "sequential":
            correr_secuencial(args.workload, False)
        elif args.mode == "threading":
            correr_con_hilos(args.workload, trabajadores, False)
        else:
            correr_con_procesos(args.workload, trabajadores, False, contexto)

    # ---- Corridas medidas -----------------------------------------------------------
    codigo_salida = 0
    for numero_de_corrida in range(1, args.runs + 1):
        inicio = time.perf_counter()
        if args.mode == "sequential":
            resultado, cpu = correr_secuencial(args.workload, args.log_lp)
        elif args.mode == "threading":
            resultado, cpu = correr_con_hilos(args.workload, trabajadores, args.log_lp)
        else:
            resultado, cpu = correr_con_procesos(args.workload, trabajadores,
                                                 args.log_lp, contexto)
        segundos = time.perf_counter() - inicio
        cpu_pct = cpu / segundos * 100

        if args.expect is not None and resultado != args.expect:
            print(f"ERROR: resultado {resultado} distinto del esperado {args.expect}",
                  file=sys.stderr)
            codigo_salida = 2

        if args.csv:
            print(f"Python,{nombre_so()},{args.mode},{procesos},{hilos_por_proceso},"
                  f"{trabajadores},{afinidad},{args.workload},{numero_de_corrida},"
                  f"{segundos:.3f},{cpu_pct:.0f},{resultado}")
        else:
            print(f"corrida {numero_de_corrida}: tiempo = {segundos:.3f} s | "
                  f"CPU total = {cpu_pct:.0f}% | resultado = {resultado}")
        sys.stdout.flush()

    return codigo_salida


if __name__ == "__main__":
    sys.exit(main())
