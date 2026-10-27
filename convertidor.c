/*
 * Convertidor de expresiones matemáticas de notación infija a prefija.
 *
 * Estrategia:
 *   1. Tokenización de la línea de entrada (números, identificadores,
 *      operadores, paréntesis y un token final).
 *   2. Análisis sintáctico descendente recursivo con precedencia y
 *      asociatividad correctas. Se construye un árbol de expresión.
 *   3. Recorrido en preorden del árbol para obtener la notación prefija.
 *
 * Operadores admitidos: +, -, *, /, %, ^ (potencia, asociativa a derecha).
 * También se admiten operadores unarios + y -.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_MENSAJE_ERROR 256

/* ------------------------------------------------------------------------- */
/* Tipos de datos                                                            */
/* ------------------------------------------------------------------------- */

typedef enum {
    TOKEN_NUMERO,
    TOKEN_IDENTIFICADOR,
    TOKEN_OPERADOR,
    TOKEN_PARENTESIS_IZQ,
    TOKEN_PARENTESIS_DER,
    TOKEN_FIN
} TipoToken;

typedef struct {
    TipoToken tipo;
    char *lexema;
    size_t posicion; /* Posición del primer carácter, comenzando en 1. */
} Token;

typedef struct {
    Token *elementos;
    size_t cantidad;
    size_t capacidad;
} ListaTokens;

typedef struct NodoExpresion {
    char *texto;                    /* Operando u operador. */
    struct NodoExpresion *izquierdo;
    struct NodoExpresion *derecho;
    int aridad;                     /* 0 = operando, 1 = unario, 2 = binario. */
} NodoExpresion;

typedef struct {
    const ListaTokens *tokens;
    size_t indice;
    char *mensaje_error;
    size_t tam_mensaje_error;
    bool hubo_error;
} Analizador;

/* ------------------------------------------------------------------------- */
/* Utilidades generales                                                      */
/* ------------------------------------------------------------------------- */

static char *duplicar_cadena(const char *texto)
{
    size_t longitud = strlen(texto);
    char *copia = malloc(longitud + 1);

    if (copia == NULL) {
        return NULL;
    }

    memcpy(copia, texto, longitud + 1);
    return copia;
}

/* ------------------------------------------------------------------------- */
/* Gestión de la lista de tokens                                             */
/* ------------------------------------------------------------------------- */

static void inicializar_lista_tokens(ListaTokens *lista)
{
    lista->elementos = NULL;
    lista->cantidad = 0;
    lista->capacidad = 0;
}

static void liberar_lista_tokens(ListaTokens *lista)
{
    size_t i;

    if (lista->elementos != NULL) {
        for (i = 0; i < lista->cantidad; i++) {
            free(lista->elementos[i].lexema);
        }
        free(lista->elementos);
    }

    inicializar_lista_tokens(lista);
}

static bool agregar_token(ListaTokens *lista, TipoToken tipo,
                          const char *inicio, size_t longitud, size_t posicion)
{
    Token nuevo_token;
    char *lexema;

    if (lista->cantidad == lista->capacidad) {
        size_t nueva_capacidad = (lista->capacidad == 0) ? 16 : lista->capacidad * 2;
        Token *nuevos_elementos = realloc(lista->elementos,
                                          nueva_capacidad * sizeof(*nuevos_elementos));

        if (nuevos_elementos == NULL) {
            return false;
        }

        lista->elementos = nuevos_elementos;
        lista->capacidad = nueva_capacidad;
    }

    lexema = malloc(longitud + 1);
    if (lexema == NULL) {
        return false;
    }

    if (longitud > 0) {
        memcpy(lexema, inicio, longitud);
    }
    lexema[longitud] = '\0';

    nuevo_token.tipo = tipo;
    nuevo_token.lexema = lexema;
    nuevo_token.posicion = posicion;

    lista->elementos[lista->cantidad] = nuevo_token;
    lista->cantidad++;

    return true;
}

/* ------------------------------------------------------------------------- */
/* Tokenización                                                              */
/* ------------------------------------------------------------------------- */

static bool tokenizar(const char *expresion, ListaTokens *tokens,
                      char *mensaje_error, size_t tam_mensaje_error)
{
    size_t i = 0;

    while (expresion[i] != '\0') {
        unsigned char caracter_actual = (unsigned char) expresion[i];
        size_t inicio;

        if (isspace(caracter_actual)) {
            i++;
            continue;
        }

        inicio = i;

        /* Número entero o decimal: 12, 3.14, .5, 7. */
        if (isdigit(caracter_actual) ||
            (caracter_actual == '.' &&
             isdigit((unsigned char) expresion[i + 1]))) {
            bool punto_decimal_encontrado = false;

            while (isdigit((unsigned char) expresion[i]) ||
                   (!punto_decimal_encontrado && expresion[i] == '.')) {
                if (expresion[i] == '.') {
                    punto_decimal_encontrado = true;
                }
                i++;
            }

            if (!agregar_token(tokens, TOKEN_NUMERO, expresion + inicio,
                               i - inicio, inicio + 1)) {
                goto error_memoria;
            }
            continue;
        }

        /* Identificador: letra o guion bajo, seguido de alfanuméricos. */
        if (isalpha(caracter_actual) || caracter_actual == '_') {
            i++;

            while (isalnum((unsigned char) expresion[i]) ||
                   expresion[i] == '_') {
                i++;
            }

            if (!agregar_token(tokens, TOKEN_IDENTIFICADOR, expresion + inicio,
                               i - inicio, inicio + 1)) {
                goto error_memoria;
            }
            continue;
        }

        /* Operadores aritméticos de un solo carácter. */
        if (strchr("+-*/%^", caracter_actual) != NULL) {
            i++;

            if (!agregar_token(tokens, TOKEN_OPERADOR, expresion + inicio,
                               i - inicio, inicio + 1)) {
                goto error_memoria;
            }
            continue;
        }

        if (caracter_actual == '(') {
            i++;

            if (!agregar_token(tokens, TOKEN_PARENTESIS_IZQ, expresion + inicio,
                               i - inicio, inicio + 1)) {
                goto error_memoria;
            }
            continue;
        }

        if (caracter_actual == ')') {
            i++;

            if (!agregar_token(tokens, TOKEN_PARENTESIS_DER, expresion + inicio,
                               i - inicio, inicio + 1)) {
                goto error_memoria;
            }
            continue;
        }

        snprintf(mensaje_error, tam_mensaje_error,
                 "Carácter no válido '%c' en la posición %zu.",
                 caracter_actual, i + 1);
        liberar_lista_tokens(tokens);
        return false;
    }

    if (!agregar_token(tokens, TOKEN_FIN, "", 0, i + 1)) {
        goto error_memoria;
    }

    return true;

error_memoria:
    snprintf(mensaje_error, tam_mensaje_error,
             "No hay memoria suficiente para tokenizar la expresión.");
    liberar_lista_tokens(tokens);
    return false;
}

/* ------------------------------------------------------------------------- */
/* Árbol de expresión                                                        */
/* ------------------------------------------------------------------------- */

static void liberar_nodo(NodoExpresion *nodo)
{
    if (nodo == NULL) {
        return;
    }

    liberar_nodo(nodo->izquierdo);
    liberar_nodo(nodo->derecho);
    free(nodo->texto);
    free(nodo);
}

static NodoExpresion *crear_nodo(const char *texto,
                                 NodoExpresion *izquierdo,
                                 NodoExpresion *derecho,
                                 int aridad)
{
    NodoExpresion *nodo = malloc(sizeof(*nodo));

    if (nodo == NULL) {
        liberar_nodo(izquierdo);
        liberar_nodo(derecho);
        return NULL;
    }

    nodo->texto = duplicar_cadena(texto);
    if (nodo->texto == NULL) {
        free(nodo);
        liberar_nodo(izquierdo);
        liberar_nodo(derecho);
        return NULL;
    }

    nodo->izquierdo = izquierdo;
    nodo->derecho = derecho;
    nodo->aridad = aridad;

    return nodo;
}

/* ------------------------------------------------------------------------- */
/* Análisis sintáctico                                                       */
/* ------------------------------------------------------------------------- */

static const Token *token_actual(const Analizador *analizador)
{
    return &analizador->tokens->elementos[analizador->indice];
}

static void avanzar_token(Analizador *analizador)
{
    if (token_actual(analizador)->tipo != TOKEN_FIN) {
        analizador->indice++;
    }
}

static bool es_operador(const Token *token, const char *operadores)
{
    return token->tipo == TOKEN_OPERADOR &&
           token->lexema[0] != '\0' &&
           token->lexema[1] == '\0' &&
           strchr(operadores, token->lexema[0]) != NULL;
}

static void registrar_error(Analizador *analizador, const char *descripcion,
                            const Token *token)
{
    if (analizador->hubo_error) {
        return;
    }

    if (token->tipo == TOKEN_FIN) {
        snprintf(analizador->mensaje_error, analizador->tam_mensaje_error,
                 "%s al final de la expresión.", descripcion);
    } else {
        snprintf(analizador->mensaje_error, analizador->tam_mensaje_error,
                 "%s: '%s' (posición %zu).",
                 descripcion, token->lexema, token->posicion);
    }

    analizador->hubo_error = true;
}

static void registrar_error_memoria(Analizador *analizador)
{
    if (analizador->hubo_error) {
        return;
    }

    snprintf(analizador->mensaje_error, analizador->tam_mensaje_error,
             "No hay memoria suficiente para construir el árbol de expresión.");
    analizador->hubo_error = true;
}

static NodoExpresion *analizar_expresion(Analizador *analizador);
static NodoExpresion *analizar_producto(Analizador *analizador);
static NodoExpresion *analizar_unario(Analizador *analizador);

static NodoExpresion *analizar_primario(Analizador *analizador)
{
    const Token *token = token_actual(analizador);

    if (token->tipo == TOKEN_NUMERO ||
        token->tipo == TOKEN_IDENTIFICADOR) {
        NodoExpresion *operando = crear_nodo(token->lexema, NULL, NULL, 0);

        if (operando == NULL) {
            registrar_error_memoria(analizador);
            return NULL;
        }

        avanzar_token(analizador);
        return operando;
    }

    if (token->tipo == TOKEN_PARENTESIS_IZQ) {
        NodoExpresion *interior;

        avanzar_token(analizador);
        interior = analizar_expresion(analizador);

        if (interior == NULL) {
            return NULL;
        }

        if (token_actual(analizador)->tipo != TOKEN_PARENTESIS_DER) {
            registrar_error(analizador, "Se esperaba ')'", token_actual(analizador));
            liberar_nodo(interior);
            return NULL;
        }

        avanzar_token(analizador);
        return interior;
    }

    registrar_error(analizador, "Se esperaba un operando", token);
    return NULL;
}

/* Potencia: asociativa por la derecha y con mayor precedencia que el unario. */
static NodoExpresion *analizar_potencia(Analizador *analizador)
{
    NodoExpresion *izquierdo = analizar_primario(analizador);

    if (izquierdo == NULL) {
        return NULL;
    }

    if (es_operador(token_actual(analizador), "^")) {
        const Token *operador = token_actual(analizador);
        NodoExpresion *derecho;
        NodoExpresion *nuevo_nodo;

        avanzar_token(analizador);
        derecho = analizar_unario(analizador);

        if (derecho == NULL) {
            liberar_nodo(izquierdo);
            return NULL;
        }

        nuevo_nodo = crear_nodo(operador->lexema, izquierdo, derecho, 2);
        if (nuevo_nodo == NULL) {
            registrar_error_memoria(analizador);
            return NULL;
        }

        return nuevo_nodo;
    }

    return izquierdo;
}

static NodoExpresion *analizar_unario(Analizador *analizador)
{
    if (es_operador(token_actual(analizador), "+-")) {
        const Token *operador = token_actual(analizador);
        NodoExpresion *operando;
        NodoExpresion *nuevo_nodo;

        avanzar_token(analizador);
        operando = analizar_unario(analizador);

        if (operando == NULL) {
            return NULL;
        }

        nuevo_nodo = crear_nodo(operador->lexema, NULL, operando, 1);
        if (nuevo_nodo == NULL) {
            registrar_error_memoria(analizador);
            return NULL;
        }

        return nuevo_nodo;
    }

    return analizar_potencia(analizador);
}

static NodoExpresion *analizar_producto(Analizador *analizador)
{
    NodoExpresion *izquierdo = analizar_unario(analizador);

    if (izquierdo == NULL) {
        return NULL;
    }

    while (es_operador(token_actual(analizador), "*/%")) {
        const Token *operador = token_actual(analizador);
        NodoExpresion *derecho;
        NodoExpresion *nuevo_nodo;

        avanzar_token(analizador);
        derecho = analizar_unario(analizador);

        if (derecho == NULL) {
            liberar_nodo(izquierdo);
            return NULL;
        }

        nuevo_nodo = crear_nodo(operador->lexema, izquierdo, derecho, 2);
        if (nuevo_nodo == NULL) {
            registrar_error_memoria(analizador);
            return NULL;
        }

        izquierdo = nuevo_nodo;
    }

    return izquierdo;
}

static NodoExpresion *analizar_expresion(Analizador *analizador)
{
    NodoExpresion *izquierdo = analizar_producto(analizador);

    if (izquierdo == NULL) {
        return NULL;
    }

    while (es_operador(token_actual(analizador), "+-")) {
        const Token *operador = token_actual(analizador);
        NodoExpresion *derecho;
        NodoExpresion *nuevo_nodo;

        avanzar_token(analizador);
        derecho = analizar_producto(analizador);

        if (derecho == NULL) {
            liberar_nodo(izquierdo);
            return NULL;
        }

        nuevo_nodo = crear_nodo(operador->lexema, izquierdo, derecho, 2);
        if (nuevo_nodo == NULL) {
            registrar_error_memoria(analizador);
            return NULL;
        }

        izquierdo = nuevo_nodo;
    }

    return izquierdo;
}

static NodoExpresion *analizar_tokens(const ListaTokens *tokens,
                                      char *mensaje_error,
                                      size_t tam_mensaje_error)
{
    Analizador analizador;
    NodoExpresion *raiz;

    analizador.tokens = tokens;
    analizador.indice = 0;
    analizador.mensaje_error = mensaje_error;
    analizador.tam_mensaje_error = tam_mensaje_error;
    analizador.hubo_error = false;
    mensaje_error[0] = '\0';

    raiz = analizar_expresion(&analizador);

    if (raiz == NULL) {
        if (!analizador.hubo_error) {
            snprintf(mensaje_error, tam_mensaje_error,
                     "No se pudo analizar la expresión.");
        }
        return NULL;
    }

    if (token_actual(&analizador)->tipo != TOKEN_FIN) {
        registrar_error(&analizador, "Token inesperado después de la expresión",
                        token_actual(&analizador));
        liberar_nodo(raiz);
        return NULL;
    }

    return raiz;
}

/* ------------------------------------------------------------------------- */
/* Salida en notación prefija                                                */
/* ------------------------------------------------------------------------- */

static void imprimir_prefija(const NodoExpresion *nodo, FILE *salida)
{
    if (nodo == NULL) {
        return;
    }

    if (nodo->aridad == 0) {
        fputs(nodo->texto, salida);
        return;
    }

    fputs(nodo->texto, salida);

    if (nodo->izquierdo != NULL) {
        fputc(' ', salida);
        imprimir_prefija(nodo->izquierdo, salida);
    }

    fputc(' ', salida);
    imprimir_prefija(nodo->derecho, salida);
}

/* ------------------------------------------------------------------------- */
/* Conversión completa de una expresión                                      */
/* ------------------------------------------------------------------------- */

static bool convertir_y_escribir(const char *expresion, FILE *salida,
                                 char *mensaje_error,
                                 size_t tam_mensaje_error)
{
    ListaTokens tokens;
    NodoExpresion *raiz;

    inicializar_lista_tokens(&tokens);

    if (!tokenizar(expresion, &tokens, mensaje_error, tam_mensaje_error)) {
        return false;
    }

    if (tokens.cantidad == 1) {
        snprintf(mensaje_error, tam_mensaje_error,
                 "La expresión está vacía.");
        liberar_lista_tokens(&tokens);
        return false;
    }

    raiz = analizar_tokens(&tokens, mensaje_error, tam_mensaje_error);
    if (raiz == NULL) {
        liberar_lista_tokens(&tokens);
        return false;
    }

    imprimir_prefija(raiz, salida);
    fputc('\n', salida);

    liberar_nodo(raiz);
    liberar_lista_tokens(&tokens);

    return true;
}

/* ------------------------------------------------------------------------- */
/* Lectura de líneas y archivos                                              */
/* ------------------------------------------------------------------------- */

static char *leer_linea(FILE *archivo, bool *fallo_memoria)
{
    size_t capacidad = 128;
    size_t longitud = 0;
    char *linea = malloc(capacidad);
    int caracter;

    *fallo_memoria = false;

    if (linea == NULL) {
        *fallo_memoria = true;
        return NULL;
    }

    while ((caracter = fgetc(archivo)) != EOF && caracter != '\n') {
        if (longitud + 1 >= capacidad) {
            size_t nueva_capacidad = capacidad * 2;
            char *nueva_linea = realloc(linea, nueva_capacidad);

            if (nueva_linea == NULL) {
                free(linea);
                *fallo_memoria = true;
                return NULL;
            }

            linea = nueva_linea;
            capacidad = nueva_capacidad;
        }

        linea[longitud] = (char) caracter;
        longitud++;
    }

    if (caracter == EOF && longitud == 0) {
        free(linea);
        return NULL;
    }

    /* Compatibilidad con archivos de texto con finales de línea CRLF. */
    if (longitud > 0 && linea[longitud - 1] == '\r') {
        longitud--;
    }

    linea[longitud] = '\0';
    return linea;
}

static bool es_linea_vacia(const char *linea)
{
    size_t i;

    for (i = 0; linea[i] != '\0'; i++) {
        if (!isspace((unsigned char) linea[i])) {
            return false;
        }
    }

    return true;
}

static int procesar_archivo(const char *ruta)
{
    FILE *archivo = fopen(ruta, "r");
    char mensaje_error[TAM_MENSAJE_ERROR];
    char *linea;
    size_t numero_linea = 0;
    size_t conversiones_correctas = 0;
    size_t lineas_con_error = 0;
    bool fallo_memoria = false;
    int estado = EXIT_SUCCESS;

    if (archivo == NULL) {
        fprintf(stderr, "No se pudo abrir el archivo '%s': %s\n",
                ruta, strerror(errno));
        return EXIT_FAILURE;
    }

    printf("Archivo: %s\n\n", ruta);

    while ((linea = leer_linea(archivo, &fallo_memoria)) != NULL) {
        numero_linea++;

        if (!es_linea_vacia(linea)) {
            printf("Línea %zu: %s\n", numero_linea, linea);
            printf("Prefija : ");

            if (convertir_y_escribir(linea, stdout, mensaje_error,
                                     sizeof(mensaje_error))) {
                conversiones_correctas++;
            } else {
                printf("\nError   : %s\n", mensaje_error);
                lineas_con_error++;
            }

            printf("\n");
        }

        free(linea);
    }

    if (fallo_memoria) {
        fprintf(stderr, "No hay memoria suficiente para leer el archivo.\n");
        estado = EXIT_FAILURE;
    } else if (ferror(archivo)) {
        perror("Error al leer el archivo");
        estado = EXIT_FAILURE;
    }

    if (fclose(archivo) != 0) {
        perror("Error al cerrar el archivo");
        estado = EXIT_FAILURE;
    }

    if (numero_linea == 0) {
        printf("El archivo está vacío.\n");
    } else if (conversiones_correctas + lineas_con_error == 0) {
        printf("El archivo no contiene expresiones no vacías.\n");
    } else {
        printf("Resumen: %zu conversión(es) correcta(s), "
               "%zu línea(s) con error.\n",
               conversiones_correctas, lineas_con_error);
    }

    if (lineas_con_error > 0) {
        estado = EXIT_FAILURE;
    }

    return estado;
}

/* ------------------------------------------------------------------------- */
/* Interfaz de usuario                                                       */
/* ------------------------------------------------------------------------- */

static bool interpretar_opcion(const char *texto, int *opcion)
{
    char *fin = NULL;
    long valor;

    errno = 0;
    valor = strtol(texto, &fin, 10);

    if (fin == texto || errno == ERANGE) {
        return false;
    }

    while (isspace((unsigned char) *fin)) {
        fin++;
    }

    if (*fin != '\0' || valor < 0 || valor > 2) {
        return false;
    }

    *opcion = (int) valor;
    return true;
}

static int convertir_una_expresion(const char *expresion)
{
    char mensaje_error[TAM_MENSAJE_ERROR];

    printf("Entrada : %s\n", expresion);
    printf("Prefija : ");

    if (!convertir_y_escribir(expresion, stdout, mensaje_error,
                              sizeof(mensaje_error))) {
        printf("\nError   : %s\n", mensaje_error);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int ejecutar_menu(void)
{
    char mensaje_error[TAM_MENSAJE_ERROR];

    for (;;) {
        char *linea;
        bool fallo_memoria = false;
        int opcion;

        printf("\n=== Convertidor de infija a prefija ===\n");
        printf("1) Introducir una expresión\n");
        printf("2) Procesar un archivo de texto\n");
        printf("0) Salir\n");
        printf("Opción: ");
        fflush(stdout);

        linea = leer_linea(stdin, &fallo_memoria);

        if (linea == NULL) {
            if (fallo_memoria) {
                fprintf(stderr, "No hay memoria suficiente.\n");
                return EXIT_FAILURE;
            }
            printf("\n");
            return EXIT_SUCCESS;
        }

        if (!interpretar_opcion(linea, &opcion)) {
            free(linea);
            printf("Opción no válida. Intente de nuevo.\n");
            continue;
        }
        free(linea);

        if (opcion == 0) {
            printf("Hasta pronto.\n");
            return EXIT_SUCCESS;
        }

        if (opcion == 1) {
            char *expresion;

            printf("Expresión infija: ");
            fflush(stdout);
            expresion = leer_linea(stdin, &fallo_memoria);

            if (expresion == NULL) {
                if (fallo_memoria) {
                    fprintf(stderr, "No hay memoria suficiente.\n");
                    return EXIT_FAILURE;
                }
                printf("\n");
                return EXIT_SUCCESS;
            }

            if (es_linea_vacia(expresion)) {
                printf("La expresión no puede estar vacía.\n");
            } else {
                printf("\nEntrada : %s\n", expresion);
                printf("Prefija : ");

                if (!convertir_y_escribir(expresion, stdout, mensaje_error,
                                          sizeof(mensaje_error))) {
                    printf("\nError   : %s\n", mensaje_error);
                }
            }

            free(expresion);
        } else {
            char *ruta;

            printf("Ruta del archivo: ");
            fflush(stdout);
            ruta = leer_linea(stdin, &fallo_memoria);

            if (ruta == NULL) {
                if (fallo_memoria) {
                    fprintf(stderr, "No hay memoria suficiente.\n");
                    return EXIT_FAILURE;
                }
                printf("\n");
                return EXIT_SUCCESS;
            }

            if (es_linea_vacia(ruta)) {
                printf("La ruta no puede estar vacía.\n");
            } else {
                procesar_archivo(ruta);
            }

            free(ruta);
        }
    }
}

static void mostrar_uso(FILE *salida, const char *programa)
{
    fprintf(salida,
            "Uso:\n"
            "  %s\n"
            "  %s --expresion \"A + B * C\"\n"
            "  %s --archivo expresiones.txt\n"
            "  %s --ayuda\n\n"
            "Opciones:\n"
            "  -e, --expresion TEXTO   Convierte una sola expresión.\n"
            "  -f, --archivo RUTA      Convierte las expresiones de un archivo.\n"
            "  -h, --ayuda             Muestra esta ayuda.\n\n"
            "Operadores: +, -, *, /, %%, ^ y paréntesis. "
            "Se admite + y - unarios.\n",
            programa, programa, programa, programa);
}

int main(int argc, char *argv[])
{
    if (argc == 1) {
        return ejecutar_menu();
    }

    if (argc == 2 &&
        (strcmp(argv[1], "--ayuda") == 0 || strcmp(argv[1], "-h") == 0)) {
        mostrar_uso(stdout, argv[0]);
        return EXIT_SUCCESS;
    }

    if (argc == 3 &&
        (strcmp(argv[1], "--expresion") == 0 ||
         strcmp(argv[1], "-e") == 0)) {
        return convertir_una_expresion(argv[2]);
    }

    if (argc == 3 &&
        (strcmp(argv[1], "--archivo") == 0 ||
         strcmp(argv[1], "-f") == 0)) {
        return procesar_archivo(argv[2]);
    }

    mostrar_uso(stderr, argv[0]);
    return EXIT_FAILURE;
}
