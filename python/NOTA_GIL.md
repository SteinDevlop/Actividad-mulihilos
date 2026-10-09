# Nota técnica: el GIL en Python (`threading` vs `multiprocessing`)

> **Autor:** P2 · **Para:** P4 (informe, sección 4 y complementos "C++ vs Python").
> **Estado:** ⚠️ **PRELIMINAR.** Las cifras de abajo se midieron en un entorno de pruebas (VM Linux de **2 vCPU**, Python 3.13 con GIL activo), **no** en los equipos oficiales. Cuando P3 entregue los CSV oficiales (`resultados_linux.csv`, `resultados_windows.csv`), hay que **reemplazar las tablas** y confirmar que las conclusiones se mantienen. La forma de reproducirlo es `python3 matriz.py` (ver `README.md`).

## 1. Qué es el GIL, en dos líneas

El **GIL** (*Global Interpreter Lock*) es un candado del intérprete CPython: en cada instante **solo un hilo** puede ejecutar código Python, aunque haya varios procesadores lógicos libres. Los hilos pueden *alternarse* (concurrencia), pero no *correr a la vez* (paralelismo) mientras ejecutan bytecode. Cada **proceso** tiene su propio intérprete y su propio GIL, así que varios procesos sí corren en paralelo.

## 2. Qué se midió

* Cálculo: el checksum de la Fase 0, con **N = 10 000 000**, en Python puro (sin NumPy ni optimizaciones).
* Modos: `sequential`, `threading` y `multiprocessing` con 1, 2, 4 y 8 trabajadores.
* **5 repeticiones** por configuración, con 1 corrida de calentamiento descartada.
* Resultado numérico verificado en cada corrida: **399 999 987 067 243** (coincide con el kernel C++ de contraste).
* Entorno: Linux, 2 LP visibles, Python 3.13.16 (`sys._is_gil_enabled()` = `True`).

## 3. Resultados (promedio de 5 corridas, todos los LP)

Speedup = tiempo secuencial (2,030 s) ÷ tiempo del modo.

| Modo | Trabajadores | Tiempo (s) | Desv. (%) | CPU total (%) | Speedup |
|---|---|---|---|---|---|
| sequential | 1 | 2,030 | 3,4 | 99 | 1,00 |
| threading | 1 | 2,082 | 6,5 | 99 | 0,98 |
| threading | 2 | 2,295 | 4,5 | 99 | 0,88 |
| threading | 4 | 2,436 | 7,0 | 99 | 0,83 |
| threading | 8 | 2,199 | 1,6 | 100 | 0,92 |
| multiprocessing | 1 | 2,092 | 4,0 | 98 | 0,97 |
| multiprocessing | 2 | 1,118 | 7,8 | 192 | 1,82 |
| multiprocessing | 4 | 1,086 | 3,6 | 193 | 1,87 |
| multiprocessing | 8 | 1,083 | 6,9 | 192 | 1,87 |

Prueba con **1 solo LP** (`taskset -c 0`, 4 trabajadores, 3 corridas):

| Modo | Tiempo promedio (s) | CPU total (%) |
|---|---|---|
| threading | 2,110 | ~98 |
| multiprocessing | 2,159 | ~99 |

Datos crudos: `resultados_preliminares_sandbox.csv` y `resultados_preliminares_sandbox_1lp.csv`.

## 4. Interpretación (qué se observa · por qué · qué demuestra)

**Evidencia A — `threading` no acelera.**
* *Qué se observa:* con 2, 4 u 8 hilos el tiempo es igual o algo **peor** que el secuencial (0,83–0,92 de speedup), y el CPU total se queda en ~100 %, es decir, **un solo LP ocupado** aunque hay 2 disponibles.
* *Por qué ocurre:* el GIL deja ejecutar un hilo a la vez. Los demás esperan el candado y el intérprete gasta tiempo cambiando de hilo (peor con más hilos: 4 hilos fue el más lento).
* *Qué demuestra:* en carga CPU-bound, los hilos de Python dan **concurrencia pero no paralelismo**. Es el mismo comportamiento que la hipótesis H2 predice para la afinidad a 1 LP, pero aquí aparece **con todos los LP disponibles**.

**Evidencia B — `multiprocessing` sí acelera, hasta el número de LP.**
* *Qué se observa:* con 2 procesos el tiempo baja de 2,03 s a 1,12 s (speedup 1,82) y el CPU sube a ~192 % (dos LP ocupados). Con 4 y 8 procesos **ya no mejora** (1,09 s y 1,08 s).
* *Por qué ocurre:* cada proceso tiene su propio GIL, así que el scheduler del SO puede ubicarlos en LP distintos. Como el equipo tiene solo 2 LP, más de 2 procesos solo se reparten el tiempo de los mismos 2 LP.
* *Qué demuestra:* el paralelismo real en Python se logra con procesos, y el techo lo pone el hardware (hipótesis H1 y H3: sube pero no linealmente, y se estanca cuando trabajadores > LP). El speedup de 1,82 y no 2,00 refleja el costo de crear procesos y juntar resultados.

**Evidencia C — con 1 LP ambos modos son iguales.**
* *Qué se observa:* con `taskset -c 0`, `threading` y `multiprocessing` con 4 trabajadores tardan ~2,1 s, igual que el secuencial.
* *Por qué ocurre:* sin un segundo LP no hay dónde ejecutar en paralelo, ni con procesos.
* *Qué demuestra:* la ventaja de `multiprocessing` depende de que el hardware ofrezca más de un LP; con un LP solo hay concurrencia.

## 5. Para el informe (P4)

1. **Cita sugerida:** "En CPython el GIL impide que varios hilos ejecuten bytecode a la vez; por eso `threading` no reduce el tiempo en una carga CPU-bound, mientras que `multiprocessing` sí lo reduce hasta saturar los LP."
2. **Comparación con C++:** en C++ los hilos del mismo proceso sí corren en paralelo; en Python solo los procesos lo hacen. Comparar las gráficas "tiempo vs hilos" de ambos lenguajes.
3. **Tiempo absoluto:** Python tardó ~2 s con 10 M, mientras que en C++ el workload es de ~2 000 M para durar 5–10 s: el kernel en Python es cientos de veces más lento. Por eso los workloads son distintos y **solo se comparan tendencias (speedup, CPU %), no tiempos absolutos**.
4. **Windows:** `multiprocessing` usa `spawn` (equivale a `CreateProcess`), que arranca un intérprete nuevo por proceso; se espera un costo de arranque mayor que con `fork` en Linux.

## 6. Limitaciones

* Entorno de prueba de **2 vCPU virtuales**: no permite estudiar bien el caso trabajadores > LP con muchos LP ni el nivel 16.
* Los números oficiales dependerán del equipo de P3; **verificar la versión de Python** y que el GIL esté activo (`python -c "import sys; print(sys._is_gil_enabled())"` debe dar `True`). Las compilaciones *free-threaded* (3.13t / 3.14t) quitan el GIL y cambiarían la conclusión.
* El CPU % de `multiprocessing` se calcula sumando el CPU medido dentro de cada hijo; no incluye el arranque del intérprete en `spawn` (Windows), así que ahí puede quedar algo por debajo de lo que muestra el Administrador de tareas.
* Desviación entre repeticiones: casi todas < 10 %; la máxima fue 7,8 % (multiprocessing, 2 procesos), dentro del criterio de la Fase 0.
