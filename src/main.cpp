#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include "config.h"
#include "dialogo.h"
#include "dvr.h"
#include "icono.h"
#include "visor.h"

#ifndef SDK_LIB_DIR
#define SDK_LIB_DIR "./sdk/lib"
#endif

static void mostrarAyuda(const char *programa)
{
    printf("Uso: %s [opciones]\n\n", programa);
    printf("  --config   abre el formulario de conexión aunque ya exista configuración\n");
    printf("  --debug    muestra en la terminal la información del equipo y sus canales\n");
    printf("  --help     muestra esta ayuda\n\n");
    printf("Configuración: %s\n", rutaConfig().c_str());
    printf("Ruta del SDK: variable VISOR_HIK_SDK o %s\n", SDK_LIB_DIR);
    printf("\nVisor HIK no está afiliado ni respaldado por Hikvision. Hikvision y Hik-Connect son\n");
    printf("marcas registradas de Hangzhou Hikvision Digital Technology Co., Ltd.\n");
}

int main(int argc, char *argv[])
{
    // el SDK y GTK usan hilos con X11; PlayCtrl solo funciona sobre X11 o XWayland
    XInitThreads();
    setenv("GDK_BACKEND", "x11", 1);

    bool forzarDialogo = false;
    bool debug = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0) forzarDialogo = true;
        else if (strcmp(argv[i], "--debug") == 0) debug = true;
        else if (strcmp(argv[i], "--help") == 0) {
            mostrarAyuda(argv[0]);
            return 0;
        }
    }

    // el nombre fija la clase de las ventanas de GTK, igual que en el .desktop
    g_set_prgname("visor-hik");
    if (!gtk_init_check(&argc, &argv)) {
        printf("No se pudo iniciar la interfaz gráfica\n");
        return 1;
    }
    iconoIniciar();

    const char *rutaSdk = getenv("VISOR_HIK_SDK");
    if (!dvrInicializar(rutaSdk && *rutaSdk ? rutaSdk : SDK_LIB_DIR)) {
        printf("No se pudo inicializar el SDK de Hikvision\n");
        return 1;
    }

    Config cfg;
    bool hayConfig = cargarConfig(cfg);
    std::string mensaje;
    NET_DVR_DEVICEINFO_V40 info;
    LONG uid = -1;

    // pide los datos hasta que el login funcione o el usuario cancele
    while (uid < 0) {
        if (!hayConfig || forzarDialogo || !mensaje.empty()) {
            if (!mostrarDialogo(cfg, mensaje)) {
                dvrFinalizar();
                return 0;
            }
            if (!guardarConfig(cfg))
                printf("No se pudo guardar %s\n", rutaConfig().c_str());
            hayConfig = true;
            forzarDialogo = false;
        }
        uid = dvrLogin(cfg.ip, cfg.puerto, cfg.usuario, cfg.password, info, mensaje);
        if (uid < 0) printf("%s\n", mensaje.c_str());
    }

    std::vector<int> canales = parsearCanales(cfg.canales);
    if (canales.empty()) {
        canales = dvrDetectarCanales(uid, info, debug);
    } else if (debug) {
        printf("Equipo: %s, canales de la configuración: %s\n", dvrSerie(info).c_str(), cfg.canales.c_str());
    }

    int resultado = 0;
    if (canales.empty()) {
        printf("No se encontraron cámaras. Prueba con --debug o indica los canales con --config\n");
        resultado = 1;
    } else {
        std::string titulo = "Visor HIK - " + cfg.ip;
        resultado = ejecutarVisor(uid, canales, cfg.clave, titulo);
    }

    NET_DVR_Logout(uid);
    dvrFinalizar();
    return resultado;
}
