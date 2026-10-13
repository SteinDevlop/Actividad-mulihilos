# Procedimiento de pruebas: Windows (Python y C++)

## 1. Entorno de ejecución

| Campo | Valor |
|---|---|
| Tipo de entorno | Instalación nativa de Windows |
| Sistema operativo | Windows 11 Home Single Language 64 bits |
| Versión / compilación | 10.0.26300 (`systeminfo`) |
| Plan de energía | Equilibrado |

## 2. Hardware

| Campo | Valor |
|---|---|
| Modelo de CPU | AMD Ryzen 5 5500U with Radeon Graphics |
| Núcleos físicos | 6 |
| Hilos lógicos (threads) | 12 |
| Memoria RAM | 10.85 GB |

## 3. Herramientas y versiones

| Herramienta | Versión | Comando |
|---|---|---|
| Compilador C++ (g++ / MinGW) | MinGW.org GCC-6.3.0-1 (6.3.0) | `g++ --version` |
| Python | 3.13.1 | `python --version` |
| GIL de Python | Activo (`True`) | `python -c "import sys; print(getattr(sys, '_is_gil_enabled', lambda: True)())"` |

Comando de compilación del programa en C++ (ejecutar dentro de `cpp\`):

```powershell
g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main.exe
```

El ejecutable resultante queda en `cpp\main.exe`. Si el `cpp\README.md` indica otras banderas o se usó otro compilador, escribirlas aquí exactamente.

## 4. Parámetros del experimento

Valores por defecto de `pruebas\windows\prueba.ps1` (se pueden cambiar con parámetros del script).

| Parámetro | Valor | Parámetro del script |
|---|---|---|
| Niveles de CPU (afinidad) | 1, 2 y 3 CPUs | `-CpuLevels` |
| Repeticiones medidas por configuración | 5 | `-Runs` |
| Corridas de calentamiento (se descartan) | 1 | `-Warmup` |
| Workload C++ | 100000000 | `-CppWorkload` |
| Workload Python | 10000000 | `-PyWorkload` |
| Workers C++ (hilos / procesos) | 1, 2, 4, 8 | `-CppWorkers` |
| Workers Python (hilos / procesos) | 1, 2, 4, 8 | `-PyWorkers` |
| Programa C++ | `.\cpp\main.exe` | `-CppExe` |
| Programa Python | `.\python\checksum.py` | `-PyScript` |
| Intérprete de Python | `python` (alternativa: `py`) | `-Python` |
| Lenguaje a probar | `python` (por defecto), `cpp` o `all` | `-Target` |

Notas:

- El workload de C++ debe calibrarse en cada equipo hasta que el modo secuencial dure entre 5 y 10 s (referencia: 1.5 a 2.5 mil millones). Con 100000000 el tiempo será menor.
- El workload de Python recomendado es de 10 a 20 millones, calibrado en cada equipo.
- El equipo tiene 12 CPUs lógicas, por lo que se ejecutan los tres niveles. Si un nivel supera las CPUs del equipo, el script lo omite.

### 4.1 Configuraciones medidas

Cada configuración se mide con cada nivel de CPU (1, 2 y 3).

**Python** (9 configuraciones por nivel de CPU):

| Modo | Workers | Descripción |
|---|---|---|
| `sequential` | 1 (sin `--workers`) | Línea base |
| `threading` | 1, 2, 4, 8 | Hilos en un proceso (sujetos al GIL) |
| `multiprocessing` | 1, 2, 4, 8 | Procesos independientes |

**C++** (10 configuraciones por nivel de CPU):

| Experimento | Modo | Workers | Descripción |
|---|---|---|---|
| E0 | `sequential` | 1 | Línea base |
| E1 / E1-C | `threads` | 1, 2, 4, 8 | Hilos en un proceso |
| E2 | `processes` | 1, 2, 4, 8 (`--processes`) | Procesos independientes, 1 hilo cada uno |
| E3 | `mixed` | 8 en total | 2 procesos con 5 y 3 hilos |

### 4.2 Total de mediciones

| Lenguaje | Configuraciones | Niveles de CPU | Repeticiones | Filas esperadas |
|---|---|---|---|---|
| Python | 9 | 3 | 5 | 135 |
| C++ | 10 | 3 | 5 | 150 |

Cada configuración ejecuta además 1 corrida de calentamiento que no se guarda.

## 5. Limitación de CPUs

Se usa la afinidad de procesador de Windows. El script fija la afinidad en su propio proceso de PowerShell antes de lanzar cada configuración; los procesos hijos la heredan desde su creación (no hay condiciones de carrera). La máscara se calcula como `(1 -shl k) - 1`:

```powershell
$self.ProcessorAffinity = [IntPtr]((1 -shl $cpus) - 1)
```

Al terminar (incluso si hay error) el script restaura la afinidad original de la sesión.

Máscaras de referencia:

| CPUs | Máscara (decimal / hex) | Binario |
|---|---|---|
| 1 | 1 / 0x1 | `1` |
| 2 | 3 / 0x3 | `11` |
| 3 | 7 / 0x7 | `111` |
| 4 | 15 / 0xF | `1111` |

Verificación manual (opcional): con un programa en ejecución, Administrador de tareas > Detalles > clic derecho sobre el proceso > "Establecer afinidad". Solo deben estar marcadas las CPUs del nivel probado.

## 6. Procedimiento paso a paso

1. Clonar o actualizar el repositorio.
2. Verificar que Python y el compilador funcionan:
   ```powershell
   python --version
   g++ --version
   ```
3. Instalar las dependencias de Python (si las hay) según `python\README.md`.
4. Compilar el programa en C++ (dentro de `cpp\`):
   ```powershell
   cd cpp
   g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main.exe
   cd ..
   ```
5. Ejecutar una prueba manual de cada programa desde la raíz del repositorio y confirmar que imprimen resultados correctos:
   ```powershell
   .\cpp\main.exe --mode threads --workers 2 --workload 100000000
   python .\python\checksum.py --mode threading --workers 2 --workload 10000000
   ```
6. Cerrar programas innecesarios y no usar el equipo durante las mediciones.
7. Si PowerShell bloquea scripts, habilitarlos solo para la sesión:
   ```powershell
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
   ```
8. Ejecutar el script de pruebas (desde la raíz del repositorio; el script también se posiciona solo en la raíz):
   ```powershell
   .\pruebas\windows\prueba.ps1 -Target all
   ```
   Variantes:
   ```powershell
   .\pruebas\windows\prueba.ps1                   # solo Python (por defecto)
   .\pruebas\windows\prueba.ps1 -Target cpp       # solo C++
   .\pruebas\windows\prueba.ps1 -Runs 3 -PyWorkload 20000000
   ```
9. Revisar los resultados generados en `pruebas\windows\resultados\` y, si hubo fallos, `errores.log`.

## 7. Comandos ejecutados por el script

Para cada nivel de CPU `k` (1, 2, 3), el script aplica la afinidad y ejecuta cada configuración **una sola vez**, pasando al programa los argumentos `--csv --runs 5 --warmup 1` (y `--expect <valor>` si el workload tiene resultado de referencia). Las repeticiones las hace el propio programa.

```powershell
# Python
python .\python\checksum.py --mode sequential --workload 10000000 --csv --runs 5 --warmup 1
python .\python\checksum.py --mode threading --workers <1|2|4|8> --workload 10000000 --csv --runs 5 --warmup 1
python .\python\checksum.py --mode multiprocessing --workers <1|2|4|8> --workload 10000000 --csv --runs 5 --warmup 1

# C++
.\cpp\main.exe --mode sequential --workload 100000000 --csv --runs 5 --warmup 1
.\cpp\main.exe --mode threads --workers <1|2|4|8> --workload 100000000 --csv --runs 5 --warmup 1
.\cpp\main.exe --mode processes --processes <1|2|4|8> --workload 100000000 --csv --runs 5 --warmup 1
.\cpp\main.exe --mode mixed --workload 100000000 --csv --runs 5 --warmup 1
```

### 7.1 Validación del resultado

Con `--expect`, el programa verifica que el resultado coincida con el valor de referencia. El script solo lo pasa si el workload está en esta tabla; con otro valor no se valida.

| Workload | Resultado esperado |
|---|---|
| 1 | 0 |
| 7 | 163 |
| 1000 | 4032535 |
| 1000000 | 3999998828414 |
| 10000000 | 399999987067243 |
| 20000000 | 1599999973856681 |
| 100000000 | 39999999869145770 |

### 7.2 Manejo de errores

Si una configuración termina con código distinto de 0 o no produce filas CSV, el script emite un aviso, registra el comando y la salida en `errores.log` y continúa con la siguiente configuración.

## 8. Archivos generados

Carpeta: `pruebas\windows\resultados\`

| Archivo | Contenido |
|---|---|
| `python_results.csv` | Python: `sequential`, `threading` y `multiprocessing` (135 filas esperadas) |
| `cpp_results.csv` | C++: `sequential`, `threads`, `processes` y `mixed` (150 filas esperadas) |
| `hardware_info.txt` | CPU, sistema operativo, RAM, versión de Python, estado del GIL, versión de g++ y parámetros usados |
| `errores.log` | Fallos de configuraciones (vacío si no hubo errores) |

Formato de los CSV (13 columnas: `cpus` más el CSV v2 oficial de 12 columnas):

```csv
cpus,language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result
```

Se conservan las mediciones individuales, no solo promedios. Las corridas de calentamiento no se guardan.

## 9. Incidencias

| Fecha | Descripción | Acción tomada |
|---|---|---|
| 2026-10-13 | `hardware_info.txt` muestra la tilde de "Parámetros" mal codificada (`ParÃ¡metros`) | Solo es un problema de codificación del encabezado; no afecta a los datos medidos |

## 10. Reproducibilidad

Para repetir las pruebas basta con seguir la sección 6 en un equipo con al menos 3 CPUs lógicas (si tiene menos, el script omite los niveles que no caben). Los resultados pueden variar ligeramente según el hardware y la carga del sistema, pero la tendencia (secuencial vs. paralelo, hilos vs. procesos, `threading` limitado por el GIL frente a `multiprocessing`) debería mantenerse.