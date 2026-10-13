/**
 * main.cpp
 * Laboratorio de Procesos e Hilos.
 * 
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <process.h>
#else
    #define _GNU_SOURCE
    #include <pthread.h>
    #include <unistd.h>
    #include <sched.h>
    #include <sys/wait.h>
    #include <sys/resource.h>
    #include <sys/time.h>
#endif

// ============================================================================
// CONSTANTES Y ESTRUCTURAS DE DATOS
// ============================================================================

const char* CSV_HEADER = "language,os,mode,processes,threads_per_process,total_workers,lp_affinity,workload,run,time_seconds,cpu_total_pct,result";

/**
 * @brief Estructura de rango para división de carga entre trabajadores.
 */
struct RangeBlock {
    uint64_t start;
    uint64_t stop;
};

/**
 * @brief Argumentos pasados a cada hilo de trabajo nativo.
 */
struct ThreadTask {
    uint64_t start;
    uint64_t stop;
    uint64_t partial_result;
    unsigned int worker_id;
    bool log_lp;
};

// PASO 1. KERNEL ARITMÉTICO Y PARTICIONAMIENTO DEL RANGO

inline uint64_t compute_kernel(uint64_t start, uint64_t stop) {
    volatile uint64_t acc = 0;
    for (uint64_t i = start; i < stop; ++i) {
        uint64_t term = ((i % 97ULL) * (i % 53ULL)) ^ (i << 3ULL);
        acc = acc + term;
    }
    return acc;
}

/**
 Divide el rango [0, n) equitativamente en `workers` bloques contiguos.
         El remanente (si n no es múltiplo de workers) se asigna al último bloque.
         Garantiza cobertura total sin huecos ni solapamientos, idéntico a Python.
 **/
inline std::vector<RangeBlock> partition_range(uint64_t n, unsigned int workers) {
    std::vector<RangeBlock> blocks;
    blocks.reserve(workers);
    uint64_t chunk = n / workers;
    for (unsigned int k = 0; k < workers; ++k) {
        uint64_t start = k * chunk;
        uint64_t stop = (k == workers - 1) ? n : (k + 1) * chunk;
        blocks.push_back({start, stop});
    }
    return blocks;
}


// PASO 2. CONSULTA DE SISTEMA OPERATIVO, CPU Y AFINIDAD

inline std::string get_os_name() {
#ifdef _WIN32
    return "windows";
#else
    return "linux";
#endif
}

inline int get_current_lp() {
#ifdef _WIN32
    return static_cast<int>(GetCurrentProcessorNumber());
#else
    return sched_getcpu();
#endif
}

inline std::string get_affinity_label() {
#ifdef _WIN32
    DWORD_PTR processAffinityMask = 0, systemAffinityMask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask, &systemAffinityMask)) {
        int count = 0;
        for (DWORD_PTR m = processAffinityMask; m > 0; m >>= 1) {
            if (m & 1) count++;
        }
        int total = 0;
        for (DWORD_PTR m = systemAffinityMask; m > 0; m >>= 1) {
            if (m & 1) total++;
        }
        if (count < total) {
            return std::to_string(count);
        }
    }
    return "all";
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (sched_getaffinity(0, sizeof(cpuset), &cpuset) == 0) {
        int count = CPU_COUNT(&cpuset);
        long total = sysconf(_SC_NPROCESSORS_CONF);
        if (total > 0 && count < total) {
            return std::to_string(count);
        }
    }
    return "all";
#endif
}

// PASO 3. FUNCIONES WORKER PARA HILOS NATIVOS

#ifdef _WIN32
static DWORD WINAPI win32_thread_worker(LPVOID lpParam) {
    ThreadTask* task = reinterpret_cast<ThreadTask*>(lpParam);
    int lp_start = -1;
    if (task->log_lp) {
        lp_start = get_current_lp();
    }
    
    task->partial_result = compute_kernel(task->start, task->stop);
    
    if (task->log_lp) {
        int lp_end = get_current_lp();
        fprintf(stderr, "[Thread %u] LP inicio: %d, LP fin: %d (migro: %s)\n",
                task->worker_id, lp_start, lp_end, (lp_start != lp_end ? "SI" : "NO"));
        fflush(stderr);
    }
    return 0;
}
#else
static void* posix_thread_worker(void* arg) {
    ThreadTask* task = reinterpret_cast<ThreadTask*>(arg);
    int lp_start = -1;
    if (task->log_lp) {
        lp_start = get_current_lp();
    }
    
    task->partial_result = compute_kernel(task->start, task->stop);
    
    if (task->log_lp) {
        int lp_end = get_current_lp();
        fprintf(stderr, "[Thread %u] LP inicio: %d, LP fin: %d (migro: %s)\n",
                task->worker_id, lp_start, lp_end, (lp_start != lp_end ? "SI" : "NO"));
        fflush(stderr);
    }
    return nullptr;
}
#endif


uint64_t run_threads_helper(uint64_t start, uint64_t stop, unsigned int threads, bool log_lp, unsigned int base_worker_id = 0) {
    if (threads <= 1) {
        int lp_start = -1;
        if (log_lp) lp_start = get_current_lp();
        uint64_t res = compute_kernel(start, stop);
        if (log_lp) {
            int lp_end = get_current_lp();
            fprintf(stderr, "[Worker %u] LP inicio: %d, LP fin: %d (migro: %s)\n",
                    base_worker_id, lp_start, lp_end, (lp_start != lp_end ? "SI" : "NO"));
            fflush(stderr);
        }
        return res;
    }

    uint64_t range_len = stop - start;
    std::vector<RangeBlock> sub_blocks = partition_range(range_len, threads);
    std::vector<ThreadTask> tasks(threads);

#ifdef _WIN32
    std::vector<HANDLE> handles(threads);
    for (unsigned int i = 0; i < threads; ++i) {
        tasks[i].start = start + sub_blocks[i].start;
        tasks[i].stop = start + sub_blocks[i].stop;
        tasks[i].partial_result = 0;
        tasks[i].worker_id = base_worker_id + i;
        tasks[i].log_lp = log_lp;

        handles[i] = CreateThread(NULL, 0, win32_thread_worker, &tasks[i], 0, NULL);
        if (handles[i] == NULL) {
            std::cerr << "Error al crear hilo Win32 " << i << std::endl;
            exit(1);
        }
    }

    if (threads <= 64) {
        WaitForMultipleObjects(threads, handles.data(), TRUE, INFINITE);
    } else {
        for (unsigned int i = 0; i < threads; ++i) {
            WaitForSingleObject(handles[i], INFINITE);
        }
    }

    uint64_t total = 0;
    for (unsigned int i = 0; i < threads; ++i) {
        total += tasks[i].partial_result;
        CloseHandle(handles[i]);
    }
    return total;
#else
    std::vector<pthread_t> handles(threads);
    for (unsigned int i = 0; i < threads; ++i) {
        tasks[i].start = start + sub_blocks[i].start;
        tasks[i].stop = start + sub_blocks[i].stop;
        tasks[i].partial_result = 0;
        tasks[i].worker_id = base_worker_id + i;
        tasks[i].log_lp = log_lp;

        int rc = pthread_create(&handles[i], nullptr, posix_thread_worker, &tasks[i]);
        if (rc != 0) {
            std::cerr << "Error al crear pthread " << i << std::endl;
            exit(1);
        }
    }

    uint64_t total = 0;
    for (unsigned int i = 0; i < threads; ++i) {
        pthread_join(handles[i], nullptr);
        total += tasks[i].partial_result;
    }
    return total;
#endif
}

// PASO 4. EJECUCIÓN MULTIPROCESO (E2) Y MIXTO (E3)

#ifdef _WIN32
static uint64_t filetime_to_100ns(const FILETIME& ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart;
}

static uint64_t get_process_cpu_100ns(HANDLE hProc) {
    FILETIME ftCreate, ftExit, ftKernel, ftUser;
    if (GetProcessTimes(hProc, &ftCreate, &ftExit, &ftKernel, &ftUser)) {
        return filetime_to_100ns(ftKernel) + filetime_to_100ns(ftUser);
    }
    return 0;
}
#endif


#ifdef _WIN32
uint64_t run_multiprocess_windows(const std::vector<RangeBlock>& blocks, const std::vector<unsigned int>& threads_per_proc,
                                 bool log_lp, double& out_child_cpu_sec) {
    size_t num_proc = blocks.size();
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    struct ChildContext {
        HANDLE hProcess;
        HANDLE hThread;
        HANDLE hReadPipe;
    };
    std::vector<ChildContext> children(num_proc);

    unsigned int worker_accum = 0;
    for (size_t i = 0; i < num_proc; ++i) {
        HANDLE hReadPipe = NULL, hWritePipe = NULL;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
            std::cerr << "Error en CreatePipe para hijo " << i << std::endl;
            exit(1);
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdOutput = hWritePipe;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        std::ostringstream cmd;
        cmd << "\"" << exePath << "\" --child"
            << " --start " << blocks[i].start
            << " --stop " << blocks[i].stop
            << " --worker-id " << worker_accum
            << " --threads " << threads_per_proc[i];
        if (log_lp) cmd << " --log-lp";

        std::string cmdStr = cmd.str();
        std::vector<char> cmdBuf(cmdStr.begin(), cmdStr.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
        CloseHandle(hWritePipe); // El padre cierra su copia del extremo de escritura para permitir EOF

        if (!success) {
            std::cerr << "Error en CreateProcess para hijo " << i << std::endl;
            exit(1);
        }

        children[i].hProcess = pi.hProcess;
        children[i].hThread = pi.hThread;
        children[i].hReadPipe = hReadPipe;
        worker_accum += threads_per_proc[i];
    }

    uint64_t total_result = 0;
    uint64_t total_child_cpu_100ns = 0;

    for (size_t i = 0; i < num_proc; ++i) {
        WaitForSingleObject(children[i].hProcess, INFINITE);

        char buffer[128];
        DWORD bytesRead = 0;
        std::string outStr;
        while (ReadFile(children[i].hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            outStr += buffer;
        }
        CloseHandle(children[i].hReadPipe);

        uint64_t partial = 0;
        if (!outStr.empty()) {
            partial = std::strtoull(outStr.c_str(), nullptr, 10);
        }
        total_result += partial;

        total_child_cpu_100ns += get_process_cpu_100ns(children[i].hProcess);
        CloseHandle(children[i].hProcess);
        CloseHandle(children[i].hThread);
    }

    out_child_cpu_sec = static_cast<double>(total_child_cpu_100ns) * 1e-7;
    return total_result;
}
#else

uint64_t run_multiprocess_linux(const std::vector<RangeBlock>& blocks, const std::vector<unsigned int>& threads_per_proc,
                               bool log_lp) {
    size_t num_proc = blocks.size();
    std::vector<int> read_pipes(num_proc);
    std::vector<pid_t> pids(num_proc);

    unsigned int worker_accum = 0;
    for (size_t i = 0; i < num_proc; ++i) {
        int pfd[2];
        if (pipe(pfd) < 0) {
            perror("pipe");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Proceso hijo
            close(pfd[0]);
            uint64_t partial = run_threads_helper(blocks[i].start, blocks[i].stop, threads_per_proc[i], log_lp, worker_accum);
            ssize_t written = write(pfd[1], &partial, sizeof(partial));
            (void)written;
            close(pfd[1]);
            _exit(0);
        } else {
            // Proceso padre
            close(pfd[1]);
            read_pipes[i] = pfd[0];
            pids[i] = pid;
            worker_accum += threads_per_proc[i];
        }
    }

    uint64_t total_result = 0;
    for (size_t i = 0; i < num_proc; ++i) {
        int status;
        waitpid(pids[i], &status, 0);

        uint64_t partial = 0;
        ssize_t r = read(read_pipes[i], &partial, sizeof(partial));
        (void)r;
        close(read_pipes[i]);
        total_result += partial;
    }
    return total_result;
}
#endif

// PASO 5. MEDIDOR Y EJECUTOR PRINCIPAL DE CONFIGURACIONES

struct RunMetrics {
    double elapsed_seconds;
    long cpu_total_pct;
    uint64_t result;
};


RunMetrics execute_configuration(const std::string& mode, unsigned int workers, unsigned int processes,
                                 uint64_t workload, bool log_lp) {
    RunMetrics m;

#ifdef _WIN32
    uint64_t parent_start_cpu = get_process_cpu_100ns(GetCurrentProcess());
    double child_cpu_sec = 0.0;
#else
    struct rusage r_self_start, r_child_start;
    getrusage(RUSAGE_SELF, &r_self_start);
    getrusage(RUSAGE_CHILDREN, &r_child_start);
#endif

    auto t_start = std::chrono::steady_clock::now();

    if (mode == "sequential") {
        m.result = run_threads_helper(0, workload, 1, log_lp, 0);
    } else if (mode == "threads" || mode == "threading" || mode == "parallel") {
        m.result = run_threads_helper(0, workload, workers, log_lp, 0);
    } else if (mode == "processes" || mode == "multiprocess") {
        std::vector<RangeBlock> blocks = partition_range(workload, processes);
        std::vector<unsigned int> threads_per_proc(processes, 1);
#ifdef _WIN32
        m.result = run_multiprocess_windows(blocks, threads_per_proc, log_lp, child_cpu_sec);
#else
        m.result = run_multiprocess_linux(blocks, threads_per_proc, log_lp);
#endif
    } else if (mode == "mixed") {
        // Escenario E3: 2 procesos. Proceso A: 5 hilos, Proceso B: 3 hilos (Total 8 trabajadores)
        uint64_t cut = (workload / 8ULL) * 5ULL;
        std::vector<RangeBlock> blocks = {{0, cut}, {cut, workload}};
        std::vector<unsigned int> threads_per_proc = {5, 3};
#ifdef _WIN32
        m.result = run_multiprocess_windows(blocks, threads_per_proc, log_lp, child_cpu_sec);
#else
        m.result = run_multiprocess_linux(blocks, threads_per_proc, log_lp);
#endif
    } else {
        std::cerr << "Modo desconocido: " << mode << std::endl;
        exit(1);
    }

    auto t_end = std::chrono::steady_clock::now();
    m.elapsed_seconds = std::chrono::duration<double>(t_end - t_start).count();

#ifdef _WIN32
    uint64_t parent_end_cpu = get_process_cpu_100ns(GetCurrentProcess());
    double parent_cpu_sec = static_cast<double>(parent_end_cpu - parent_start_cpu) * 1e-7;
    double total_cpu_sec = parent_cpu_sec + child_cpu_sec;
#else
    struct rusage r_self_end, r_child_end;
    getrusage(RUSAGE_SELF, &r_self_end);
    getrusage(RUSAGE_CHILDREN, &r_child_end);

    double self_cpu = (r_self_end.ru_utime.tv_sec - r_self_start.ru_utime.tv_sec) +
                      (r_self_end.ru_utime.tv_usec - r_self_start.ru_utime.tv_usec) * 1e-6 +
                      (r_self_end.ru_stime.tv_sec - r_self_start.ru_stime.tv_sec) +
                      (r_self_end.ru_stime.tv_usec - r_self_start.ru_stime.tv_usec) * 1e-6;

    double child_cpu = (r_child_end.ru_utime.tv_sec - r_child_start.ru_utime.tv_sec) +
                       (r_child_end.ru_utime.tv_usec - r_child_start.ru_utime.tv_usec) * 1e-6 +
                       (r_child_end.ru_stime.tv_sec - r_child_start.ru_stime.tv_sec) +
                       (r_child_end.ru_stime.tv_usec - r_child_start.ru_stime.tv_usec) * 1e-6;
    double total_cpu_sec = self_cpu + child_cpu;
#endif

    if (m.elapsed_seconds > 1e-6) {
        m.cpu_total_pct = static_cast<long>(std::round((total_cpu_sec / m.elapsed_seconds) * 100.0));
    } else {
        m.cpu_total_pct = 0;
    }

    return m;
}

// PASO 6. UTILIDADES Y PROCESAMIENTO DE ARGUMENTOS CLI

uint64_t parse_u64_clean(const std::string& str) {
    std::string clean;
    for (char c : str) {
        if (c != '_' && c != ',') clean += c;
    }
    return std::strtoull(clean.c_str(), nullptr, 10);
}

void print_help(const char* progName) {
    std::cout << "Laboratorio de Procesos e Hilos — Programa C++17 (P1)\n\n"
              << "USO:\n"
              << "  " << progName << " [OPCIONES]\n\n"
              << "MODOS DISPONIBLES:\n"
              << "  --mode sequential     1 proceso, 1 hilo (Escenario E0 Línea Base)\n"
              << "  --mode threads        1 proceso, W hilos (Escenarios E1 y E1-C)\n"
              << "  --mode processes      P procesos, 1 hilo c/u (Escenario E2 Multiproceso)\n"
              << "  --mode mixed          2 procesos (A con 5 hilos, B con 3 hilos, Escenario E3)\n\n"
              << "OPCIONES:\n"
              << "  --workload, -n <N>    Cantidad de iteraciones (ej. 2000000000 o 2_000_000_000)\n"
              << "  --workers, -w <W>     Cantidad de hilos o procesos (1, 2, 4, 8, 16)\n"
              << "  --processes, -p <P>   Cantidad de procesos (para --mode processes)\n"
              << "  --runs <R>            Número de repeticiones (defecto: 1)\n"
              << "  --warmup <K>          Corridas iniciales de calentamiento descartadas\n"
              << "  --csv                 Emite los resultados en formato CSV v2\n"
              << "  --header              Imprime la cabecera CSV v2 antes de los datos\n"
              << "  --log-lp              Imprime por stderr el procesador lógico de inicio y fin\n"
              << "  --affinity-label <L>  Sobrescribe la etiqueta de afinidad en el CSV\n"
              << "  --expect <VAL>        Valida que el resultado sea igual a VAL (error 2 si falla)\n"
              << "  --verbose             Muestra metadatos detallados de ejecución\n"
              << "  --help, -h            Muestra esta ayuda\n";
}

// PASO 7. PUNTO DE ENTRADA PRINCIPAL

int main(int argc, char* argv[]) {
    std::string mode = "sequential";
    uint64_t workload = 2000000000ULL;
    unsigned int workers = 1;
    unsigned int processes = 1;
    unsigned int runs = 1;
    unsigned int warmup = 0;
    bool csv_mode = false;
    bool print_header = false;
    bool log_lp = false;
    bool verbose = false;
    std::string affinity_override = "";
    uint64_t expected_val = 0;
    bool has_expected = false;

    // Parámetros internos de proceso hijo (modo child en Windows)
    bool is_child = false;
    uint64_t child_start = 0;
    uint64_t child_stop = 0;
    unsigned int child_worker_id = 0;
    unsigned int child_threads = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--mode" && i + 1 < argc) {
            mode = argv[++i];
        } else if ((arg == "--workload" || arg == "-n") && i + 1 < argc) {
            workload = parse_u64_clean(argv[++i]);
        } else if ((arg == "--workers" || arg == "-w" || arg == "--threads") && i + 1 < argc) {
            workers = std::stoul(argv[++i]);
        } else if ((arg == "--processes" || arg == "-p") && i + 1 < argc) {
            processes = std::stoul(argv[++i]);
        } else if ((arg == "--runs" || arg == "--repeat") && i + 1 < argc) {
            runs = std::stoul(argv[++i]);
        } else if (arg == "--warmup" && i + 1 < argc) {
            warmup = std::stoul(argv[++i]);
        } else if (arg == "--csv") {
            csv_mode = true;
        } else if (arg == "--header" || arg == "--csv-header") {
            print_header = true;
        } else if (arg == "--log-lp") {
            log_lp = true;
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--affinity-label" && i + 1 < argc) {
            affinity_override = argv[++i];
        } else if (arg == "--expect" && i + 1 < argc) {
            expected_val = parse_u64_clean(argv[++i]);
            has_expected = true;
        } else if (arg == "--child") {
            is_child = true;
        } else if (arg == "--start" && i + 1 < argc) {
            child_start = parse_u64_clean(argv[++i]);
        } else if (arg == "--stop" && i + 1 < argc) {
            child_stop = parse_u64_clean(argv[++i]);
        } else if (arg == "--worker-id" && i + 1 < argc) {
            child_worker_id = std::stoul(argv[++i]);
        }
    }

    // Si este proceso fue invocado como proceso hijo trabajador (Windows IPC)
    if (is_child) {
        uint64_t child_res = run_threads_helper(child_start, child_stop, child_threads, log_lp, child_worker_id);
        std::cout << child_res << std::endl;
        return 0;
    }

    // Normalización de parámetros según el modo
    if (mode == "parallel" || mode == "threading") {
        mode = "threads";
    } else if (mode == "multiprocess") {
        mode = "processes";
    }

    if (mode == "processes") {
        if (processes <= 1 && workers > 1) {
            processes = workers;
        }
    } else if (mode == "threads") {
        processes = 1;
    } else if (mode == "mixed") {
        processes = 2;
        workers = 8;
    } else if (mode == "sequential") {
        processes = 1;
        workers = 1;
    }

    std::string affinity_label = affinity_override.empty() ? get_affinity_label() : affinity_override;
    std::string os_name = get_os_name();

    if (print_header && csv_mode) {
        std::cout << CSV_HEADER << "\n";
    }

    if (verbose && !csv_mode) {
        std::cout << "--- Configuración del Benchmark C++ ---\n"
                  << "SO detectado:       " << os_name << "\n"
                  << "Modo:               " << mode << "\n"
                  << "Procesos:           " << processes << "\n"
                  << "Hilos por proceso:  " << (mode == "mixed" ? "5,3" : std::to_string(mode == "processes" ? 1 : workers)) << "\n"
                  << "Total trabajadores: " << (mode == "processes" ? processes : workers) << "\n"
                  << "Afinidad LP:        " << affinity_label << "\n"
                  << "Carga (workload):   " << workload << "\n"
                  << "Repeticiones:       " << runs << "\n"
                  << "--------------------------------------\n";
    }

    // Corridas de calentamiento
    for (unsigned int w = 0; w < warmup; ++w) {
        execute_configuration(mode, workers, processes, workload, false);
    }

    // Corridas medidas
    for (unsigned int r = 1; r <= runs; ++r) {
        RunMetrics metrics = execute_configuration(mode, workers, processes, workload, log_lp);

        if (has_expected && metrics.result != expected_val) {
            std::cerr << "ERROR: El resultado (" << metrics.result
                      << ") no coincide con el valor esperado (" << expected_val << ")!\n";
            return 2;
        }

        if (csv_mode) {
            std::string t_proc = (mode == "mixed") ? "mixed" : (mode == "processes" ? "1" : std::to_string(workers));
            unsigned int total_w = (mode == "mixed") ? 8 : (mode == "processes" ? processes : workers);

            std::cout << "C++,"
                      << os_name << ","
                      << mode << ","
                      << processes << ","
                      << t_proc << ","
                      << total_w << ","
                      << affinity_label << ","
                      << workload << ","
                      << r << ","
                      << std::fixed << std::setprecision(6) << metrics.elapsed_seconds << ","
                      << metrics.cpu_total_pct << ","
                      << metrics.result << "\n";
        } else {
            std::cout << "Language: C++\n"
                      << "OS: " << os_name << "\n"
                      << "Mode: " << mode << "\n"
                      << "Processes: " << processes << "\n"
                      << "Threads per process: " << (mode == "mixed" ? "5,3" : (mode == "processes" ? "1" : std::to_string(workers))) << "\n"
                      << "Total workers: " << (mode == "mixed" ? 8 : (mode == "processes" ? processes : workers)) << "\n"
                      << "Affinity: " << affinity_label << "\n"
                      << "Workload: " << workload << "\n"
                      << "Run: " << r << "\n"
                      << "Execution time: " << std::fixed << std::setprecision(4) << metrics.elapsed_seconds << " seconds\n"
                      << "CPU total: " << metrics.cpu_total_pct << "%\n"
                      << "Result: " << metrics.result << "\n";
            if (r < runs) std::cout << "\n";
        }
    }

    return 0;
}
