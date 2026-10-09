# Plan de Trabajo Unificado — Laboratorio de Procesos e Hilos

## Índice

1. Cómo usar este documento
2. Visión general del proyecto
3. Fase 0 — Definición del problema, la solución y el diseño experimental
4. Fichas de trabajo por persona (P1, P2, P3, P4)
5. Matriz de entradas y salidas
6. Preguntas de análisis: quién aporta qué
7. Estructura del informe y rúbrica
8. Cronograma e hitos
9. Convenciones del repositorio
10. Lista de verificación final

---

## 1. Cómo usar este documento

* **Todos** leen las secciones 2, 3, 5 y 8.
* **Cada persona** lee a fondo su ficha en la sección 4 y marca su checklist.
* Cada ficha tiene siempre la misma estructura:

| Bloque | Qué responde |
|---|---|
| **Objetivo** | ¿Para qué existe mi rol? |
| **Entra** | ¿Qué necesito recibir para empezar? |
| **Tareas clave** | ¿Qué hago, en qué orden? |
| **Entregables** | ¿Qué archivos o resultados debo producir y a quién van? |
| **Hasta dónde llegar** | ¿Qué incluye mi trabajo y qué **no** me corresponde? |
| **Terminado cuando…** | Criterio objetivo para dar mi parte por cerrada. |

**Regla de oro:** la Fase 0 ya está aprobada; **sus definiciones (escenarios, kernel, workload, banderas, matriz, protocolo) no se cambian** sin acuerdo de los 4 y sin dejarlo documentado.

---

## 2. Visión general del proyecto

### 2.1 Qué se hace

Se calcula una suma de verificación (checksum) de 64 bits sobre un rango grande de enteros. Esa misma carga se reparte entre distintos números de **hilos** y **procesos**, en **Linux y Windows**, y se mide el tiempo y el uso de CPU. Los resultados se explican con el hardware (sockets, núcleos, procesadores lógicos) y con el scheduler del sistema operativo.

### 2.2 Resumen de roles

| Persona | Rol | En una frase | Alcance máximo |
|---|---|---|---|
| **P1** | Desarrollo en C++ | Construye el programa que crea hilos y procesos | Código completo, comentado, funcionando en ambos SO |
| **P2** | Python (obligatorio) | Replica el mismo kernel en Python y explica el GIL | Scripts equivalentes + nota del GIL |
| **P3** | Diseño experimental y mediciones | Ejecuta todos los experimentos y recolecta evidencia | CSV, hardware, capturas, procedimientos, limitaciones |
| **P4** | Planteamiento, informe e interpretación | Convierte datos en un informe interpretado | Informe final de 10 secciones con gráficas y respuestas |

### 2.3 Flujo de trabajo

```text
Fase 0 (todos)
   ├─→ P1 (C++) ─┐
   ├─→ P2 (Py) ──┼─→ P3 (pruebas) ─→ P4 (informe final)
   ├─→ P3 empieza ya: hardware + herramientas de scheduler
   └─→ P4 empieza ya: introducción, marco conceptual, plantillas
```

---

## 3. Fase 0 — Definición del problema, la solución y el diseño experimental

> Estado: ✅ **APROBADA** por P1, P2, P3 y P4.
> Los valores marcados con 🔧 son los que se **calibran en cada equipo real** (workload, tabla de equipos); una vez fijados no se cambian.
> Esta fase equivale a `docs/00_definicion.md`, que ya está completo.

### 3.1 Quién redacta y quién valida

| Decisión | Redacta | Valida |
|---|---|---|
| Planteamiento del problema | P4 | Todos |
| Procesos requeridos e hilos por proceso | P1 | P2, P3 |
| Diseño experimental (matriz, repeticiones, métricas, afinidad) | P3 | P1, P2 |
| Kernel de carga, workload y banderas de compilación | P1 | P2 |

**Entra:** guía del docente (PDF), código y resultados heredados.
**Sale:** `docs/00_definicion.md` aprobado por los 4.

### 3.2 Planteamiento del problema (redacta: P4)

**Contexto.** Cuando un programa crea varios hilos o procesos, el sistema operativo debe decidir cuáles se ejecutan sobre los procesadores lógicos (LP) disponibles. El efecto sobre el tiempo y el uso de CPU depende del hardware, del scheduler y del tipo de carga.

**Problema.** Calcular una suma de verificación (checksum) de 64 bits sobre un rango grande de enteros `[0, N)`. Es una carga **CPU-bound sintética**: no usa disco ni red, así que el tiempo refleja la CPU y la planificación, no la entrada/salida.

**Pregunta central.**
> ¿Cómo cambian el tiempo de ejecución y la utilización de CPU al repartir esa misma carga entre 1, 2, 4, 8 y 16 hilos, o entre varios procesos, en Windows y Linux, y cómo se explica con el scheduler y los procesadores lógicos?

**Hipótesis (se contrastan en el informe).**

* **H1.** Con varios LP disponibles, el tiempo baja al aumentar hilos, pero **no de forma lineal**.
* **H2.** Con afinidad a **1 LP**, más hilos **no reducen el tiempo** (hay concurrencia, no paralelismo).
* **H3.** Con más hilos que LP, el tiempo **deja de mejorar** y puede empeorar levemente.
* **H4.** Hilos y procesos obtienen tiempos similares en carga CPU-bound; la diferencia está en el costo de creación y la memoria.
* **H5.** El scheduler **puede migrar** un hilo entre LP durante su ejecución.

### 3.3 Solución: procesos e hilos por proceso (redacta: P1)

El rango `[0, N)` se reparte entre todos los hilos. Cada hilo calcula una suma parcial y el resultado final es la suma de los parciales. Como la suma es conmutativa, **el resultado debe ser idéntico en todas las configuraciones**; esa es la prueba de que el reparto es correcto.

| Escenario | Procesos | Hilos por proceso | Total de hilos | Propósito |
|---|---|---|---|---|
| **E0** Secuencial | 1 | 1 | 1 | Línea base |
| **E1** Multihilo | 1 | 1, 2, 4, 8, 16 | 1–16 | Escalamiento y paralelismo |
| **E1-C** Multihilo con 1 LP | 1 | 1, 2, 4, 8 | 1–8 | Concurrencia (afinidad a 1 LP) |
| **E2** Multiproceso | 1, 2, 4, 8 | 1 | 1–8 | Hilos vs procesos (mismo N) |
| **E3** Mixto | 2 | **A: 5 hilos, B: 3 hilos** | 8 | Hilos distintos por proceso |

**Roles dentro del programa.**

* **Proceso padre (coordinador):** lee parámetros, reparte rangos, crea hijos (si aplica), espera, combina resultados y mide.
* **Proceso hijo (trabajador):** recibe un sub-rango, crea sus hilos, calcula y entrega su parcial.
* **Hilo:** calcula la suma de su sub-rango y registra en qué LP corre.

**APIs.**

| Acción | Linux | Windows |
|---|---|---|
| Crear hilo | `pthread_create` | `CreateThread` |
| Esperar hilo | `pthread_join` | `WaitForSingleObject` / `WaitForMultipleObjects` |
| Crear proceso | `fork` | `CreateProcess` (relanza el `.exe` con `--child`) |
| Esperar proceso | `waitpid` | `WaitForSingleObject` |
| LP actual | `sched_getcpu` | `GetCurrentProcessorNumber` |

🔧 Si un equipo tiene menos de 16 LP, el nivel 16 se mantiene igual: sirve precisamente para estudiar el caso **hilos > LP**.

### 3.4 Carga de trabajo (redacta: P1; valida: P2)

**Kernel (idéntico en C++ y Python):**

```text
f(i) = ((i mod 97) × (i mod 53)) XOR (i << 3)
resultado = Σ f(i)  (módulo 2^64)
```

| Parámetro | Valor |
|---|---|
| Workload C++ 🔧 | Calibrar para que E0 dure **5–10 s** (inicio: 2 000 000 000) |
| Workload Python 🔧 | ~10–20 M (el kernel es ~100× más lento) |
| Compilación | `g++ -std=c++17 -O0 -pthread` (Linux) / `g++ -std=c++17 -O0` (Windows) — **mismas banderas** |
| Evitar optimización | Acumulador `volatile` + `-O0` |

**Calibración (se hace una vez, en cada equipo).**

1. Correr E0 con 100 M y medir el tiempo.
2. Escalar linealmente hasta ~5–10 s.
3. Confirmar que al **duplicar el workload se duplica el tiempo**; si no, la carga está siendo optimizada.
4. Fijar el valor y **no cambiarlo** durante todas las pruebas.

### 3.5 Diseño experimental (redacta: P3)

**Variables.**

| Tipo | Variables |
|---|---|
| Independientes | Modo, n.º de hilos, n.º de procesos, afinidad (1 LP / todos), SO |
| Dependientes | Tiempo (s), CPU total (%), CPU/LP observado, migraciones, resultado numérico |
| Controladas | Workload, banderas de compilación, plan de energía, aplicaciones abiertas, equipo conectado a corriente |

**Matriz de corridas (por sistema operativo, 5 repeticiones cada una).**

| Bloque | Configuraciones | Corridas |
|---|---|---|
| E0 | 1 | 5 |
| E1 | 1, 2, 4, 8, 16 hilos (todos los LP) | 25 |
| E1-C | 1, 2, 4, 8 hilos (afinidad 1 LP) | 20 |
| E2 | 1, 2, 4, 8 procesos | 20 |
| E3 | 5+3 hilos | 5 |
| **Total** | | **75 por SO** (~10–15 min cada SO con workload calibrado) |
| Python (**obligatorio**) | `threading` y `multiprocessing`: 1, 2, 4, 8 | 40 por SO |

**Protocolo (obligatorio).**

1. Cerrar aplicaciones innecesarias; equipo conectado y con plan de alto rendimiento.
2. Una corrida de calentamiento que se **descarta** antes de cada bloque.
3. Mantener el mismo orden de bloques en ambos SO.
4. Registrar la hora de inicio y la temperatura/estado del equipo si es relevante.
5. Detener si el equipo muestra comportamiento anormal.

**Criterios de validez.**

* El **resultado numérico es igual** en todas las configuraciones del mismo workload.
* Desviación entre repeticiones **< 10 %**; si es mayor, repetir esa configuración.
* Si E0 dura menos de 3 s, subir el workload.
* Mínimo aceptable: 3 repeticiones por configuración; la meta es 5.

### 3.6 Métricas y formato de datos (redacta: P3)

**CSV v2** (una línea por corrida; lo imprime el programa con `--csv`):

```csv
language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result
C++,linux,threads,1,8,8,all,2000000000,1,5.231,780,15594732105878822272
C++,linux,threads,1,8,8,1,2000000000,1,40.882,100,15594732105878822272
```

**Cómo se mide cada cosa.**

| Dato | Fuente |
|---|---|
| Tiempo | El programa (`steady_clock`) |
| CPU total (%) | El programa (`getrusage` / `GetProcessTimes`), contrastado con la herramienta del SO |
| LP y migraciones por hilo | El programa con `--log-lp` |
| Afinidad | `taskset -c 0` (Linux), `start /affinity 1` (Windows) |
| Scheduler | `top`, `htop`, `ps -eLo pid,tid,psr,comm`, `pidstat -t -p PID 1` / Administrador de tareas, Monitor de recursos |
| Hardware | `lscpu`, `lscpu -e`, `nproc` / `Get-CimInstance Win32_Processor` y `Win32_ComputerSystem` |

**Derivadas (las calcula P4).** Promedio, desviación estándar, rango, speedup `S = T1/TN`, eficiencia `E = S/N`.

### 3.7 Caracterización de los equipos (se llena en la reunión)

| Dato | Equipo Windows | Equipo Linux |
|---|---|---|
| Sistema / versión | 🔧 | 🔧 |
| ¿Real, VM o WSL? | 🔧 | 🔧 |
| Modelo de CPU | 🔧 | 🔧 |
| Sockets | 🔧 | 🔧 |
| Núcleos por socket | 🔧 | 🔧 |
| Hilos por núcleo (SMT) | 🔧 | 🔧 |
| Procesadores lógicos | 🔧 | 🔧 |
| Nodos NUMA | 🔧 | 🔧 |
| Compilador y versión | 🔧 | 🔧 |

> Si el equipo Linux es una VM con pocos LP (el heredado tenía 2 vCPU), la prueba de paralelismo queda limitada. Documentarlo o usar WSL/equipo real.

### 3.8 Estructura del repositorio

```text
docs/00_definicion.md
cpp/            (main.cpp, README.md)
python/         (código y README.md)
pruebas/{linux,windows}/   (scripts, procedimiento.md, resultados/)
informe/
```

### 3.9 Riesgos previstos

| Riesgo | Mitigación |
|---|---|
| El compilador optimiza la carga | `-O0` + `volatile`; verificar que duplicar el workload duplica el tiempo |
| VM con pocos LP | Documentar la limitación; usar WSL o equipo real |
| `fork` no existe en Windows | `CreateProcess` relanzando el `.exe` |
| Ruido de otros procesos | Protocolo de la sección 3.5; repeticiones y desviación |
| Python demasiado lento | Workload reducido y documentado |
| Compilador antiguo en Windows | Documentar versión; preferir MSYS2/GCC reciente |

### 3.10 Checklist de aprobación de la Fase 0

* [x] **P4:** planteamiento del problema e hipótesis (3.2).
* [x] **P1:** escenarios, APIs, kernel y workload (3.3 y 3.4).
* [x] **P2:** equivalencia con Python confirmada (kernel, partición y workload).
* [x] **P3:** matriz, protocolo, métricas y tabla de equipos (3.5 a 3.7).
* [x] P1 y P2 acordaron kernel y partición del rango idénticos.
* [x] Se fijaron workloads y banderas de compilación.
* [x] Los valores 🔧 fueron completados (o se calibran en cada equipo según 3.4).
* [x] Los cuatro integrantes aprueban:

| Integrante | Aprobado |
|---|---|
| P1 | ✅ |
| P2 | ✅ |
| P3 | ✅ |
| P4 | ✅ |

**Responsabilidades al cierre de la Fase 0:**

| Persona | Entrega al final de la Fase 0 |
|---|---|
| **P1** | Secciones 3.3 y 3.4 aprobadas |
| **P2** | Confirmación de kernel, partición y workload de Python |
| **P3** | Secciones 3.5, 3.6 y 3.7 aprobadas |
| **P4** | Sección 3.2 aprobada; consolida el documento en `docs/00_definicion.md` |

---

## 4. Fichas de trabajo por persona

---

### 4.1 PERSONA 1 — Desarrollo en C++ (hilos y procesos)

#### Objetivo
Construir el programa en C++ que ejecuta el cálculo en modo secuencial, multihilo, multiproceso y mixto, en Linux y Windows, con resultados numéricos idénticos y salida lista para medir. Es la pieza de la que dependen P2, P3 y P4.

#### Entra
* `docs/00_definicion.md` aprobado (escenarios, kernel, workload, banderas).
* Código base `cpp/main.cpp` (ya en C++).
* Guía del docente, secciones 8, 9 y 12.

#### Tareas clave

**Etapa A — Base**
* [ ] Confirmar que `main.cpp` compila con `g++ -std=c++17 -O0 -pthread` y que la salida indica `Language: C++`.
* [ ] Corregir advertencias (`_POSIX_C_SOURCE`, formato `PRIu64`).
* [ ] Versión secuencial (E0).

**Etapa B — Modos de ejecución**
* [ ] Multihilo POSIX (`pthread_create` / `pthread_join`).
* [ ] Multihilo Windows (`CreateThread` / `WaitForMultipleObjects`).
* [ ] Multiproceso Linux (`fork` / `waitpid`).
* [ ] Multiproceso Windows (`CreateProcess`, relanzando el `.exe` con `--child`).
* [ ] Escenario mixto (proceso A con 5 hilos, proceso B con 3 hilos).
* [ ] Hilos, procesos y workload configurables por línea de comandos.

**Etapa C — Instrumentación para P3**
* [ ] Cada hilo imprime su procesador lógico al inicio y al fin (`sched_getcpu` / `GetCurrentProcessorNumber`), activable con `--log-lp`.
* [ ] Salida CSV v2 con `--csv` (formato en 3.6).
* [ ] Medición de tiempo con `steady_clock` y de CPU con `getrusage` / `GetProcessTimes`.

**Etapa D — Calidad y validación**
* [ ] Calibrar el workload (secuencial ≈ 5–10 s) y comprobar que el tiempo crece con él (duplicar workload ⇒ duplicar tiempo).
* [ ] El resultado numérico es idéntico en todos los modos.
* [ ] Código comentado (rúbrica: 20 %).
* [ ] Compilar y probar en Linux y Windows con las mismas banderas.

**Etapa E — Documentación**
* [ ] `cpp/README.md` con compilación, ejecución y ejemplos de cada modo.
* [ ] Redactar la respuesta a "¿qué diferencia observó entre hilos y procesos?".

#### Entregables

| Entregable | Contenido | Va a |
|---|---|---|
| `cpp/main.cpp` | Completo, comentado, sin advertencias | P2, P3 |
| `cpp/README.md` | Compilación, ejecución, ejemplos de todos los modos | P3 |
| Binarios compilables | Linux y Windows, mismas banderas | P3 |
| Valor de referencia | Resultado numérico esperado por workload | P2, P3 |
| Nota técnica hilos vs procesos | Diferencias de implementación, creación, memoria | P4 |
| Explicación de la implementación | Cómo se reparte el rango y cómo se crean hilos/procesos | P4 |

#### Hasta dónde llegar
* **Sí incluye:** todos los modos de la matriz (E0–E3), CSV v2, log de LP, README, corrección de errores que reporte P3.
* **No incluye:** ejecutar la batería de mediciones, capturas del scheduler ni redactar el informe. Tampoco optimizar el kernel (debe quedar en `-O0` con acumulador `volatile`).
* **Atención a errores:** responder los reportes de P3 con prioridad; son lo único que puede bloquear las mediciones.

#### Terminado cuando…
P3 compila y ejecuta **todos los modos solo con el README**, sin preguntarte nada.

---

### 4.2 PERSONA 2 — Python (obligatorio)

#### Objetivo
Reproducir en Python el mismo experimento (mismo kernel y misma partición) para comparar con C++ y demostrar con evidencia el efecto del GIL en `threading` frente a `multiprocessing`.

#### Entra
* `docs/00_definicion.md` aprobado.
* Kernel y partición de P1.
* Código heredado `python/`.

#### Tareas clave

**Etapa A — Equivalencia**
* [ ] Mismo kernel y misma partición del rango que C++.
* [ ] Mismo resultado numérico que C++ para el mismo rango.
* [ ] Workload reducido (~10–20 M) para que los 5 niveles de workers sean viables.

**Etapa B — Modos**
* [ ] Modos `sequential`, `threading`, `multiprocessing`.
* [ ] Workers configurables (1, 2, 4, 8, 16).

**Etapa C — Salida y portabilidad**
* [ ] Salida CSV formato v2 (con `language=Python`).
* [ ] Funciona en Linux y Windows.

**Etapa D — Documentación y análisis**
* [ ] `python/README.md` con dependencias y ejemplos.
* [ ] Nota técnica sobre el GIL **con evidencia de las mediciones** (no solo teoría).
* [ ] Apoyar a P1 validando la equivalencia y la Fase 0.

#### Entregables

| Entregable | Contenido | Va a |
|---|---|---|
| `python/` actualizado | Scripts de los 3 modos | P3 |
| `python/README.md` | Dependencias, ejecución, ejemplos | P3 |
| Nota del GIL | `threading` vs `multiprocessing`, apoyada en datos | P4 |
| Comparación C++ vs Python | Diferencias de tiempo y escalamiento | P4 |

#### Hasta dónde llegar
* **Sí incluye:** equivalencia exacta con C++, CSV v2, README, nota del GIL, validar la Fase 0 y el kernel de P1.
* **No incluye:** medir (lo hace P3), pruebas de afinidad, capturas del scheduler ni el informe. Python es **obligatorio**: forma parte de la matriz (40 corridas por SO) y de la comparación C++ vs Python del informe.
* **Prioridad si hay poco tiempo:** primero equivalencia y CSV; la nota del GIL se completa con los datos de P3.

#### Terminado cuando…
P3 ejecuta **todos los modos de Python sin información adicional**.

---

### 4.3 PERSONA 3 — Diseño experimental, pruebas y mediciones

#### Objetivo
Ejecutar de forma reproducible toda la batería experimental en Linux y Windows y entregar a P4 un paquete de datos y evidencias completo, para que el informe se arme sin volver a medir.

#### Entra
* `docs/00_definicion.md` aprobado (matriz y métricas).
* Ejecutables y README de P1; scripts de P2.
* Guía del docente, secciones 7, 10, 11, 12 y 13.

#### Tareas clave

**Etapa A — Mientras P1 y P2 desarrollan (puede empezar ya)**
* [ ] **Exp. 1:** tabla de hardware en Windows y Linux (socket, cores por socket, threads por core, CPUs, modelo, NUMA).
  * Windows: `Win32_Processor` y `Win32_ComputerSystem`.
  * Linux: `lscpu`, `lscpu -e`, `nproc`.
* [ ] Interpretación de 4–6 líneas por equipo.
* [ ] Documentar si Linux es VM, WSL o equipo real y cuántos LP tiene.
* [ ] Preparar y probar las herramientas del scheduler (`top`, `htop`, `ps -eLf`, `pidstat`, Administrador de tareas, Monitor de recursos).
* [ ] Escribir los scripts de ejecución y el esqueleto de `procedimiento.md`.

**Etapa B — Mediciones principales (cuando P1 entregue)**
* [ ] Calibrar el workload en cada equipo y fijarlo (ver 3.4).
* [ ] **Exp. 4:** ejecutar 1, 2, 4, 8 hilos (y 16, que se mantiene siempre para estudiar hilos > LP).
* [ ] **Prueba de concurrencia:** afinidad a 1 LP (`taskset -c 0` / `start /affinity 1`).
* [ ] **Prueba de paralelismo:** todos los LP.
* [ ] Probar hilos > LP y documentar qué ocurre.
* [ ] **Exp. 6:** hilos vs procesos con el mismo N.
* [ ] Ejecutar el escenario mixto (E3).
* [ ] **Exp. 7:** 5 repeticiones por configuración (mínimo 3); promedio y desviación o rango. Repetir configuraciones con desviación > 10 %.
* [ ] Registrar CPU total y CPU/LP observado en cada corrida.
* [ ] Python (obligatorio): 40 corridas por SO (`threading` y `multiprocessing`: 1, 2, 4, 8).

**Etapa C — Evidencia del scheduler**
* [ ] **Exp. 5:** capturas del scheduler.
  * Linux: `top`, `htop`, `ps -eLf`, `pidstat -t -p PID 1`, `lscpu -e`.
  * Windows: Administrador de tareas (Rendimiento y Detalles), Monitor de recursos.
* [ ] Evidencia de migración de hilos (`ps -eLo pid,tid,psr,comm`, `htop` o el log de P1).

**Etapa D — Validación y cierre**
* [ ] Verificar que el resultado numérico coincide en todas las configuraciones.
* [ ] Completar `procedimiento.md` de Linux y Windows (sin campos entre corchetes).
* [ ] Documentar limitaciones (VM, compilador antiguo en Windows, carga del equipo).
* [ ] Aportar datos para las preguntas de análisis 1–10 (sección 6).
* [ ] Reportar errores a P1 y P2 con la configuración exacta que los provoca.

#### Entregables

| Entregable | Contenido | Va a |
|---|---|---|
| CSV v2 | Todas las mediciones individuales (no solo promedios) | P4 |
| Tablas de hardware | Windows y Linux + interpretación de 4–6 líneas | P4 |
| Carpeta de capturas | Scheduler, uso de CPU por LP, migración de hilos | P4 |
| `procedimiento.md` (Linux y Windows) | Pasos exactos, completos y reproducibles | P4 |
| Scripts reproducibles | Ejecución de toda la matriz | P4 |
| Lista de limitaciones | VM, compilador, carga del equipo, etc. | P4 |
| Reportes de errores | Configuración + comportamiento observado | P1, P2 |

#### Hasta dónde llegar
* **Sí incluye:** diseño de la matriz (Fase 0), ejecución de todas las corridas, recolección de evidencia, validación numérica, documentación de procedimientos y limitaciones.
* **No incluye:** gráficas finales, interpretación ni redacción del informe (P4); modificar el código de P1/P2 (se reportan los errores).
* **Datos en crudo:** entrega **todas** las corridas individuales, incluso las repetidas; P4 calcula las derivadas.

#### Terminado cuando…
P4 arma **todas las tablas y gráficas sin volver a ejecutar nada ni pedir datos**.

---

### 4.4 PERSONA 4 — Planteamiento, informe e interpretación

#### Objetivo
Coordinar la Fase 0 y producir el informe final (10 secciones) donde cada evidencia esté interpretada con "¿qué se observa? ¿por qué ocurre? ¿qué demuestra?". La interpretación pesa **25 %** de la nota.

#### Entra
* Guía del docente (estructura, rúbrica y preguntas).
* Definición de la Fase 0 (la redacta ella/él).
* Paquete de datos de P3; explicaciones de P1 y P2.

#### Tareas clave

**Etapa A — Mientras otros desarrollan (puede empezar ya)**
* [ ] Redactar el planteamiento del problema e hipótesis y coordinar la Fase 0 (consolidar `docs/00_definicion.md`).
* [ ] Avanzar introducción y marco conceptual: proceso, hilo, CPU, socket, núcleo, LP, scheduler, concurrencia, paralelismo.
* [ ] Preparar plantillas de tablas y gráficas.

**Etapa B — Cuando lleguen insumos de P1 y P3**
* [ ] Caracterización del equipo (desde P3).
* [ ] Implementación: explicar hilos y procesos en C++ (y Python).
* [ ] Experimentos: procedimiento, configuraciones y herramientas.

**Etapa C — Resultados y análisis (con el paquete completo de P3)**
* [ ] Resultados: tablas R1–R3, promedio y desviación.
* [ ] Gráfica de tiempo vs número de hilos.
* [ ] Gráfica de CPU vs número de hilos.
* [ ] Tabla comparativa Windows/Linux (sección 16 de la guía).
* [ ] Análisis: cada evidencia con ¿qué se observa? ¿por qué ocurre? ¿qué demuestra?
* [ ] Responder las 10 preguntas de análisis (sección 6) y contrastar H1–H5.
* [ ] Complementos: C++ vs Python, speedup, eficiencia, Amdahl.

**Etapa D — Cierre**
* [ ] Conclusiones: qué ocurrió, por qué y relación con hardware y scheduler.
* [ ] Referencias.
* [ ] Verificación cruzada: **cada cifra del informe coincide con los CSV**.

#### Entregables

| Entregable | Contenido | Va a |
|---|---|---|
| `docs/00_definicion.md` | Fase 0 consolidada y aprobada | Todos |
| Informe final | 10 secciones con anexos | Docente |
| Gráficas y tablas | Tiempo vs hilos, CPU vs hilos, R1–R3, Windows/Linux | Informe |
| Respuestas a las 10 preguntas | Con evidencia de P3/P1 | Informe |
| Referencias | Bibliografía y documentación citada | Informe |

#### Hasta dónde llegar
* **Sí incluye:** redacción, gráficas, cálculo de derivadas (promedio, desviación, speedup, eficiencia, Amdahl), interpretación, verificación de cifras.
* **No incluye:** ejecutar mediciones ni modificar código. Si falta un dato, se lo pide a P3 **una sola vez, con lista completa**, y no se inventan cifras.
* **Criterio de calidad:** ninguna gráfica o tabla sin interpretación al lado.

#### Terminado cuando…
**Todas las cifras tienen respaldo en el repositorio y cada evidencia está interpretada.**

---

## 5. Matriz de entradas y salidas

| De → A | Qué se entrega |
|---|---|
| Todos → Fase 0 | Aprobación de `docs/00_definicion.md` |
| P1 → P2 | Kernel, partición y resultado de referencia |
| P1 → P3 | Ejecutables, README, log de LP por hilo |
| P1 → P4 | Explicación de la implementación y nota hilos vs procesos |
| P2 → P3 | Scripts de Python y README |
| P2 → P4 | Nota del GIL y comparación con C++ |
| P3 → P1, P2 | Errores y ajustes |
| P3 → P4 | CSV v2, hardware, capturas, procedimiento, limitaciones |
| P4 → Docente | Informe final |

---

## 6. Preguntas de análisis: quién aporta qué

| # | Pregunta | Datos | Redacta |
|---|---|---|---|
| 1 | CPUs físicas, sockets, núcleos y LP | P3 | P4 |
| 2 | ¿LP coincide con núcleos? | P3 | P4 |
| 3 | Evidencia de concurrencia | P3 (1 LP) | P4 |
| 4 | Evidencia de paralelismo | P3 (CPU total, LP por hilo) | P4 |
| 5 | Hilos > LP | P3 | P4 |
| 6 | ¿Baja el tiempo linealmente? | P3 | P4 |
| 7 | ¿Migran los hilos? ¿Cómo se observa? | P1 + P3 | P4 |
| 8 | Hilos vs procesos | P1 | P4 |
| 9 | Limitaciones | P3 | P4 |
| 10 | Windows vs Linux | P3 | P4 |

---

## 7. Estructura del informe y rúbrica

| Sección | Autor principal |
|---|---|
| 1. Introducción | P4 |
| 2. Marco conceptual | P4 |
| 3. Caracterización del equipo | P3 → P4 |
| 4. Implementación | P1 (+P2) → P4 |
| 5. Experimentos | P3 → P4 |
| 6. Resultados | P3 → P4 |
| 7. Análisis | P4 |
| 8. Comparación Windows/Linux | P3 + P4 |
| 9. Conclusiones | P4 |
| 10. Referencias | P4 |

| Criterio | Peso | Dónde se gana |
|---|---|---|
| Fundamentos | 15 % | Marco conceptual (P4) |
| Implementación | 20 % | Código funcional y comentado (P1, P2) |
| Diseño experimental | 15 % | Fase 0 y matriz (P3) |
| Medición | 15 % | Tablas, repeticiones, gráficas (P3, P4) |
| **Interpretación** | **25 %** | Qué/por qué/qué demuestra (P4 con datos de P3) |
| Presentación | 10 % | Informe ordenado (P4) |

---

## 8. Cronograma e hitos

> Las fechas se fijan en la reunión de la Fase 0. Los hitos son por dependencia, no por calendario.

| Hito | Cuándo | Responsable | Se cumple cuando… |
|---|---|---|---|
| **H0** Fase 0 aprobada | ✅ **Cumplido** | Todos | Los 4 aprobaron la sección 3.10 |
| **H1** Código C++ listo | Tras H0 | P1 | P3 compila y ejecuta todos los modos con el README |
| **H2** Python listo | Tras H0, en paralelo a H1 | P2 | P3 ejecuta todos los modos de Python |
| **H3** Hardware y herramientas listos | Tras H0, en paralelo | P3 | Tablas de hardware y herramientas del scheduler probadas |
| **H4** Mediciones completas | Tras H1 y H2 | P3 | Matriz completa en ambos SO, CSV v2 y capturas |
| **H5** Borrador de informe sin datos | Tras H0, en paralelo | P4 | Secciones 1 y 2 y plantillas listas |
| **H6** Informe final | Tras H4 | P4 | Verificación cruzada de cifras superada |

**Quién espera a quién**

| Persona | Puede empezar | Se bloquea hasta… |
|---|---|---|
| P1 | Tras H0 | — |
| P2 | Tras H0 (necesita kernel de P1) | Kernel y partición de P1 |
| P3 | Tras H0 (hardware y herramientas) | Ejecutables de P1 para medir |
| P4 | Tras H0 (introducción y marco) | Datos de P3 para resultados y análisis |

---

## 9. Convenciones del repositorio

```text
docs/00_definicion.md
cpp/                        main.cpp, README.md
python/                     código y README.md
pruebas/linux/              scripts, procedimiento.md, resultados/
pruebas/windows/            scripts, procedimiento.md, resultados/
informe/
```

* Un solo formato de datos: **CSV v2** (ver 3.6).
* Mismas banderas de compilación en ambos SO.
* Resultados crudos nunca se editan a mano; si hay un error, se repite la corrida.
* Cada cifra del informe debe poder rastrearse a una línea de un CSV.

---

## 10. Lista de verificación final

**Antes de entregar al docente**

* [x] Fase 0 aprobada y archivada en `docs/00_definicion.md`.
* [ ] `main.cpp` compila sin advertencias en Linux y Windows y dice `Language: C++`.
* [ ] El resultado numérico es idéntico en todas las configuraciones (C++ y Python).
* [ ] Matriz completa: 75 corridas por SO en C++ y 40 de Python por SO.
* [ ] Desviación entre repeticiones < 10 % (o justificada).
* [ ] Tablas de hardware con interpretación de ambos equipos.
* [ ] Capturas del scheduler y evidencia de migración.
* [ ] `procedimiento.md` sin campos entre corchetes.
* [ ] Limitaciones documentadas.
* [ ] Las 10 preguntas de análisis respondidas.
* [ ] H1–H5 contrastadas (cumplida / no cumplida y por qué).
* [ ] Cada evidencia con: ¿qué se observa? ¿por qué ocurre? ¿qué demuestra?
* [ ] Cada cifra del informe coincide con los CSV.
* [ ] Informe con las 10 secciones, referencias y anexos.