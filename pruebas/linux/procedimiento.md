# Procedimiento de pruebas: Linux

## 1. Entorno de ejecución

| Campo | Valor |
|---|---|
| Tipo de entorno | [GitHub Codespace] |
| Sistema operativo | [p. ej. Debian GNU/Linux 13 "Trixie"] |

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
| Compilador C (gcc) | [versión] | `gcc --version` |
| Python | [versión] | `python3 --version` |
| taskset | [versión] | `taskset --version` |
| Dependencias de Python | [ninguna / lista] | Ver `python/README.md` |

Banderas de compilación del programa en C (deben ser equivalentes a las usadas en Windows):

```bash
gcc -O2 -pthread -o cpp/main cpp/main.c
```

## 3. Parámetros del experimento

| Parámetro | Valor |
|---|---|
| Workload | 100000000 |
| Repeticiones por configuración | 5 |
| CPUs utilizadas | 2 (CPU 0 y CPU 1) |
| Workers en modo secuencial | 1 |
| Workers en modo paralelo | 2 |

Configuraciones medidas:

| Lenguaje | Modo | Workers | CPUs |
|---|---|---|---|
| C | `sequential` | 1 | 2 |
| C | `parallel` | 2 | 2 |
| Python | `sequential` | 1 | 2 |
| Python | `threading` | 2 | 2 |
| Python | `multiprocessing` | 2 | 2 |

Total: 5 configuraciones x 5 repeticiones = 25 corridas.

## 4. Limitación de CPUs

Se usa `taskset` para restringir cada proceso a las CPUs 0 y 1:

```bash
taskset -c 0,1 <comando>
```

Verificación de que la restricción funciona (debe imprimir `2`):

```bash
taskset -c 0,1 nproc
```

## 5. Procedimiento paso a paso

1. Clonar o actualizar el repositorio y situarse en su raíz.
2. Instalar las herramientas necesarias:
   ```bash
   sudo apt update
   sudo apt install -y build-essential python3 python3-pip python3-venv util-linux procps
   ```
3. Instalar las dependencias de Python (si las hay) según `python/README.md`.
4. Compilar el programa en C:
   ```bash
   gcc -O2 -pthread -o cpp/main cpp/main.c
   ```
5. Verificar la restricción a 2 CPUs: `taskset -c 0,1 nproc`.
6. Ejecutar una prueba manual de cada programa y confirmar que imprime `Execution time: X seconds`:
   ```bash
   taskset -c 0,1 ./cpp/main --mode parallel --workers 2 --workload 100000000
   taskset -c 0,1 python3 python/main.py --mode threading --workers 2 --workload 100000000
   ```
7. Cerrar programas innecesarios y no usar el equipo durante las mediciones.
8. Ejecutar el script de pruebas desde la raíz del repositorio:
   ```bash
   chmod +x pruebas/linux/prueba.sh
   ./pruebas/linux/prueba.sh
   ```
9. Revisar los resultados generados en `pruebas/linux/resultados/`.

## 6. Comandos ejecutados por el script

Para cada corrida, el script ejecuta, con `run` de 1 a 5:

```bash
# C
taskset -c 0,1 ./cpp/main --mode sequential --workers 1 --workload 100000000
taskset -c 0,1 ./cpp/main --mode parallel   --workers 2 --workload 100000000

# Python
taskset -c 0,1 python3 python/main.py --mode sequential      --workers 1 --workload 100000000
taskset -c 0,1 python3 python/main.py --mode threading       --workers 2 --workload 100000000
taskset -c 0,1 python3 python/main.py --mode multiprocessing --workers 2 --workload 100000000
```

El tiempo se extrae de la línea `Execution time: X seconds` de la salida del programa y se guarda una fila por corrida.

## 7. Archivos generados

| Archivo | Contenido |
|---|---|
| `resultados/cpp_results.csv` | 10 filas: C secuencial y paralelo (5 repeticiones cada uno) |
| `resultados/python_results.csv` | 15 filas: Python secuencial, `threading` y `multiprocessing` |
| `resultados/hardware_info.txt` | Salida de `lscpu`, `nproc`, `free -h`, versiones |

Formato de los CSV:

```csv
language,mode,workers,cpus,workload,run,time_seconds
```

Se conservan las mediciones individuales, no solo promedios.

## 8. Incidencias

| Fecha | Descripción | Acción tomada |
|---|---|---|
| [2026-10-5] | Sin incidencias | - |


## 9. Reproducibilidad

Para repetir las pruebas basta con seguir la sección 5 en un equipo con al menos 2 CPUs. Los resultados pueden variar ligeramente según el hardware y la carga del sistema, pero la tendencia (secuencial vs. paralelo, `threading` vs. `multiprocessing`) debería mantenerse.