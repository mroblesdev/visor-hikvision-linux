#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <vector>
#include <glib.h>
#include "camara.h"
#include "playm4.h"

// avisa a PlayCtrl que cambió el tamaño de la ventana, si la versión lo soporta
typedef int (*FnCambioVentana)(int);
static FnCambioVentana fnCambioVentana = NULL;

static LONG g_uid = -1;
static char g_clave[64];
static bool g_cifrado = false;
static Camara *g_cams = NULL;
static int g_nCams = 0;
static volatile long g_caidoDesde = 0;

long ahoraMs()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

// recibe el stream de cada cámara y lo manda a su decodificador
static void CALLBACK alRecibirDatos(LONG handle, DWORD tipo, BYTE *buf, DWORD tam, void *user)
{
    Camara *c = (Camara *)user;
    c->ultimoDato = ahoraMs();
    c->reconectando = false;
    if (tipo == NET_DVR_SYSHEAD) {
        if (c->port >= 0) return;
        int port;
        if (!PlayM4_GetPort(&port)) return;
        PlayM4_SetStreamOpenMode(port, PLAYM4_STREAM_REALTIME);
        if (!PlayM4_OpenStream(port, buf, tam, 4 * 1024 * 1024)) {
            PlayM4_FreePort(port);
            return;
        }
        if (g_cifrado) PlayM4_SetSecretKey(port, 1, g_clave, 128);
        PlayM4_Play(port, c->video);
        c->port = port;
    } else if (tipo == NET_DVR_STREAMDATA && c->port >= 0) {
        PlayM4_InputData(c->port, buf, tam);
    }
}

static void marcarReconectando(LONG handle, bool valor)
{
    for (int i = 0; i < g_nCams; i++)
        if (g_cams[i].hVivo == handle) g_cams[i].reconectando = valor;
}

// el SDK avisa aquí cuando se pierde o se recupera la conexión; corre en otro hilo
static void CALLBACK alExcepcion(DWORD tipo, LONG uid, LONG handle, void *user)
{
    switch (tipo) {
    case EXCEPTION_EXCHANGE:
    case EXCEPTION_RELOGIN:
        if (!g_caidoDesde) {
            g_caidoDesde = ahoraMs();
            printf("Se perdió la conexión con el equipo, reintentando...\n");
        }
        break;
    case RESUME_EXCHANGE:
    case RELOGIN_SUCCESS:
        dvrMarcarConectado();
        break;
    case EXCEPTION_PREVIEW:
    case EXCEPTION_RECONNECT:
        marcarReconectando(handle, true);
        break;
    case PREVIEW_RECONNECTSUCCESS:
        marcarReconectando(handle, false);
        break;
    }
}

void camarasIniciar(LONG uid, const std::string &clave, Camara *cams, int n)
{
    g_uid = uid;
    memset(g_clave, 0, sizeof(g_clave));
    strncpy(g_clave, clave.c_str(), sizeof(g_clave) - 1);
    g_cifrado = !clave.empty();
    g_cams = cams;
    g_nCams = n;
    g_caidoDesde = 0;

    fnCambioVentana = (FnCambioVentana)dlsym(RTLD_DEFAULT, "PlayM4_WndResolutionChange");
    if (!fnCambioVentana)
        printf("PlayM4_WndResolutionChange no disponible, el zoom reiniciará el video\n");
    NET_DVR_SetExceptionCallBack_V30(0, NULL, alExcepcion, NULL);
}

void camarasFinalizar()
{
    NET_DVR_SetExceptionCallBack_V30(0, NULL, NULL, NULL);
    g_nCams = 0;
}

long dvrCaidoDesde()
{
    return g_caidoDesde;
}

void dvrMarcarConectado()
{
    if (!g_caidoDesde) return;
    g_caidoDesde = 0;
    printf("Conexión con el equipo recuperada\n");
}

static LONG abrirStream(Camara *c, int stream)
{
    NET_DVR_PREVIEWINFO pv;
    memset(&pv, 0, sizeof(pv));
    pv.lChannel = c->canal;
    pv.dwStreamType = stream;
    pv.dwLinkMode = 0;
    pv.bBlocked = 1;
    c->stream = stream;
    c->port = -1;
    return NET_DVR_RealPlay_V40(g_uid, &pv, alRecibirDatos, c);
}

// inicia la vista en vivo; si el equipo no tiene substream usa el principal
bool iniciarCanal(Camara *c, int stream)
{
    long ahora = ahoraMs();
    c->ultimoIntento = ahora;
    if (!c->ultimoDato) c->ultimoDato = ahora;
    c->reconectando = false;

    c->hVivo = abrirStream(c, stream);
    if (c->hVivo < 0 && stream == STREAM_SUB)
        c->hVivo = abrirStream(c, STREAM_PRINCIPAL);
    if (c->hVivo < 0) {
        // al reintentar no se repite el mismo mensaje cada vez
        if (!c->falloAvisado) printf("Canal %d: error %u\n", c->canal, NET_DVR_GetLastError());
        c->falloAvisado = true;
        return false;
    }
    if (c->falloAvisado) printf("Canal %d: reconectado\n", c->canal);
    c->falloAvisado = false;
    return true;
}

// detiene la vista en vivo y libera el decodificador
void detenerCanal(Camara *c)
{
    if (c->hVivo >= 0) NET_DVR_StopRealPlay(c->hVivo);
    c->hVivo = -1;
    if (c->port >= 0) {
        PlayM4_Stop(c->port);
        PlayM4_CloseStream(c->port);
        PlayM4_FreePort(c->port);
    }
    c->port = -1;
}

void reiniciarCanal(Camara *c, int stream)
{
    detenerCanal(c);
    iniciarCanal(c, stream);
}

// hace que el decodificador tome el nuevo tamaño de la ventana
void refrescarCanal(Camara *c)
{
    if (c->hVivo < 0) return;
    if (fnCambioVentana) {
        if (c->port >= 0) fnCambioVentana(c->port);
    } else {
        reiniciarCanal(c, c->stream);
    }
}

static std::string carpetaFotos()
{
    const char *imagenes = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
    if (imagenes) return std::string(imagenes) + "/visor-hik";
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/visor-hik";
}

// guarda el cuadro que se está viendo como JPG en ~/Imágenes/visor-hik, solo legible por el usuario
bool tomarFoto(Camara *c, std::string &ruta)
{
    int port = c->port;
    if (port < 0) return false;
    int w = 0, h = 0;
    if (!PlayM4_GetPictureSize(port, &w, &h) || w <= 0 || h <= 0) return false;

    std::vector<unsigned char> jpg((size_t)w * h * 3);
    unsigned int tam = 0;
    if (!PlayM4_GetJPEG(port, jpg.data(), jpg.size(), &tam) || tam == 0) {
        printf("Canal %d: no se pudo obtener la imagen, error %u\n", c->canal, PlayM4_GetLastError(port));
        return false;
    }

    std::string carpeta = carpetaFotos();
    g_mkdir_with_parents(carpeta.c_str(), 0700);
    char fecha[32];
    time_t t = time(NULL);
    struct tm local;
    localtime_r(&t, &local);
    strftime(fecha, sizeof(fecha), "%Y-%m-%d_%H-%M-%S", &local);

    // si ya hay una foto en el mismo segundo se agrega un número; O_EXCL evita pisar un archivo existente
    int fd = -1;
    for (int i = 1; fd < 0 && i < 100; i++) {
        char nombre[96];
        if (i == 1) snprintf(nombre, sizeof(nombre), "camara%d_%s.jpg", c->canal, fecha);
        else snprintf(nombre, sizeof(nombre), "camara%d_%s_%d.jpg", c->canal, fecha, i);
        ruta = carpeta + "/" + nombre;
        fd = open(ruta.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0 && errno != EEXIST) return false;
    }
    if (fd < 0) return false;
    bool ok = write(fd, jpg.data(), tam) == (ssize_t)tam;
    close(fd);
    return ok;
}
