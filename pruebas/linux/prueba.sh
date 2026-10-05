#!/usr/bin/env bash
# Ejecuta las pruebas de C++ y Python limitadas a 2 CPUs (CPU 0 y 1) en Linux.
# Uso: chmod +x run_tests_linux.sh && ./run_tests_linux.sh

# ===== CONFIGURACIÓN (ajusta según los README de P1 y P2) =====
WORKLOAD=100000000
RUNS=5
CPU_LIST="0,1"          # CPUs permitidas (taskset)
CPUS=2                  # cantidad de CPUs (para el CSV)
CPP_EXE="./cpp/main"        # <-- pon el nombre real del ejecutable de P1
PY_SCRIPT="./python/main.py"
PYTHON="python3"
# ==============================================================

OUT_DIR="pruebas/linux/resultados"
mkdir -p "$OUT_DIR"
HEADER="language,mode,workers,cpus,workload,run,time_seconds"

# run_test <lang> <modo> <workers> <csv> <comando...>
run_test() {
  local lang="$1" mode="$2" workers="$3" csv="$4"
  shift 4
  for run in $(seq 1 "$RUNS"); do
    local output t
    output=$(taskset -c "$CPU_LIST" "$@" --mode "$mode" --workers "$workers" --workload "$WORKLOAD" 2>&1)
    t=$(echo "$output" | awk '/Execution time/ {print $3; exit}')
    if [ -z "$t" ]; then
      t="ERROR"
      echo "AVISO: sin tiempo en $lang/$mode run $run" >&2
      echo "$output" >&2
    fi
    echo "$lang,$mode,$workers,$CPUS,$WORKLOAD,$run,$t" >> "$csv"
    echo "$lang $mode run $run -> $t"
  done
}

# ---------- Verificación de afinidad ----------
echo "CPUs visibles con taskset -c $CPU_LIST: $(taskset -c "$CPU_LIST" nproc)"

# ---------- Información de hardware (para procedimiento.md) ----------
{
  echo "=== lscpu ===";        lscpu
  echo; echo "=== nproc ===";   nproc
  echo; echo "=== memoria ==="; free -h
  echo; echo "=== kernel ===";  uname -a
  echo; echo "=== SO ===";      cat /etc/os-release
  echo; echo "=== g++ ===";     g++ --version 2>&1 | head -n 1
  echo; echo "=== python ===";  $PYTHON --version 2>&1
} > "$OUT_DIR/hardware_info.txt"
echo "Hardware guardado en $OUT_DIR/hardware_info.txt"

# ---------- C++ ----------
CSV_CPP="$OUT_DIR/cpp_results.csv"
echo "$HEADER" > "$CSV_CPP"
run_test cpp sequential 1 "$CSV_CPP" "$CPP_EXE"
run_test cpp parallel   2 "$CSV_CPP" "$CPP_EXE"

# ---------- Python ----------
CSV_PY="$OUT_DIR/python_results.csv"
echo "$HEADER" > "$CSV_PY"
run_test python sequential      1 "$CSV_PY" $PYTHON "$PY_SCRIPT"
run_test python threading       2 "$CSV_PY" $PYTHON "$PY_SCRIPT"
run_test python multiprocessing 2 "$CSV_PY" $PYTHON "$PY_SCRIPT"

echo; echo "===== $CSV_CPP ====="; cat "$CSV_CPP"
echo; echo "===== $CSV_PY ====="; cat "$CSV_PY"