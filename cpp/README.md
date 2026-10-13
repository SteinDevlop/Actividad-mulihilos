# Implementación en C++17 — Laboratorio de Procesos e Hilos (P1)

Este directorio contiene la implementación oficial de referencia desarrollada en **C++17** (`main.cpp`) para la evaluación experimental de **concurrencia, paralelismo, planificación del sistema operativo (scheduler) y migración de hilos** en **Windows y Linux**.

El software implementa un cálculo estrictamente **CPU-bound** (suma de verificación de 64 bits) unificado y matemáticamente compatible con la versión de Python, permitiendo comparar de forma directa el comportamiento del scheduler, la saturación de los procesadores lógicos (LP) y los tiempos de ejecución.

---

## 1. Algoritmo y Kernel de Carga (Fase 0)

Para cada entero $i \in [0, N)$ se calcula la función:
$$f(i) = ((i \bmod 97) \times (i \bmod 53)) \oplus (i \ll 3) \pmod{2^{64}}$$

El resultado global es la suma acumulada:
$$\text{Resultado} = \sum_{i=0}^{N-1} f(i) \pmod{2^{64}}$$

### Control de Optimizaciones del Compilador
Para garantizar que las mediciones reflejen fielmente el costo del procesamiento y no sean alteradas por optimizaciones del compilador (eliminación de código muerto, vectorización SIMD agresiva o evaluación en tiempo de compilación), el programa:
1. Utiliza un acumulador con calificador **`volatile`**.
2. Se compila obligatoriamente con la bandera de optimización **`-O0`**.

### Particionamiento del Rango $[0, N)$
El rango total se divide equitativamente entre $W$ trabajadores en bloques de tamaño $N / W$. El residuo ($N \pmod W$) se asigna al último bloque. Al ser la adición modular conmutativa y asociativa, **el resultado numérico debe ser exactamente idéntico en todos los modos y configuraciones**.

---

## 2. Soporte Nativo en Windows y Linux

El programa utiliza exclusivamente las **APIs nativas de bajo nivel** de cada sistema operativo (sin bibliotecas externas):

| Operación | Linux | Windows |
|---|---|---|
| **Creación de Hilos** | `pthread_create` (`<pthread.h>`) | `CreateThread` (`<windows.h>`) |
| **Sincronización de Hilos** | `pthread_join` | `WaitForMultipleObjects` / `WaitForSingleObject` |
| **Creación de Procesos** | `fork()` (`<unistd.h>`) | `CreateProcessA` relanzando el binario con `--child` |
| **Sincronización de Procesos** | `waitpid()` (`<sys/wait.h>`) | `WaitForSingleObject(hProcess)` |
| **Comunicación Interproceso (IPC)** | Tuberías anónimas POSIX (`pipe()`) | Tuberías anónimas Win32 (`CreatePipe`) |
| **Identificación de CPU / LP** | `sched_getcpu()` (`<sched.h>`) | `GetCurrentProcessorNumber()` (`<windows.h>`) |
| **Afinidad de Procesador** | `sched_getaffinity()` | `GetProcessAffinityMask()` |
| **Medición de Tiempo Monotónico** | `std::chrono::steady_clock` | `std::chrono::steady_clock` |
| **Medición de Tiempo de CPU** | `getrusage()` (`RUSAGE_SELF` + `RUSAGE_CHILDREN`) | `GetProcessTimes()` (Padre + Procesos Hijos) |

---

## 3. Instrucciones de Compilación

Persona 3 debe compilar `main.cpp` con las mismas banderas en ambos sistemas operativos:

### En Windows (MinGW-w64 / GCC)
Desde PowerShell o CMD dentro de la carpeta `cpp/`:
```powershell
g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main.exe
```

### En Linux (GCC / G++)
Desde la terminal en la carpeta `cpp/`:
```bash
g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main
```

> **Verificación post-compilación:** Al ejecutar `./main --help` (o `.\main.exe --help`), el programa debe iniciar y mostrar el menú de ayuda sin errores ni advertencias.

---

## 4. Calibración del Workload (Para Persona 3)

Antes de lanzar la batería de experimentos oficiales, Persona 3 debe calibrar el parámetro `--workload` en cada máquina física o entorno de pruebas:

1. Ejecutar el modo secuencial con un valor inicial de 100 millones:
   ```powershell
   .\main.exe --mode sequential --workload 100000000
   ```
2. Escalar linealmente el workload hasta que el tiempo secuencial esté entre **5 y 10 segundos** (por ejemplo, entre `1500000000` y `2500000000`, es decir, 1.5 a 2.5 mil millones).
3. **Comprobar linealidad:** Confirmar que al duplicar el workload, el tiempo se duplica aproximadamente (garantía de que la carga es estrictamente CPU-bound y no está siendo optimizada).
4. **Fijar el workload:** Una vez calibrado, **no cambiarlo** durante todas las corridas de la matriz de C++ en esa máquina.

---

## 5. Parámetros de Línea de Comandos

| Parámetro | Alias | Valor por Defecto | Descripción |
|---|---|---|---|
| `--mode` | — | `sequential` | Modo de ejecución: `sequential`, `threads`, `processes`, `mixed`. |
| `--workers` | `-w`, `--threads` | `1` | Número de trabajadores concurrentes (hilos o procesos). |
| `--processes` | `-p` | `1` | Número de procesos independientes (para `--mode processes`). |
| `--workload` | `-n` | `2000000000` | Tamaño $N$ del rango de iteraciones. Admite guiones bajos (ej. `2_000_000_000`). |
| `--runs` | `--repeat` | `1` | Cantidad de repeticiones de la medición. |
| `--warmup` | — | `0` | Corridas previas de calentamiento que se ejecutan y se descartan. |
| `--csv` | — | Desactivado | Emite la salida en formato oficial **CSV v2**. |
| `--header` | `--csv-header` | Desactivado | Imprime la cabecera CSV antes de los registros. |
| `--log-lp` | — | Desactivado | Imprime por `stderr` el procesador lógico de inicio y fin de cada trabajador. |
| `--affinity-label` | — | Auto-detectado | Sobrescribe la etiqueta de afinidad en el CSV (`all`, `1`, etc.). |
| `--expect` | — | — | Valida que el resultado sea igual al valor esperado (código de salida 2 si falla). |
| `--verbose` | — | Desactivado | Imprime detalles del SO, modo y configuración detectada. |
| `--help` | `-h` | — | Muestra la ayuda en pantalla. |

---

## 6. Guía de Ejecución de los Escenarios Experimentales (Para P3)

A continuación se detallan los comandos exactos para ejecutar cada uno de los bloques de la matriz experimental requerida por la Fase 0:

### Escenario E0: Línea Base Secuencial
* 1 proceso, 1 hilo, workload calibrado, 5 repeticiones:
```powershell
# Windows
.\main.exe --mode sequential --workload 2000000000 --runs 5 --csv --header

# Linux
./main --mode sequential --workload 2000000000 --runs 5 --csv --header
```

### Escenario E1: Multihilo en Todos los Procesadores Lógicos (Paralelismo)
* 1 proceso, variando $W \in \{1, 2, 4, 8, 16\}$ hilos, todos los núcleos disponibles:
```powershell
# Windows (ejemplo con 4 y 8 hilos, 5 repeticiones)
.\main.exe --mode threads --workers 4 --workload 2000000000 --runs 5 --csv
.\main.exe --mode threads --workers 8 --workload 2000000000 --runs 5 --csv

# Linux
./main --mode threads --workers 4 --workload 2000000000 --runs 5 --csv
./main --mode threads --workers 8 --workload 2000000000 --runs 5 --csv
```

### Escenario E1-C: Multihilo con 1 LP (Concurrencia vs. Paralelismo)
* 1 proceso, variando $W \in \{1, 2, 4, 8\}$ hilos, pero **fijando la afinidad a 1 solo procesador lógico**:

**En Windows (usando afinidad a Core 0):**
```powershell
# Opción 1: Con start /affinity 1 (CMD o PowerShell)
cmd /c "start /affinity 1 /wait main.exe --mode threads --workers 4 --workload 2000000000 --runs 5 --csv"

# Opción 2: Fijando afinidad en la sesión de PowerShell
(Get-Process -Id $PID).ProcessorAffinity = 1
.\main.exe --mode threads --workers 4 --workload 2000000000 --runs 5 --csv --affinity-label 1
```

**En Linux (usando `taskset -c 0`):**
```bash
taskset -c 0 ./main --mode threads --workers 4 --workload 2000000000 --runs 5 --csv --affinity-label 1
```

> **Hipótesis H2 a validar:** Con afinidad a 1 LP, aumentar los hilos no reduce el tiempo de ejecución (tiempo constante o ligeramente peor por context switches), y la CPU no superará el 100%.

### Escenario E2: Multiproceso Nativo (Hilos vs. Procesos)
* Variando $P \in \{1, 2, 4, 8\}$ procesos independientes con 1 hilo cada uno:
```powershell
# Windows
.\main.exe --mode processes --processes 2 --workload 2000000000 --runs 5 --csv
.\main.exe --mode processes --processes 4 --workload 2000000000 --runs 5 --csv
.\main.exe --mode processes --processes 8 --workload 2000000000 --runs 5 --csv

# Linux
./main --mode processes --processes 2 --workload 2000000000 --runs 5 --csv
./main --mode processes --processes 4 --workload 2000000000 --runs 5 --csv
./main --mode processes --processes 8 --workload 2000000000 --runs 5 --csv
```

### Escenario E3: Escenario Mixto (2 Procesos: 5 y 3 Hilos)
* 2 procesos donde el Proceso A crea 5 hilos y el Proceso B crea 3 hilos (total 8 trabajadores):
```powershell
# Windows
.\main.exe --mode mixed --workload 2000000000 --runs 5 --csv

# Linux
./main --mode mixed --workload 2000000000 --runs 5 --csv
```

---

## 7. Evidencia de Migración del Scheduler (`--log-lp`)

Para recolectar evidencia de la migración de hilos entre núcleos (Hipótesis H5 y Pregunta 7 del informe), ejecutar con el flag `--log-lp`:

```powershell
.\main.exe --mode threads --workers 4 --workload 500000000 --log-lp
```

Salida típica observada por `stderr`:
```text
[Thread 0] LP inicio: 11, LP fin: 11 (migro: NO)
[Thread 3] LP inicio: 8, LP fin: 4 (migro: SI)
[Thread 2] LP inicio: 7, LP fin: 9 (migro: SI)
[Thread 1] LP inicio: 5, LP fin: 1 (migro: SI)
```
Si `LP inicio != LP fin`, el planificador del sistema operativo migró dinámicamente el hilo de un núcleo lógico a otro durante su ejecución para balancear la temperatura o la carga del sistema.

---

## 8. Formato de Resultados (CSV v2)

La salida con `--csv` genera registros de 12 columnas:

```csv
language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result
C++,windows,threads,1,4,4,all,2000000000,1,0.812300,392,39999999869145770
C++,windows,threads,1,4,4,1,2000000000,1,3.210400,99,39999999869145770
C++,windows,processes,4,1,4,all,2000000000,1,0.852100,385,39999999869145770
C++,windows,mixed,2,5,3,8,all,2000000000,1,0.825000,388,39999999869145770
```

---

## 9. Tabla de Valores de Referencia para Validación

Para comprobar que el cómputo es idéntico a Python antes de realizar mediciones masivas, usar `--expect`:

| Workload ($N$) | Resultado Numérico de 64 bits Esperado |
|---|---|
| $1$ | `0` |
| $7$ | `163` |
| $1\,000$ | `4032535` |
| $1\,000\,000$ | `3999998828414` |
| $10\,000\,000$ | `399999987067243` |
| $20\,000\,000$ | `1599999973856681` |
| $100\,000\,000$ | `39999999869145770` |

Ejemplo de validación automatizada:
```powershell
.\main.exe --mode mixed --workload 1000000 --expect 3999998828414
```
Si el resultado coincide, el programa termina con código `0`. Si no coincide, imprime un mensaje de error y termina con código `2`.

---

## 10. Nota Técnica: Diferencias Observadas entre Hilos y Procesos (Insumo para P4)

A solicitud del Plan de Trabajo (entregable de P1 para P4), se resumen las diferencias observadas en la implementación:

1. **Espacio de Memoria:**
   * **Hilos:** Comparten el mismo espacio de direcciones virtuales, tabla de descriptores y memoria dinámica (heap). Cada hilo únicamente posee su propia pila (stack) y registros de CPU. Esto permite consolidar resultados en memoria compartida sin sobrecosto de copia.
   * **Procesos:** Cada proceso hijo posee un espacio de direcciones aislado y protegido. No comparten memoria de forma predeterminada; los resultados parciales deben transmitirse mediante mecanismos de comunicación interproceso (IPC, tuberías anónimas `pipe`/`CreatePipe`).

2. **Sobrecarga de Creación y Destrucción:**
   * La creación de un hilo (`CreateThread` / `pthread_create`) requiere únicamente reservar la pila y una estructura de control en el kernel.
   * La creación de un proceso (`CreateProcess` / `fork`) implica duplicar estructuras del kernel, inicializar tablas de páginas, cargar el ejecutable y crear el hilo primario, resultando en un costo de arranque considerablemente mayor que se aprecia claramente en cargas de corta duración ($N$ pequeño).

3. **Rendimiento en Cargas CPU-Bound:**
   * Una vez que los trabajadores (hilos o procesos) están en ejecución continua dentro del bucle de la ALU, **el tiempo de cálculo puro es prácticamente idéntico**, ya que ambos son unidades planificables por el scheduler sobre los mismos procesadores lógicos. La ventaja de los hilos radica en su menor consumo de memoria y menor tiempo de inicio/finalización.
