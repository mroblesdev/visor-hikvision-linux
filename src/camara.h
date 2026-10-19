#pragma once
#include <string>
#include <X11/Xlib.h>
#include "HCNetSDK.h"
#include "controles.h"

#define STREAM_PRINCIPAL 0
#define STREAM_SUB 1

struct Camara {
    int canal;
    int stream;
    int streamMosaico;
    int streamGrande;
    volatile int port;
    volatile LONG hVivo;
    volatile long ultimoDato;
    volatile bool reconectando;
    long ultimoIntento;
    bool falloAvisado;
    Window marco;
    Window video;
    Window etiqueta;
    Window aviso;
    Window entrada;
    Barra barra;
    int x, y, w, h;
    int vw, vh;
    double zoom;
    int offX, offY;
    char textoEtiqueta[48];
    long ocultarEtiquetaEn;
    const char *textoAviso;
};

long ahoraMs();

// guarda la sesión y la clave, y registra el aviso de caídas del SDK
void camarasIniciar(LONG uid, const std::string &clave, Camara *cams, int n);
void camarasFinalizar();

bool iniciarCanal(Camara *c, int stream);
void detenerCanal(Camara *c);
void reiniciarCanal(Camara *c, int stream);
void refrescarCanal(Camara *c);
bool tomarFoto(Camara *c, std::string &ruta);

// desde cuándo se perdió la conexión con el equipo, o 0 si está conectado
long dvrCaidoDesde();
void dvrMarcarConectado();
