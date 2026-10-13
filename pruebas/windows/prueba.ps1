# Pruebas de Python y C++ en Windows, repetidas con 1, 2 y 3 CPUs (afinidad).
# Ubicación recomendada: pruebas\windows\prueba.ps1
#
# Uso desde PowerShell:
#   .\pruebas\windows\prueba.ps1                 # solo Python (por defecto)
#   .\pruebas\windows\prueba.ps1 -Target cpp     # solo C++
#   .\pruebas\windows\prueba.ps1 -Target all     # ambos
#   .\pruebas\windows\prueba.ps1 -Runs 3 -PyWorkload 20000000
# Si PowerShell bloquea el script:
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

param(
  [ValidateSet("python", "cpp", "all")] [string]$Target = "python",
  [int[]]$CpuLevels = @(1, 2, 3),        # cantidad de CPUs: máscara 1 | 3 | 7
  [int]$Runs   = 5,                      # repeticiones medidas por configuración
  [int]$Warmup = 1,                      # corridas de calentamiento (se descartan)

  # C++ (P1): calibrar hasta que el secuencial dure 5-10 s (1.5 a 2.5 mil millones)
  [long]$CppWorkload = 100000000,
  [string]$CppExe = ".\cpp\main.exe",
  [int[]]$CppWorkers = @(1, 2, 4, 8),    # hilos (E1/E1-C) y procesos (E2)

  # Python (P2): workload recomendado 10-20 M, calibrado en cada equipo
  [long]$PyWorkload = 10000000,
  [string]$PyScript = ".\python\checksum.py",
  [string]$Python = "python",            # si no funciona, probar "py"
  [int[]]$PyWorkers = @(1, 2, 4, 8)
)

# Resultados de referencia (tabla de los README); con otro N no se valida
$expectedTable = @{
  1 = "0"; 7 = "163"; 1000 = "4032535"; 1000000 = "3999998828414"
  10000000 = "399999987067243"; 20000000 = "1599999973856681"
  100000000 = "39999999869145770"
}
function Get-Expected([long]$n) { if ($expectedTable.ContainsKey([int64]$n)) { $expectedTable[[int64]$n] } else { "" } }

# Trabajar siempre desde la raíz del repo (dos niveles arriba de pruebas\windows)
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $root

$outDir = "pruebas\windows\resultados"
New-Item -ItemType Directory -Force $outDir | Out-Null
$errLog = Join-Path $outDir "errores.log"
"" | Out-File $errLog -Encoding utf8

# Cabecera = columna cpus + CSV v2 oficial de 12 columnas
$header = "cpus,language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result"
$totalCpus = [Environment]::ProcessorCount

# --- Comprobaciones previas ---
if ($Target -ne "python" -and -not (Test-Path $CppExe)) {
  Write-Error "No existe $CppExe. Compílalo: g++ -std=c++17 -O0 -Wall -Wextra -pthread main.cpp -o main.exe (dentro de cpp\)"; exit 1
}
if ($Target -ne "cpp") {
  if (-not (Test-Path $PyScript)) { Write-Error "No existe $PyScript."; exit 1 }
  if (-not (Get-Command $Python -ErrorAction SilentlyContinue)) { Write-Error "No se encontró '$Python' en el PATH."; exit 1 }
}

# --- Información de hardware (para procedimiento.md) ---
$hw = Join-Path $outDir "hardware_info.txt"
@(
  "=== CPU ===",
  (Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors | Format-List | Out-String),
  "=== SO ===",
  (Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, OSArchitecture | Format-List | Out-String),
  "=== Memoria (GB) ===",
  [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2),
  "",
  "=== Python ===",
  (& $Python --version 2>&1 | Out-String),
  "=== GIL activo (debe ser True; en Python < 3.13 no existe el atributo) ===",
  (& $Python -c "import sys; print(getattr(sys, '_is_gil_enabled', lambda: True)())" 2>&1 | Out-String),
  "=== Compilador (g++) ===",
  ((g++ --version 2>&1 | Select-Object -First 1) | Out-String),
  "=== Parámetros ===",
  "Runs=$Runs Warmup=$Warmup CpuLevels=$($CpuLevels -join ',')",
  "CppWorkload=$CppWorkload PyWorkload=$PyWorkload"
) | Out-File $hw -Encoding utf8
Write-Host "Hardware guardado en $hw"

# La afinidad se fija en ESTE proceso de PowerShell antes de lanzar cada prueba;
# los procesos hijos la heredan desde su creación (sin condiciones de carrera).
$self = [System.Diagnostics.Process]::GetCurrentProcess()
$origAffinity = $self.ProcessorAffinity

# Run-Config <csv> <cpus> <expect> <archivo> <argumentos...>
# Ejecuta UNA configuración (con -Runs repeticiones) limitada a <cpus> CPUs.
function Run-Config($csv, [int]$cpus, $expect, $file, [string[]]$cmdArgs) {
  $self.ProcessorAffinity = [IntPtr]((1 -shl $cpus) - 1)
  $a = @($cmdArgs) + @("--csv", "--runs", $Runs, "--warmup", $Warmup)
  if ($expect) { $a += @("--expect", $expect) }

  $out = & $file @a 2>&1
  $rc = $LASTEXITCODE
  $lines = @($out | ForEach-Object { "$_" })
  $rows = @($lines | Where-Object { $_ -match '^(C\+\+|Python),' })

  if ($rc -ne 0 -or $rows.Count -eq 0) {
    $msg = "AVISO: fallo (código $rc) con $cpus CPU(s): $file $($a -join ' ')"
    Write-Warning $msg
    $msg | Out-File $errLog -Append -Encoding utf8
    $lines | Out-File $errLog -Append -Encoding utf8
    return
  }
  $rows | ForEach-Object { "$cpus,$_" } | Out-File $csv -Append -Encoding ascii
  $times = ($rows | ForEach-Object { ($_ -split ',')[-3] }) -join " "
  Write-Host "  cpus=$cpus | $($cmdArgs -join ' ') -> $times"
}

# ======================= Python =======================
function Test-Python {
  $csv = "$outDir\python_results.csv"
  $header | Out-File $csv -Encoding ascii
  $exp = Get-Expected $PyWorkload
  foreach ($k in $CpuLevels) {
    if ($k -gt $totalCpus) { Write-Host "Se omite $k CPUs (el equipo solo tiene $totalCpus)"; continue }
    Write-Host "--- Python con $k CPU(s) ---"
    Run-Config $csv $k $exp $Python @($PyScript, "--mode", "sequential", "--workload", $PyWorkload)
    foreach ($w in $PyWorkers) {
      Run-Config $csv $k $exp $Python @($PyScript, "--mode", "threading", "--workers", $w, "--workload", $PyWorkload)
    }
    foreach ($w in $PyWorkers) {
      Run-Config $csv $k $exp $Python @($PyScript, "--mode", "multiprocessing", "--workers", $w, "--workload", $PyWorkload)
    }
  }
}

# ======================= C++ =======================
function Test-Cpp {
  $csv = "$outDir\cpp_results.csv"
  $header | Out-File $csv -Encoding ascii
  $exp = Get-Expected $CppWorkload
  foreach ($k in $CpuLevels) {
    if ($k -gt $totalCpus) { Write-Host "Se omite $k CPUs (el equipo solo tiene $totalCpus)"; continue }
    Write-Host "--- C++ con $k CPU(s) ---"
    # E0: línea base
    Run-Config $csv $k $exp $CppExe @("--mode", "sequential", "--workload", $CppWorkload)
    # E1 / E1-C: hilos en un proceso
    foreach ($w in $CppWorkers) {
      Run-Config $csv $k $exp $CppExe @("--mode", "threads", "--workers", $w, "--workload", $CppWorkload)
    }
    # E2: procesos independientes (1 hilo cada uno)
    foreach ($w in $CppWorkers) {
      Run-Config $csv $k $exp $CppExe @("--mode", "processes", "--processes", $w, "--workload", $CppWorkload)
    }
    # E3: mixto (2 procesos: 5 y 3 hilos)
    Run-Config $csv $k $exp $CppExe @("--mode", "mixed", "--workload", $CppWorkload)
  }
}

try {
  if ($Target -ne "cpp")    { Test-Python }
  if ($Target -ne "python") { Test-Cpp }
}
finally {
  $self.ProcessorAffinity = $origAffinity   # restaurar la afinidad de la sesión
}

Write-Host "`nListo. Resultados en $outDir (errores, si hubo: $errLog)"