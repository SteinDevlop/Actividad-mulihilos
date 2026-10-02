# P2 — Implementación Python

Aplicación en Python (solo biblioteca estándar) que ejecuta **la misma carga CPU-bound** de tres formas:

| Modalidad | Qué hace | ¿Paralelismo CPU real en CPython? |
|---|---|---|
| `sequential` | Un único flujo de ejecución en el hilo principal. | No (línea base). |
| `threading` | Varios *threads* dentro de un solo proceso. | **No** (ver sección GIL). |
| `multiprocessing` | Varios **procesos** del SO, cada uno con su intérprete. | **Sí**. |

La aplicación solo controla `mode`, `workers` y `workload`. **No controla la afinidad de CPU**: eso lo hace P3 desde el sistema operativo (ver "Limitar CPUs").

---

## Inicio rápido (comandos para copiar y pegar)

Todos los comandos se ejecutan desde la carpeta `python/`. No hay nada que instalar aparte de Python 3.9+.

### Linux

```bash
# 1) Preparar (opcional pero recomendado)
python3 -m venv .venv && source .venv/bin/activate

# 2) Ejecutar cada modalidad (ejemplo con 100 millones de iteraciones)
python main.py --mode sequential      --workload 100000000
python main.py --mode threading       --workers 4 --workload 100000000
python main.py --mode multiprocessing --workers 4 --workload 100000000

# 3) Misma ejecución con salida CSV (una línea, lista para registrar)
python main.py --mode multiprocessing --workers 4 --workload 100000000 --csv
```

### Windows (PowerShell)

```powershell
# 1) Preparar (opcional pero recomendado)
py -m venv .venv
.venv\Scripts\Activate.ps1

# 2) Ejecutar cada modalidad
python main.py --mode sequential      --workload 100000000
python main.py --mode threading       --workers 4 --workload 100000000
python main.py --mode multiprocessing --workers 4 --workload 100000000

# 3) Misma ejecución con salida CSV
python main.py --mode multiprocessing --workers 4 --workload 100000000 --csv
```

### Matriz de pruebas con CPUs limitadas (para P3)

Cambia `100000000` por el workload elegido. Usa `--repeat 3` (o más) para tener varias muestras. Regla sugerida: `--workers` = número de CPUs permitidas.

**Linux** (`taskset`; `N` = CPUs permitidas, de 1 a las que tenga la máquina):

```bash
python main.py --mode sequential --workload 100000000 --csv-header > results_linux.csv
taskset -c 0 python main.py --mode sequential --workload 100000000 --repeat 3 --csv >> results_linux.csv

for N in 1 2 3 4; do
  for MODE in threading multiprocessing; do
    taskset -c 0-$((N-1)) python main.py --mode $MODE --workers $N --workload 100000000 --repeat 3 --csv >> results_linux.csv
  done
done
```

**Windows** (PowerShell; la máscara de afinidad es un número que tiene un bit por CPU: 1 CPU = `0x1`, 2 = `0x3`, 3 = `0x7`, 4 = `0xF`, 5 = `0x1F`, 6 = `0x3F`, 7 = `0x7F`, 8 = `0xFF`):

```powershell
python main.py --mode sequential --workload 100000000 --csv-header | Out-File results_windows.csv -Encoding ascii

$masks = @{ 1 = 0x1; 2 = 0x3; 3 = 0x7; 4 = 0xF }
foreach ($n in 1..4) {
  (Get-Process -Id $PID).ProcessorAffinity = $masks[$n]
  foreach ($mode in "sequential","threading","multiprocessing") {
    python main.py --mode $mode --workers $n --workload 100000000 --repeat 3 --csv | Out-File results_windows.csv -Append -Encoding ascii
  }
}
(Get-Process -Id $PID).ProcessorAffinity = [Math]::Pow(2, [Environment]::ProcessorCount) - 1   # restaurar todas las CPUs
```

## 2. Requisitos e instalación

- **Python 3.9 o superior** recomendado. *Verificado únicamente con Python 3.12.3 en Linux* (ver sección 9).
- **Sin dependencias externas** (`requirements.txt` está vacío a propósito). No hay nada que `pip install`.

### Linux

```bash
cd python
python3 --version                 # >= 3.9
python3 -m venv .venv             # opcional, pero recomendado
source .venv/bin/activate
pip install -r requirements.txt   # no instala nada; se incluye por completitud
python main.py --mode sequential --workload 20000000
```

Si `venv` falta en Debian/Ubuntu: `sudo apt install python3-venv`.

### Windows (PowerShell)

```powershell
cd python
py --version                      # >= 3.9
py -m venv .venv
.venv\Scripts\Activate.ps1        # en cmd.exe: .venv\Scripts\activate.bat
pip install -r requirements.txt
python main.py --mode sequential --workload 20000000
```

Si PowerShell bloquea el script de activación: `Set-ExecutionPolicy -Scope Process RemoteSigned`, o usa `cmd.exe`.

### Diferencias entre sistemas

- Windows no tiene `fork`: los procesos se crean con **`spawn`** (cada proceso hijo reimporta los módulos). Linux por defecto usaba `fork` (hasta Python 3.13; `forkserver` desde 3.14).
- Para que la comparación sea justa, esta aplicación usa **`spawn` en ambos sistemas por defecto**. El coste de arrancar procesos queda incluido en el tiempo medido y es mayor en Windows: es un fenómeno real del SO, no un error.
- Se puede cambiar con `--start-method fork|forkserver` (solo Linux/macOS; en Windows solo existe `spawn`).
- Usa `python` o `py` según tu instalación; en Linux suele ser `python3` fuera del entorno virtual.

## 3. Uso

```
python main.py --mode {sequential,threading,multiprocessing} [opciones]
# equivalente: cd src && python -m cpubench --mode ...
```

| Parámetro | Default | Descripción |
|---|---|---|
| `--mode` | (obligatorio) | `sequential`, `threading` o `multiprocessing`. |
| `--workload N` | `20000000` | Iteraciones totales. Acepta `100_000_000`. Debe ser ≥ 1. |
| `--workers N` | `1` | Threads o procesos. Debe ser ≥ 1. En `sequential` se ignora (se reporta `Workers: 1` y se avisa por stderr). |
| `--repeat N` | `1` | Repite la ejecución N veces; una salida por repetición. |
| `--csv` | off | Una línea CSV por repetición en lugar de texto. |
| `--csv-header` | off | Imprime antes la cabecera CSV (implica `--csv`). |
| `--start-method` | `spawn` | Solo `multiprocessing`: `spawn`, `fork`, `forkserver`. |
| `--verbose` | off | En modo texto, añade versión de Python, SO, estado del GIL y start method. |

Argumentos inválidos (workers < 1, workload no numérico, modo desconocido) terminan con código de salida 2 y un mensaje.

### Ejemplos

```bash
python main.py --mode sequential --workload 100000000
python main.py --mode threading --workers 4 --workload 100000000
python main.py --mode multiprocessing --workers 4 --workload 100000000
python main.py --mode multiprocessing --workers 4 --workload 20000000 --repeat 5 --csv-header
```

## 4. Formato de salida

Texto (por defecto):

```
Language: Python
Mode: multiprocessing
Workers: 4
Workload: 100000000
Execution time: 5.3821 seconds
Result: 15594732105878822272
```

CSV (`--csv`), columnas fijas, separador coma, tiempo con 6 decimales:

```
language,mode,workers,workload,time_seconds,result
Python,multiprocessing,4,100000000,5.382100,15594732105878822272
```

Los avisos van a **stderr**, nunca a stdout, así que se puede redirigir stdout directamente a un `.csv`.

## 5. Qué se mide

Con `time.perf_counter()`, exactamente la llamada del executor, igual en las tres modalidades:

- **Incluido:** creación de threads/procesos (y del `Pool`), ejecución, comunicación de parciales y agregación.
- **Excluido:** parseo de argumentos, importaciones, impresión de resultados.

En `multiprocessing` el arranque de procesos cuenta como parte del coste de la modalidad; por eso con workloads muy pequeños será más lento que `sequential`. Usa workloads grandes (segundos) para que ese coste sea despreciable.

## 6. `threading`, CPU-bound y el GIL

CPython tiene el **Global Interpreter Lock (GIL)**: solo un thread ejecuta bytecode Python a la vez dentro de un proceso. La carga de este proyecto es Python puro y limitada por CPU, así que:

- Con `threading`, los threads **se turnan** en lugar de ejecutarse en paralelo. Aunque haya varios núcleos disponibles, el tiempo es ≈ el del secuencial, y a menudo algo peor por el cambio de contexto entre threads.
- Que `threading` **no acelere** esta carga no es un bug: es el resultado esperado y el motivo por el que se incluye en el experimento.
- `threading` sí ayuda en cargas I/O-bound (el GIL se libera mientras se espera), pero esta carga no hace I/O.
- `multiprocessing` evita el GIL usando un intérprete (y un GIL) por proceso, por lo que sí puede usar varios núcleos.
- Excepción: en builds *free-threaded* de CPython (3.13t/3.14t, opcionales), el GIL está desactivado y `threading` podría paralelizar. Comprueba con `--verbose` la línea `GIL:`. En CPython estándar aparece `enabled`.

## 7. Limitar CPUs (responsabilidad de P3, fuera del programa)

El programa no toca la afinidad. Los procesos hijos heredan la afinidad del padre, así que basta limitar el proceso que lanza Python. **`workers` y CPUs disponibles son variables independientes**: se recomienda probar p. ej. `--workers` igual al número de CPUs permitidas.

**Linux** (`taskset`, lista de CPUs):

```bash
taskset -c 0     python main.py --mode multiprocessing --workers 1 --workload 100000000 --csv
taskset -c 0-3   python main.py --mode multiprocessing --workers 4 --workload 100000000 --csv
```

**Windows** (máscara de afinidad en hexadecimal: `1`=CPU0, `3`=CPU0-1, `F`=CPU0-3, `FF`=CPU0-7):

```bat
start "" /b /wait /affinity F python main.py --mode multiprocessing --workers 4 --workload 100000000 --csv
```

Alternativa PowerShell (fija la afinidad de la propia sesión; los hijos la heredan):

```powershell
(Get-Process -Id $PID).ProcessorAffinity = 0xF
python main.py --mode multiprocessing --workers 4 --workload 100000000 --csv
```

### Barrido de ejemplo para generar un CSV (Linux)

```bash
python main.py --mode sequential --workload 100000000 --csv-header > results.csv
for cpus in 1 2 3 4; do
  last=$((cpus-1))
  for mode in threading multiprocessing; do
    taskset -c 0-$last python main.py --mode $mode --workers $cpus --workload 100000000 --repeat 3 --csv >> results.csv
  done
done
```

(`results*.csv` ya está en `.gitignore`.) Nota: en este barrido el secuencial se ejecuta sin `taskset`; para fijarlo a una CPU, antepón `taskset -c 0`.

## 8. Estructura

```
python/
├── main.py                 # punto de entrada
├── src/cpubench/
│   ├── workload.py         # kernel, partición, agregación (sin hilos/procesos)
│   ├── executors.py        # sequential / threading / multiprocessing
│   ├── timing.py           # perf_counter
│   ├── report.py           # formato texto/CSV
│   ├── cli.py              # argumentos, validación, orquestación
│   └── __main__.py         # python -m cpubench
├── tests/
├── requirements.txt
└── .gitignore
```
