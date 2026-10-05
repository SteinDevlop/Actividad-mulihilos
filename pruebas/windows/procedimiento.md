# Procedimiento de pruebas: Windows

## 1. Entorno de ejecución

| Campo | Valor |
|---|---|
| Tipo de entorno | Instalación nativa de Windows |
| Sistema operativo | Windows 11 Pro 64 bits |
| Versión / compilación | `systeminfo` |
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
| Compilador C (gcc / MinGW-w64 o MSVC) | 6.3.0 | `gcc --version` |
| Python | 3.13.1 | `python --version` |

Banderas de compilación del programa en C (deben ser equivalentes a las usadas en Linux):

```powershell
gcc -O2 -pthread -o cpp\main.exe cpp\main.c
```

Si se usó otro compilador (por ejemplo MSVC) o el `cpp\README.md` indica otras banderas, escribirlas aquí exactamente.

## 4. Parámetros del experimento

Idénticos a los usados en Linux.

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

## 5. Limitación de CPUs

Se usa la afinidad de procesador de Windows. El script lanza cada programa y le asigna una máscara de afinidad:

```powershell
$p.ProcessorAffinity = 3    # binario 11 = CPU 0 y CPU 1
```

Máscaras de referencia:

| CPUs | Máscara (decimal / hex) |
|---|---|
| 1 | 1 / 0x1 |
| 2 | 3 / 0x3 |
| 3 | 7 / 0x7 |
| 4 | 15 / 0xF |
| 5 | 31 / 0x1F |

Verificación manual (opcional): con el programa en ejecución, Administrador de tareas > Detalles > clic derecho sobre el proceso > "Establecer afinidad". Solo deben estar marcadas 2 CPUs.

## 6. Procedimiento paso a paso

1. Clonar o actualizar el repositorio.
2. Verificar que Python y el compilador funcionan:
   ```powershell
   python --version
   gcc --version
   ```
3. Instalar las dependencias de Python (si las hay) según `python\README.md`.
4. Compilar el programa en C desde la raíz del repositorio:
   ```powershell
   gcc -O2 -pthread -o cpp\main.exe cpp\main.c
   ```
5. Ejecutar una prueba manual de cada programa y confirmar que imprime `Execution time: X seconds`:
   ```powershell
   .\cpp\main.exe --mode parallel --workers 2 --workload 100000000
   python .\python\main.py --mode threading --workers 2 --workload 100000000
   ```
6. Cerrar programas innecesarios y no usar el equipo durante las mediciones.
7. Si PowerShell bloquea scripts, habilitarlos solo para la sesión:
   ```powershell
   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
   ```
8. Ejecutar el script de pruebas:
   ```powershell
   .\pruebas\windows\prueba.ps1
   ```
9. Revisar los resultados generados en `pruebas\windows\resultados\`.

## 7. Comandos ejecutados por el script

Para cada corrida, con `run` de 1 a 5 (cada proceso se lanza con `Start-Process` y afinidad `3`):

```powershell
# C
.\cpp\main.exe --mode sequential --workers 1 --workload 100000000
.\cpp\main.exe --mode parallel   --workers 2 --workload 100000000

# Python
python .\python\main.py --mode sequential      --workers 1 --workload 100000000
python .\python\main.py --mode threading       --workers 2 --workload 100000000
python .\python\main.py --mode multiprocessing --workers 2 --workload 100000000
```

El tiempo se extrae de la línea `Execution time: X seconds` de la salida del programa y se guarda una fila por corrida.

## 8. Archivos generados

| Archivo | Contenido |
|---|---|
| `resultados\cpp_results.csv` | 10 filas: C secuencial y paralelo (5 repeticiones cada uno) |
| `resultados\python_results.csv` | 15 filas: Python secuencial, `threading` y `multiprocessing` |
| `resultados\hardware_info.txt` | CPU, sistema operativo, RAM y versiones de Python y gcc |

Formato de los CSV:

```csv
language,mode,workers,cpus,workload,run,time_seconds
```

Se conservan las mediciones individuales, no solo promedios.

## 9. Incidencias

| Fecha | Descripción | Acción tomada |
|---|---|---|
| [2026-10-5] | Sin incidencias | - |

## 10. Reproducibilidad

Para repetir las pruebas basta con seguir la sección 6 en un equipo con al menos 2 CPUs lógicas. Los resultados pueden variar ligeramente según el hardware y la carga del sistema, pero la tendencia (secuencial vs. paralelo, `threading` vs. `multiprocessing`) debería mantenerse.