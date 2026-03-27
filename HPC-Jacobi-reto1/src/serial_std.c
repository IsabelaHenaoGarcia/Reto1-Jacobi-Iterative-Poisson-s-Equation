/**
 * ============================================================================
 *  IMPLEMENTACIÓN SECUENCIAL DEL MÉTODO DE JACOBI
 *  PARA LA ECUACIÓN DE POISSON 1D
 * ============================================================================
 *
 *  Este programa resuelve numéricamente la ecuación de Poisson en 1D:
 *
 *      -u''(x) = f(x),   x ∈ (0,1)
 *      u(0) = u(1) = 0
 *
 *  utilizando el método iterativo de Jacobi sobre una malla uniforme.
 *
 *  La implementación corresponde a la versión secuencial (baseline),
 *  la cual se utiliza como referencia para comparar con versiones paralelas
 *  (pthreads y fork).
 *
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "poisson.h"

/* ---------------- CONFIGURACIÓN POR DEFECTO ---------------- */

/* Número de nodos interiores */
#define DEFAULT_N     2000

/* Número máximo de iteraciones */
#define DEFAULT_ITERS 5000

/* Tolerancia para criterio de convergencia (norma RMS) */
#define TOLERANCE     1e-6


int main(int argc, char *argv[]) {

    /* ------------------------------------------------------------------------
     * PARÁMETROS DE ENTRADA
     * ------------------------------------------------------------------------
     * Se permiten argumentos por línea de comandos:
     *   argv[1] → número de nodos interiores (n)
     *   argv[2] → número máximo de iteraciones
     * Si no se proporcionan, se usan valores por defecto.
     */
    int n      = (argc > 1) ? atoi(argv[1]) : DEFAULT_N;
    int max_it = (argc > 2) ? atoi(argv[2]) : DEFAULT_ITERS;

    /* Paso de discretización: h = 1/(n+1) */
    double h = 1.0 / (n + 1);


    /* ------------------------------------------------------------------------
     * RESERVA DE MEMORIA
     * ------------------------------------------------------------------------
     * Se utilizan tres arreglos:
     *   u      → solución actual (iteración k)
     *   u_new  → solución siguiente (iteración k+1)
     *   f      → término fuente
     *
     * Se asignan n+2 posiciones para incluir nodos frontera:
     *   índice 0     → frontera izquierda
     *   índice n+1   → frontera derecha
     */
    double *u     = calloc((size_t)(n + 2), sizeof(double));
    double *u_new = calloc((size_t)(n + 2), sizeof(double));
    double *f     = malloc((size_t)(n + 2) * sizeof(double));


    /* ------------------------------------------------------------------------
     * INICIALIZACIÓN DEL PROBLEMA
     * ------------------------------------------------------------------------
     */
    initialize_grid(u, n);     /* Aplica condiciones de frontera */
    compute_rhs(f, n, h);      /* Calcula el término fuente f(x) */


    /* ------------------------------------------------------------------------
     * MEDICIÓN DE TIEMPO (wall-clock time)
     * ------------------------------------------------------------------------
     */
    double t_start = wall_time();


    /* ------------------------------------------------------------------------
     * BUCLE PRINCIPAL DEL MÉTODO DE JACOBI
     * ------------------------------------------------------------------------
     *
     * En cada iteración:
     *   1. Se calcula u_new usando valores de u (iteración anterior)
     *   2. Se evalúa el residual RMS para verificar convergencia
     *   3. Se intercambian punteros (double buffering)
     */
    int iter = 0;

    for (; iter < max_it; iter++) {

        /* ---- Paso de actualización de Jacobi ---- */
        for (int i = 1; i <= n; i++) {
            u_new[i] = 0.5 * (u[i - 1] + u[i + 1] + h * h * f[i]);
        }

        /* ---- Evaluación del residual RMS ---- */
        double diff = rms_residual(u_new, f, n, h);

        /* ---- Intercambio de buffers (O(1)) ---- */
        double *tmp = u;
        u = u_new;
        u_new = tmp;

        /* ---- Criterio de convergencia ---- */
        if (diff < TOLERANCE) break;
    }


    /* ------------------------------------------------------------------------
     * CÁLCULO DEL TIEMPO TOTAL
     * ------------------------------------------------------------------------
     */
    double elapsed_ms = (wall_time() - t_start) * 1000.0;


    /* ------------------------------------------------------------------------
     * SALIDA DE RESULTADOS
     * ------------------------------------------------------------------------
     *
     * stderr → información detallada (para logs/debug)
     * stdout → solo tiempo (útil para scripts de benchmarking)
     */
    fprintf(stderr,
        "serial n=%-6d iters=%-6d error=%.4e  time=%.3f ms\n",
        n, iter + 1, max_error(u, n, h), elapsed_ms
    );

    printf("%.3f\n", elapsed_ms);


    /* ------------------------------------------------------------------------
     * LIBERACIÓN DE MEMORIA
     * ------------------------------------------------------------------------
     */
    free(u);
    free(u_new);
    free(f);

    return 0;
}
