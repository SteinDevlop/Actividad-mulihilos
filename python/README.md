# Python — Laboratorio de Procesos e Hilos (P2)

Versión en Python del mismo cálculo que hace `cpp/main.cpp`. Sirve para comparar con C++ y para mostrar el efecto del GIL.

## Qué calcula

Suma de verificación de 64 bits sobre `i = 0 … N-1`:

```text
f(i)      = ((i mod 97) × (i mod 53)) XOR (i << 3)
resultado = f(0) + f(1) + … + f(N-1)      (módulo 2^64)
```

El rango se corta en `W` bloques seguidos de tamaño `N // W`; el último bloque se queda con el resto. Cada trabajador suma su bloque y al final se suman los parciales. **El resultado tiene que ser idéntico en todos los modos.**

## Requisitos

* Python **3.9 o superior** (probado con 3.13). **No hay dependencias externas**: solo la biblioteca estándar.
* Linux o Windows.
* Para la prueba de 1 LP en Linux: `taskset` (paquete `util-linux`). En Windows: `start /affinity 1`.

## Archivos

| Archivo | Para qué sirve |
|---|---|
| `checksum.py` | Programa principal: una configuración, una o varias corridas. |
| `matriz.py` | Ejecuta **toda** la matriz de Python y guarda un CSV v2. |
| `test_equivalencia.py` | Comprueba que el resultado coincide con C++ en todos los modos. |
| `NOTA_GIL.md` | Nota técnica del GIL con mediciones (para P4). |
| `resultados_preliminares_sandbox*.csv` | Datos de prueba del entorno de P2 (no oficiales). |

## Modos y escenarios

| `--mode` | Procesos | Hilos por proceso | Escenario |
|---|---|---|---|
| `sequential` | 1 | 1 | E0 (línea base) |
| `threading` | 1 | W | E1 y E1-C (con afinidad a 1 LP) |
| `multiprocessing` | W | 1 | E2 |

En Linux los procesos se crean con `fork` (como `fork()` en C++). En Windows se usa `spawn`, que lanza un intérprete nuevo (equivale a `CreateProcess`).

## Uso rápido

```bash
# 1 corrida, salida para leer
python3 checksum.py --mode sequential --workload 10000000

# 5 corridas con 4 hilos, formato CSV v2 con cabecera
python3 checksum.py --mode threading --workers 4 --workload 10000000 --runs 5 --csv --header

# 8 procesos, validando el resultado
python3 checksum.py --mode multiprocessing -w 8 -n 10000000 --expect 399999987067243

# Prueba de concurrencia: afinidad a 1 LP
taskset -c 0 python3 checksum.py --mode threading -w 4 -n 10000000 --csv        # Linux
start /affinity 1 python checksum.py --mode threading -w 4 -n 10000000 --csv    # Windows (cmd)

# Ver en qué LP corre cada trabajador (para evidencia de migración)
python3 checksum.py --mode threading -w 4 -n 10000000 --log-lp
```

En Windows usa `python` en lugar de `python3`.

### Opciones de `checksum.py`

| Opción | Significado | Por defecto |
|---|---|---|
| `--mode` | `sequential`, `threading` o `multiprocessing` | `sequential` |
| `--workers`, `-w` | Hilos o procesos (1, 2, 4, 8, 16) | 1 |
| `--workload`, `-n` | Tamaño N del rango | 10 000 000 |
| `--runs` | Repeticiones medidas | 1 |
| `--warmup` | Corridas de calentamiento que se descartan | 0 |
| `--csv` / `--header` | Salida CSV v2 / con cabecera | texto legible |
| `--log-lp` | Cada trabajador imprime su LP al inicio y al fin (stderr) | apagado |
| `--expect` | Resultado esperado; si no coincide, termina con error (código 2) | — |
| `--affinity-label` | Fuerza el valor de `lp_affinity` en el CSV | se detecta |

## Ejecutar toda la matriz (para P3)

```bash
python3 matriz.py --workload 10000000 --runs 5 --out resultados_linux.csv
taskset -c 0 python3 matriz.py --workload 10000000 --runs 5 --out resultados_linux_1lp.csv
```

En Windows: `python matriz.py --workload 10000000 --runs 5 --out resultados_windows.csv`.

Lanza `sequential` y `threading` / `multiprocessing` con 1, 2, 4 y 8 trabajadores. Con `--runs 5` son **45 corridas** (5 secuenciales + 20 de `threading` + 20 de `multiprocessing`; las 40 de la matriz de la guía más la línea base). Cada configuración corre en un proceso nuevo, con 1 calentamiento descartado, y el resultado se valida contra la referencia. Tarda unos 2 minutos con N = 10 M. Para agregar el nivel 16: `--levels 1 2 4 8 16`.

## Formato de salida (CSV v2)

```csv
language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result
Python,linux,threading,1,4,4,all,10000000,1,2.436,99,399999987067243
Python,linux,multiprocessing,4,1,4,1,10000000,1,2.141,99,399999987067243
```

| Campo | Cómo se obtiene |
|---|---|
| `language` | Siempre `Python` |
| `os` | `linux` o `windows` |
| `processes`, `threads_per_process`, `total_workers` | Según el modo (tabla de arriba) |
| `lp_affinity` | `all` si el proceso puede usar todos los LP; si no, cuántos LP tiene permitidos (p. ej. `1` con `taskset -c 0`) |
| `time_seconds` | Tiempo real (`perf_counter`) de la corrida, incluyendo la creación de hilos o procesos |
| `cpu_total_pct` | CPU consumido ÷ tiempo real × 100. Con `threading` cuenta todos los hilos; con `multiprocessing` suma el CPU medido dentro de cada hijo más el del padre |
| `result` | Checksum de 64 bits (entero sin signo) |

## Workload y valores de referencia

Fase 0: workload de Python **~10–20 M**, calibrado en cada equipo. Referencias (calculadas con un kernel C++ independiente, `g++ -O0`, y coincidentes con Python):

| N | Resultado esperado |
|---|---|
| 1 000 000 | 3 999 998 828 414 |
| 10 000 000 | 399 999 987 067 243 |
| 20 000 000 | 1 599 999 973 856 681 |

**Calibración sugerida:** correr `--mode sequential` con N = 10 M. Si dura menos de ~2 s en el equipo oficial, subir N (hasta ~20 M) y fijarlo; no cambiarlo durante las pruebas. En el entorno de P2, 10 M tardó ≈ 2,0 s. Si cambia N, el resultado esperado hay que calcularlo (la tabla solo cubre los tres valores de arriba; con otro N, `matriz.py` no valida contra referencia, pero se puede comparar el `result` entre todos los modos del mismo N).

## Comprobar la equivalencia con C++

```bash
python3 test_equivalencia.py          # rápido (N hasta 1 M)
python3 test_equivalencia.py --full   # incluye 10 M y 20 M
```

Prueba los 3 modos con 1, 2, 4, 8 y 16 trabajadores (incluye casos donde N no es múltiplo de W) y comprueba que el reparto cubre el rango sin huecos ni solapes. Resultado actual: **45 de 45 pruebas correctas**.

> **Para P1:** el resultado no depende del reparto (la suma es conmutativa), pero la tabla de referencia sí depende del kernel. Si `main.cpp` oficial da otro número para el mismo N, hay que revisar el kernel antes de medir. Confirmar también que el último bloque de C++ se queda con el resto de `N / W`.

## Qué se espera ver

* `threading`: **no** baja el tiempo (GIL); el CPU total queda cerca de 100 %.
* `multiprocessing`: baja el tiempo hasta llegar al número de LP; el CPU total sube a ~100 % × LP usados.
* Con 1 LP (`taskset`): ambos modos tardan lo mismo que el secuencial.

El análisis completo, con cifras, está en `NOTA_GIL.md`.

## Limitaciones conocidas

* Python es cientos de veces más lento que C++ en este kernel: **solo se comparan tendencias** (speedup, CPU %), no tiempos absolutos.
* Use un Python **con GIL** (CPython estándar). Verificar con `python -c "import sys; print(sys._is_gil_enabled())"` (debe imprimir `True`).
* En Windows, el CPU % de `multiprocessing` no incluye el arranque del intérprete hijo; contrastar con el Administrador de tareas.
* `--log-lp` usa `sched_getcpu` (Linux) y `GetCurrentProcessorNumber` (Windows) vía `ctypes`; si no están disponibles, muestra `lp=-1`.
