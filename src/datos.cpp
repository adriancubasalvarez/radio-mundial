#include "datos.h"
#include "hal.h"
#include "idioma.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Copia un texto JSON a un char[] fijo sin pasarse del tamaño */
static void copiar(char *destino, size_t tam, JsonVariantConst origen)
{
    const char *texto = origen | "";
    strncpy(destino, texto, tam - 1);
    destino[tam - 1] = '\0';
}

/* Adaptador para que ArduinoJson lea directamente de un archivo de la HAL */
struct LectorArchivo {
    HalArchivo *archivo;
    int read()
    {
        uint8_t c;
        return hal_archivo_leer(archivo, &c, 1) == 1 ? c : -1;
    }
    size_t readBytes(char *destino, size_t n)
    {
        return hal_archivo_leer(archivo, destino, n);
    }
};

/* Analiza un JSON leyéndolo del archivo poco a poco, sin cargarlo entero en
 * RAM (en la placa, con el audio sonando, no hay 30 KB seguidos libres).
 * El filtro, si se da, guarda solo los campos que interesan. */
static bool leer_json(const char *ruta, JsonDocument &doc, JsonDocument *filtro = NULL)
{
    HalArchivo *archivo = hal_archivo_abrir(ruta);
    if (archivo == NULL) {
        printf("No se encuentra %s\n", ruta);
        return false;
    }
    LectorArchivo lector{archivo};
    DeserializationError error = filtro != NULL
                                     ? deserializeJson(doc, lector, DeserializationOption::Filter(*filtro))
                                     : deserializeJson(doc, lector);
    hal_archivo_cerrar(archivo);
    if (error) {
        printf("%s: %s\n", ruta, error.c_str());
        return false;
    }
    return true;
}

bool datos_cargar_indice(IndiceMundo *indice)
{
    memset(indice, 0, sizeof(IndiceMundo));

    /* Solo nos interesan los campos de las regiones, no la lista de países */
    JsonDocument filtro;
    filtro["mundo"] = true;
    JsonObject f = filtro["regiones"][0].to<JsonObject>();
    f["id"] = true;
    f["nombre"] = true;
    f["nombre_en"] = true;
    f["indice"] = true;
    f["imagen"] = true;
    f["zonas"] = true;

    JsonDocument doc;
    if (!leer_json("regiones.json", doc, &filtro)) {
        return false;
    }

    JsonObjectConst mundo = doc["mundo"];
    copiar(indice->imagen, sizeof(indice->imagen), mundo["imagen"]);
    copiar(indice->zonas, sizeof(indice->zonas), mundo["zonas"]);
    indice->ancho = mundo["ancho"] | 0;
    indice->alto = mundo["alto"] | 0;

    for (JsonObjectConst r : doc["regiones"].as<JsonArrayConst>()) {
        if (indice->num_regiones >= MAX_REGIONES) {
            break;
        }
        Region *region = &indice->regiones[indice->num_regiones++];
        copiar(region->id, sizeof(region->id), r["id"]);
        copiar(region->nombre, sizeof(region->nombre), idioma_es ? r["nombre"] : r["nombre_en"]);
        copiar(region->imagen, sizeof(region->imagen), r["imagen"]);
        copiar(region->zonas, sizeof(region->zonas), r["zonas"]);
        region->indice = r["indice"] | 0;
    }
    return true;
}

const Region *datos_region_por_indice(const IndiceMundo *indice, uint8_t color)
{
    for (uint8_t i = 0; i < indice->num_regiones; i++) {
        if (indice->regiones[i].indice == color) {
            return &indice->regiones[i];
        }
    }
    return NULL;
}

/* Recorre emisoras/indice.txt («código<TAB>número» por línea) llamando a
 * visitar(código, número, dato) en cada país. Se lee línea a línea: apenas
 * gasta RAM. Devuelve false si no existe el índice. */
static bool recorrer_indice(void (*visitar)(const char *, int, void *), void *dato)
{
    HalArchivo *archivo = hal_archivo_abrir("emisoras/indice.txt");
    if (archivo == NULL) {
        return false;
    }
    char linea[24];
    while (hal_archivo_linea(archivo, linea, sizeof(linea))) {
        char *tab = strchr(linea, '\t');
        if (tab != NULL) {
            *tab = '\0';
            visitar(linea, atoi(tab + 1), dato);
        }
    }
    hal_archivo_cerrar(archivo);
    return true;
}

static void anotar_cuenta(const char *codigo, int numero, void *dato)
{
    ListaPaises *lista = (ListaPaises *)dato;
    for (uint8_t i = 0; i < lista->num; i++) {
        if (strcmp(lista->paises[i].codigo, codigo) == 0) {
            lista->paises[i].emisoras = (uint8_t)(numero > 255 ? 255 : numero);
        }
    }
}

/* Rellena cuántas emisoras tiene cada país. Si no existe el índice, se
 * supone que todos tienen alguna para no bloquear la navegación. */
static void contar_emisoras(ListaPaises *lista)
{
    if (!recorrer_indice(anotar_cuenta, lista)) {
        for (uint8_t i = 0; i < lista->num; i++) {
            lista->paises[i].emisoras = 1;
        }
    }
}

/* Letra base de un carácter para ordenar sin tildes: los acentuados de
 * Latin-1 en UTF-8 (0xC3 0x80..0xBF) se reducen a su letra sin tilde */
static char letra_base(const unsigned char **p)
{
    unsigned char c = **p;
    (*p)++;
    if (c == 0xC3 && **p >= 0x80 && **p <= 0xBF) {
        static const char tabla[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
        c = (unsigned char)tabla[**p - 0x80];
        (*p)++;
    }
    return (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
}

static int comparar_nombres(const char *a, const char *b)
{
    const unsigned char *pa = (const unsigned char *)a, *pb = (const unsigned char *)b;
    while (*pa && *pb) {
        char ca = letra_base(&pa), cb = letra_base(&pb);
        if (ca != cb) {
            return ca - cb;
        }
    }
    return *pa - *pb;
}

/* Orden alfabético (inserción: son pocos países por región) */
static void ordenar_paises(ListaPaises *lista)
{
    for (uint8_t i = 1; i < lista->num; i++) {
        Pais clave = lista->paises[i];
        int j = i - 1;
        while (j >= 0 && comparar_nombres(lista->paises[j].nombre, clave.nombre) > 0) {
            lista->paises[j + 1] = lista->paises[j];
            j--;
        }
        lista->paises[j + 1] = clave;
    }
}

bool datos_cargar_paises(const char *id_region, ListaPaises *lista)
{
    memset(lista, 0, sizeof(ListaPaises));

    /* Solo los países de esta región (regiones/<id>.json, 1–3 KB): leer el
     * índice completo no cabe en la placa con el audio sonando */
    char ruta[48];
    snprintf(ruta, sizeof(ruta), "regiones/%s.json", id_region);
    JsonDocument doc;
    if (!leer_json(ruta, doc)) {
        return false;
    }
    for (JsonObjectConst p : doc.as<JsonArrayConst>()) {
        if (lista->num >= MAX_PAISES) {
            break;
        }
        Pais *pais = &lista->paises[lista->num++];
        copiar(pais->codigo, sizeof(pais->codigo), p["codigo"]);
        copiar(pais->nombre, sizeof(pais->nombre), idioma_es ? p["nombre"] : p["en"]);
        pais->indice = p["indice"] | 0;
    }
    /* Los archivos vienen en orden alfabético español; en inglés se reordena */
    if (!idioma_es) {
        ordenar_paises(lista);
    }
    contar_emisoras(lista);
    return true;
}

const Pais *datos_pais_por_indice(const ListaPaises *lista, uint8_t color)
{
    for (uint8_t i = 0; i < lista->num; i++) {
        if (lista->paises[i].indice == color) {
            return &lista->paises[i];
        }
    }
    return NULL;
}

bool datos_cargar_emisoras(const char *codigo, ListaEmisoras *lista)
{
    memset(lista, 0, sizeof(ListaEmisoras));

    char ruta[32];
    snprintf(ruta, sizeof(ruta), "emisoras/%s.json", codigo);
    JsonDocument doc;
    if (!leer_json(ruta, doc)) {
        return false;
    }

    JsonArrayConst arr = doc.as<JsonArrayConst>();
    lista->emisoras = (Emisora *)calloc(arr.size(), sizeof(Emisora));
    if (lista->emisoras == NULL) {
        return false;
    }
    for (JsonObjectConst e : arr) {
        Emisora *emisora = &lista->emisoras[lista->num++];
        copiar(emisora->nombre, sizeof(emisora->nombre), e["n"]);
        copiar(emisora->url, sizeof(emisora->url), e["u"]);
        copiar(emisora->codec, sizeof(emisora->codec), e["c"]);
        emisora->bitrate = e["b"] | 0;
    }
    return true;
}

void datos_liberar_emisoras(ListaEmisoras *lista)
{
    free(lista->emisoras);
    lista->emisoras = NULL;
    lista->num = 0;
}

/* Busca el nombre de un país en regiones.json */
bool datos_buscar_pais(const char *codigo, Pais *pais)
{
    /* paises.txt: «código<TAB>español<TAB>inglés» por línea; se lee línea a línea */
    HalArchivo *archivo = hal_archivo_abrir("paises.txt");
    if (archivo == NULL) {
        return false;
    }
    char linea[120];
    bool encontrado = false;
    while (!encontrado && hal_archivo_linea(archivo, linea, sizeof(linea))) {
        char *tab = strchr(linea, '\t');
        if (tab == NULL) {
            continue;
        }
        *tab = '\0';
        if (strcmp(linea, codigo) == 0) {
            char *nombre = tab + 1;
            char *tab_en = strchr(nombre, '\t');
            if (tab_en != NULL) {
                *tab_en = '\0';
                if (!idioma_es) {
                    nombre = tab_en + 1;
                }
            }
            memset(pais, 0, sizeof(Pais));
            strncpy(pais->codigo, linea, sizeof(pais->codigo) - 1);
            strncpy(pais->nombre, nombre, sizeof(pais->nombre) - 1);
            encontrado = true;
        }
    }
    hal_archivo_cerrar(archivo);
    return encontrado;
}

/* Para elegir país al azar: primero se cuentan los que tienen emisoras y
 * luego se recorre el índice otra vez hasta llegar al elegido */
typedef struct {
    size_t contador;      /* países con emisoras vistos hasta ahora */
    size_t elegido;       /* posición elegida (en la segunda pasada) */
    char codigo[3];       /* resultado */
} Sorteo;

static void contar_con_emisoras(const char *codigo, int numero, void *dato)
{
    (void)codigo;
    ((Sorteo *)dato)->contador += (numero > 0);
}

static void buscar_elegido(const char *codigo, int numero, void *dato)
{
    Sorteo *s = (Sorteo *)dato;
    if (numero > 0 && s->contador++ == s->elegido) {
        strncpy(s->codigo, codigo, sizeof(s->codigo) - 1);
    }
}

bool datos_emisora_aleatoria(Emisora *emisora, Pais *pais)
{
    Sorteo sorteo;
    memset(&sorteo, 0, sizeof(sorteo));
    if (!recorrer_indice(contar_con_emisoras, &sorteo) || sorteo.contador == 0) {
        return false;
    }
    size_t con_emisoras = sorteo.contador;

    /* Se intenta unas cuantas veces por si algún archivo falla */
    for (int intento = 0; intento < 5; intento++) {
        memset(&sorteo, 0, sizeof(sorteo));
        sorteo.elegido = hal_aleatorio() % con_emisoras;
        recorrer_indice(buscar_elegido, &sorteo);
        const char *codigo = sorteo.codigo;
        ListaEmisoras lista;
        if (codigo[0] == '\0' || !datos_cargar_emisoras(codigo, &lista) || lista.num == 0) {
            continue;
        }
        *emisora = lista.emisoras[hal_aleatorio() % lista.num];
        datos_liberar_emisoras(&lista);
        if (!datos_buscar_pais(codigo, pais)) {
            memset(pais, 0, sizeof(Pais));
            strncpy(pais->codigo, codigo, sizeof(pais->codigo) - 1);
            strncpy(pais->nombre, codigo, sizeof(pais->nombre) - 1);
        }
        return true;
    }
    return false;
}

/* ---------- Búsqueda y géneros ---------- */

/* Misma tabla de letras que GENEROS en tools/build_stations.py */
const Genero GENEROS[] = {
    {'p', "Pop & hits", "Pop y éxitos"},
    {'r', "Rock", "Rock"},
    {'n', "News & talk", "Noticias y tertulia"},
    {'d', "Sports", "Deportes"},
    {'j', "Jazz, blues & soul", "Jazz, blues y soul"},
    {'c', "Classical", "Clásica"},
    {'e', "Electronic & dance", "Electrónica y dance"},
    {'l', "Latin", "Latina"},
    {'h', "Hip-hop & R&B", "Hip-hop y R&B"},
    {'o', "Oldies & classics", "Éxitos de siempre"},
    {'f', "Religious", "Religiosa"},
    {'t', "Folk, country & traditional", "Folk, country y tradicional"},
    {'a', "Chill & lounge", "Chill y lounge"},
};
const int NUM_GENEROS = sizeof(GENEROS) / sizeof(GENEROS[0]);

#define MAX_PALABRAS 6

/* Separa la línea de buscar.txt en sus 5 campos (modifica la línea) */
static bool campos(char *linea, char *partes[5])
{
    int n = 0;
    partes[n++] = linea;
    for (char *p = linea; *p && n < 5; p++) {
        if (*p == '\t') {
            *p = '\0';
            partes[n++] = p + 1;
        }
    }
    return n == 5;
}

int datos_buscar(const char *texto, char genero, Resultado *resultados, int max)
{
    /* El texto tecleado se pasa a minúsculas y se parte en palabras.
     * El teclado solo tiene letras sin tilde, números y espacio, igual que
     * el campo «nombre para buscar» de buscar.txt. */
    char consulta[64];
    size_t i = 0;
    for (; texto[i] && i < sizeof(consulta) - 1; i++) {
        char c = texto[i];
        consulta[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    consulta[i] = '\0';

    char *palabras[MAX_PALABRAS];
    int num_palabras = 0;
    for (char *p = strtok(consulta, " "); p && num_palabras < MAX_PALABRAS; p = strtok(NULL, " ")) {
        palabras[num_palabras++] = p;
    }
    if (num_palabras == 0 && genero == 0) {
        return 0;
    }

    HalArchivo *archivo = hal_archivo_abrir("buscar.txt");
    if (archivo == NULL) {
        return 0;
    }
    int encontrados = 0;
    char linea[200];
    while (encontrados < max && hal_archivo_linea(archivo, linea, sizeof(linea))) {
        char *partes[5];
        if (!campos(linea, partes)) {
            continue;
        }
        if (genero != 0 && strchr(partes[2], genero) == NULL) {
            continue;
        }
        bool vale = true;
        for (int k = 0; k < num_palabras && vale; k++) {
            vale = strstr(partes[3], palabras[k]) != NULL;
        }
        if (!vale) {
            continue;
        }
        Resultado *r = &resultados[encontrados++];
        strncpy(r->codigo, partes[0], sizeof(r->codigo) - 1);
        r->codigo[sizeof(r->codigo) - 1] = '\0';
        r->posicion = (uint16_t)atoi(partes[1]);
        strncpy(r->nombre, partes[4], sizeof(r->nombre) - 1);
        r->nombre[sizeof(r->nombre) - 1] = '\0';
    }
    hal_archivo_cerrar(archivo);
    return encontrados;
}

bool datos_emisora_de_resultado(const Resultado *resultado, Emisora *emisora, Pais *pais)
{
    ListaEmisoras lista;
    if (!datos_cargar_emisoras(resultado->codigo, &lista)) {
        return false;
    }
    bool ok = resultado->posicion < lista.num;
    if (ok) {
        *emisora = lista.emisoras[resultado->posicion];
    }
    datos_liberar_emisoras(&lista);
    if (ok && !datos_buscar_pais(resultado->codigo, pais)) {
        memset(pais, 0, sizeof(Pais));
        strncpy(pais->codigo, resultado->codigo, sizeof(pais->codigo) - 1);
        strncpy(pais->nombre, resultado->codigo, sizeof(pais->nombre) - 1);
    }
    return ok;
}

/* ---------- Emisoras propias en la SD ---------- */

bool datos_hay_mis_emisoras(void)
{
    size_t tam = 0;
    uint8_t *texto = hal_sd_leer(ARCHIVO_MIS_EMISORAS, &tam);
    free(texto);
    return texto != NULL;
}

bool datos_cargar_mis_emisoras(ListaEmisoras *lista)
{
    memset(lista, 0, sizeof(ListaEmisoras));
    size_t tam = 0;
    char *texto = (char *)hal_sd_leer(ARCHIVO_MIS_EMISORAS, &tam);
    if (texto == NULL) {
        return false;
    }

    /* Primera pasada: contar líneas para reservar memoria */
    size_t lineas = 1;
    for (size_t i = 0; i < tam; i++) {
        lineas += (texto[i] == '\n');
    }
    lista->emisoras = (Emisora *)calloc(lineas, sizeof(Emisora));
    if (lista->emisoras == NULL) {
        free(texto);
        return false;
    }

    /* Segunda pasada: «nombre;url» en cada línea */
    for (char *linea = strtok(texto, "\r\n"); linea; linea = strtok(NULL, "\r\n")) {
        while (*linea == ' ') {
            linea++;
        }
        char *separador = strchr(linea, ';');
        if (*linea == '#' || separador == NULL) {
            continue;
        }
        *separador = '\0';
        const char *url = separador + 1;
        while (*url == ' ') {
            url++;
        }
        if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
            continue;
        }
        Emisora *e = &lista->emisoras[lista->num++];
        strncpy(e->nombre, linea, sizeof(e->nombre) - 1);
        strncpy(e->url, url, sizeof(e->url) - 1);
        /* La URL puede acabar en ;algo (columna extra): se corta ahí */
        char *fin = strchr(e->url, ';');
        if (fin) {
            *fin = '\0';
        }
        strcpy(e->codec, "?");
    }
    free(texto);
    return true;
}