# Implementación en C — Concurrencia y Paralelismo de Hilos

Este módulo contiene la implementación completa desarrollada en **lenguaje C puro** (`main.c`) para la evaluación de concurrencia y paralelismo en sistemas operativos **Windows** y **Linux**. 

El software implementa una carga de trabajo estrictamente **CPU-bound** idéntica y matemáticamente compatible con la versión de Python, permitiendo comparar de forma directa el comportamiento del planificador del sistema operativo (*scheduler*), la distribución de carga en los núcleos del procesador y los tiempos de ejecución.

---

## 1. Guía de Creación y Ejecución de Hilos en Ambos Sistemas Operativos

En lenguaje C, la gestión de hilos a bajo nivel depende directamente de la interfaz del sistema operativo subyacente. El código fuente de `main.c` implementa soporte nativo condicional para ambos entornos sin dependencias externas:

### En Windows (Win32 Thread API)
En Windows, los hilos son administrados directamente por el kernel a través de la API Win32 provista en `<windows.h>`:

1. **Creación:** Se utiliza la función `CreateThread`:
   ```c
   handles[i] = CreateThread(
       NULL,                // Atributos de seguridad por defecto
       0,                   // Tamaño inicial de pila por defecto (1 MB)
       thread_worker_entry, // Función que ejecutará el hilo (DWORD WINAPI)
       &tasks[i],           // Puntero con los datos asignados a la tarea
       0,                   // Flags (0 indica inicio inmediato)
       NULL                 // Puntero para recibir el Thread ID del sistema
   );
   ```
2. **Sincronización (Join):** La sincronización y espera de finalización se realiza mediante `WaitForMultipleObjects`:
   ```c
   WaitForMultipleObjects(workers, handles, TRUE, INFINITE);
   ```
   Esta función bloquea al hilo principal hasta que todos los hilos (`TRUE`) hayan concluido su cómputo.
3. **Liberación de Recursos:** Cada identificador de hilo devuelto por el kernel debe cerrarse con `CloseHandle(handles[i])` para evitar fugas de memoria en la tabla de *handles* del SO.

### En Linux (POSIX Threads / pthreads)
En entornos tipo UNIX / Linux, la creación y control de hilos se realiza conforme al estándar IEEE POSIX 1003.1c mediante `<pthread.h>`:

1. **Creación:** Se utiliza la llamada al sistema `pthread_create`:
   ```c
   int rc = pthread_create(
       &handles[i],         // Puntero donde se almacena el identificador pthread_t
       NULL,                // Atributos por defecto (joinable, prioridad estándar)
       thread_worker_entry, // Función de entrada (void* (*)(void*))
       &tasks[i]            // Argumento pasado a la función del hilo
   );
   ```
2. **Sincronización (Join):** La espera de cada hilo se efectúa de manera individual iterando con `pthread_join`:
   ```c
   for (unsigned int i = 0; i < workers; ++i) {
       pthread_join(handles[i], NULL);
   }
   ```
   `pthread_join` suspende el hilo principal hasta que el hilo indicado termine y libera automáticamente los recursos del hilo en el kernel.

---

## 2. Explicación del Código Fuente y Manejo de Concurrencia

El programa `main.c` está estructurado en módulos lógicos que garantizan una medición rigurosa:

* **Carga CPU-bound (`kernel`):**
  Aplica transformaciones aritméticas de 64 bits (`XOR`, desplazamientos de bits y multiplicaciones modulares) sobre un rango `[start, stop)`. No realiza operaciones de entrada/salida (I/O), accesos a disco, llamadas a red ni retardos (`sleep`), asegurando que el uso de CPU dependa exclusivamente del procesamiento continuo de instrucciones en la ALU del microprocesador.
* **Particionamiento (`run_parallel`):**
  El rango total de iteraciones se divide equitativamente entre los `workers` solicitados. Si la división no es exacta, el remanente (`workload % workers`) se distribuye entre los primeros hilos, garantizando que todo el trabajo sea cubierto sin solapamientos ni omisiones.
* **Mitigación de *False Sharing*:**
  Cada hilo acumula su cálculo dentro de variables locales o registros del CPU y escribe su resultado en la estructura `ThreadTask` únicamente al terminar su intervalo. Esto evita que múltiples núcleos compitan por invalidar la misma línea de memoria caché (L1/L2).
* **Medición de Tiempo de Alta Resolución:**
  - En Windows se utiliza `QueryPerformanceCounter` y `QueryPerformanceFrequency`.
  - En Linux se emplea `clock_gettime(CLOCK_MONOTONIC, ...)`.
  Ambos mecanismos son inmunes a cambios de hora del sistema y miden exclusivamente el tiempo de procesamiento de los ejecutores, excluyendo el parseo de argumentos de línea de comandos.

---

## 3. Instrucciones de Compilación

Persona 3 debe compilar el archivo `main.c` siguiendo las instrucciones correspondientes a su sistema operativo.

### En Windows
El ejecutable puede compilarse con GCC (MinGW) o con MSVC (Visual Studio):

* **Opción con MinGW / GCC (Recomendado):**
  ```powershell
  gcc -O3 -Wall -Wextra main.c -o main.exe
  ```
* **Opción con Microsoft Visual C++ (MSVC / Developer Command Prompt):**
  ```cmd
  cl /O2 /W4 main.c /Fe:main.exe
  ```

### En Linux
En cualquier distribución de Linux (Ubuntu, Debian, Fedora, Arch, etc.), Persona 3 debe utilizar `gcc` enlazando la biblioteca de hilos POSIX (`-pthread`):

```bash
gcc -O3 -Wall -Wextra main.c -pthread -o main
```

---

## 4. Guía de Ejecución para Persona 3 (Pruebas y Mediciones)

Persona 3 puede ejecutar el programa configurando el modo, la cantidad de hilos y la carga de trabajo desde la línea de comandos sin alterar el código fuente.

### Parámetros Disponibles
| Parámetro | Valor por defecto | Descripción |
|---|---|---|
| `--mode` | *(Obligatorio)* | Modalidad: `sequential`, `parallel` o `threading`. |
| `--workload N` | `20000000` | Número total de iteraciones (admite guiones bajos, ej. `100_000_000` o `1000000000`). |
| `--workers N` | `1` | Número de hilos de trabajo (se ignora en `sequential`). |
| `--repeat N` | `1` | Cantidad de repeticiones consecutivas. |
| `--csv` | Desactivado | Emite la salida en formato CSV. |
| `--csv-header` | Desactivado | Imprime el encabezado CSV antes de los registros. |
| `--verbose` | Desactivado | Imprime detalles del compilador, SO y núcleos de hardware detectados. |

### Ejemplos Básicos de Ejecución

**En Windows:**
```powershell
# 1. Ejecución secuencial (línea base)
.\main.exe --mode sequential --workload 100000000

# 2. Ejecución paralela con 2, 4 y 8 hilos
.\main.exe --mode parallel --workers 2 --workload 100000000
.\main.exe --mode parallel --workers 4 --workload 100000000
.\main.exe --mode parallel --workers 8 --workload 100000000

# 3. Salida CSV lista para guardar resultados
.\main.exe --mode parallel --workers 4 --workload 100000000 --repeat 3 --csv
```

**En Linux:**
```bash
# 1. Ejecución secuencial (línea base)
./main --mode sequential --workload 100000000

# 2. Ejecución paralela con 2, 4 y 8 hilos
./main --mode parallel --workers 2 --workload 100000000
./main --mode parallel --workers 4 --workload 100000000
./main --mode parallel --workers 8 --workload 100000000

# 3. Salida CSV lista para guardar resultados
./main --mode parallel --workers 4 --workload 100000000 --repeat 3 --csv
```

---

## 5. Procedimiento para Limitar CPUs y Generar la Matriz de Pruebas

Persona 3 debe controlar la cantidad de núcleos de CPU asignados al proceso mediante las herramientas del sistema operativo.

### En Windows (PowerShell)
En Windows, Persona 3 utilizará la propiedad `ProcessorAffinity` del proceso de PowerShell para definir la máscara de bits de CPUs permitidas (1 CPU = `0x1`, 2 CPUs = `0x3`, 3 CPUs = `0x7`, 4 CPUs = `0xF`):

```powershell
# 1. Crear el archivo CSV con encabezado
.\main.exe --mode sequential --workload 100000000 --csv-header | Out-File cpp_results_windows.csv -Encoding ascii

# 2. Ejecutar secuencial en 1 núcleo (3 repeticiones)
(Get-Process -Id $PID).ProcessorAffinity = 0x1
.\main.exe --mode sequential --workload 100000000 --repeat 3 --csv | Out-File cpp_results_windows.csv -Append -Encoding ascii

# 3. Ejecutar matriz paralela variando núcleos y workers (1 a 4 CPUs)
$masks = @{ 1 = 0x1; 2 = 0x3; 3 = 0x7; 4 = 0xF }
foreach ($n in 1..4) {
    (Get-Process -Id $PID).ProcessorAffinity = $masks[$n]
    .\main.exe --mode parallel --workers $n --workload 100000000 --repeat 3 --csv | Out-File cpp_results_windows.csv -Append -Encoding ascii
}

# 4. Restaurar la afinidad a todos los núcleos del equipo
(Get-Process -Id $PID).ProcessorAffinity = [Math]::Pow(2, [Environment]::ProcessorCount) - 1
```

### En Linux (Bash con `taskset`)
En Linux, Persona 3 utilizará el comando `taskset -c` indicando la lista o rango de CPUs permitidas:

```bash
# 1. Crear el archivo CSV con encabezado
./main --mode sequential --workload 100000000 --csv-header > cpp_results_linux.csv

# 2. Ejecutar secuencial fijado al Core 0 (3 repeticiones)
taskset -c 0 ./main --mode sequential --workload 100000000 --repeat 3 --csv >> cpp_results_linux.csv

# 3. Ejecutar matriz paralela variando núcleos y workers (1 a 4 CPUs)
for N in 1 2 3 4; do
    last_core=$((N - 1))
    taskset -c 0-$last_core ./main --mode parallel --workers $N --workload 100000000 --repeat 3 --csv >> cpp_results_linux.csv
done
```

---

## 6. Monitoreo y Verificación de Carga por Núcleo en Tiempo Real

Para cumplir con la comprobación experimental requerida por el docente, Persona 3 debe observar cómo los hilos activan simultáneamente los diferentes núcleos de cómputo.

> **Nota para la observación visual:** Como C compila a código de máquina nativo optimizado, el cómputo de `100_000_000` de iteraciones puede demorar menos de 0.1 segundos. Para que la carga permanezca en pantalla durante varios segundos y se aprecie en las gráficas, Persona 3 debe usar un `--workload` mayor, por ejemplo `2000000000` (2 mil millones) o `5000000000` (5 mil millones).

### En Windows
1. Abrir el **Administrador de Tareas** (`Ctrl + Shift + Esc`).
2. Ir a la pestaña **Rendimiento** y seleccionar **CPU**.
3. Hacer clic derecho sobre la gráfica de CPU > **Cambiar gráfico a** > **Procesadores lógicos**.
4. Al ejecutar:
   ```powershell
   .\main.exe --mode parallel --workers 4 --workload 3000000000
   ```
   Persona 3 observará cómo **Core 0, Core 1, Core 2 y Core 3 suben simultáneamente al 100% de uso**.
5. También se puede utilizar el **Monitor de Rendimiento y Recursos** (`resmon.exe`) para visualizar el consumo individual por hilo y núcleo.

### En Linux
1. **Herramienta `htop`:**
   Ejecutar `htop` en una terminal. La parte superior muestra barras independientes para cada CPU (`1`, `2`, `3`, `4`...). Al lanzar la ejecución paralela, Persona 3 verá cómo las barras correspondientes a los núcleos asignados se saturan al 100%.
2. **Herramienta `top`:**
   Ejecutar `top` y presionar la tecla numérica `1` para desplegar el uso detallado de cada núcleo (`Cpu0`, `Cpu1`, `Cpu2`, etc.).
3. **Herramienta `mpstat` (Reporte por segundo):**
   Para imprimir en terminal el uso por núcleo segundo a segundo como solicita la guía:
   ```bash
   mpstat -P ALL 1
   ```
   En otra terminal, Persona 3 lanza el benchmark y observará el reporte continuo de `%usr` subiendo en cada CPU.

---

## 7. Tabla Comparativa de Herramientas del Sistema Operativo

A continuación se resumen las herramientas y comandos disponibles en cada sistema operativo para la manipulación y análisis de concurrencia y paralelismo:

| Función | Herramienta en Windows | Herramienta / Comando en Linux |
|---|---|---|
| **Creación nativa de hilos en C** | API Win32 (`CreateThread`, `<windows.h>`) | Estándar POSIX (`pthread_create`, `<pthread.h>`) |
| **Sincronización de hilos** | `WaitForMultipleObjects`, `WaitForSingleObject` | `pthread_join` |
| **Monitoreo gráfico por núcleo** | Administrador de Tareas (Procesadores lógicos), Monitor de Recursos (`resmon.exe`) | `htop`, `glances`, Monitor del Sistema GNOME |
| **Monitoreo por consola** | PowerShell `Get-Process`, `typeperf` | `top` (tecla `1`), `htop`, `mpstat -P ALL 1` |
| **Asignación / Afinidad de CPU** | `(Get-Process).ProcessorAffinity = 0x...`, `start /affinity <MASK>` | `taskset -c <CPUs>` |
| **Prioridad de planificación** | `Set-Process -Priority`, Administrador de Tareas | `nice`, `renice`, `chrt` |
| **Inspección de hardware / núcleos** | `wmic cpu get NumberOfCores,NumberOfLogicalProcessors` | `lscpu`, `nproc`, `cat /proc/cpuinfo` |

---

## 8. Formato de Salida de los Resultados

### Salida Estándar de Texto
```text
Language: C
Mode: parallel
Workers: 4
Workload: 100000000
Execution time: 0.0192 seconds
Result: 15594732105878822272
```

### Salida en CSV (`--csv`)
```csv
language,mode,workers,workload,time_seconds,result
C,parallel,4,100000000,0.019200,15594732105878822272
```

> **Verificación de consistencia:** Para un `--workload` de `100000000`, tanto en C como en Python, el campo `Result` es exactamente `15594732105878822272`. Esto garantiza que ambas implementaciones realizaron la misma cantidad exacta de trabajo computacional.
