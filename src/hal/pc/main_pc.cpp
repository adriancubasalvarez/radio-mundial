/**
 * Punto de entrada del simulador en el PC.
 */
#include "app.h"
#include "hal.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifdef _WIN32
    /* Que la consola muestre bien las tildes (los textos van en UTF-8) */
    SetConsoleOutputCP(CP_UTF8);
#endif
    /* Mensajes a la consola al instante, sin esperar a llenar el búfer */
    setvbuf(stdout, NULL, _IONBF, 0);

    app_inicio();
    while (app_bucle()) {
    }
    audio_parar();
    return 0;
}
