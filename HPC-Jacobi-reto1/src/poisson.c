#include "poisson.h"
#include <math.h>
#include <string.h>
#include <time.h>

/**
 * Obtiene el tiempo real transcurrido en segundos utilizando el reloj monótono.
 * El reloj monótono no se ve afectado por ajustes del sistema, ideal para medir
 * intervalos de tiempo y rendimiento.
 * 
 * @return Tiempo en segundos desde un punto de referencia arbitrario.
 */
double obtener_tiempo_muro(void) {
    struct timespec marca_temporal;
    clock_gettime(CLOCK_MONOTONIC, &marca_temporal);
    return marca_temporal.tv_sec + marca_temporal.tv_nsec * 1e-9;
}

/**
 * Inicializa todos los elementos de la malla con cero, incluyendo los nodos de frontera.
 * La malla tiene tamaño (cantidad_puntos + 2) para incluir los puntos fantasma
 * en los extremos [0] y [cantidad_puntos+1] que facilitan la aplicación de
 * condiciones de contorno.
 * 
 * @param malla          Arreglo que representa la malla de puntos.
 * @param cantidad_puntos Número de puntos interiores (excluyendo fronteras).
 */
void inicializar_malla(double *malla, int cantidad_puntos) {
    memset(malla, 0, (size_t)(cantidad_puntos + 2) * sizeof(double));
}

/**
 * Calcula el término fuente f(x) para la ecuación -u'' = f.
 * Se evalúa en cada punto interior de la malla usando la expresión:
 * f(x) = -x·(x+3)·e^x
 * 
 * Esta función corresponde a la solución exacta u(x) = x·(x-1)·e^x.
 * 
 * @param termino_fuente  Arreglo donde se almacenará el término fuente.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 */
void calcular_termino_fuente(double *termino_fuente, int cantidad_puntos, double paso_espaciado) {
    for (int indice = 1; indice <= cantidad_puntos; indice++) {
        double valor_x = indice * paso_espaciado;  // Coordenada x en el punto actual
        termino_fuente[indice] = -valor_x * (valor_x + 3.0) * exp(valor_x);
    }
}

/**
 * Calcula el error máximo en norma infinito entre la solución numérica
 * y la solución exacta conocida u(x) = x·(x-1)·e^x.
 * 
 * El error máximo se define como: max|u_numérica(x) - u_exacta(x)|
 * 
 * @param solucion        Arreglo con la solución numérica.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 * @return El valor máximo del error absoluto en los puntos interiores.
 */
double error_maximo(const double *solucion, int cantidad_puntos, double paso_espaciado) {
    double valor_maximo = 0.0;
    
    for (int indice = 1; indice <= cantidad_puntos; indice++) {
        double valor_x = indice * paso_espaciado;
        double diferencia = fabs(solucion[indice] - valor_x * (valor_x - 1.0) * exp(valor_x));
        
        if (diferencia > valor_maximo) {
            valor_maximo = diferencia;  // Actualiza el máximo encontrado
        }
    }
    
    return valor_maximo;
}

/**
 * Calcula la norma RMS (root mean square) del residuo de la solución numérica.
 * 
 * El residuo se define como: r_i = (-u_{i-1} + 2u_i - u_{i+1}) / h² - f_i
 * donde h es el paso de espaciado.
 * 
 * Este criterio de convergencia es recomendado por Burkardt (2011), sección 9.
 * Proporciona una medida global de qué tan bien la solución satisface
 * la ecuación discretizada.
 * 
 * @param solucion        Arreglo con la solución numérica.
 * @param termino_fuente  Arreglo con el término fuente evaluado.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 * @return La raíz del error cuadrático medio del residuo.
 */
double residuo_rms(const double *solucion, const double *termino_fuente, int cantidad_puntos, double paso_espaciado) {
    double paso_cuadrado = paso_espaciado * paso_espaciado;  // h²
    double suma_acumulada = 0.0;
    
    for (int indice = 1; indice <= cantidad_puntos; indice++) {
        // Aproximación de la segunda derivada mediante diferencias finitas centradas
        double residuo = (-solucion[indice - 1] + 2.0 * solucion[indice] - solucion[indice + 1]) 
                         / paso_cuadrado - termino_fuente[indice];
        
        suma_acumulada += residuo * residuo;  // Acumula el cuadrado del residuo
    }
    
    // RMS = sqrt( (1/N) * Σ r_i² )
    return sqrt(suma_acumulada / cantidad_puntos);
}