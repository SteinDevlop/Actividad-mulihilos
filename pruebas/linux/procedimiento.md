# Procedimiento de pruebas: Linux

## 1. Entorno de ejecución

| Campo | Valor |
|---|---|
| Tipo de entorno | [GitHub Codespace] |
| Sistema operativo | [p. ej. Debian GNU/Linux 13 "Trixie"] |
| CPUs totales del equipo (`nproc`) | [debe ser ≥ 3 para medir el nivel de 3 CPUs] |

> Un Codespace de 2 núcleos **no** permite el nivel de 3 CPUs: el script lo omite y lo avisa. Usar una máquina de 4 núcleos o más.

```bash
lscpu
nproc
free -h
uname -a
cat /etc/os-release
```

## 2. Herramientas y versiones

| Herramienta | Versión | Comando |
|---|---|---|
| Compilador C++ (g++) | [versión] | `g++ --version` |
| Python | [versión; CPython con GIL] | `python3 --version` |
| taskset | [versión] | `taskset --version` |
| Dependencias de Python | Ninguna (solo biblioteca estándar) | Ver `python/README.md` |

Verificar que Python tiene el GIL activo (debe imprimir `True`; en Python < 3.13 el atributo no existe y el GIL está activo):

```bash
python3 -c "import sys; print(getattr(sys, '_is_gil_enabled', lambda: True)())"
```

Banderas de compilación de C++ (deben ser idénticas a las usadas en Windows; ver `cpp/README.md`):

```bash
cd cpp
g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main
```

`-O0` es obligatorio: evita que el compilador optimice o elimine el cálculo.

## 3. Parámetros del experimento

| Parámetro | C++ | Python |
|---|---|---|
| Programa | `cpp/main` | `python/checksum.py` |
| Workload (N) | 100000000 (calibrar, ver nota) | 10000000 |
| Repeticiones medidas por configuración | 5 | 5 |
| Calentamiento descartado por configuración | 1 | 1 |
| Niveles de CPU | 1, 2 y 3 | 1, 2 y 3 |
| Trabajadores | 1, 2, 4, 8 | 1, 2, 4, 8 |

Resultado esperado (validado con `--expect`; el programa termina con código 2 si no coincide):

| N | Resultado |
|---|---|
| 10 000 000 | `399999987067243` |
| 100 000 000 | `39999999869145770` |

> **Calibración (C++):** `cpp/README.md` indica ajustar el workload hasta que el modo secuencial dure entre 5 y 10 s (aprox. 1.5 a 2.5 mil millones). Con 100 M se hace una prueba rápida. Una vez fijado, **no cambiarlo** durante toda la matriz. Con un N fuera de la tabla de referencia no se valida el resultado.
> **Calibración (Python):** workload entre 10 y 20 M; si el secuencial dura menos de ~2 s, subirlo.

Configuraciones medidas **por cada nivel de CPU** (1, 2 y 3):

| Lenguaje | Escenario | Modo | Trabajadores |
|---|---|---|---|
| C++ | E0 | `sequential` | 1 |
| C++ | E1 / E1-C | `threads` | 1, 2, 4, 8 |
| C++ | E2 | `processes` | 1, 2, 4, 8 |
| C++ | E3 | `mixed` | 2 procesos (5 y 3 hilos) |
| Python | E0 | `sequential` | 1 |
| Python | E1 / E1-C | `threading` | 1, 2, 4, 8 |
| Python | E2 | `multiprocessing` | 1, 2, 4, 8 |

Total de corridas medidas:

| Lenguaje | Configuraciones | × Niveles de CPU | × Repeticiones | Corridas |
|---|---|---|---|---|
| C++ | 10 | 3 | 5 | 150 |
| Python | 9 | 3 | 5 | 135 |
| **Total** | | | | **285** |

Los calentamientos se ejecutan pero no se guardan.

## 4. Limitación de CPUs

Se usa `taskset` para restringir cada configuración a las primeras *k* CPUs:

| Nivel | Comando |
|---|---|
| 1 CPU | `taskset -c 0 <comando>` |
| 2 CPUs | `taskset -c 0,1 <comando>` |
| 3 CPUs | `taskset -c 0,1,2 <comando>` |

Verificación de que la restricción funciona (debe imprimir `1`, `2` y `3`):

```bash
taskset -c 0 nproc
taskset -c 0,1 nproc
taskset -c 0,1,2 nproc
```

El propio programa detecta la afinidad y la registra en la columna `lp_affinity` del CSV (con `all` cuando no hay restricción), lo que permite comprobar a posteriori que cada fila se midió con el número de CPUs esperado.

## 5. Procedimiento paso a paso

1. Clonar o actualizar el repositorio y situarse en su raíz.
2. Instalar las herramientas necesarias:
   ```bash
   sudo apt update
   sudo apt install -y build-essential python3 util-linux procps
   ```
3. Compilar C++ (el script también lo compila si no existe el ejecutable):
   ```bash
   cd cpp && g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main && cd ..
   ./cpp/main --help
   ```
4. Verificar la restricción de CPUs (sección 4) y el GIL (sección 2).
5. Validar que ambos programas dan el resultado de referencia:
   ```bash
   ./cpp/main --mode threads --workers 2 --workload 1000000 --expect 3999998828414
   python3 python/checksum.py --mode threading --workers 2 --workload 1000000 --expect 3999998828414
   ```
6. Calibrar el workload de C++ (ver nota de la sección 3) y fijarlo.
7. Cerrar programas innecesarios y no usar el equipo durante las mediciones.
8. Hacer una prueba rápida del script:
   ```bash
   chmod +x pruebas/linux/prueba.sh
   RUNS=1 CPU_LEVELS="1 2" ./pruebas/linux/prueba.sh cpp
   ```
9. Ejecutar la matriz completa desde la raíz del repositorio:
   ```bash
   ./pruebas/linux/prueba.sh all
   ```
   (`cpp` o `python` para correr solo uno; si se calibró otro workload: `CPP_WORKLOAD=2000000000 ./pruebas/linux/prueba.sh cpp`).
10. Revisar los resultados en `pruebas/linux/resultados/` y que `errores.log` esté vacío.

## 6. Comandos ejecutados por el script

Para cada nivel de CPU `k` en {1, 2, 3} (con `CPUS` = `0`, `0,1` o `0,1,2`), el script lanza **una invocación por configuración**, que hace internamente las 5 repeticiones y el calentamiento:

```bash
# C++  (N = CPP_WORKLOAD, validado con --expect si N está en la tabla)
taskset -c $CPUS ./cpp/main --mode sequential --workload $N                    --csv --runs 5 --warmup 1
taskset -c $CPUS ./cpp/main --mode threads    --workers  W --workload $N       --csv --runs 5 --warmup 1   # W = 1,2,4,8
taskset -c $CPUS ./cpp/main --mode processes  --processes P --workload $N      --csv --runs 5 --warmup 1   # P = 1,2,4,8
taskset -c $CPUS ./cpp/main --mode mixed      --workload $N                    --csv --runs 5 --warmup 1

# Python  (N = PY_WORKLOAD)
taskset -c $CPUS python3 python/checksum.py --mode sequential      --workload $N               --csv --runs 5 --warmup 1
taskset -c $CPUS python3 python/checksum.py --mode threading       --workers W --workload $N   --csv --runs 5 --warmup 1   # W = 1,2,4,8
taskset -c $CPUS python3 python/checksum.py --mode multiprocessing --workers W --workload $N   --csv --runs 5 --warmup 1   # W = 1,2,4,8
```

Cada programa imprime filas en formato CSV v2; el script las guarda anteponiendo la columna `cpus`. Si una configuración falla (código de salida distinto de 0, por ejemplo 2 si el resultado no coincide con `--expect`), no se guarda ninguna fila y se registra en `errores.log`.

## 7. Archivos generados

Directorio `pruebas/linux/resultados/`:

| Archivo | Contenido |
|---|---|
| `cpp_results.csv` | 150 filas: C++ (E0, E1, E2, E3) con 1, 2 y 3 CPUs |
| `python_results.csv` | 135 filas: Python (`sequential`, `threading`, `multiprocessing`) con 1, 2 y 3 CPUs |
| `hardware_info.txt` | `lscpu`, `nproc`, `free -h`, kernel, SO, versiones, estado del GIL y parámetros usados |
| `errores.log` | Fallos o avisos durante la ejecución (debe quedar vacío) |

Formato de los CSV (columna `cpus` + CSV v2 oficial de 12 columnas):

```csv
cpus,language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result
```

| Campo | Significado |
|---|---|
| `cpus` | Nivel de CPUs impuesto con `taskset` (1, 2 o 3) |
| `lp_affinity` | Afinidad detectada por el programa (debe coincidir con `cpus`) |
| `time_seconds` | Tiempo real de la corrida |
| `cpu_total_pct` | CPU consumido ÷ tiempo real × 100 (con 1 CPU no debe superar ~100) |
| `result` | Checksum de 64 bits; debe ser idéntico en todos los modos para un mismo N |

Se conservan las mediciones individuales, no solo promedios.

## 8. Qué se espera ver

- **1 CPU:** aumentar hilos o procesos no reduce el tiempo; `cpu_total_pct` ≈ 100 (hipótesis H2).
- **C++:** con 2 y 3 CPUs, `threads` y `processes` bajan el tiempo hasta cubrir el número de CPUs disponibles.
- **Python `threading`:** el tiempo no baja con más CPUs (GIL); `cpu_total_pct` ≈ 100.
- **Python `multiprocessing`:** el tiempo baja hasta el número de CPUs; `cpu_total_pct` ≈ 100 × CPUs usadas.
- Los tiempos absolutos de Python y C++ no son comparables (Python es cientos de veces más lento en este kernel); solo se comparan tendencias (speedup, CPU %).

## 9. Incidencias

| Fecha | Descripción | Acción tomada |
|---|---|---|
| [2026-10-5] | Sin incidencias | - |

## 10. Reproducibilidad

Para repetir las pruebas basta con seguir la sección 5 en un equipo con **al menos 3 CPUs**. Los resultados pueden variar ligeramente según el hardware y la carga del sistema, pero la tendencia (secuencial vs. paralelo, `threading` vs. `multiprocessing`, 1 vs. 2 vs. 3 CPUs) debería mantenerse.