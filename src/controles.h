#pragma once
#include <X11/Xlib.h>

#define MAX_BOTONES 16
#define ALTO_BARRA 32

enum Icono {
    IC_NINGUNO, IC_MENOS, IC_MAS, IC_RESTABLECER, IC_FOTO, IC_GRANDE, IC_MOSAICO,
    IC_ANTERIOR, IC_SIGUIENTE, IC_PLAY, IC_PAUSA, IC_ESTADO, IC_AVISO
};

// botón dibujado dentro de una barra; con accion 0 solo informa
struct Boton {
    int accion;
    int icono;
    char texto[24];
    const char *ayuda;
    bool activo;
    bool inactivo;
    bool separador;
    unsigned long color;
    int x, w;
};

struct Barra {
    Window ventana;
    Boton botones[MAX_BOTONES];
    int n;
    int encima;
    int w, h;
    bool visible;
};

bool controlesIniciar(Display *d);
void controlesFinalizar();

void barraCrear(Barra *b, Window padre);
void barraLimpiar(Barra *b);
Boton *barraAgregar(Barra *b, int accion, int icono, const char *texto, const char *ayuda);
void barraSeparador(Barra *b);
void barraAcomodar(Barra *b, int anchoMinimo);
void barraMostrar(Barra *b, int x, int y);
void barraOcultar(Barra *b);
void barraDibujar(Barra *b);
int barraBotonEn(Barra *b, int x, int y);

// recuadro con texto (y un ícono opcional) para el nivel de zoom y los avisos
Window crearEtiqueta(Window padre);
void medirEtiqueta(const char *texto, int icono, int *w, int *h);
void dibujarEtiqueta(Window w, const char *texto, int icono);

// tooltip de texto corto para los botones
void ayudaCrear(Window padre);
void ayudaMostrar(Barra *b, int boton, int anchoPadre, int altoPadre);
void ayudaOcultar();
void ayudaDibujar();
Window ayudaVentana();
