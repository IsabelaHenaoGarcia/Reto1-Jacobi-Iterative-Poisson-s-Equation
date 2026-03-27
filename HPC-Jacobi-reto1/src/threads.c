#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "poisson.h"

#define TAMANO_DEFECTO       2000
#define ITERACIONES_DEFECTO  5000
#define HILOS_DEFECTO        4
#define TOLERANCIA           1e-6

/* Variables globales compartidas entre todos los hilos */
static double           *solucion_compartida;
static double           *solucion_nueva_compartida;
static const double     *termino_fuente_compartido;
static double            paso_compartido;
static int               puntos_compartidos;
static int               hilos_compartidos;
static double            residuo_compartido;
static int               terminado_compartido;
static int               pasos_compartidos;
static pthread_barrier_t barrera_sincronizacion;

typedef struct {
    int identificador;
    int inicio_fila;
    int fin_fila;
    int max_iteraciones;
} ArgumentosTrabajador;

/**
 * Función ejecutada por cada hilo trabajador.
 * Implementa el método iterativo de Jacobi para resolver la ecuación de Poisson.
 * 
 * @param raw Puntero a los argumentos del trabajador.
 * @return NULL (siempre).
 */
static void *funcion_trabajador(void *raw) {
    ArgumentosTrabajador *args = (ArgumentosTrabajador *)raw;

    for (int iteracion = 0; iteracion < args->max_iteraciones; iteracion++) {
        /* Actualiza los puntos asignados a este hilo usando el método de Jacobi */
        for (int indice = args->inicio_fila; indice <= args->fin_fila; indice++) {
            solucion_nueva_compartida[indice] = 0.5 * (solucion_compartida[indice - 1] 
                                                     + solucion_compartida[indice + 1]
                                                     + paso_compartido * paso_compartido
                                                     * termino_fuente_compartido[indice]);
        }

        /* Espera a que todos los hilos terminen de escribir solucion_nueva_compartida
         * antes de que el hilo 0 calcule el residuo, que necesita leer valores
         * de los vecinos a través de los límites entre segmentos. */
        pthread_barrier_wait(&barrera_sincronizacion);

        if (args->identificador == 0) {
            /* Hilo coordinador: calcula el residuo global y verifica convergencia */
            residuo_compartido = residuo_rms(solucion_nueva_compartida, 
                                            termino_fuente_compartido,
                                            puntos_compartidos, 
                                            paso_compartido);
            terminado_compartido = (residuo_compartido < TOLERANCIA);
            pasos_compartidos = iteracion + 1;

            /* Intercambia los buffers para la siguiente iteración */
            double *temporal = solucion_compartida;
            solucion_compartida = solucion_nueva_compartida;
            solucion_nueva_compartida = temporal;
        }

        /* Espera a que el hilo coordinador termine de actualizar las variables
         * globales y los buffers antes de continuar con la siguiente iteración */
        pthread_barrier_wait(&barrera_sincronizacion);

        if (terminado_compartido) break;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int cantidad_puntos = (argc > 1) ? atoi(argv[1]) : TAMANO_DEFECTO;
    int max_iteraciones = (argc > 2) ? atoi(argv[2]) : ITERACIONES_DEFECTO;
    int cantidad_hilos  = (argc > 3) ? atoi(argv[3]) : HILOS_DEFECTO;
    double paso         = 1.0 / (cantidad_puntos + 1);

    double *solucion     = calloc((size_t)(cantidad_puntos + 2), sizeof(double));
    double *solucion_nueva = calloc((size_t)(cantidad_puntos + 2), sizeof(double));
    double *termino_fuente = malloc((size_t)(cantidad_puntos + 2) * sizeof(double));

    inicializar_malla(solucion, cantidad_puntos);
    calcular_termino_fuente(termino_fuente, cantidad_puntos, paso);

    /* Inicializa las variables globales compartidas entre hilos */
    solucion_compartida        = solucion;
    solucion_nueva_compartida  = solucion_nueva;
    termino_fuente_compartido  = termino_fuente;
    paso_compartido            = paso;
    puntos_compartidos         = cantidad_puntos;
    hilos_compartidos          = cantidad_hilos;
    terminado_compartido       = 0;
    pasos_compartidos          = 0;

    /* Inicializa la barrera para sincronizar los hilos */
    pthread_barrier_init(&barrera_sincronizacion, NULL, (unsigned)cantidad_hilos);

    ArgumentosTrabajador *args_trabajador = malloc((size_t)cantidad_hilos * sizeof(ArgumentosTrabajador));
    pthread_t  *identificadores_hilos  = malloc((size_t)cantidad_hilos * sizeof(pthread_t));

    /* Divide el trabajo entre los hilos */
    int segmento = cantidad_puntos / cantidad_hilos;
    for (int h = 0; h < cantidad_hilos; h++) {
        args_trabajador[h].identificador  = h;
        args_trabajador[h].inicio_fila    = h * segmento + 1;
        args_trabajador[h].fin_fila       = (h == cantidad_hilos - 1) ? cantidad_puntos : (h + 1) * segmento;
        args_trabajador[h].max_iteraciones = max_iteraciones;
    }

    double tiempo_inicio = obtener_tiempo_muro();

    /* Crea y lanza todos los hilos */
    for (int h = 0; h < cantidad_hilos; h++) {
        pthread_create(&identificadores_hilos[h], NULL, funcion_trabajador, &args_trabajador[h]);
    }
    
    /* Espera a que todos los hilos finalicen */
    for (int h = 0; h < cantidad_hilos; h++) {
        pthread_join(identificadores_hilos[h], NULL);
    }

    double tiempo_transcurrido_ms = (obtener_tiempo_muro() - tiempo_inicio) * 1000.0;

    fprintf(stderr, "hilos n=%-6d iter=%-6d t=%-2d  error=%.4e  tiempo=%.3f ms\n",
            cantidad_puntos, pasos_compartidos, cantidad_hilos,
            error_maximo(solucion_compartida, cantidad_puntos, paso), tiempo_transcurrido_ms);
    printf("%.3f\n", tiempo_transcurrido_ms);

    pthread_barrier_destroy(&barrera_sincronizacion);
    free(args_trabajador);
    free(identificadores_hilos);
    free(solucion);
    free(solucion_nueva);
    free(termino_fuente);
    return 0;
}