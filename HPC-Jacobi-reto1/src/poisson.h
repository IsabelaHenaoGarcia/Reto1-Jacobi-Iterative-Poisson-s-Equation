#ifndef POISSON_H
#define POISSON_H

/**
 * @file poisson.h
 * @brief Funciones para la resolución numérica de la ecuación de Poisson 1D.
 * 
 * Este módulo proporciona utilidades para resolver el problema de Poisson
 * unidimensional -u'' = f con condiciones de contorno homogéneas,
 * utilizando el método de diferencias finitas.
 */

/**
 * Obtiene el tiempo real transcurrido en segundos utilizando el reloj monótono.
 * 
 * @return Tiempo en segundos desde un punto de referencia arbitrario.
 */
double obtener_tiempo_muro(void);

/**
 * Inicializa todos los elementos de la malla con cero, incluyendo los nodos de frontera.
 * La malla tiene tamaño (num_pts + 2) para incluir los puntos fantasma en los extremos.
 * 
 * @param malla           Arreglo que representa la malla de puntos.
 * @param cantidad_puntos Número de puntos interiores (excluyendo fronteras).
 */
void inicializar_malla(double *malla, int cantidad_puntos);

/**
 * Calcula el término fuente f(x) = -x·(x+3)·e^x en los puntos interiores de la malla.
 * Esta función corresponde a la solución exacta u(x) = x·(x-1)·e^x.
 * 
 * @param termino_fuente  Arreglo donde se almacenará el término fuente.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 */
void calcular_termino_fuente(double *termino_fuente, int cantidad_puntos, double paso_espaciado);

/**
 * Calcula el error máximo en norma infinito entre la solución numérica
 * y la solución exacta conocida u(x) = x·(x-1)·e^x.
 * 
 * @param solucion        Arreglo con la solución numérica.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 * @return El valor máximo del error absoluto en los puntos interiores.
 */
double error_maximo(const double *solucion, int cantidad_puntos, double paso_espaciado);

/**
 * Calcula la norma RMS (root mean square) del residuo de la solución numérica.
 * El residuo se define como: r_i = (-u_{i-1} + 2u_i - u_{i+1}) / h² - f_i.
 * 
 * Este criterio de convergencia es recomendado por Burkardt (2011), sección 9.
 * 
 * @param solucion        Arreglo con la solución numérica.
 * @param termino_fuente  Arreglo con el término fuente evaluado.
 * @param cantidad_puntos Número de puntos interiores.
 * @param paso_espaciado  Distancia entre puntos consecutivos (h).
 * @return La raíz del error cuadrático medio del residuo.
 */
double residuo_rms(const double *solucion, const double *termino_fuente, int cantidad_puntos, double paso_espaciado);

#endif /* POISSON_H */