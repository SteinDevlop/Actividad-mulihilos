# Pruebas y mediciones (Persona 3)

Este documento explica cómo ejecutar las pruebas de rendimiento de las implementaciones en **C** (`cpp/`) y **Python** (`python/`) en **Linux** y en **Windows**, limitando la ejecución a **2 CPUs**, y cómo dejar los resultados listos para el informe.

## 1. Estructura

```text
pruebas/
├── README.md                    <- este archivo
├── linux/
│   ├── prueba.sh                <- script de pruebas para Linux
│   ├── procedimiento.md         <- procedimiento y hardware (se completa a mano)
│   └── resultados/
│       ├── cpp_results.csv
│       ├── python_results.csv
│       └── hardware_info.txt    <- generado por el script
└── windows/
    ├── prueba.ps1               <- script de pruebas para Windows
    ├── procedimiento.md
    └── resultados/
        ├── cpp_results.csv
        ├── python_results.csv
```

## 2. Parámetros del experimento

Deben ser **idénticos** en Linux y Windows para que los resultados sean comparables.

| Parámetro | Valor |
|---|---|
| Workload | 100000000 |
| Repeticiones por configuración | 5 |
| CPUs utilizadas | 2 (CPU 0 y CPU 1) |
| Workers secuencial | 1 |
| Workers paralelo | 2 |

Configuraciones medidas:

| Lenguaje | Modo | Workers |
|---|---|---|
| C/C++ | `sequential` | 1 |
| C/C++ | `parallel` | 2 |
| Python | `sequential` | 1 |
| Python | `threading` | 2 |
| Python | `multiprocessing` | 2 |

Total: 5 configuraciones x 5 repeticiones = 25 corridas por sistema operativo.

## 3. Formato de los resultados

Un CSV por lenguaje, una fila por corrida (nunca solo promedios):

```csv
language,mode,workers,cpus,workload,run,time_seconds
cpp,sequential,1,2,100000000,1,8.421
cpp,parallel,2,2,100000000,1,4.531
python,multiprocessing,2,2,100000000,1,6.721
```

## 4. Pruebas en Linux

### 4.1 Preparación (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential python3 python3-pip python3-venv util-linux procps
```

### 4.2 Compilar el programa en C

Desde la raíz del repositorio:

```bash
gcc -O2 -pthread -o cpp/main cpp/main.c
```

### 4.3 Ejecutar el script

Siempre desde la **raíz del repositorio**:

```bash
chmod +x pruebas/linux/prueba.sh
./pruebas/linux/prueba.sh
```

Variables a revisar al inicio del script: `CPP_EXE` (debe ser `./cpp/main`), `PYTHON` (`python3`, o `python` si usas entorno virtual activo), `CPU_LIST`, `WORKLOAD`, `RUNS`.

### 4.5 Comando equivalente

```bash
taskset -c 0,1 ./cpp/main --mode parallel --workers 2 --workload 100000000
```

`taskset -c 0,1` restringe el proceso a las CPUs 0 y 1.

## 5. Pruebas en Windows

### 5.1 Preparación

- Python 3 instalado y disponible como `python` (o `py`) en PowerShell: `python --version`.
- Un compilador de C: MinGW-w64/MSYS2 (`gcc`) o MSVC. Sigue `cpp/README.md`.

### 5.2 Compilar el programa en C

Desde la raíz del repositorio, por ejemplo con MinGW:

```powershell
gcc -O2 -pthread -o cpp\main.exe cpp\main.c
```

### 5.3 Prueba manual de cada programa

```powershell
.\cpp\main.exe --mode parallel --workers 2 --workload 100000000
python .\python\main.py --mode threading --workers 2 --workload 100000000
```

### 5.4 Ejecutar el script

Desde cualquier carpeta, en PowerShell:

```powershell
.\pruebas\windows\prueba.ps1
```

Si PowerShell bloquea la ejecución de scripts:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

El script fija la afinidad del proceso con la máscara `3` (binario `11` = CPU 0 y CPU 1). Otras máscaras: `1` = 1 CPU, `7` = 3 CPUs, `F` = 4 CPUs, `1F` = 5 CPUs.
