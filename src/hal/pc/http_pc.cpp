/**
 * Descargas http en el simulador (para el tiempo y la ubicación): una
 * petición HTTP/1.0 por un socket, sin librerías. Con HTTP/1.0 el servidor
 * manda el cuerpo entero y cierra, sin trozos («chunked»).
 */
#include "hal.h"

#include <stdlib.h>
#include <string.h>
#include <string>
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

char *hal_http_get(const char *url)
{
#ifdef _WIN32
    static bool winsock_listo = false;
    if (!winsock_listo) {
        WSADATA datos;
        winsock_listo = WSAStartup(MAKEWORD(2, 2), &datos) == 0;
    }
#endif
    std::string u = url;
    const std::string prefijo = "http://";
    if (u.compare(0, prefijo.size(), prefijo) != 0) {
        return NULL;
    }
    std::string resto = u.substr(prefijo.size());
    size_t barra = resto.find('/');
    std::string host = resto.substr(0, barra);
    std::string ruta = barra == std::string::npos ? "/" : resto.substr(barra);

    struct addrinfo pista = {}, *resultado = NULL;
    pista.ai_family = AF_UNSPEC;
    pista.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), "80", &pista, &resultado) != 0) {
        return NULL;
    }
    Conexion c = CONEXION_NULA;
    for (struct addrinfo *a = resultado; a != NULL; a = a->ai_next) {
        c = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (c != CONEXION_NULA && connect(c, a->ai_addr, (int)a->ai_addrlen) == 0) {
            break;
        }
        if (c != CONEXION_NULA) {
            cerrar_conexion(c);
        }
        c = CONEXION_NULA;
    }
    freeaddrinfo(resultado);
    if (c == CONEXION_NULA) {
        return NULL;
    }

    std::string peticion = "GET " + ruta + " HTTP/1.0\r\nHost: " + host +
                           "\r\nUser-Agent: RadioMundial\r\nConnection: close\r\n\r\n";
    send(c, peticion.c_str(), (int)peticion.size(), 0);
    std::string respuesta;
    char bufer[2048];
    int n;
    while ((n = recv(c, bufer, sizeof(bufer), 0)) > 0) {
        respuesta.append(bufer, n);
    }
    cerrar_conexion(c);

    /* Solo vale una respuesta 200; el cuerpo va tras la línea en blanco */
    size_t fin_cabecera = respuesta.find("\r\n\r\n");
    if (respuesta.compare(0, 12, "HTTP/1.1 200") != 0 && respuesta.compare(0, 12, "HTTP/1.0 200") != 0) {
        return NULL;
    }
    if (fin_cabecera == std::string::npos) {
        return NULL;
    }
    return strdup(respuesta.c_str() + fin_cabecera + 4);
}

void hal_segundo_plano(void (*tarea)(void))
{
    std::thread(tarea).detach();
}
