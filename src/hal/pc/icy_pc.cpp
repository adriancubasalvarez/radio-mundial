#include "icy_pc.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Conexion;
#define CONEXION_NULA INVALID_SOCKET
#define cerrar_conexion closesocket
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int Conexion;
#define CONEXION_NULA (-1)
#define cerrar_conexion close
#endif

/* Estado de un lector; lo comparten el hilo y quien lo para */
struct Lector {
    std::atomic<bool> parar{false};
    std::atomic<Conexion> conexion{CONEXION_NULA};
    std::mutex cerrojo;
    std::string titulo;
};

static std::shared_ptr<Lector> lector_actual;
static std::mutex cerrojo_actual;

#define MAX_REDIRECCIONES 3

/* Separa "http://host:puerto/ruta". Devuelve false si no es http:// */
static bool separar_url(const std::string &url, std::string &host, std::string &puerto, std::string &ruta)
{
    const std::string prefijo = "http://";
    if (url.compare(0, prefijo.size(), prefijo) != 0) {
        return false;
    }
    std::string resto = url.substr(prefijo.size());
    size_t barra = resto.find('/');
    std::string anfitrion = resto.substr(0, barra);
    ruta = barra == std::string::npos ? "/" : resto.substr(barra);
    size_t dos_puntos = anfitrion.find(':');
    host = anfitrion.substr(0, dos_puntos);
    puerto = dos_puntos == std::string::npos ? "80" : anfitrion.substr(dos_puntos + 1);
    return !host.empty();
}

static Conexion conectar(const std::string &host, const std::string &puerto)
{
    struct addrinfo pista = {}, *resultado = NULL;
    pista.ai_family = AF_UNSPEC;
    pista.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), puerto.c_str(), &pista, &resultado) != 0) {
        return CONEXION_NULA;
    }
    Conexion c = CONEXION_NULA;
    for (struct addrinfo *a = resultado; a != NULL; a = a->ai_next) {
        c = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (c == CONEXION_NULA) {
            continue;
        }
        if (connect(c, a->ai_addr, (int)a->ai_addrlen) == 0) {
            break;
        }
        cerrar_conexion(c);
        c = CONEXION_NULA;
    }
    freeaddrinfo(resultado);
    return c;
}

/* Lee exactamente n bytes (o los descarta si destino es NULL) */
static bool leer_exacto(Lector &l, Conexion c, char *destino, size_t n)
{
    char tiradero[4096];
    while (n > 0 && !l.parar) {
        size_t pedir = destino ? n : (n < sizeof(tiradero) ? n : sizeof(tiradero));
        int r = recv(c, destino ? destino : tiradero, (int)pedir, 0);
        if (r <= 0) {
            return false;
        }
        if (destino) {
            destino += r;
        }
        n -= (size_t)r;
    }
    return n == 0;
}

/* Lee las cabeceras HTTP hasta la línea en blanco */
static bool leer_cabeceras(Lector &l, Conexion c, std::string &cabeceras)
{
    char letra;
    while (cabeceras.size() < 16384) {
        if (!leer_exacto(l, c, &letra, 1)) {
            return false;
        }
        cabeceras += letra;
        if (cabeceras.size() >= 4 && cabeceras.compare(cabeceras.size() - 4, 4, "\r\n\r\n") == 0) {
            return true;
        }
    }
    return false;
}

/* Valor de una cabecera (sin distinguir mayúsculas) */
static std::string cabecera(const std::string &cabeceras, const char *nombre)
{
    std::string minus = cabeceras;
    for (char &ch : minus) {
        ch = (char)tolower((unsigned char)ch);
    }
    std::string buscar = std::string("\r\n") + nombre + ":";
    size_t pos = minus.find(buscar);
    if (pos == std::string::npos) {
        return "";
    }
    pos += buscar.size();
    size_t fin = cabeceras.find("\r\n", pos);
    std::string valor = cabeceras.substr(pos, fin - pos);
    size_t inicio = valor.find_first_not_of(' ');
    return inicio == std::string::npos ? "" : valor.substr(inicio);
}

/* Si el texto no es UTF-8 válido se interpreta como Latin-1 y se convierte */
static std::string a_utf8(const std::string &texto)
{
    size_t i = 0;
    bool valido = true;
    while (i < texto.size() && valido) {
        unsigned char c = (unsigned char)texto[i];
        int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
        if (extra < 0 || i + extra >= texto.size() + (extra == 0 ? 1 : 0)) {
            valido = extra == 0;
        }
        for (int k = 1; valido && k <= extra; k++) {
            valido = i + k < texto.size() && ((unsigned char)texto[i + k] >> 6) == 2;
        }
        i += extra + 1;
    }
    if (valido) {
        return texto;
    }
    std::string salida;
    for (unsigned char c : texto) {
        if (c < 0x80) {
            salida += (char)c;
        } else {
            salida += (char)(0xC0 | (c >> 6));
            salida += (char)(0x80 | (c & 0x3F));
        }
    }
    return salida;
}

static void hilo_lector(std::shared_ptr<Lector> l, std::string url)
{
    for (int salto = 0; salto <= MAX_REDIRECCIONES && !l->parar; salto++) {
        std::string host, puerto, ruta;
        if (!separar_url(url, host, puerto, ruta)) {
            return; /* https:// u otra cosa: sin títulos en el PC */
        }
        Conexion c = conectar(host, puerto);
        if (c == CONEXION_NULA) {
            return;
        }
        l->conexion = c;
        if (l->parar) {
            break;
        }

        std::string peticion = "GET " + ruta + " HTTP/1.0\r\nHost: " + host +
                               "\r\nIcy-MetaData: 1\r\nUser-Agent: RadioMundial/0.1\r\n\r\n";
        send(c, peticion.c_str(), (int)peticion.size(), 0);

        std::string cabeceras = "\r\n";
        if (!leer_cabeceras(*l, c, cabeceras)) {
            break;
        }
        std::string destino = cabecera(cabeceras, "location");
        if (!destino.empty() && (cabeceras.find(" 301") != std::string::npos ||
                                 cabeceras.find(" 302") != std::string::npos ||
                                 cabeceras.find(" 307") != std::string::npos)) {
            cerrar_conexion(c);
            l->conexion = CONEXION_NULA;
            url = destino;
            continue; /* seguir la redirección */
        }

        long intervalo = atol(cabecera(cabeceras, "icy-metaint").c_str());
        if (intervalo <= 0) {
            break; /* la emisora no envía metadatos */
        }

        /* Bucle: <intervalo> bytes de audio, 1 byte de longitud (×16) y metadatos */
        while (!l->parar) {
            unsigned char largo;
            if (!leer_exacto(*l, c, NULL, (size_t)intervalo) ||
                !leer_exacto(*l, c, (char *)&largo, 1)) {
                break;
            }
            if (largo == 0) {
                continue;
            }
            std::string meta(largo * 16, '\0');
            if (!leer_exacto(*l, c, &meta[0], meta.size())) {
                break;
            }
            size_t ini = meta.find("StreamTitle='");
            if (ini == std::string::npos) {
                continue;
            }
            ini += 13;
            size_t fin = meta.find("';", ini);
            std::string titulo = meta.substr(ini, fin == std::string::npos ? std::string::npos : fin - ini);
            std::lock_guard<std::mutex> g(l->cerrojo);
            l->titulo = a_utf8(titulo);
        }
        break;
    }
    Conexion c = l->conexion.exchange(CONEXION_NULA);
    if (c != CONEXION_NULA) {
        cerrar_conexion(c);
    }
}

void icy_empezar(const std::string &url)
{
#ifdef _WIN32
    static bool winsock_listo = false;
    if (!winsock_listo) {
        WSADATA datos;
        winsock_listo = WSAStartup(MAKEWORD(2, 2), &datos) == 0;
    }
#endif
    icy_parar();
    auto l = std::make_shared<Lector>();
    {
        std::lock_guard<std::mutex> g(cerrojo_actual);
        lector_actual = l;
    }
    /* El hilo va suelto: al parar se cierra su conexión y termina solo */
    std::thread(hilo_lector, l, url).detach();
}

void icy_parar(void)
{
    std::shared_ptr<Lector> l;
    {
        std::lock_guard<std::mutex> g(cerrojo_actual);
        l.swap(lector_actual);
    }
    if (l) {
        l->parar = true;
        Conexion c = l->conexion.exchange(CONEXION_NULA);
        if (c != CONEXION_NULA) {
            cerrar_conexion(c); /* despierta al hilo si está esperando datos */
        }
    }
}

std::string icy_titulo(void)
{
    std::shared_ptr<Lector> l;
    {
        std::lock_guard<std::mutex> g(cerrojo_actual);
        l = lector_actual;
    }
    if (!l) {
        return "";
    }
    std::lock_guard<std::mutex> g(l->cerrojo);
    return l->titulo;
}
