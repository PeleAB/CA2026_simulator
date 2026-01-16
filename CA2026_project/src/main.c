// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef _WIN32
#include <direct.h>  // for _getcwd on Windows
#define getcwd _getcwd
#else
#include <unistd.h>  // for getcwd on POSIX
#endif

#include "sim.h"

#define NUM_FILES 27

// Default file names (27 total) - used when running sim.exe without parameters
// Files are expected in the same directory as sim.exe
static const char *DEFAULT_FILE_NAMES[NUM_FILES] = {
    // Inputs (0-4)
    "imem0.txt", "imem1.txt", "imem2.txt", "imem3.txt", "memin.txt",
    // Outputs (5-26)
    "memout.txt",
    "regout0.txt", "regout1.txt", "regout2.txt", "regout3.txt",
    "core0trace.txt", "core1trace.txt", "core2trace.txt", "core3trace.txt",
    "bustrace.txt",
    "dsram0.txt", "dsram1.txt", "dsram2.txt", "dsram3.txt",
    "tsram0.txt", "tsram1.txt", "tsram2.txt", "tsram3.txt",
    "stats0.txt", "stats1.txt", "stats2.txt", "stats3.txt"
};

int main(int argc, char *argv[]) {
    Simulator *sim = NULL;  // Allocate on heap to avoid stack overflow
    const char *files[NUM_FILES];

    // Parse command line arguments according to PDF specification:
    // Either 0 parameters (use defaults) or exactly 27 parameters
    if (argc == 1) {
        // No arguments - use default file names from same directory as sim.exe
        for (int i = 0; i < NUM_FILES; i++) {
            files[i] = DEFAULT_FILE_NAMES[i];
        }
    } else if (argc == NUM_FILES + 1) {  // Program name + 27 file args = 28 total
        // All 27 file names provided as per PDF specification:
        // sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt
        //         regout0.txt regout1.txt regout2.txt regout3.txt
        //         core0trace.txt core1trace.txt core2trace.txt core3trace.txt
        //         bustrace.txt
        //         dsram0.txt dsram1.txt dsram2.txt dsram3.txt
        //         tsram0.txt tsram1.txt tsram2.txt tsram3.txt
        //         stats0.txt stats1.txt stats2.txt stats3.txt
        for (int i = 0; i < NUM_FILES; i++) {
            files[i] = argv[i + 1];
        }
    } else {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        fprintf(stderr, "   OR: %s imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt "
                        "regout0.txt regout1.txt regout2.txt regout3.txt "
                        "core0trace.txt core1trace.txt core2trace.txt core3trace.txt "
                        "bustrace.txt "
                        "dsram0.txt dsram1.txt dsram2.txt dsram3.txt "
                        "tsram0.txt tsram1.txt tsram2.txt tsram3.txt "
                        "stats0.txt stats1.txt stats2.txt stats3.txt\n", argv[0]);
        return 1;
    }

    // Allocate simulator on heap (avoid stack overflow - 8MB+ structure)
    sim = (Simulator *)calloc(1, sizeof(Simulator));
    if (!sim) {
        fprintf(stderr, "Error: Failed to allocate memory for simulator\n");
        return 1;
    }

    // Initialize simulator
    init_simulator(sim);

    // Load instruction memories (files 0-3: imem0.txt - imem3.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!load_imem(files[i], sim->cores[i].imem)) {
            fprintf(stderr, "Error loading %s\n", files[i]);
            free(sim);
            return 1;
        }
    }

    // Load main memory (file 4: memin.txt)
    if (!load_memin(files[4], &sim->main_memory)) {
        fprintf(stderr, "Error loading %s\n", files[4]);
        free(sim);
        return 1;
    }

    // Run simulation
    run_simulator(sim);

    // Save outputs

    // Memory output (file 5: memout.txt)
    if (!save_memout(files[5], &sim->main_memory)) {
        fprintf(stderr, "Error saving %s\n", files[5]);
        free(sim);
        return 1;
    }

    // Register outputs (files 6-9: regout0.txt - regout3.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_regout(files[6 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[6 + i]);
            free(sim);
            return 1;
        }
    }

    // Core traces (files 10-13: core0trace.txt - core3trace.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_trace(files[10 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[10 + i]);
            free(sim);
            return 1;
        }
    }

    // Bus trace (file 14: bustrace.txt)
    if (!save_bustrace(files[14], &sim->bus)) {
        fprintf(stderr, "Error saving %s\n", files[14]);
        free(sim);
        return 1;
    }

    // DSRAM outputs (files 15-18: dsram0.txt - dsram3.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_dsram(files[15 + i], &sim->cores[i].cache)) {
            fprintf(stderr, "Error saving %s\n", files[15 + i]);
            free(sim);
            return 1;
        }
    }

    // TSRAM outputs (files 19-22: tsram0.txt - tsram3.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_tsram(files[19 + i], &sim->cores[i].cache)) {
            fprintf(stderr, "Error saving %s\n", files[19 + i]);
            free(sim);
            return 1;
        }
    }

    // Statistics outputs (files 23-26: stats0.txt - stats3.txt)
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_stats(files[23 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[23 + i]);
            free(sim);
            return 1;
        }
    }

    // Free allocated memory
    free(sim);

    return 0;
}
