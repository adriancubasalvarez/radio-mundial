/**
 * Audio del simulador: lanza "ffplay" (de FFmpeg) en segundo plano con la
 * URL de la emisora.
 *
 * Es solo para oír algo en el PC: no prueba la librería de audio de la placa,
 * así que una emisora que suene aquí podría fallar en el ESP32.
 * Requisito: ffplay en el PATH (https://ffmpeg.org).
 *
 * ffplay no dice cuándo empieza a sonar, así que se considera «conectando»
 * durante sus primeros segundos de vida. Con una URL rota tarda unos 5 s en
 * rendirse y terminar; entonces el estado pasa a AUDIO_ERROR.
 */
#include "hal.h"
#include "icy_pc.h"

#include <stdio.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
static PROCESS_INFORMATION proceso;
static bool hay_proceso = false;
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
static pid_t proceso = 0;
#endif

/* Tiempo que se da a ffplay para conectar antes de darlo por «sonando» */
#define TIEMPO_CONEXION_MS 6000

static uint8_t volumen_actual = 70;
static std::string url_actual;
static bool reproduccion_pedida = false; /* el usuario quiere oír algo */
static bool icy_activo = false;          /* hay un lector de títulos en marcha */
static uint32_t inicio_ms = 0;

/* true si el proceso ffplay sigue vivo */
static bool proceso_vivo(void)
{
#ifdef _WIN32
    return hay_proceso && WaitForSingleObject(proceso.hProcess, 0) == WAIT_TIMEOUT;
#else
    return proceso > 0 && waitpid(proceso, NULL, WNOHANG) == 0;
#endif
}

static void terminar_proceso(void)
{
#ifdef _WIN32
    if (hay_proceso) {
        TerminateProcess(proceso.hProcess, 0);
        WaitForSingleObject(proceso.hProcess, 1000);
        CloseHandle(proceso.hProcess);
        CloseHandle(proceso.hThread);
        hay_proceso = false;
    }
#else
    if (proceso > 0) {
        kill(proceso, SIGTERM);
        waitpid(proceso, NULL, 0);
        proceso = 0;
    }
#endif
}

void audio_reproducir(const char *url)
{
    terminar_proceso();
    /* El lector de títulos solo se reinicia si cambia la emisora (al cambiar
     * el volumen se relanza ffplay con la misma URL y el título se conserva) */
    if (!icy_activo || url_actual != url) {
        icy_empezar(url);
        icy_activo = true;
    }
    url_actual = url;
    reproduccion_pedida = true;
    inicio_ms = hal_millis();

    /* -nodisp: sin ventana de vídeo; -autoexit: termina si se corta el stream */
    std::string volumen = std::to_string(volumen_actual);

#ifdef _WIN32
    std::string orden = "ffplay -nodisp -autoexit -loglevel error -volume " + volumen + " \"" + url + "\"";
    STARTUPINFOA inicio = {};
    inicio.cb = sizeof(inicio);
    if (CreateProcessA(NULL, &orden[0], NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &inicio, &proceso)) {
        hay_proceso = true;
    } else {
        fprintf(stderr, "No se pudo lanzar ffplay (¿está en el PATH?)\n");
    }
#else
    proceso = fork();
    if (proceso == 0) {
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "error",
               "-volume", volumen.c_str(), url, (char *)NULL);
        _exit(127); /* solo llega aquí si ffplay no existe */
    }
#endif
}

void audio_parar(void)
{
    reproduccion_pedida = false;
    terminar_proceso();
    icy_parar();
    icy_activo = false;
}

const char *audio_titulo(void)
{
    static std::string titulo;
    titulo = reproduccion_pedida ? icy_titulo() : "";
    return titulo.c_str();
}

EstadoAudio audio_estado(void)
{
    if (!reproduccion_pedida) {
        return AUDIO_PARADO;
    }
    if (!proceso_vivo()) {
        return AUDIO_ERROR;
    }
    return hal_millis() - inicio_ms < TIEMPO_CONEXION_MS ? AUDIO_CONECTANDO : AUDIO_SONANDO;
}

void audio_volumen(uint8_t volumen)
{
    /* ffplay no permite cambiar el volumen desde fuera una vez lanzado:
     * si está sonando, se relanza con el volumen nuevo (hay un corte breve). */
    volumen_actual = volumen > 100 ? 100 : volumen;
    if (reproduccion_pedida && proceso_vivo()) {
        std::string url = url_actual;
        audio_reproducir(url.c_str());
        inicio_ms -= TIEMPO_CONEXION_MS; /* ya sonaba: no volver a «conectando» */
    }
}
