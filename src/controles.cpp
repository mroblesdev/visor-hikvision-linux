#include <stdio.h>
#include <string.h>
#include <string>
#include "controles.h"

#define ALTO_BOTON 26
#define RELLENO 3
#define ESPACIO 2
#define ANCHO_SEPARADOR 11
#define ANCHO_ESTADO 20
#define MARGEN_TEXTO 6
#define MARGEN_ETIQUETA 6
#define ANCHO_ICONO_ETIQUETA 26

#define COLOR_FONDO 0x1c1c1c
#define COLOR_ENCIMA 0x3a3a3a
#define COLOR_ACTIVO 0x2f6fd0
#define COLOR_ICONO 0xf0f0f0
#define COLOR_INACTIVO 0x6a6a6a
#define COLOR_LINEA 0x505050
#define COLOR_AYUDA 0x303030
#define COLOR_AVISO 0xe6b422

static Display *dpy;
static GC gc;
static XFontStruct *fuente = NULL;
static XFontStruct *fuenteChica = NULL;
static Window ayuda = 0;
static Window padreAyuda = 0;
static char textoAyuda[96];

// las fuentes básicas de X usan Latin-1; convierte acentos y ñ desde UTF-8 y omite lo demás
static std::string aLatin1(const char *s)
{
    std::string r;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p == 0xC2 || *p == 0xC3) && p[1]) {
            r += (char)(((*p & 0x03) << 6) | (p[1] & 0x3F));
            p++;
        } else if (*p < 0x80) {
            r += (char)*p;
        }
    }
    return r;
}

static XFontStruct *cargarFuente(const char *nombre)
{
    XFontStruct *f = XLoadQueryFont(dpy, nombre);
    return f ? f : XLoadQueryFont(dpy, "fixed");
}

bool controlesIniciar(Display *d)
{
    dpy = d;
    fuente = cargarFuente("-misc-fixed-bold-r-normal--18-*-*-*-*-*-iso8859-1");
    fuenteChica = cargarFuente("-misc-fixed-bold-r-normal--13-*-*-*-*-*-iso8859-1");
    gc = XCreateGC(dpy, DefaultRootWindow(dpy), 0, NULL);
    return fuente && fuenteChica;
}

void controlesFinalizar()
{
    if (fuente) XFreeFont(dpy, fuente);
    if (fuenteChica) XFreeFont(dpy, fuenteChica);
    fuente = fuenteChica = NULL;
    XFreeGC(dpy, gc);
}

static int anchoTexto(XFontStruct *f, const std::string &t)
{
    return XTextWidth(f, t.c_str(), t.size());
}

static void escribir(Drawable d, XFontStruct *f, int x, int y, const std::string &t, unsigned long color)
{
    XSetFont(dpy, gc, f->fid);
    XSetForeground(dpy, gc, color);
    XDrawString(dpy, d, gc, x, y, t.c_str(), t.size());
}

static void linea(Drawable d, int x1, int y1, int x2, int y2)
{
    XDrawLine(dpy, d, gc, x1, y1, x2, y2);
}

static void triangulo(Drawable d, int x1, int y1, int x2, int y2, int x3, int y3)
{
    XPoint p[3] = {{(short)x1, (short)y1}, {(short)x2, (short)y2}, {(short)x3, (short)y3}};
    XFillPolygon(dpy, d, gc, p, 3, Convex, CoordModeOrigin);
}

// dibuja un ícono de unos 13 px centrado en (cx, cy)
static void dibujarIcono(Drawable d, int icono, int cx, int cy, unsigned long color, unsigned long extra)
{
    XSetForeground(dpy, gc, color);
    XSetLineAttributes(dpy, gc, 2, LineSolid, CapRound, JoinRound);
    switch (icono) {
    case IC_MENOS:
        linea(d, cx - 5, cy, cx + 5, cy);
        break;
    case IC_MAS:
        linea(d, cx - 5, cy, cx + 5, cy);
        linea(d, cx, cy - 5, cx, cy + 5);
        break;
    case IC_RESTABLECER:
        // flecha circular: arco de 270 grados con la punta hacia arriba a la derecha
        XDrawArc(dpy, d, gc, cx - 5, cy - 5, 10, 10, 90 * 64, 270 * 64);
        triangulo(d, cx + 2, cy + 1, cx + 8, cy + 1, cx + 5, cy - 3);
        break;
    case IC_FOTO:
        XDrawRectangle(dpy, d, gc, cx - 7, cy - 3, 14, 9);
        XFillRectangle(dpy, d, gc, cx - 2, cy - 5, 5, 2);
        XDrawArc(dpy, d, gc, cx - 3, cy - 1, 6, 6, 0, 360 * 64);
        break;
    case IC_GRANDE:
        // cuatro esquinas hacia afuera
        linea(d, cx - 6, cy - 6, cx - 2, cy - 6);
        linea(d, cx - 6, cy - 6, cx - 6, cy - 2);
        linea(d, cx + 6, cy - 6, cx + 2, cy - 6);
        linea(d, cx + 6, cy - 6, cx + 6, cy - 2);
        linea(d, cx - 6, cy + 6, cx - 2, cy + 6);
        linea(d, cx - 6, cy + 6, cx - 6, cy + 2);
        linea(d, cx + 6, cy + 6, cx + 2, cy + 6);
        linea(d, cx + 6, cy + 6, cx + 6, cy + 2);
        break;
    case IC_MOSAICO:
        XFillRectangle(dpy, d, gc, cx - 6, cy - 6, 5, 5);
        XFillRectangle(dpy, d, gc, cx + 1, cy - 6, 5, 5);
        XFillRectangle(dpy, d, gc, cx - 6, cy + 1, 5, 5);
        XFillRectangle(dpy, d, gc, cx + 1, cy + 1, 5, 5);
        break;
    case IC_ANTERIOR:
        linea(d, cx - 5, cy - 5, cx - 5, cy + 5);
        triangulo(d, cx + 5, cy - 6, cx + 5, cy + 6, cx - 3, cy);
        break;
    case IC_SIGUIENTE:
        linea(d, cx + 5, cy - 5, cx + 5, cy + 5);
        triangulo(d, cx - 5, cy - 6, cx - 5, cy + 6, cx + 3, cy);
        break;
    case IC_PLAY:
        triangulo(d, cx - 4, cy - 6, cx - 4, cy + 6, cx + 6, cy);
        break;
    case IC_PAUSA:
        XFillRectangle(dpy, d, gc, cx - 5, cy - 6, 3, 12);
        XFillRectangle(dpy, d, gc, cx + 2, cy - 6, 3, 12);
        break;
    case IC_ESTADO:
        XSetForeground(dpy, gc, extra);
        XFillArc(dpy, d, gc, cx - 5, cy - 5, 10, 10, 0, 360 * 64);
        break;
    case IC_AVISO:
        XSetForeground(dpy, gc, extra);
        triangulo(d, cx, cy - 9, cx - 10, cy + 8, cx + 10, cy + 8);
        XSetForeground(dpy, gc, COLOR_FONDO);
        linea(d, cx, cy - 3, cx, cy + 2);
        linea(d, cx, cy + 5, cx, cy + 5);
        break;
    }
    XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);
}

void barraCrear(Barra *b, Window padre)
{
    memset(b, 0, sizeof(*b));
    b->ventana = XCreateSimpleWindow(dpy, padre, 0, 0, 10, ALTO_BARRA, 0, 0, COLOR_FONDO);
    XSelectInput(dpy, b->ventana, ExposureMask | ButtonPressMask | PointerMotionMask |
                 EnterWindowMask | LeaveWindowMask);
    b->encima = -1;
    b->h = ALTO_BARRA;
}

void barraLimpiar(Barra *b)
{
    b->n = 0;
}

Boton *barraAgregar(Barra *b, int accion, int icono, const char *texto, const char *ayuda)
{
    static Boton sobrante;
    if (b->n >= MAX_BOTONES) return &sobrante;
    Boton *bt = &b->botones[b->n++];
    memset(bt, 0, sizeof(*bt));
    bt->accion = accion;
    bt->icono = icono;
    bt->ayuda = ayuda;
    if (texto) snprintf(bt->texto, sizeof(bt->texto), "%s", texto);
    return bt;
}

void barraSeparador(Barra *b)
{
    barraAgregar(b, 0, IC_NINGUNO, NULL, NULL)->separador = true;
}

// calcula la posición de cada botón y el ancho de la barra
void barraAcomodar(Barra *b, int anchoMinimo)
{
    int x = RELLENO;
    for (int i = 0; i < b->n; i++) {
        Boton *bt = &b->botones[i];
        if (bt->separador) bt->w = ANCHO_SEPARADOR;
        else if (bt->texto[0]) bt->w = anchoTexto(fuenteChica, aLatin1(bt->texto)) + 2 * MARGEN_TEXTO;
        else if (bt->icono == IC_ESTADO) bt->w = ANCHO_ESTADO;
        else bt->w = ALTO_BOTON;
        bt->x = x;
        x += bt->w + ESPACIO;
    }
    b->w = x - ESPACIO + RELLENO;
    if (b->w < anchoMinimo) b->w = anchoMinimo;
    if (b->encima >= b->n) b->encima = -1;
}

void barraMostrar(Barra *b, int x, int y)
{
    XMoveResizeWindow(dpy, b->ventana, x, y, b->w, b->h);
    XMapWindow(dpy, b->ventana);
    b->visible = true;
    barraDibujar(b);
}

void barraOcultar(Barra *b)
{
    if (!b->visible) return;
    XUnmapWindow(dpy, b->ventana);
    b->visible = false;
    b->encima = -1;
}

// se dibuja en un pixmap y se copia de una vez para que no parpadee
void barraDibujar(Barra *b)
{
    if (!b->visible) return;
    Pixmap p = XCreatePixmap(dpy, b->ventana, b->w, b->h, DefaultDepth(dpy, DefaultScreen(dpy)));
    XSetForeground(dpy, gc, COLOR_FONDO);
    XFillRectangle(dpy, p, gc, 0, 0, b->w, b->h);

    for (int i = 0; i < b->n; i++) {
        Boton *bt = &b->botones[i];
        int y = RELLENO;
        int cx = bt->x + bt->w / 2;
        int cy = y + ALTO_BOTON / 2;
        if (bt->separador) {
            XSetForeground(dpy, gc, COLOR_LINEA);
            linea(p, cx, y + 6, cx, y + ALTO_BOTON - 6);
            continue;
        }
        bool clicable = bt->accion > 0 && !bt->inactivo;
        unsigned long fondo = COLOR_FONDO;
        if (bt->activo) fondo = COLOR_ACTIVO;
        else if (i == b->encima && clicable) fondo = COLOR_ENCIMA;
        if (fondo != COLOR_FONDO) {
            XSetForeground(dpy, gc, fondo);
            XFillRectangle(dpy, p, gc, bt->x, y, bt->w, ALTO_BOTON);
        }
        unsigned long color = bt->inactivo ? COLOR_INACTIVO : COLOR_ICONO;
        if (bt->texto[0]) {
            std::string t = aLatin1(bt->texto);
            // los textos que se pueden presionar (HD, SD, tiempo) llevan un recuadro
            if (bt->accion > 0 && !bt->activo) {
                XSetForeground(dpy, gc, color);
                XDrawRectangle(dpy, p, gc, bt->x + 2, y + 4, bt->w - 5, ALTO_BOTON - 9);
            }
            int tx = bt->x + (bt->w - anchoTexto(fuenteChica, t)) / 2;
            int ty = cy + (fuenteChica->ascent - fuenteChica->descent) / 2;
            escribir(p, fuenteChica, tx, ty, t, color);
        } else {
            dibujarIcono(p, bt->icono, cx, cy, color, bt->color);
        }
    }
    XCopyArea(dpy, p, b->ventana, gc, 0, 0, b->w, b->h, 0, 0);
    XFreePixmap(dpy, p);
}

// regresa el botón bajo el cursor, o -1 si es un separador o un texto sin tooltip
int barraBotonEn(Barra *b, int x, int y)
{
    if (y < RELLENO || y >= RELLENO + ALTO_BOTON) return -1;
    for (int i = 0; i < b->n; i++) {
        Boton *bt = &b->botones[i];
        if (bt->separador || (bt->accion == 0 && !bt->ayuda)) continue;
        if (x >= bt->x && x < bt->x + bt->w) return i;
    }
    return -1;
}

Window crearEtiqueta(Window padre)
{
    Window w = XCreateSimpleWindow(dpy, padre, 0, 0, 10, 10, 0, 0, COLOR_FONDO);
    XSelectInput(dpy, w, ExposureMask);
    return w;
}

void medirEtiqueta(const char *texto, int icono, int *w, int *h)
{
    *w = anchoTexto(fuente, aLatin1(texto)) + 2 * MARGEN_ETIQUETA + (icono ? ANCHO_ICONO_ETIQUETA : 0);
    *h = fuente->ascent + fuente->descent + 2 * MARGEN_ETIQUETA;
}

void dibujarEtiqueta(Window w, const char *texto, int icono)
{
    XClearWindow(dpy, w);
    int x = MARGEN_ETIQUETA;
    int h = fuente->ascent + fuente->descent + 2 * MARGEN_ETIQUETA;
    if (icono) {
        dibujarIcono(w, icono, x + 10, h / 2, COLOR_ICONO, COLOR_AVISO);
        x += ANCHO_ICONO_ETIQUETA;
    }
    escribir(w, fuente, x, MARGEN_ETIQUETA + fuente->ascent, aLatin1(texto), COLOR_ICONO);
}

void ayudaCrear(Window padre)
{
    padreAyuda = padre;
    ayuda = XCreateSimpleWindow(dpy, padre, 0, 0, 10, 10, 1, COLOR_LINEA, COLOR_AYUDA);
    XSelectInput(dpy, ayuda, ExposureMask);
}

void ayudaDibujar()
{
    XClearWindow(dpy, ayuda);
    escribir(ayuda, fuenteChica, 6, 4 + fuenteChica->ascent, aLatin1(textoAyuda), COLOR_ICONO);
}

// muestra el tooltip debajo del botón, o arriba si no cabe
void ayudaMostrar(Barra *b, int boton, int anchoPadre, int altoPadre)
{
    Boton *bt = &b->botones[boton];
    if (!bt->ayuda) return;
    snprintf(textoAyuda, sizeof(textoAyuda), "%s", bt->ayuda);
    int w = anchoTexto(fuenteChica, aLatin1(textoAyuda)) + 12;
    int h = fuenteChica->ascent + fuenteChica->descent + 8;

    int px, py;
    Window hijo;
    XTranslateCoordinates(dpy, b->ventana, padreAyuda, bt->x, 0, &px, &py, &hijo);
    int x = px + bt->w / 2 - w / 2;
    int y = py + b->h + 4;
    if (y + h > altoPadre) y = py - h - 6;
    if (x + w > anchoPadre - 4) x = anchoPadre - w - 4;
    if (x < 2) x = 2;

    XMoveResizeWindow(dpy, ayuda, x, y, w, h);
    XMapRaised(dpy, ayuda);
    ayudaDibujar();
}

void ayudaOcultar()
{
    if (ayuda) XUnmapWindow(dpy, ayuda);
}

Window ayudaVentana()
{
    return ayuda;
}
