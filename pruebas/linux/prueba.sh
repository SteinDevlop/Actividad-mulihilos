#!/usr/bin/env bash
# Pruebas de C++ y Python en Linux, repetidas con 1, 2 y 3 CPUs (taskset).
# Ubicación recomendada: pruebas/linux/prueba.sh  (se ejecuta desde cualquier carpeta)
#
# Uso:
#   chmod +x prueba.sh
#   ./prueba.sh            # solo C++ (por defecto)
#   ./prueba.sh python     # solo Python
#   ./prueba.sh all        # ambos
#
# Se pueden sobreescribir valores sin editar el archivo, por ejemplo:
#   RUNS=3 CPP_WORKLOAD=2000000000 ./prueba.sh cpp

TARGET="${1:-cpp}"

# ===== CONFIGURACIÓN (según cpp/README.md y python/README.md) =====
CPU_LEVELS="${CPU_LEVELS:-1 2 3}"        # cantidad de CPUs: 0 | 0,1 | 0,1,2
RUNS="${RUNS:-5}"                        # repeticiones medidas por configuración
WARMUP="${WARMUP:-1}"                    # corridas de calentamiento (se descartan)

# C++ (P1): calibrar en cada equipo hasta que el secuencial dure 5-10 s
# (README: entre 1.5 y 2.5 mil millones). 100M sirve para probar rápido.
CPP_WORKLOAD="${CPP_WORKLOAD:-100000000}"
CPP_SRC="./cpp/main.cpp"
CPP_EXE="./cpp/main"
CPP_WORKERS="${CPP_WORKERS:-1 2 4 8}"    # hilos (E1/E1-C) y procesos (E2)

# Python (P2): workload recomendado 10-20 M, calibrado en cada equipo
PY_WORKLOAD="${PY_WORKLOAD:-10000000}"
PY_SCRIPT="./python/checksum.py"
PYTHON="${PYTHON:-python3}"
PY_WORKERS="${PY_WORKERS:-1 2 4 8}"
# ===================================================================

# Resultados de referencia (tabla de los README); con otro N no se valida
expected_for() {
  case "$1" in
    1) echo 0 ;;
    7) echo 163 ;;
    1000) echo 4032535 ;;
    1000000) echo 3999998828414 ;;
    10000000) echo 399999987067243 ;;
    20000000) echo 1599999973856681 ;;
    100000000) echo 39999999869145770 ;;
    *) echo "" ;;
  esac
}

cd "$(dirname "$0")/../.." || exit 1     # raíz del repo
OUT_DIR="pruebas/linux/resultados"
mkdir -p "$OUT_DIR"
ERR_LOG="$OUT_DIR/errores.log"
: > "$ERR_LOG"

# Cabecera = columna cpus + CSV v2 oficial de 12 columnas
HEADER="cpus,language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result"
TOTAL_CPUS=$(nproc)

# run_config <csv> <cpus> <expect> <comando...>
# Ejecuta UNA configuración (con --runs repeticiones) limitada a <cpus> CPUs.
run_config() {
  local csv="$1" cpus="$2" expect="$3"
  shift 3
  local cpu_list args out rc rows times
  cpu_list=$(seq -s, 0 $((cpus - 1)))
  args=("$@" --csv --runs "$RUNS" --warmup "$WARMUP")
  [ -n "$expect" ] && args+=(--expect "$expect")

  out=$(taskset -c "$cpu_list" "${args[@]}" 2>>"$ERR_LOG")
  rc=$?
  rows=$(echo "$out" | grep -E '^(C\+\+|Python),')

  if [ $rc -ne 0 ] || [ -z "$rows" ]; then
    echo "AVISO: fallo (código $rc) con $cpus CPU(s): ${args[*]}" | tee -a "$ERR_LOG" >&2
    echo "$out" >> "$ERR_LOG"
    return
  fi
  echo "$rows" | sed "s/^/$cpus,/" >> "$csv"
  times=$(echo "$rows" | awk -F, '{printf "%s ", $(NF-2)}')
  echo "  cpus=$cpus | $* -> $times"
}

# ---------- Verificación de afinidad ----------
echo "CPUs totales: $TOTAL_CPUS"
for k in $CPU_LEVELS; do
  [ "$k" -le "$TOTAL_CPUS" ] && echo "  taskset con $k CPU(s) -> nproc = $(taskset -c "$(seq -s, 0 $((k - 1)))" nproc)"
done

# ---------- Información de hardware (para procedimiento.md) ----------
{
  echo "=== lscpu ===";        lscpu
  echo; echo "=== nproc ===";   nproc
  echo; echo "=== memoria ==="; free -h
  echo; echo "=== kernel ===";  uname -a
  echo; echo "=== SO ===";      cat /etc/os-release
  echo; echo "=== g++ ===";     g++ --version 2>&1 | head -n 1
  echo; echo "=== python ===";  $PYTHON --version 2>&1
  echo; echo "=== GIL activo (debe ser True; en Python < 3.13 no existe el atributo) ==="
  $PYTHON -c "import sys; print(getattr(sys, '_is_gil_enabled', lambda: True)())" 2>&1
  echo; echo "=== parámetros ==="
  echo "RUNS=$RUNS WARMUP=$WARMUP CPU_LEVELS=$CPU_LEVELS"
  echo "CPP_WORKLOAD=$CPP_WORKLOAD PY_WORKLOAD=$PY_WORKLOAD"
} > "$OUT_DIR/hardware_info.txt"
echo "Hardware guardado en $OUT_DIR/hardware_info.txt"

# ======================= C++ =======================
test_cpp() {
  if [ ! -x "$CPP_EXE" ]; then
    echo "No existe $CPP_EXE; compilando con las banderas oficiales..."
    g++ -std=c++17 -O0 -Wall -Wextra -pthread "$CPP_SRC" -o "$CPP_EXE" || { echo "Error al compilar" >&2; return 1; }
  fi
  local csv="$OUT_DIR/cpp_results.csv" exp k w
  echo "$HEADER" > "$csv"
  exp=$(expected_for "$CPP_WORKLOAD")
  for k in $CPU_LEVELS; do
    if [ "$k" -gt "$TOTAL_CPUS" ]; then echo "Se omite $k CPUs (el equipo solo tiene $TOTAL_CPUS)"; continue; fi
    echo "--- C++ con $k CPU(s) ---"
    # E0: línea base
    run_config "$csv" "$k" "$exp" "$CPP_EXE" --mode sequential --workload "$CPP_WORKLOAD"
    # E1 / E1-C: hilos en un proceso
    for w in $CPP_WORKERS; do
      run_config "$csv" "$k" "$exp" "$CPP_EXE" --mode threads --workers "$w" --workload "$CPP_WORKLOAD"
    done
    # E2: procesos independientes (1 hilo cada uno)
    for w in $CPP_WORKERS; do
      run_config "$csv" "$k" "$exp" "$CPP_EXE" --mode processes --processes "$w" --workload "$CPP_WORKLOAD"
    done
    # E3: mixto (2 procesos: 5 y 3 hilos)
    run_config "$csv" "$k" "$exp" "$CPP_EXE" --mode mixed --workload "$CPP_WORKLOAD"
  done
}

# ======================= Python =======================
test_python() {
  local csv="$OUT_DIR/python_results.csv" exp k w
  echo "$HEADER" > "$csv"
  exp=$(expected_for "$PY_WORKLOAD")
  for k in $CPU_LEVELS; do
    if [ "$k" -gt "$TOTAL_CPUS" ]; then echo "Se omite $k CPUs (el equipo solo tiene $TOTAL_CPUS)"; continue; fi
    echo "--- Python con $k CPU(s) ---"
    run_config "$csv" "$k" "$exp" $PYTHON "$PY_SCRIPT" --mode sequential --workload "$PY_WORKLOAD"
    for w in $PY_WORKERS; do
      run_config "$csv" "$k" "$exp" $PYTHON "$PY_SCRIPT" --mode threading --workers "$w" --workload "$PY_WORKLOAD"
    done
    for w in $PY_WORKERS; do
      run_config "$csv" "$k" "$exp" $PYTHON "$PY_SCRIPT" --mode multiprocessing --workers "$w" --workload "$PY_WORKLOAD"
    done
  done
}

case "$TARGET" in
  cpp)    test_cpp ;;
  python) test_python ;;
  all)    test_cpp; test_python ;;
  *)      echo "Uso: $0 [cpp|python|all]" >&2; exit 1 ;;
esac

echo
echo "Listo. Resultados en $OUT_DIR (errores, si hubo: $ERR_LOG)"