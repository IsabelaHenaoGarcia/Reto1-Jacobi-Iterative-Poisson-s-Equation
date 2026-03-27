#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "poisson.h"

#define TAMANO_DEFECTO     2000
#define ITERACIONES_DEFECTO 5000
#define TOLERANCIA         1e-6
#define LINEA_CACHE        64

/*
 * Asigna un arreglo de doubles alineado a LINEA_CACHE bytes.
 * La función malloc estándar solo garantiza alineación de 8 bytes, por lo que
 * el primer elemento de un arreglo puede quedar dividido entre dos líneas de caché,
 * haciendo que cada acceso a ese elemento toque dos líneas en lugar de una.
 * Alinear a 64 bytes elimina esa división y asegura que el prefetcher de hardware
 * trabaje sobre líneas completas.
 *
 * @param cantidad_puntos Número de puntos interiores en la malla.
 * @return Puntero al arreglo alineado, inicializado con ceros.
 */
static double *asignar_alineado(int cantidad_puntos) {
    size_t bytes_totales = (size_t)(cantidad_puntos + 2) * sizeof(double);
    void  *memoria       = aligned_alloc(LINEA_CACHE, bytes_totales);
    if (!memoria) {
        perror("aligned_alloc");
        exit(1);
    }
    memset(memoria, 0, bytes_totales);
    return (double *)memoria;
}

int main(int argc, char *argv[]) {
    int cantidad_puntos = (argc > 1) ? atoi(argv[1]) : TAMANO_DEFECTO;
    int max_iteraciones = (argc > 2) ? atoi(argv[2]) : ITERACIONES_DEFECTO;
    double paso         = 1.0 / (cantidad_puntos + 1);
    double paso_cuadrado = paso * paso;

    double *solucion     = asignar_alineado(cantidad_puntos);
    double *solucion_nueva = asignar_alineado(cantidad_puntos);
    double *termino_fuente = asignar_alineado(cantidad_puntos);

    calcular_termino_fuente(termino_fuente, cantidad_puntos, paso);

    double tiempo_inicio = obtener_tiempo_muro();

    int iteracion = 0;
    for (; iteracion < max_iteraciones; iteracion++) {
        /* Actualiza todos los puntos interiores usando el método de Jacobi */
        for (int indice = 1; indice <= cantidad_puntos; indice++) {
            solucion_nueva[indice] = 0.5 * (solucion[indice - 1] + solucion[indice + 1]
                                            + paso_cuadrado * termino_fuente[indice]);
        }

        /* Calcula el residuo RMS para verificar convergencia */
        double residuo = residuo_rms(solucion_nueva, termino_fuente, cantidad_puntos, paso);

        /* Intercambia los buffers para la siguiente iteración */
        double *temporal = solucion;
        solucion = solucion_nueva;
        solucion_nueva = temporal;

        if (residuo < TOLERANCIA) break;
    }

    double tiempo_transcurrido_ms = (obtener_tiempo_muro() - tiempo_inicio) * 1000.0;

    fprintf(stderr, "serial_cache n=%-6d iter=%-6d error=%.4e  tiempo=%.3f ms\n",
            cantidad_puntos, iteracion + 1,
            error_maximo(solucion, cantidad_puntos, paso), tiempo_transcurrido_ms);
    printf("%.3f\n", tiempo_transcurrido_ms);

    free(solucion);
    free(solucion_nueva);
    free(termino_fuente);
    return 0;
}