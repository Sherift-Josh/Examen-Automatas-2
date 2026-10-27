/*
 * Tokenizador léxico independiente para un subconjunto del lenguaje C.
 *
 * El programa puede leer código desde la terminal o desde un archivo. Para
 * cada token muestra su categoría, su lexema y la línea/columna donde inicia.
 *
 * Características:
 *   - Identificadores, palabras reservadas y tipos de dato.
 *   - Números enteros y decimales, incluyendo notación exponencial.
 *   - Operadores simples y compuestos.
 *   - Paréntesis, llaves, corchetes y signos de puntuación.
 *   - Cadenas, caracteres y comentarios de línea y de bloque.
 *   - Detección y reporte de errores léxicos sin detener el análisis.
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAM_MENSAJE_ERROR 512

/* ------------------------------------------------------------------------- */
/* Tipos de datos                                                            */
/* ------------------------------------------------------------------------- */

typedef enum {
    TOKEN_PALABRA_RESERVADA,
    TOKEN_TIPO_DATO,
    TOKEN_IDENTIFICADOR,
    TOKEN_NUMERO_ENTERO,
    TOKEN_NUMERO_DECIMAL,
    TOKEN_CADENA,
    TOKEN_CARACTER,
    TOKEN_OPERADOR,
    TOKEN_PARENTESIS_IZQ,
    TOKEN_PARENTESIS_DER,
    TOKEN_LLAVE_IZQ,
    TOKEN_LLAVE_DER,
    TOKEN_CORCHETE_IZQ,
    TOKEN_CORCHETE_DER,
    TOKEN_COMA,
    TOKEN_PUNTO_Y_COMA,
    TOKEN_DOS_PUNTOS,
    TOKEN_PUNTO,
    TOKEN_ERROR
} TipoToken;

typedef struct {
    TipoToken tipo;
    char *lexema;
    size_t linea;
    size_t columna;
    char *detalle; /* Solo se usa para tokens de error. */
} Token;

typedef struct {
    Token *elementos;
    size_t cantidad;
    size_t capacidad;
} ListaTokens;

typedef struct {
    const char *fuente;
    size_t posicion;
    size_t linea;
    size_t columna;
} Lexer;

/* ------------------------------------------------------------------------- */
/* Utilidades                                                                */
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

static void inicializar_lista_tokens(ListaTokens *lista)
{
    lista->elementos = NULL;
    lista->cantidad = 0;
    lista->capacidad = 0;
}

static void liberar_lista_tokens(ListaTokens *lista)
{
    size_t i;

    for (i = 0; i < lista->cantidad; i++) {
        free(lista->elementos[i].lexema);
        free(lista->elementos[i].detalle);
    }

    free(lista->elementos);
    inicializar_lista_tokens(lista);
}

static bool agregar_token(ListaTokens *lista, TipoToken tipo,
                          const char *inicio, size_t longitud,
                          size_t linea, size_t columna,
                          const char *detalle)
{
    Token nuevo_token;
    char *lexema;
    char *copia_detalle = NULL;

    if (lista->cantidad == lista->capacidad) {
        size_t nueva_capacidad = (lista->capacidad == 0)
                                     ? 32
                                     : lista->capacidad * 2;
        Token *nuevos_elementos = realloc(
            lista->elementos, nueva_capacidad * sizeof(*nuevos_elementos));

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

    if (detalle != NULL) {
        copia_detalle = duplicar_cadena(detalle);
        if (copia_detalle == NULL) {
            free(lexema);
            return false;
        }
    }

    nuevo_token.tipo = tipo;
    nuevo_token.lexema = lexema;
    nuevo_token.linea = linea;
    nuevo_token.columna = columna;
    nuevo_token.detalle = copia_detalle;

    lista->elementos[lista->cantidad] = nuevo_token;
    lista->cantidad++;

    return true;
}

/* ------------------------------------------------------------------------- */
/* Operaciones básicas del lexer                                             */
/* ------------------------------------------------------------------------- */

static char caracter_actual(const Lexer *lexer)
{
    return lexer->fuente[lexer->posicion];
}

static char caracter_siguiente(const Lexer *lexer)
{
    if (lexer->fuente[lexer->posicion] == '\0') {
        return '\0';
    }

    return lexer->fuente[lexer->posicion + 1];
}

static void avanzar_caracter(Lexer *lexer)
{
    char actual = lexer->fuente[lexer->posicion];

    if (actual == '\0') {
        return;
    }

    if (actual == '\n') {
        lexer->linea++;
        lexer->columna = 1;
    } else {
        lexer->columna++;
    }

    lexer->posicion++;
}

static bool coincide_texto(const Lexer *lexer, const char *texto)
{
    size_t longitud = strlen(texto);

    return strncmp(lexer->fuente + lexer->posicion, texto, longitud) == 0;
}

/* ------------------------------------------------------------------------- */
/* Clasificación de identificadores                                          */
/* ------------------------------------------------------------------------- */

static const char *const PALABRAS_RESERVADAS[] = {
    "auto",      "break",    "case",     "const",    "continue",
    "default",   "do",       "else",     "enum",     "extern",
    "for",       "goto",     "if",       "register", "return",
    "short",     "signed",   "sizeof",   "static",   "struct",
    "switch",    "typedef",  "union",    "unsigned", "volatile",
    "while"
};

static const char *const TIPOS_DE_DATO[] = {
    "char", "double", "float", "int", "void"
};

static bool aparece_en_lista(const char *inicio, size_t longitud,
                             const char *const *lista, size_t cantidad)
{
    size_t i;

    for (i = 0; i < cantidad; i++) {
        const char *candidata = lista[i];

        if (strlen(candidata) == longitud &&
            memcmp(inicio, candidata, longitud) == 0) {
            return true;
        }
    }

    return false;
}

static TipoToken clasificar_identificador(const char *inicio, size_t longitud)
{
    if (aparece_en_lista(inicio, longitud, PALABRAS_RESERVADAS,
                         sizeof(PALABRAS_RESERVADAS) /
                             sizeof(PALABRAS_RESERVADAS[0]))) {
        return TOKEN_PALABRA_RESERVADA;
    }

    if (aparece_en_lista(inicio, longitud, TIPOS_DE_DATO,
                         sizeof(TIPOS_DE_DATO) / sizeof(TIPOS_DE_DATO[0]))) {
        return TOKEN_TIPO_DATO;
    }

    return TOKEN_IDENTIFICADOR;
}

/* ------------------------------------------------------------------------- */
/* Reconocimiento de números y operadores                                    */
/* ------------------------------------------------------------------------- */

static TipoToken escanear_numero(Lexer *lexer)
{
    bool tiene_parte_decimal = false;

    while (isdigit((unsigned char) caracter_actual(lexer))) {
        avanzar_caracter(lexer);
    }

    if (caracter_actual(lexer) == '.') {
        tiene_parte_decimal = true;
        avanzar_caracter(lexer);

        while (isdigit((unsigned char) caracter_actual(lexer))) {
            avanzar_caracter(lexer);
        }
    }

    if (caracter_actual(lexer) == 'e' ||
        caracter_actual(lexer) == 'E') {
        size_t posicion_exponente = lexer->posicion + 1;

        if (lexer->fuente[posicion_exponente] == '+' ||
            lexer->fuente[posicion_exponente] == '-') {
            posicion_exponente++;
        }

        if (isdigit((unsigned char) lexer->fuente[posicion_exponente])) {
            tiene_parte_decimal = true;
            avanzar_caracter(lexer); /* e o E */

            if (caracter_actual(lexer) == '+' ||
                caracter_actual(lexer) == '-') {
                avanzar_caracter(lexer);
            }

            while (isdigit((unsigned char) caracter_actual(lexer))) {
                avanzar_caracter(lexer);
            }
        }
    }

    if (tiene_parte_decimal &&
        (caracter_actual(lexer) == 'f' ||
         caracter_actual(lexer) == 'F')) {
        avanzar_caracter(lexer);
    }

    return tiene_parte_decimal ? TOKEN_NUMERO_DECIMAL
                               : TOKEN_NUMERO_ENTERO;
}

static const char *const OPERADORES_COMPUESTOS[] = {
    "==", "!=", "<=", ">=", "&&", "||", "++", "--",
    "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>", "->"
};

static bool es_operador_simple(char caracter)
{
    return strchr("+-*/%=<>!&|^~?", caracter) != NULL;
}

/* ------------------------------------------------------------------------- */
/* Análisis léxico                                                           */
/* ------------------------------------------------------------------------- */

static bool tokenizar(const char *fuente, ListaTokens *tokens,
                      char *mensaje_error, size_t tam_mensaje_error)
{
    Lexer lexer;

    lexer.fuente = fuente;
    lexer.posicion = 0;
    lexer.linea = 1;
    lexer.columna = 1;

    while (caracter_actual(&lexer) != '\0') {
        unsigned char actual = (unsigned char) caracter_actual(&lexer);
        size_t linea_inicio;
        size_t columna_inicio;

        if (isspace(actual)) {
            avanzar_caracter(&lexer);
            continue;
        }

        linea_inicio = lexer.linea;
        columna_inicio = lexer.columna;

        /* Comentario de línea: se ignora hasta el final de la línea. */
        if (caracter_actual(&lexer) == '/' &&
            caracter_siguiente(&lexer) == '/') {
            avanzar_caracter(&lexer);
            avanzar_caracter(&lexer);

            while (caracter_actual(&lexer) != '\0' &&
                   caracter_actual(&lexer) != '\n') {
                avanzar_caracter(&lexer);
            }
            continue;
        }

        /* Comentario de bloque: se ignora, incluso si ocupa varias líneas. */
        if (caracter_actual(&lexer) == '/' &&
            caracter_siguiente(&lexer) == '*') {
            bool comentario_cerrado = false;

            avanzar_caracter(&lexer);
            avanzar_caracter(&lexer);

            while (caracter_actual(&lexer) != '\0') {
                if (caracter_actual(&lexer) == '*' &&
                    caracter_siguiente(&lexer) == '/') {
                    avanzar_caracter(&lexer);
                    avanzar_caracter(&lexer);
                    comentario_cerrado = true;
                    break;
                }
                avanzar_caracter(&lexer);
            }

            if (!comentario_cerrado) {
                if (!agregar_token(tokens, TOKEN_ERROR, "/*", 2,
                                   linea_inicio, columna_inicio,
                                   "Comentario de bloque sin cerrar.")) {
                    goto error_memoria;
                }
            }
            continue;
        }

        /* Cadena o carácter literal. */
        if (caracter_actual(&lexer) == '"' ||
            caracter_actual(&lexer) == '\'') {
            size_t inicio = lexer.posicion;
            char delimitador = caracter_actual(&lexer);
            bool literal_cerrado = false;
            TipoToken tipo_literal = (delimitador == '"')
                                         ? TOKEN_CADENA
                                         : TOKEN_CARACTER;
            const char *mensaje = (delimitador == '"')
                                      ? "Cadena sin cerrar."
                                      : "Carácter literal sin cerrar.";

            avanzar_caracter(&lexer);

            while (caracter_actual(&lexer) != '\0') {
                char actual_literal = caracter_actual(&lexer);

                if (actual_literal == '\\') {
                    avanzar_caracter(&lexer);
                    if (caracter_actual(&lexer) != '\0') {
                        avanzar_caracter(&lexer);
                    }
                    continue;
                }

                if (actual_literal == delimitador) {
                    avanzar_caracter(&lexer);
                    literal_cerrado = true;
                    break;
                }

                if (actual_literal == '\n') {
                    break;
                }

                avanzar_caracter(&lexer);
            }

            if (!agregar_token(tokens,
                               literal_cerrado ? tipo_literal : TOKEN_ERROR,
                               fuente + inicio, lexer.posicion - inicio,
                               linea_inicio, columna_inicio,
                               literal_cerrado ? NULL : mensaje)) {
                goto error_memoria;
            }
            continue;
        }

        /* Identificadores, palabras reservadas y tipos de dato. */
        if (isalpha(actual) || actual == '_') {
            size_t inicio = lexer.posicion;
            size_t longitud;
            TipoToken tipo_identificador;

            avanzar_caracter(&lexer);

            while (isalnum((unsigned char) caracter_actual(&lexer)) ||
                   caracter_actual(&lexer) == '_') {
                avanzar_caracter(&lexer);
            }

            longitud = lexer.posicion - inicio;
            tipo_identificador = clasificar_identificador(fuente + inicio,
                                                          longitud);

            if (!agregar_token(tokens, tipo_identificador, fuente + inicio,
                               longitud, linea_inicio, columna_inicio, NULL)) {
                goto error_memoria;
            }
            continue;
        }

        /* Números enteros y decimales. */
        if (isdigit(actual) ||
            (actual == '.' &&
             isdigit((unsigned char) caracter_siguiente(&lexer)))) {
            size_t inicio = lexer.posicion;
            TipoToken tipo_numero = escanear_numero(&lexer);

            if (!agregar_token(tokens, tipo_numero, fuente + inicio,
                               lexer.posicion - inicio, linea_inicio,
                               columna_inicio, NULL)) {
                goto error_memoria;
            }
            continue;
        }

        /* Operadores compuestos: se comprueba primero la coincidencia más
         * larga disponible en la tabla. */
        {
            size_t i;
            bool operador_encontrado = false;

            for (i = 0;
                 i < sizeof(OPERADORES_COMPUESTOS) /
                         sizeof(OPERADORES_COMPUESTOS[0]);
                 i++) {
                const char *operador = OPERADORES_COMPUESTOS[i];

                if (coincide_texto(&lexer, operador)) {
                    size_t longitud = strlen(operador);
                    size_t inicio = lexer.posicion;
                    size_t j;

                    for (j = 0; j < longitud; j++) {
                        avanzar_caracter(&lexer);
                    }

                    if (!agregar_token(tokens, TOKEN_OPERADOR, fuente + inicio,
                                       longitud, linea_inicio, columna_inicio,
                                       NULL)) {
                        goto error_memoria;
                    }

                    operador_encontrado = true;
                    break;
                }
            }

            if (operador_encontrado) {
                continue;
            }
        }

        /* Operadores de un solo carácter. */
        if (es_operador_simple((char) actual)) {
            size_t inicio = lexer.posicion;

            avanzar_caracter(&lexer);

            if (!agregar_token(tokens, TOKEN_OPERADOR, fuente + inicio, 1,
                               linea_inicio, columna_inicio, NULL)) {
                goto error_memoria;
            }
            continue;
        }

        /* Delimitadores y signos de puntuación. */
        {
            TipoToken tipo_delimitador = TOKEN_ERROR;
            size_t inicio = lexer.posicion;

            switch ((char) actual) {
            case '(':
                tipo_delimitador = TOKEN_PARENTESIS_IZQ;
                break;
            case ')':
                tipo_delimitador = TOKEN_PARENTESIS_DER;
                break;
            case '{':
                tipo_delimitador = TOKEN_LLAVE_IZQ;
                break;
            case '}':
                tipo_delimitador = TOKEN_LLAVE_DER;
                break;
            case '[':
                tipo_delimitador = TOKEN_CORCHETE_IZQ;
                break;
            case ']':
                tipo_delimitador = TOKEN_CORCHETE_DER;
                break;
            case ',':
                tipo_delimitador = TOKEN_COMA;
                break;
            case ';':
                tipo_delimitador = TOKEN_PUNTO_Y_COMA;
                break;
            case ':':
                tipo_delimitador = TOKEN_DOS_PUNTOS;
                break;
            case '.':
                tipo_delimitador = TOKEN_PUNTO;
                break;
            default:
                break;
            }

            if (tipo_delimitador != TOKEN_ERROR) {
                avanzar_caracter(&lexer);

                if (!agregar_token(tokens, tipo_delimitador, fuente + inicio,
                                   1, linea_inicio, columna_inicio, NULL)) {
                    goto error_memoria;
                }
                continue;
            }
        }

        /* Carácter no reconocido por el lenguaje. */
        {
            size_t inicio = lexer.posicion;

            avanzar_caracter(&lexer);

            if (!agregar_token(tokens, TOKEN_ERROR, fuente + inicio, 1,
                               linea_inicio, columna_inicio,
                               "Carácter no permitido.")) {
                goto error_memoria;
            }
        }
    }

    return true;

error_memoria:
    snprintf(mensaje_error, tam_mensaje_error,
             "No hay memoria suficiente para almacenar los tokens.");
    liberar_lista_tokens(tokens);
    return false;
}

/* ------------------------------------------------------------------------- */
/* Presentación de resultados                                                */
/* ------------------------------------------------------------------------- */

static const char *nombre_tipo_token(TipoToken tipo)
{
    switch (tipo) {
    case TOKEN_PALABRA_RESERVADA:
        return "PALABRA_RESERVADA";
    case TOKEN_TIPO_DATO:
        return "TIPO_DATO";
    case TOKEN_IDENTIFICADOR:
        return "IDENTIFICADOR";
    case TOKEN_NUMERO_ENTERO:
        return "ENTERO";
    case TOKEN_NUMERO_DECIMAL:
        return "DECIMAL";
    case TOKEN_CADENA:
        return "CADENA";
    case TOKEN_CARACTER:
        return "CARACTER";
    case TOKEN_OPERADOR:
        return "OPERADOR";
    case TOKEN_PARENTESIS_IZQ:
        return "PARENTESIS_IZQ";
    case TOKEN_PARENTESIS_DER:
        return "PARENTESIS_DER";
    case TOKEN_LLAVE_IZQ:
        return "LLAVE_IZQ";
    case TOKEN_LLAVE_DER:
        return "LLAVE_DER";
    case TOKEN_CORCHETE_IZQ:
        return "CORCHETE_IZQ";
    case TOKEN_CORCHETE_DER:
        return "CORCHETE_DER";
    case TOKEN_COMA:
        return "COMA";
    case TOKEN_PUNTO_Y_COMA:
        return "PUNTO_Y_COMA";
    case TOKEN_DOS_PUNTOS:
        return "DOS_PUNTOS";
    case TOKEN_PUNTO:
        return "PUNTO";
    case TOKEN_ERROR:
        return "ERROR_LEXICO";
    default:
        return "DESCONOCIDO";
    }
}

static size_t imprimir_tokens(const ListaTokens *tokens)
{
    size_t i;
    size_t errores = 0;

    if (tokens->cantidad == 0) {
        printf("No se encontraron tokens.\n");
        return 0;
    }

    for (i = 0; i < tokens->cantidad; i++) {
        const Token *token = &tokens->elementos[i];

        printf("[%zu:%zu] %-20s %s\n", token->linea, token->columna,
               nombre_tipo_token(token->tipo), token->lexema);

        if (token->tipo == TOKEN_ERROR) {
            errores++;
            if (token->detalle != NULL) {
                printf("         Detalle: %s\n", token->detalle);
            }
        }
    }

    printf("\nTokens: %zu | Errores léxicos: %zu\n",
           tokens->cantidad, errores);

    return errores;
}

static void analizar_fuente(const char *fuente)
{
    ListaTokens tokens;
    char mensaje_error[TAM_MENSAJE_ERROR];

    inicializar_lista_tokens(&tokens);

    if (!tokenizar(fuente, &tokens, mensaje_error, sizeof(mensaje_error))) {
        fprintf(stderr, "Error: %s\n", mensaje_error);
        return;
    }

    imprimir_tokens(&tokens);
    liberar_lista_tokens(&tokens);
}

/* ------------------------------------------------------------------------- */
/* Lectura de datos                                                          */
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

    if (longitud > 0 && linea[longitud - 1] == '\r') {
        longitud--;
    }

    linea[longitud] = '\0';
    return linea;
}

static char *leer_archivo_completo(const char *ruta, char *mensaje_error,
                                   size_t tam_mensaje_error)
{
    FILE *archivo = fopen(ruta, "r");
    size_t capacidad = 4096;
    size_t longitud = 0;
    char *contenido;
    int caracter;

    if (archivo == NULL) {
        snprintf(mensaje_error, tam_mensaje_error,
                 "No se pudo abrir el archivo '%s': %s",
                 ruta, strerror(errno));
        return NULL;
    }

    contenido = malloc(capacidad);
    if (contenido == NULL) {
        fclose(archivo);
        snprintf(mensaje_error, tam_mensaje_error,
                 "No hay memoria suficiente para leer el archivo.");
        return NULL;
    }

    while ((caracter = fgetc(archivo)) != EOF) {
        if (longitud + 1 >= capacidad) {
            size_t nueva_capacidad = capacidad * 2;
            char *nuevo_contenido = realloc(contenido, nueva_capacidad);

            if (nuevo_contenido == NULL) {
                free(contenido);
                fclose(archivo);
                snprintf(mensaje_error, tam_mensaje_error,
                         "No hay memoria suficiente para leer el archivo.");
                return NULL;
            }

            contenido = nuevo_contenido;
            capacidad = nueva_capacidad;
        }

        contenido[longitud] = (char) caracter;
        longitud++;
    }

    if (ferror(archivo)) {
        snprintf(mensaje_error, tam_mensaje_error,
                 "Error al leer el archivo '%s'.", ruta);
        free(contenido);
        fclose(archivo);
        return NULL;
    }

    contenido[longitud] = '\0';

    if (fclose(archivo) != 0) {
        snprintf(mensaje_error, tam_mensaje_error,
                 "Error al cerrar el archivo '%s'.", ruta);
        free(contenido);
        return NULL;
    }

    return contenido;
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

static void procesar_archivo(const char *ruta)
{
    char mensaje_error[TAM_MENSAJE_ERROR];
    char *contenido = leer_archivo_completo(ruta, mensaje_error,
                                            sizeof(mensaje_error));

    if (contenido == NULL) {
        fprintf(stderr, "%s\n", mensaje_error);
        return;
    }

    printf("\nArchivo: %s\n\n", ruta);
    analizar_fuente(contenido);
    free(contenido);
}

/* ------------------------------------------------------------------------- */
/* Menú principal                                                            */
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

static int ejecutar_menu(void)
{
    for (;;) {
        char *linea;
        bool fallo_memoria = false;
        int opcion;

        printf("\n=== Tokenizador léxico ===\n");
        printf("1) Introducir código por terminal\n");
        printf("2) Procesar un archivo\n");
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
            char *codigo;

            printf("Código: ");
            fflush(stdout);
            codigo = leer_linea(stdin, &fallo_memoria);

            if (codigo == NULL) {
                if (fallo_memoria) {
                    fprintf(stderr, "No hay memoria suficiente.\n");
                    return EXIT_FAILURE;
                }
                printf("\n");
                return EXIT_SUCCESS;
            }

            if (es_linea_vacia(codigo)) {
                printf("No se ingresó código.\n");
            } else {
                printf("\n");
                analizar_fuente(codigo);
            }

            free(codigo);
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

int main(void)
{
    return ejecutar_menu();
}
