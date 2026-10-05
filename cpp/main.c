/**
 * @file main.c
 * @brief Implementación en lenguaje C para el benchmark CPU-bound con soporte
 *        secuencial y paralelo multihilo (Nativo en Windows y Linux).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    typedef HANDLE thread_handle_t;
#else
    #define _POSIX_C_SOURCE 199309L
    #include <pthread.h>
    #include <time.h>
    #include <unistd.h>
    typedef pthread_t thread_handle_t;
#endif

/* Constantes para el algoritmo CPU-bound de 64 bits */
#define MULT_A 0x9E3779B97F4A7C15ULL
#define MULT_B 0xBF58476D1CE4E5B9ULL

#define DEFAULT_WORKLOAD 20000000ULL
#define CSV_HEADER "language,mode,workers,workload,time_seconds,result"

/**
 * @brief Estructura de datos asignada a cada hilo de trabajo.
 */
typedef struct {
    uint64_t start;          /* Límite inferior del rango (inclusivo) */
    uint64_t stop;           /* Límite superior del rango (exclusivo) */
    uint64_t partial_result; /* Resultado acumulado del cálculo del hilo */
} ThreadTask;

/**
 * @brief Obtiene la marca de tiempo actual en segundos con alta resolución monotónica.
 */
static double get_time_seconds(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static bool freq_initialized = false;
    if (!freq_initialized) {
        QueryPerformanceFrequency(&freq);
        freq_initialized = true;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
#endif
}

/**
 * @brief Obtiene la cantidad aproximada de núcleos lógicos del sistema.
 */
static unsigned int get_hardware_concurrency(void) {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (unsigned int)sysinfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    return (nprocs > 0) ? (unsigned int)nprocs : 1;
#endif
}

/**
 * @brief Función de cómputo intensivo CPU-bound puro.
 */
static uint64_t kernel(uint64_t start, uint64_t stop) {
    uint64_t acc = 0;
    for (uint64_t i = start; i < stop; ++i) {
        uint64_t term = ((i * MULT_A) ^ (i >> 7)) * MULT_B;
        acc += term;
    }
    return acc;
}

/**
 * @brief Función de entrada (worker) ejecutada por cada hilo.
 */
#ifdef _WIN32
static DWORD WINAPI thread_worker_entry(LPVOID lpParam) {
    ThreadTask* task = (ThreadTask*)lpParam;
    task->partial_result = kernel(task->start, task->stop);
    return 0;
}
#else
static void* thread_worker_entry(void* arg) {
    ThreadTask* task = (ThreadTask*)arg;
    task->partial_result = kernel(task->start, task->stop);
    return NULL;
}
#endif

/**
 * @brief Ejecución secuencial en el hilo principal.
 */
static uint64_t run_sequential(uint64_t workload) {
    return kernel(0, workload);
}

/**
 * @brief Ejecución paralela repartiendo el rango de trabajo entre `workers` hilos nativos.
 */
static uint64_t run_parallel(uint64_t workload, unsigned int workers) {
    if (workers <= 1) {
        return run_sequential(workload);
    }

    ThreadTask* tasks = (ThreadTask*)malloc(workers * sizeof(ThreadTask));
    if (!tasks) {
        fprintf(stderr, "Error: no se pudo asignar memoria para las tareas de los hilos.\n");
        exit(1);
    }

    thread_handle_t* handles = (thread_handle_t*)malloc(workers * sizeof(thread_handle_t));
    if (!handles) {
        fprintf(stderr, "Error: no se pudo asignar memoria para los identificadores de hilos.\n");
        free(tasks);
        exit(1);
    }

    /* Particionamiento equilibrado del rango total entre los workers */
    uint64_t base = workload / workers;
    uint64_t extra = workload % workers;
    uint64_t current_start = 0;

    for (unsigned int i = 0; i < workers; ++i) {
        uint64_t size = base + (i < extra ? 1 : 0);
        tasks[i].start = current_start;
        tasks[i].stop = current_start + size;
        tasks[i].partial_result = 0;
        current_start += size;
    }

    /* Creación y lanzamiento de hilos en el sistema operativo */
#ifdef _WIN32
    for (unsigned int i = 0; i < workers; ++i) {
        handles[i] = CreateThread(NULL, 0, thread_worker_entry, &tasks[i], 0, NULL);
        if (handles[i] == NULL) {
            fprintf(stderr, "Error: CreateThread falló para el hilo %u (código: %lu)\n", i, GetLastError());
            exit(1);
        }
    }

    /* Espera de terminación (join) de todos los hilos Win32 */
    WaitForMultipleObjects(workers, handles, TRUE, INFINITE);

    for (unsigned int i = 0; i < workers; ++i) {
        CloseHandle(handles[i]);
    }
#else
    for (unsigned int i = 0; i < workers; ++i) {
        int rc = pthread_create(&handles[i], NULL, thread_worker_entry, &tasks[i]);
        if (rc != 0) {
            fprintf(stderr, "Error: pthread_create falló para el hilo %u (código: %d)\n", i, rc);
            exit(1);
        }
    }

    /* Espera de terminación (join) de todos los hilos POSIX */
    for (unsigned int i = 0; i < workers; ++i) {
        pthread_join(handles[i], NULL);
    }
#endif

    /* Agregación de resultados parciales (suma modular de 64 bits) */
    uint64_t total = 0;
    for (unsigned int i = 0; i < workers; ++i) {
        total += tasks[i].partial_result;
    }

    free(handles);
    free(tasks);
    return total;
}

/* ========================================================================= */
/* Parseo de Argumentos de Línea de Comandos                                 */
/* ========================================================================= */

static void print_help(const char* prog) {
    printf("Uso: %s --mode {sequential|parallel|threading} [opciones]\n\n", prog);
    printf("Benchmark CPU-bound en lenguaje C puro con soporte multihilo nativo.\n\n");
    printf("Opciones:\n");
    printf("  --mode M         Modalidad obligatoria: 'sequential', 'parallel' o 'threading'\n");
    printf("                   ('threading' se maneja como alias de 'parallel').\n");
    printf("  --workload N     Iteraciones totales (default: %" PRIu64 "; admite '100_000_000').\n", DEFAULT_WORKLOAD);
    printf("  --workers N      Cantidad de hilos a crear (default: 1; se ignora en sequential).\n");
    printf("  --repeat N       Repeticiones consecutivas de la prueba (default: 1).\n");
    printf("  --csv            Imprime resultados en una línea CSV por corrida.\n");
    printf("  --csv-header     Imprime la cabecera CSV antes de los registros (implica --csv).\n");
    printf("  --verbose        Imprime detalles del compilador, SO y núcleos disponibles.\n");
    printf("  -h, --help       Muestra esta pantalla de ayuda y finaliza.\n");
}

static uint64_t parse_workload_string(const char* s) {
    char clean[64];
    size_t pos = 0;
    for (size_t i = 0; s[i] != '\0' && pos < sizeof(clean) - 1; ++i) {
        if (s[i] != '_') {
            clean[pos++] = s[i];
        }
    }
    clean[pos] = '\0';
    return (uint64_t)strtoull(clean, NULL, 10);
}

int main(int argc, char* argv[]) {
    char mode[32] = "";
    uint64_t workload = DEFAULT_WORKLOAD;
    unsigned int workers = 1;
    unsigned int repeat = 1;
    bool csv = false;
    bool csv_header = false;
    bool verbose = false;

    /* Parseo manual de argumentos de línea de comandos */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--mode") == 0) {
            if (i + 1 < argc) {
                strncpy(mode, argv[++i], sizeof(mode) - 1);
            } else {
                fprintf(stderr, "Error: --mode requiere un argumento ('sequential' o 'parallel').\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--workload") == 0) {
            if (i + 1 < argc) {
                workload = parse_workload_string(argv[++i]);
                if (workload < 1) {
                    fprintf(stderr, "Error: --workload debe ser >= 1.\n");
                    return 2;
                }
            } else {
                fprintf(stderr, "Error: --workload requiere un valor numérico.\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--workers") == 0) {
            if (i + 1 < argc) {
                workers = (unsigned int)atoi(argv[++i]);
                if (workers < 1) {
                    fprintf(stderr, "Error: --workers debe ser >= 1.\n");
                    return 2;
                }
            } else {
                fprintf(stderr, "Error: --workers requiere un valor numérico.\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--repeat") == 0) {
            if (i + 1 < argc) {
                repeat = (unsigned int)atoi(argv[++i]);
                if (repeat < 1) {
                    fprintf(stderr, "Error: --repeat debe ser >= 1.\n");
                    return 2;
                }
            } else {
                fprintf(stderr, "Error: --repeat requiere un valor numérico.\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--csv") == 0) {
            csv = true;
        } else if (strcmp(argv[i], "--csv-header") == 0) {
            csv_header = true;
            csv = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else {
            fprintf(stderr, "Error: argumento desconocido '%s'. Use --help para ver la lista.\n", argv[i]);
            return 2;
        }
    }

    /* Validación del modo */
    if (strlen(mode) == 0) {
        fprintf(stderr, "Error: el argumento --mode es obligatorio ('sequential', 'parallel' o 'threading').\n");
        return 2;
    }

    for (size_t i = 0; mode[i]; ++i) {
        mode[i] = (char)tolower((unsigned char)mode[i]);
    }

    if (strcmp(mode, "sequential") != 0 && strcmp(mode, "parallel") != 0 && strcmp(mode, "threading") != 0) {
        fprintf(stderr, "Error: modo '%s' no reconocido. Opciones válidas: sequential, parallel, threading.\n", mode);
        return 2;
    }

    /* Normalización de workers para modo sequential */
    unsigned int effective_workers = workers;
    if (strcmp(mode, "sequential") == 0) {
        if (workers != 1) {
            fprintf(stderr, "Aviso: --workers se ignora en modo sequential (se reporta Workers: 1).\n");
        }
        effective_workers = 1;
    }

    if (csv_header) {
        printf("%s\n", CSV_HEADER);
    }

    /* Bucle de repeticiones y medición */
    for (unsigned int run = 0; run < repeat; ++run) {
        double start_time = get_time_seconds();
        uint64_t result = 0;

        if (strcmp(mode, "sequential") == 0) {
            result = run_sequential(workload);
        } else {
            result = run_parallel(workload, effective_workers);
        }

        double elapsed = get_time_seconds() - start_time;

        if (csv) {
            printf("C,%s,%u,%" PRIu64 ",%.6f,%" PRIu64 "\n",
                   mode, effective_workers, workload, elapsed, result);
            fflush(stdout);
        } else {
            if (run > 0) {
                printf("\n");
            }
            printf("Language: C\n");
            printf("Mode: %s\n", mode);
            printf("Workers: %u\n", effective_workers);
            printf("Workload: %" PRIu64 "\n", workload);
            printf("Execution time: %.4f seconds\n", elapsed);
            printf("Result: %" PRIu64 "\n", result);
            fflush(stdout);
        }
    }

    if (verbose && !csv) {
        printf("\n--- Información del Entorno ---\n");
#if defined(__GNUC__)
        printf("Compilador: GCC %s\n", __VERSION__);
#elif defined(_MSC_VER)
        printf("Compilador: MSVC %d\n", _MSC_VER);
#elif defined(__clang__)
        printf("Compilador: Clang %s\n", __clang_version__);
#else
        printf("Compilador: Desconocido\n");
#endif
#ifdef _WIN32
        printf("Sistema Operativo: Windows (API nativa Win32 CreateThread)\n");
#else
        printf("Sistema Operativo: Linux/POSIX (Librería nativa POSIX pthreads)\n");
#endif
        printf("Núcleos de hardware detectados: %u\n", get_hardware_concurrency());
        printf("Modelo de concurrencia: Hilos nativos a nivel de kernel (sin GIL, paralelismo real)\n");
    }

    return 0;
}
