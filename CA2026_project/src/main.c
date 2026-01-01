// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>  // for _getcwd
#include "sim.h"

// Default file names (27 total)
// Inputs from ../inputs/
// Outputs to ../examples/example_061225_win/my_outputs/
static const char *DEFAULT_FILES[27] = {
    // Inputs (0-4)
    "../inputs/imem0.txt", "../inputs/imem1.txt", "../inputs/imem2.txt", "../inputs/imem3.txt",
    "../inputs/memin.txt",
    // Outputs (5-26)
    "../examples/example_061225_win/my_outputs/memout.txt",
    "../examples/example_061225_win/my_outputs/regout0.txt", "../examples/example_061225_win/my_outputs/regout1.txt", "../examples/example_061225_win/my_outputs/regout2.txt", "../examples/example_061225_win/my_outputs/regout3.txt",
    "../examples/example_061225_win/my_outputs/core0trace.txt", "../examples/example_061225_win/my_outputs/core1trace.txt", "../examples/example_061225_win/my_outputs/core2trace.txt", "../examples/example_061225_win/my_outputs/core3trace.txt",
    "../examples/example_061225_win/my_outputs/bustrace.txt",
    "../examples/example_061225_win/my_outputs/dsram0.txt", "../examples/example_061225_win/my_outputs/dsram1.txt", "../examples/example_061225_win/my_outputs/dsram2.txt", "../examples/example_061225_win/my_outputs/dsram3.txt",
    "../examples/example_061225_win/my_outputs/tsram0.txt", "../examples/example_061225_win/my_outputs/tsram1.txt", "../examples/example_061225_win/my_outputs/tsram2.txt", "../examples/example_061225_win/my_outputs/tsram3.txt",
    "../examples/example_061225_win/my_outputs/stats0.txt", "../examples/example_061225_win/my_outputs/stats1.txt", "../examples/example_061225_win/my_outputs/stats2.txt", "../examples/example_061225_win/my_outputs/stats3.txt"
};

#define NUM_FILES 27



int main(int argc, char *argv[]) {
    Simulator *sim = NULL;  // Allocate on heap to avoid stack overflow
    const char *files[NUM_FILES];

    // Print current working directory for debugging
    char cwd[1024];
    if (_getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }

    // Parse command line arguments or use defaults
    printf("DEBUG: argc = %d\n", argc);
    if (argc == 1) {
        // No arguments - use default file names
        for (int i = 0; i < NUM_FILES; i++) {
            files[i] = DEFAULT_FILES[i];
        }
        printf("Using default file names\n");
    } else if (argc == 6) { 
        // 5 arguments: imem0-3, memin. Use defaults for outputs.
        for (int i = 0; i < 5; i++) {
            files[i] = argv[i + 1];
        }
        for (int i = 5; i < NUM_FILES; i++) {
            files[i] = DEFAULT_FILES[i];
        }
        printf("Using custom inputs, default outputs\n");
    } else if (argc == NUM_FILES + 1) {  // Program name + 27 file args = 28 total
        // All file names provided
        for (int i = 0; i < NUM_FILES; i++) {
            files[i] = argv[i + 1];
        }
    } else {
        fprintf(stderr, "Usage: %s [imem0.txt imem1.txt imem2.txt imem3.txt memin.txt]\n", argv[0]);
        fprintf(stderr, "   OR: %s [all 27 files]\n", argv[0]);
        return 1;
    }

    // Allocate simulator on heap (avoid stack overflow - 8MB+ structure)
    printf("Allocating simulator memory...\n");
    sim = (Simulator *)calloc(1, sizeof(Simulator));
    if (!sim) {
        fprintf(stderr, "Error: Failed to allocate memory for simulator\n");
        return 1;
    }

    // Initialize simulator
    printf("Initializing simulator...\n");
    init_simulator(sim);

    // Load instruction memories
    printf("Loading instruction memories...\n");
    for (int i = 0; i < NUM_CORES; i++) {
        if (!load_imem(files[i], sim->cores[i].imem)) {
            fprintf(stderr, "Error loading %s\n", files[i]);
            free(sim);
            return 1;
        }
    }

    // Generate .asm files from loaded instructions for verification
    printf("Generating .asm files for verification...\n");
    for (int i = 0; i < NUM_CORES; i++) {
        char asm_filename[64];
        sprintf(asm_filename, "outputs/imem%d.asm", i);
        if (!save_assembly(asm_filename, sim->cores[i].imem, IMEM_SIZE)) {
            fprintf(stderr, "Warning: Failed to save %s\n", asm_filename);
        }
    }

    // Load main memory
    printf("Loading main memory...\n");
    if (!load_memin(files[4], &sim->main_memory)) {
        fprintf(stderr, "Error loading %s\n", files[4]);
        free(sim);
        return 1;
    }
    

    // Run simulation
    printf("Starting simulation...\n");
    run_simulator(sim);
    printf("Simulation completed after %llu cycles\n", sim->global_cycle);

    // Save outputs
    printf("Saving outputs...\n");

    // Memory output
    if (!save_memout(files[5], &sim->main_memory)) {
        fprintf(stderr, "Error saving %s\n", files[5]);
        free(sim);
        return 1;
    }

    // Register outputs
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_regout(files[6 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[6 + i]);
            free(sim);
            return 1;
        }
    }

    // Core traces
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_trace(files[10 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[10 + i]);
            free(sim);
            return 1;
        }
    }

    // Bus trace
    if (!save_bustrace(files[14], &sim->bus)) {
        fprintf(stderr, "Error saving %s\n", files[14]);
        free(sim);
        return 1;
    }

    // DSRAM outputs
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_dsram(files[15 + i], &sim->cores[i].cache)) {
            fprintf(stderr, "Error saving %s\n", files[15 + i]);
            free(sim);
            return 1;
        }
    }

    // TSRAM outputs
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_tsram(files[19 + i], &sim->cores[i].cache)) {
            fprintf(stderr, "Error saving %s\n", files[19 + i]);
            free(sim);
            return 1;
        }
    }

    // Statistics outputs
    for (int i = 0; i < NUM_CORES; i++) {
        if (!save_stats(files[23 + i], &sim->cores[i])) {
            fprintf(stderr, "Error saving %s\n", files[23 + i]);
            free(sim);
            return 1;
        }
    }

    printf("All outputs saved successfully\n");
    printf("\nSimulation Summary:\n");
    for (int i = 0; i < NUM_CORES; i++) {
        printf("Core %d: %llu cycles, %llu instructions\n",
               i, sim->cores[i].cycles, sim->cores[i].instructions);
    }

    // Free allocated memory
    free(sim);

    return 0;
}
