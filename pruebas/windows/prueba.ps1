# Pruebas de C y Python en Windows limitadas a 2 CPUs (CPU 0 y 1).
# Ubicación recomendada: pruebas\windows\prueba.ps1
# Ejecutar desde PowerShell:  .\pruebas\windows\prueba.ps1
# Si PowerShell bloquea el script:
#   Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass

# ===== CONFIGURACIÓN (ajusta según los README de P1 y P2) =====
$workload = 100000000
$runs     = 5
$mask     = 3                     # máscara de afinidad: 3 = CPU 0 y 1
$cpus     = 2
$cppExe   = ".\cpp\main.exe"      # ejecutable compilado de P1 (ver cpp\README.md)
$pyScript = ".\python\main.py"
$python   = "python"              # si no funciona, prueba "py"
# ==============================================================

# Trabajar siempre desde la raíz del repo (dos niveles arriba de pruebas\windows)
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $root

$outDir = "pruebas\windows\resultados"
New-Item -ItemType Directory -Force $outDir | Out-Null
$header = "language,mode,workers,cpus,workload,run,time_seconds"
$tmpOut = Join-Path $outDir "out.tmp"

# --- Comprobaciones previas ---
if (-not (Test-Path $cppExe))   { Write-Error "No existe $cppExe. Compílalo primero (ver cpp\README.md)."; exit 1 }
if (-not (Test-Path $pyScript)) { Write-Error "No existe $pyScript."; exit 1 }
if (-not (Get-Command $python -ErrorAction SilentlyContinue)) { Write-Error "No se encontró '$python' en el PATH."; exit 1 }

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
  (& $python --version 2>&1 | Out-String),
  "=== Compilador (gcc) ===",
  ((gcc --version 2>&1 | Select-Object -First 1) | Out-String)
) | Out-File $hw -Encoding utf8
Write-Host "Hardware guardado en $hw"

function Run-Test($lang, $file, $prefixArgs, $mode, $workers, $csv) {
  for ($run = 1; $run -le $runs; $run++) {
    $argList = "$prefixArgs --mode $mode --workers $workers --workload $workload".Trim()
    $p = Start-Process -FilePath $file -ArgumentList $argList `
         -RedirectStandardOutput $tmpOut -NoNewWindow -PassThru
    try { $p.ProcessorAffinity = [IntPtr]$mask } catch { Write-Warning "No se pudo fijar la afinidad (el proceso terminó muy rápido)." }
    $p.WaitForExit()
    $m = Select-String -Path $tmpOut -Pattern "Execution time: ([\d.,]+)"
    if ($m) { $t = $m.Matches[0].Groups[1].Value } else { $t = "ERROR"; Write-Warning "Sin tiempo en $lang/$mode run $run" }
    "$lang,$mode,$workers,$cpus,$workload,$run,$t" | Out-File $csv -Append -Encoding ascii
    Write-Host "$lang $mode run $run -> $t"
  }
}

# ---------- C / C++ ----------
$csvCpp = "$outDir\cpp_results.csv"
$header | Out-File $csvCpp -Encoding ascii
Run-Test "cpp" $cppExe "" "sequential" 1 $csvCpp
Run-Test "cpp" $cppExe "" "parallel"   2 $csvCpp

# ---------- Python ----------
$csvPy = "$outDir\python_results.csv"
$header | Out-File $csvPy -Encoding ascii
Run-Test "python" $python $pyScript "sequential"      1 $csvPy
Run-Test "python" $python $pyScript "threading"       2 $csvPy
Run-Test "python" $python $pyScript "multiprocessing" 2 $csvPy

Remove-Item $tmpOut -ErrorAction SilentlyContinue

Write-Host "`n===== $csvCpp ====="; Get-Content $csvCpp
Write-Host "`n===== $csvPy ====="; Get-Content $csvPy