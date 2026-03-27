#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "poisson.h"

#define DEFAULT_N      2000
#define DEFAULT_ITERS  5000
#define DEFAULT_PROCS  4
#define TOLERANCE      1e-6

/*
 * Fork-once architecture: all worker processes are created once before the
 * main loop and synchronize each iteration using semaphores in shared memory.
 * This mirrors the pthread_barrier model in threads.c, making the comparison
 * between the two parallelism strategies structurally equivalent.
 *
 * Synchronization model (asymmetric, same logic as thread 0 in threads.c):
 *   peers_ready  -- workers 1..p-1 post once; worker 0 waits p-1 times.
 *   proceed      -- worker 0 posts p-1 times; workers 1..p-1 each wait once.
 *
 * Worker 0 acts as coordinator: waits for all peers, computes the global RMS
 * residual over the full sol_new, checks convergence, and flips the buffers.
 *
 * Buffer swap: instead of swapping pointers (which are local to each process),
 * a shared flip flag determines which buffer is sol (read) and which is sol_new
 * (write) in each iteration.  After worker 0 flips the flag, all workers see
 * the updated state before the next iteration begins.
 *
 * Shared memory layout (single mmap allocation):
 *
 *   [ arena_a[0..num_pts+1] | arena_b[0..num_pts+1] | rhs[0..num_pts+1] | SyncBlock ]
 */

typedef struct {
    sem_t peers_ready;
    sem_t proceed;
    int   flip;
    int   converged;
    int   steps_done;
} SyncBlock;

typedef struct {
    double    *arena_a;
    double    *arena_b;
    double    *rhs;
    SyncBlock *sync;
} SharedMem;

static SharedMem alloc_shared(int num_pts) {
    size_t arr_bytes = (size_t)(num_pts + 2) * sizeof(double);
    size_t total     = 3 * arr_bytes + sizeof(SyncBlock);

    char *base = mmap(NULL, total, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) { perror("mmap"); exit(1); }

    SharedMem sm;
    sm.arena_a = (double *)(base);
    sm.arena_b = (double *)(base + arr_bytes);
    sm.rhs     = (double *)(base + 2 * arr_bytes);
    sm.sync    = (SyncBlock *)(base + 3 * arr_bytes);
    return sm;
}

static void free_shared(SharedMem sm, int num_pts) {
    size_t arr_bytes = (size_t)(num_pts + 2) * sizeof(double);
    munmap(sm.arena_a, 3 * arr_bytes + sizeof(SyncBlock));
}

int main(int argc, char *argv[]) {
    int    num_pts   = (argc > 1) ? atoi(argv[1]) : DEFAULT_N;
    int    max_steps = (argc > 2) ? atoi(argv[2]) : DEFAULT_ITERS;
    int    num_procs = (argc > 3) ? atoi(argv[3]) : DEFAULT_PROCS;
    double spacing   = 1.0 / (num_pts + 1);

    SharedMem sm = alloc_shared(num_pts);

    initialize_grid(sm.arena_a, num_pts);
    initialize_grid(sm.arena_b, num_pts);
    compute_rhs(sm.rhs, num_pts, spacing);

    /* pshared=1: semaphores are shared across forked processes. */
    sem_init(&sm.sync->peers_ready, 1, 0);
    sem_init(&sm.sync->proceed,     1, 0);
    sm.sync->flip       = 0;
    sm.sync->converged  = 0;
    sm.sync->steps_done = 0;

    int chunk = num_pts / num_procs;

    double t_begin = wall_time();

    pid_t *pids = malloc((size_t)num_procs * sizeof(pid_t));

    for (int rank = 0; rank < num_procs; rank++) {
        int seg_start = rank * chunk + 1;
        int seg_end   = (rank == num_procs - 1) ? num_pts : (rank + 1) * chunk;

        pids[rank] = fork();
        if (pids[rank] < 0) { perror("fork"); exit(1); }

        if (pids[rank] == 0) {
            for (int step = 0; step < max_steps; step++) {
                double *sol_rd  = (sm.sync->flip == 0) ? sm.arena_a : sm.arena_b;
                double *sol_wr  = (sm.sync->flip == 0) ? sm.arena_b : sm.arena_a;

                for (int idx = seg_start; idx <= seg_end; idx++)
                    sol_wr[idx] = 0.5 * (sol_rd[idx - 1] + sol_rd[idx + 1]
                                         + spacing * spacing * sm.rhs[idx]);

                if (rank == 0) {
                    /* Wait for all peers to finish writing sol_new before
                     * calling rms_residual, which reads neighbor values
                     * across chunk boundaries. */
                    for (int k = 1; k < num_procs; k++)
                        sem_wait(&sm.sync->peers_ready);

                    sm.sync->converged  = (rms_residual(sol_wr, sm.rhs,
                                                        num_pts, spacing)
                                           < TOLERANCE);
                    sm.sync->steps_done = step + 1;
                    sm.sync->flip      ^= 1;

                    for (int k = 1; k < num_procs; k++)
                        sem_post(&sm.sync->proceed);
                } else {
                    sem_post(&sm.sync->peers_ready);
                    sem_wait(&sm.sync->proceed);
                }

                if (sm.sync->converged) break;
            }
            exit(0);
        }
    }

    for (int rank = 0; rank < num_procs; rank++)
        waitpid(pids[rank], NULL, 0);

    double elapsed_ms = (wall_time() - t_begin) * 1000.0;

    /* After the last flip, the solution lives on the current read side. */
    double *sol_final = (sm.sync->flip == 0) ? sm.arena_a : sm.arena_b;

    fprintf(stderr,
            "processes n=%-6d iters=%-6d p=%-2d  error=%.4e  time=%.3f ms\n",
            num_pts, sm.sync->steps_done, num_procs,
            max_error(sol_final, num_pts, spacing), elapsed_ms);
    printf("%.3f\n", elapsed_ms);

    sem_destroy(&sm.sync->peers_ready);
    sem_destroy(&sm.sync->proceed);
    free(pids);
    free_shared(sm, num_pts);
    return 0;
}
