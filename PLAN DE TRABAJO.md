# Plan de Trabajo

## 1. Organización general

El proyecto se divide entre cuatro personas. Cada integrante tiene una responsabilidad independiente y debe entregar su trabajo completamente documentado para que la siguiente persona pueda continuar sin depender de explicaciones adicionales.


# 2. Persona 1 — Desarrollo C++

## Tareas

P1 es responsable de desarrollar completamente la implementación en C++.

Debe crear:

* Una versión secuencial.
* Una versión paralela/multihilo.
* Una carga de trabajo CPU-bound.
* Configuración del número de threads.
* Medición del tiempo de ejecución.
* Compatibilidad con Windows y Linux.
* Documentación para ejecutar el programa.

## Implementación

La aplicación debe permitir ejecutar una misma carga de trabajo bajo diferentes configuraciones.

La cantidad de trabajo y el número de threads deben poder configurarse sin modificar directamente el código fuente.

Ejemplo conceptual:

```text
Secuencial
    ↓
Carga CPU-bound

Paralelo
    ↓
Carga CPU-bound
    ↓
Thread 1
Thread 2
Thread 3
...
```

El programa debe mostrar información suficiente para identificar cada ejecución, por ejemplo:

```text
Mode: parallel
Workers: 8
Workload: 100000000
Execution time: 4.531 seconds
```

También debe existir documentación para compilar y ejecutar en ambos sistemas operativos.

## Checklist P1

* [ ] Existe la versión secuencial.
* [ ] Existe la versión paralela.
* [ ] La versión paralela utiliza threads.
* [ ] El número de threads es configurable.
* [ ] La carga de trabajo es CPU-bound.
* [ ] La carga de trabajo es configurable.
* [ ] Se mide el tiempo de ejecución.
* [ ] El programa muestra claramente los resultados.
* [ ] Compila correctamente en Linux.
* [ ] Compila correctamente en Windows.
* [ ] Se probó la ejecución en Linux.
* [ ] Se probó la ejecución en Windows.
* [ ] Existe README con instrucciones de compilación.
* [ ] Existe README con instrucciones de ejecución.
* [ ] Se incluyen ejemplos de ejecución.
* [ ] Todo el código necesario está dentro de `cpp/`.
* [ ] P3 puede ejecutar el programa sin modificar el código.
* [ ] Se entregó la implementación completa a P3.

### Criterio de terminado

> P1 termina cuando P3 puede recibir la carpeta `cpp/`, seguir el README y ejecutar todas las modalidades sin necesitar información adicional de P1.

---

# 3. Persona 2 — Desarrollo Python

## Tareas

P2 es responsable de desarrollar completamente la implementación en Python.

Debe crear:

* Una versión secuencial.
* Una versión paralela.
* Una carga de trabajo CPU-bound.
* Configuración del número de workers.
* Medición del tiempo.
* Compatibilidad con Windows y Linux.
* Documentación para ejecutar el programa.

Cuando forme parte del experimento, también se puede incluir la comparación entre:

* `threading`
* `multiprocessing`

Esto permitirá analizar el comportamiento de Python ante una carga CPU-bound.

## Implementación

La carga de trabajo debe ser equivalente a la utilizada en C++ para que posteriormente puedan compararse los resultados.

Ejemplo:

```text
Secuencial
    ↓
Carga CPU-bound

Paralelo
    ↓
Carga CPU-bound
    ↓
Worker 1
Worker 2
Worker 3
...
```

El programa debe mostrar información como:

```text
Mode: multiprocessing
Workers: 8
Workload: 100000000
Execution time: 6.721 seconds
```

## Checklist P2

* [ ] Existe la versión secuencial.
* [ ] Existe la versión paralela.
* [ ] La cantidad de workers es configurable.
* [ ] La carga de trabajo es CPU-bound.
* [ ] La carga de trabajo es configurable.
* [ ] La carga es equivalente a la utilizada en C++.
* [ ] Se mide el tiempo de ejecución.
* [ ] El programa muestra claramente los resultados.
* [ ] `threading` está implementado si forma parte del experimento.
* [ ] `multiprocessing` está implementado si forma parte del experimento.
* [ ] Funciona en Linux.
* [ ] Funciona en Windows.
* [ ] Todas las dependencias están documentadas.
* [ ] Existe README con instrucciones de ejecución.
* [ ] Se incluyen ejemplos de ejecución.
* [ ] Todo el código necesario está dentro de `python/`.
* [ ] P3 puede ejecutar el programa sin modificar el código.
* [ ] Se entregó la implementación completa a P3.

### Criterio de terminado

> P2 termina cuando P3 puede instalar las dependencias, seguir el README y ejecutar todas las modalidades de Python sin necesitar información adicional de P2.

---

# 4. Persona 3 — Pruebas y mediciones

## Tareas

P3 recibe las implementaciones terminadas de P1 y P2.

Su responsabilidad es ejecutar y medir ambos programas en:

* Windows.
* Linux.

Debe mantener controladas las variables del experimento y registrar los resultados.

## Implementación de las pruebas

Se debe definir previamente:

* Workload.
* Número de workers.
* Número de repeticiones.
* Cantidad de CPUs a utilizar.

Una configuración inicial puede ser:

```text
CPUs:
1
2
3
4
5
```

El número exacto puede ajustarse según las capacidades del equipo utilizado.

Para cada configuración se deben registrar las mediciones.

Ejemplo:

```text
language,mode,workers,cpus,workload,run,time_seconds
cpp,parallel,8,1,100000000,1,8.421
cpp,parallel,8,2,100000000,1,4.531
python,multiprocessing,8,1,100000000,1,12.381
```

Se deben conservar las mediciones individuales y no únicamente el promedio.

## Windows

Documentar:

* Hardware utilizado.
* Sistema operativo.
* CPU disponible.
* Cantidad de cores/threads.
* Herramientas utilizadas.
* Procedimiento utilizado para limitar/seleccionar CPUs.
* Comandos o pasos utilizados.

## Linux

Documentar:

* Hardware utilizado.
* Sistema operativo.
* CPU disponible.
* Cantidad de cores/threads.
* Herramientas utilizadas.
* Procedimiento utilizado para limitar/seleccionar CPUs.
* Comandos utilizados.

Por ejemplo, pueden utilizarse herramientas como:

```text
lscpu
nproc
taskset
time
ps
top
htop
```

## Archivos de resultados

P3 debe entregar algo similar a:

```text
pruebas/
├── windows/
│   ├── resultados/
│   │   ├── cpp_results.csv
│   │   └── python_results.csv
│   └── procedimiento.md
│
└── linux/
    ├── resultados/
    │   ├── cpp_results.csv
    │   └── python_results.csv
    └── procedimiento.md
```

## Checklist P3

* [ ] Se recibió correctamente el código de P1.
* [ ] Se recibió correctamente el código de P2.
* [ ] C++ funciona en Windows.
* [ ] C++ funciona en Linux.
* [ ] Python funciona en Windows.
* [ ] Python funciona en Linux.
* [ ] Se documentó el hardware.
* [ ] Se documentó Windows.
* [ ] Se documentó Linux.
* [ ] Se definió el workload.
* [ ] Se definió el número de workers.
* [ ] Se definió el número de repeticiones.
* [ ] Se definieron las configuraciones de CPU.
* [ ] Se ejecutó la versión secuencial.
* [ ] Se ejecutó la versión paralela.
* [ ] Se probaron las diferentes cantidades de CPUs.
* [ ] Las pruebas se realizaron en Windows.
* [ ] Las pruebas se realizaron en Linux.
* [ ] Se registraron las mediciones individuales.
* [ ] Los resultados están organizados en CSV.
* [ ] Se documentó el procedimiento de pruebas.
* [ ] Se documentaron los comandos utilizados.
* [ ] Las pruebas pueden ser reproducidas.
* [ ] Se entregaron todos los resultados a P4.

### Criterio de terminado

> P3 termina cuando P4 puede utilizar los resultados para crear tablas, gráficas y análisis sin tener que volver a ejecutar los programas ni solicitar información adicional.

---

# 5. Persona 4 — Informe final

## Tareas

P4 es responsable de integrar todo el trabajo y crear el informe final.

Debe utilizar:

* Implementación C++ de P1.
* Implementación Python de P2.
* Resultados de Windows de P3.
* Resultados de Linux de P3.
* Procedimientos y documentación de P3.

## Estructura del informe

```text
1. Introducción
2. Objetivos
3. Marco teórico
4. Herramientas y entorno
5. Metodología experimental
6. Implementación C++
7. Implementación Python
8. Pruebas realizadas
9. Resultados
10. Análisis
11. Comparación C++ vs Python
12. Comparación Windows vs Linux
13. Ley de Amdahl
14. Conclusiones
15. Referencias
16. Anexos
```

## Análisis

El informe debe analizar, a partir de los datos obtenidos:

* Tiempo de ejecución.
* Comportamiento al aumentar las CPUs.
* Diferencia entre ejecución secuencial y paralela.
* Speedup.
* Eficiencia.
* Diferencias entre C++ y Python.
* Diferencias entre Windows y Linux.
* Comportamiento de `threading` y `multiprocessing` cuando corresponda.
* Relación de los resultados con la Ley de Amdahl.

## Checklist P4

* [ ] Se recibió el código C++.
* [ ] Se recibió el código Python.
* [ ] Se recibieron los resultados de Windows.
* [ ] Se recibieron los resultados de Linux.
* [ ] Se recibió el procedimiento experimental.
* [ ] Se incluyó la introducción.
* [ ] Se incluyeron los objetivos.
* [ ] Se desarrolló el marco teórico.
* [ ] Se explicó la metodología.
* [ ] Se explicó la implementación C++.
* [ ] Se explicó la implementación Python.
* [ ] Se documentaron las pruebas.
* [ ] Se incluyeron los resultados.
* [ ] Se incluyeron tablas.
* [ ] Se incluyeron gráficas.
* [ ] Se calculó el speedup.
* [ ] Se calculó la eficiencia.
* [ ] Se analizaron las diferentes cantidades de CPUs.
* [ ] Se compararon C++ y Python.
* [ ] Se compararon Windows y Linux.
* [ ] Se relacionaron los resultados con la Ley de Amdahl.
* [ ] Se elaboraron las conclusiones.
* [ ] Se incluyeron las referencias.
* [ ] Se revisó que los datos del informe coincidan con los resultados de P3.
* [ ] El informe está completo y listo para entregar.

### Criterio de terminado

> P4 termina cuando el informe puede entregarse directamente al docente y todos los resultados presentados pueden ser respaldados por los archivos y mediciones almacenados en el repositorio.

---

# 6. Regla de entrega

Cada persona debe cumplir **todo su checklist antes de marcar su tarea como terminada**.

La dependencia del proyecto será:

```text
P1 ──────────┐
             ├──→ P3 ───→ P4
P2 ──────────┘
```

Por tanto:

**P1 y P2 → entregan implementaciones completas.**

**P3 → entrega pruebas y datos completos.**

**P4 → integra todo y genera el informe final.**