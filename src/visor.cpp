#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include "visor.h"
#include "camara.h"
#include "controles.h"
#include "icono.h"

#define MAX_CAMS 64
#define SEPARACION 2
#define DOBLE_CLIC_MS 400
#define MARGEN 8
#define TICK_MS 100
#define TIEMPO_ETIQUETA_MS 1500
#define TIEMPO_CONTROLES_MS 2000
#define TIEMPO_AYUDA_MS 500
#define SIN_SENAL_MS 5000
#define REINTENTO_MS 10000
#define REINICIO_MS 20000
#define SIN_CONEXION_MS 30000
#define ZOOM_PASO 1.25
#define ZOOM_MAX 8.0

enum Accion {
    A_ALEJAR = 1, A_ACERCAR, A_RESTABLECER, A_STREAM, A_FOTO, A_GRANDE, A_VOLVER,
    A_MOSAICO_SD, A_MOSAICO_HD, A_ANTERIOR, A_SIGUIENTE, A_RECORRIDO, A_INTERVALO
};

static const int INTERVALOS[] = {5, 10, 30, 60};
#define N_INTERVALOS 4

static Display *dpy;
static Window g_main;
static Camara cams[MAX_CAMS];
static int nCams = 0;
static int g_grande = -1;
static int anchoMain = 1280, altoMain = 720;

// barras de controles: se muestran al mover el mouse y se ocultan solas
static Barra g_barraSup;
static bool g_controles = false;
static int g_camControles = -1;
static long g_ocultarControlesEn = 0;
static Barra *g_barraEncima = NULL;
static long g_ayudaEn = 0;

// 0 conectado, 1 reconectando, 2 sin conexión
static int g_estadoDvr = 0;

static bool g_recorrido = false;
static int g_intervalo = 1;
static long g_siguienteCambio = 0;

// muestra los errores de X11 sin cerrar el programa
static int alErrorX(Display *d, XErrorEvent *e)
{
    char texto[128];
    XGetErrorText(d, e->error_code, texto, sizeof(texto));
    printf("Aviso X11: %s (ventana 0x%lx)\n", texto, e->resourceid);
    return 0;
}

static int streamDeseado(int i)
{
    return i == g_grande ? cams[i].streamGrande : cams[i].streamMosaico;
}

static void ocultarEtiqueta(Camara *c)
{
    XUnmapWindow(dpy, c->etiqueta);
    c->ocultarEtiquetaEn = 0;
}

// muestra un texto corto por un momento en la esquina superior derecha, debajo de la barra general
static void mostrarEtiqueta(Camara *c, const char *texto)
{
    snprintf(c->textoEtiqueta, sizeof(c->textoEtiqueta), "%s", texto);
    int w, h;
    medirEtiqueta(c->textoEtiqueta, IC_NINGUNO, &w, &h);
    int y = c->y < ALTO_BARRA ? ALTO_BARRA - c->y + MARGEN : MARGEN;
    XMoveResizeWindow(dpy, c->etiqueta, c->w - w - MARGEN, y, w, h);
    XMapWindow(dpy, c->etiqueta);
    dibujarEtiqueta(c->etiqueta, c->textoEtiqueta, IC_NINGUNO);
    c->ocultarEtiquetaEn = ahoraMs() + TIEMPO_ETIQUETA_MS;
}

static void mostrarNivelZoom(Camara *c)
{
    if (c->zoom <= 1.0) {
        if (c->ocultarEtiquetaEn) ocultarEtiqueta(c);
        return;
    }
    char texto[16];
    snprintf(texto, sizeof(texto), "%.1fx", c->zoom);
    mostrarEtiqueta(c, texto);
}

static const char *calcularAviso(Camara *c, long ahora)
{
    bool caido = dvrCaidoDesde() != 0;
    if (c->hVivo < 0) return caido ? "Reconectando..." : "Cámara desconectada";
    if (ahora - c->ultimoDato > SIN_SENAL_MS) return caido || c->reconectando ? "Reconectando..." : "Sin señal";
    return NULL;
}

// coloca el aviso al centro de la cámara, o lo oculta si no hay
static void colocarAviso(Camara *c)
{
    if (!c->textoAviso) {
        XUnmapWindow(dpy, c->aviso);
        return;
    }
    int w, h;
    medirEtiqueta(c->textoAviso, IC_AVISO, &w, &h);
    XMoveResizeWindow(dpy, c->aviso, (c->w - w) / 2, (c->h - h) / 2, w, h);
    XMapWindow(dpy, c->aviso);
    dibujarEtiqueta(c->aviso, c->textoAviso, IC_AVISO);
}

static void armarBarraCamara(int i)
{
    Camara *c = &cams[i];
    Barra *b = &c->barra;
    barraLimpiar(b);
    barraAgregar(b, A_ALEJAR, IC_MENOS, NULL, "Alejar")->inactivo = c->zoom <= 1.0;
    barraAgregar(b, A_ACERCAR, IC_MAS, NULL, "Acercar")->inactivo = c->zoom >= ZOOM_MAX;
    barraAgregar(b, A_RESTABLECER, IC_RESTABLECER, NULL, "Restablecer zoom")->inactivo = c->zoom <= 1.0;
    barraSeparador(b);
    bool hd = c->stream == STREAM_PRINCIPAL;
    barraAgregar(b, A_STREAM, IC_NINGUNO, hd ? "HD" : "SD",
                 hd ? "Calidad alta, cambiar a SD" : "Calidad baja, cambiar a HD");
    barraAgregar(b, A_FOTO, IC_FOTO, NULL, "Tomar foto")->inactivo = c->port < 0;
    if (i == g_grande) barraAgregar(b, A_VOLVER, IC_MOSAICO, NULL, "Regresar al mosaico");
    else barraAgregar(b, A_GRANDE, IC_GRANDE, NULL, "Ver en grande");
    barraAcomodar(b, 0);
}

static void armarBarraSuperior()
{
    static const char *ESTADOS[] = {"Conectado al equipo", "Reconectando con el equipo...", "Sin conexión con el equipo"};
    static const unsigned long COLORES[] = {0x3cc85a, 0xe6b422, 0xe04040};
    Barra *b = &g_barraSup;
    barraLimpiar(b);
    barraAgregar(b, 0, IC_ESTADO, NULL, ESTADOS[g_estadoDvr])->color = COLORES[g_estadoDvr];
    barraSeparador(b);

    if (g_grande < 0) {
        bool todasSD = true, todasHD = true;
        for (int i = 0; i < nCams; i++) {
            if (cams[i].streamMosaico == STREAM_SUB) todasHD = false;
            else todasSD = false;
        }
        barraAgregar(b, A_MOSAICO_SD, IC_NINGUNO, "SD", "Mosaico en calidad baja")->activo = todasSD;
        barraAgregar(b, A_MOSAICO_HD, IC_NINGUNO, "HD", "Mosaico en calidad alta")->activo = todasHD;
    } else {
        barraAgregar(b, A_VOLVER, IC_MOSAICO, NULL, "Regresar al mosaico");
        if (nCams > 1) {
            char texto[40];
            snprintf(texto, sizeof(texto), "Cámara %d / %d", g_grande + 1, nCams);
            barraAgregar(b, A_ANTERIOR, IC_ANTERIOR, NULL, "Cámara anterior");
            barraAgregar(b, 0, IC_NINGUNO, texto, NULL);
            barraAgregar(b, A_SIGUIENTE, IC_SIGUIENTE, NULL, "Cámara siguiente");
        }
    }

    if (nCams > 1) {
        char texto[16];
        snprintf(texto, sizeof(texto), "%d s", INTERVALOS[g_intervalo]);
        barraSeparador(b);
        barraAgregar(b, A_RECORRIDO, g_recorrido ? IC_PAUSA : IC_PLAY, NULL,
                     g_recorrido ? "Detener recorrido" : "Recorrido automático por las cámaras");
        barraAgregar(b, A_INTERVALO, IC_NINGUNO, texto, "Tiempo por cámara en el recorrido");
    }
    barraAcomodar(b, anchoMain);
}

// la barra de la cámara va abajo a la izquierda; en cámaras muy chicas no se muestra
static void colocarBarraCamara(int i)
{
    Camara *c = &cams[i];
    armarBarraCamara(i);
    if (c->barra.w > c->w - 2 * MARGEN || c->barra.h > c->h / 2) {
        barraOcultar(&c->barra);
        return;
    }
    barraMostrar(&c->barra, MARGEN, c->h - c->barra.h - MARGEN);
}

// vuelve a armar las barras visibles con el estado actual
static void actualizarControles()
{
    if (g_controles || g_estadoDvr) {
        armarBarraSuperior();
        barraMostrar(&g_barraSup, 0, 0);
    } else {
        barraOcultar(&g_barraSup);
    }
    if (g_controles && g_camControles >= 0) colocarBarraCamara(g_camControles);
}

// solo se pospone cuando se usa la cámara (clic, zoom, arrastre o botones), no al mover el mouse
static void posponerRecorrido()
{
    if (g_recorrido) g_siguienteCambio = ahoraMs() + INTERVALOS[g_intervalo] * 1000L;
}

// muestra las barras al mover el mouse; i es la cámara bajo el cursor o -1 para conservar la actual
static void mostrarControles(int i)
{
    bool cambio = !g_controles;
    if (i >= 0 && i != g_camControles) {
        if (g_camControles >= 0) barraOcultar(&cams[g_camControles].barra);
        g_camControles = i;
        cambio = true;
    }
    g_controles = true;
    g_ocultarControlesEn = ahoraMs() + TIEMPO_CONTROLES_MS;
    if (cambio) actualizarControles();
}

static void ocultarControles()
{
    if (!g_controles) return;
    g_controles = false;
    if (g_camControles >= 0) barraOcultar(&cams[g_camControles].barra);
    g_camControles = -1;
    g_barraEncima = NULL;
    g_ayudaEn = 0;
    ayudaOcultar();
    actualizarControles();
}

// agranda la ventana de video dentro del marco para hacer el zoom
static void aplicarZoom(Camara *c)
{
    int vw = (int)(c->w * c->zoom);
    int vh = (int)(c->h * c->zoom);
    if (c->offX > 0) c->offX = 0;
    if (c->offY > 0) c->offY = 0;
    if (c->offX < c->w - vw) c->offX = c->w - vw;
    if (c->offY < c->h - vh) c->offY = c->h - vh;
    XMoveResizeWindow(dpy, c->video, c->offX, c->offY, vw, vh);
    if (vw != c->vw || vh != c->vh) {
        c->vw = vw;
        c->vh = vh;
        mostrarNivelZoom(c);
        XSync(dpy, False);
        refrescarCanal(c);
    }
}

static void reiniciarZoom(Camara *c)
{
    c->zoom = 1.0;
    c->offX = 0;
    c->offY = 0;
}

// cambia el zoom manteniendo fijo el punto (mx, my)
static void cambiarZoom(Camara *c, double factor, int mx, int my)
{
    double nuevo = c->zoom * factor;
    if (nuevo < 1.0) nuevo = 1.0;
    if (nuevo > ZOOM_MAX) nuevo = ZOOM_MAX;
    double f = nuevo / c->zoom;
    c->offX = (int)(mx - (mx - c->offX) * f);
    c->offY = (int)(my - (my - c->offY) * f);
    c->zoom = nuevo;
    aplicarZoom(c);
}

// acomoda las cámaras en mosaico o muestra solo la seleccionada
static void acomodar()
{
    int cols = (int)ceil(sqrt((double)nCams));
    int filas = (nCams + cols - 1) / cols;
    int cw = anchoMain / cols;
    int ch = altoMain / filas;

    for (int i = 0; i < nCams; i++) {
        Camara *c = &cams[i];
        if (g_grande >= 0) {
            if (i != g_grande) {
                XUnmapWindow(dpy, c->marco);
                continue;
            }
            c->x = 0;
            c->y = 0;
            c->w = anchoMain;
            c->h = altoMain;
        } else {
            c->x = (i % cols) * cw;
            c->y = (i / cols) * ch;
            c->w = cw - SEPARACION;
            c->h = ch - SEPARACION;
        }
        XMoveResizeWindow(dpy, c->marco, c->x, c->y, c->w, c->h);
        XMoveResizeWindow(dpy, c->entrada, 0, 0, c->w, c->h);
        XMapWindow(dpy, c->marco);
        colocarAviso(c);
        aplicarZoom(c);
    }
    actualizarControles();
    XFlush(dpy);
}

// pasa al mosaico (nuevo = -1) o a una cámara en grande; primero acomoda y después arranca el video
static void cambiarVista(int nuevo)
{
    int anterior = g_grande;
    if (nuevo == anterior) return;
    if (anterior >= 0) {
        detenerCanal(&cams[anterior]);
        reiniciarZoom(&cams[anterior]);
    }
    if (nuevo >= 0) {
        detenerCanal(&cams[nuevo]);
        reiniciarZoom(&cams[nuevo]);
    }
    g_grande = nuevo;
    acomodar();
    XSync(dpy, False);
    if (anterior >= 0) iniciarCanal(&cams[anterior], streamDeseado(anterior));
    if (nuevo >= 0) iniciarCanal(&cams[nuevo], streamDeseado(nuevo));
    actualizarControles();
}

static void volverAlMosaico()
{
    g_recorrido = false;
    cambiarVista(-1);
}

// en el recorrido y con anterior/siguiente se anuncia qué cámara es
static void irACamara(int i)
{
    cambiarVista(i);
    char texto[32];
    snprintf(texto, sizeof(texto), "Cámara %d de %d", i + 1, nCams);
    mostrarEtiqueta(&cams[i], texto);
}

static void cambiarStreamMosaico(int stream)
{
    for (int i = 0; i < nCams; i++) {
        Camara *c = &cams[i];
        if (c->streamMosaico == stream) continue;
        c->streamMosaico = stream;
        if (i != g_grande) reiniciarCanal(c, stream);
    }
}

static void ejecutarAccion(int accion, int i)
{
    Camara *c = i >= 0 ? &cams[i] : NULL;
    switch (accion) {
    case A_ALEJAR:
    case A_ACERCAR:
        if (c) cambiarZoom(c, accion == A_ACERCAR ? ZOOM_PASO : 1 / ZOOM_PASO, c->w / 2, c->h / 2);
        break;
    case A_RESTABLECER:
        if (c) {
            reiniciarZoom(c);
            aplicarZoom(c);
        }
        break;
    case A_STREAM:
        if (c) {
            int nuevo = c->stream == STREAM_PRINCIPAL ? STREAM_SUB : STREAM_PRINCIPAL;
            if (i == g_grande) c->streamGrande = nuevo;
            else c->streamMosaico = nuevo;
            reiniciarCanal(c, nuevo);
        }
        break;
    case A_FOTO:
        if (c) {
            std::string ruta;
            if (tomarFoto(c, ruta)) {
                printf("Foto guardada en %s\n", ruta.c_str());
                mostrarEtiqueta(c, "Foto guardada");
            } else {
                mostrarEtiqueta(c, "No se pudo tomar la foto");
            }
        }
        break;
    case A_GRANDE:
        if (c) cambiarVista(i);
        break;
    case A_VOLVER:
        volverAlMosaico();
        break;
    case A_MOSAICO_SD:
        cambiarStreamMosaico(STREAM_SUB);
        break;
    case A_MOSAICO_HD:
        cambiarStreamMosaico(STREAM_PRINCIPAL);
        break;
    case A_ANTERIOR:
        irACamara((g_grande + nCams - 1) % nCams);
        break;
    case A_SIGUIENTE:
        irACamara((g_grande + 1) % nCams);
        break;
    case A_RECORRIDO:
        g_recorrido = !g_recorrido;
        if (g_recorrido && g_grande < 0) irACamara(0);
        break;
    case A_INTERVALO:
        g_intervalo = (g_intervalo + 1) % N_INTERVALOS;
        break;
    }
    posponerRecorrido();
    actualizarControles();
}

// reintenta los canales caídos; solo uno por vuelta porque cada intento bloquea la ventana
static void reconectar(long ahora)
{
    if (dvrCaidoDesde()) return;
    for (int i = 0; i < nCams; i++) {
        Camara *c = &cams[i];
        bool reintentar = c->hVivo < 0 && ahora - c->ultimoIntento >= REINTENTO_MS;
        bool reiniciar = c->hVivo >= 0 && ahora - c->ultimoDato >= REINICIO_MS &&
                         ahora - c->ultimoIntento >= REINICIO_MS;
        if (!reintentar && !reiniciar) continue;
        reiniciarCanal(c, streamDeseado(i));
        return;
    }
}

// se llama varias veces por segundo: oculta lo que ya cumplió su tiempo y revisa la conexión
static void revisarTiempos(long ahora)
{
    for (int i = 0; i < nCams; i++) {
        Camara *c = &cams[i];
        if (c->ocultarEtiquetaEn && ahora >= c->ocultarEtiquetaEn) ocultarEtiqueta(c);
        const char *aviso = calcularAviso(c, ahora);
        if (aviso != c->textoAviso) {
            c->textoAviso = aviso;
            colocarAviso(c);
        }
    }

    if (g_ayudaEn && ahora >= g_ayudaEn) {
        g_ayudaEn = 0;
        if (g_barraEncima && g_barraEncima->encima >= 0)
            ayudaMostrar(g_barraEncima, g_barraEncima->encima, anchoMain, altoMain);
    }
    if (g_controles && !g_barraEncima && ahora >= g_ocultarControlesEn) ocultarControles();

    // si vuelve a llegar video después de la caída, la conexión ya se recuperó
    long caido = dvrCaidoDesde();
    for (int i = 0; caido && i < nCams; i++) {
        if (cams[i].ultimoDato > caido + 1000) {
            dvrMarcarConectado();
            caido = 0;
        }
    }
    int estado = !caido ? 0 : (ahora - caido < SIN_CONEXION_MS ? 1 : 2);
    if (estado != g_estadoDvr) {
        g_estadoDvr = estado;
        actualizarControles();
    }

    // el decodificador se abre un momento después de arrancar el canal; ahí se habilita la foto
    if (g_controles && g_camControles >= 0) {
        Camara *c = &cams[g_camControles];
        for (int k = 0; k < c->barra.n; k++) {
            Boton *bt = &c->barra.botones[k];
            if (bt->accion == A_FOTO && bt->inactivo != (c->port < 0)) colocarBarraCamara(g_camControles);
        }
    }

    reconectar(ahora);

    if (g_recorrido && ahora >= g_siguienteCambio && g_grande >= 0) {
        irACamara((g_grande + 1) % nCams);
        g_siguienteCambio = ahoraMs() + INTERVALOS[g_intervalo] * 1000L;
        actualizarControles();
    }
}

// espera el siguiente evento de X revisando los tiempos mientras tanto
static void siguienteEvento(XEvent *ev)
{
    static long ultimaRevision = 0;
    while (1) {
        long ahora = ahoraMs();
        if (ahora - ultimaRevision >= TICK_MS) {
            ultimaRevision = ahora;
            revisarTiempos(ahora);
        }
        if (XPending(dpy)) break;

        int fd = ConnectionNumber(dpy);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval espera = {0, TICK_MS * 1000};
        select(fd + 1, &fds, NULL, NULL, &espera);
    }
    XNextEvent(dpy, ev);
}

static void crearVentanas(Camara *c)
{
    int scr = DefaultScreen(dpy);
    c->marco = XCreateSimpleWindow(dpy, g_main, 0, 0, 10, 10, 0, 0, BlackPixel(dpy, scr));
    c->video = XCreateSimpleWindow(dpy, c->marco, 0, 0, 10, 10, 0, 0, BlackPixel(dpy, scr));
    // se crean en orden: lo que va después queda encima del video
    c->etiqueta = crearEtiqueta(c->marco);
    c->aviso = crearEtiqueta(c->marco);
    // capa invisible encima del video para recibir los clics
    c->entrada = XCreateWindow(dpy, c->marco, 0, 0, 10, 10, 0, 0, InputOnly, CopyFromParent, 0, NULL);
    XSelectInput(dpy, c->entrada, ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    // la barra va encima de la capa de entrada para recibir sus propios clics
    barraCrear(&c->barra, c->marco);
    XMapWindow(dpy, c->video);
    XMapWindow(dpy, c->entrada);
    reiniciarZoom(c);
    c->vw = 10;
    c->vh = 10;
    c->hVivo = -1;
    c->port = -1;
    c->streamMosaico = STREAM_SUB;
    c->streamGrande = STREAM_PRINCIPAL;
}

static int buscarCamara(Window w)
{
    for (int i = 0; i < nCams; i++) {
        Camara *c = &cams[i];
        if (c->entrada == w || c->etiqueta == w || c->aviso == w || c->barra.ventana == w) return i;
    }
    return -1;
}

// regresa la barra dueña de la ventana; cam queda con la cámara de la barra o -1 si es la general
static Barra *buscarBarra(Window w, int *cam)
{
    *cam = -1;
    if (w == g_barraSup.ventana) return &g_barraSup;
    int i = buscarCamara(w);
    if (i >= 0 && cams[i].barra.ventana == w) {
        *cam = i;
        return &cams[i].barra;
    }
    return NULL;
}

static void redibujar(Window w)
{
    if (w == g_barraSup.ventana) {
        barraDibujar(&g_barraSup);
        return;
    }
    if (w == ayudaVentana()) {
        ayudaDibujar();
        return;
    }
    int i = buscarCamara(w);
    if (i < 0) return;
    Camara *c = &cams[i];
    if (w == c->etiqueta) dibujarEtiqueta(w, c->textoEtiqueta, IC_NINGUNO);
    else if (w == c->aviso && c->textoAviso) dibujarEtiqueta(w, c->textoAviso, IC_AVISO);
    else if (w == c->barra.ventana) barraDibujar(&c->barra);
}

// actualiza el botón bajo el cursor y programa su tooltip
static void moverSobreBarra(Barra *b, int x, int y)
{
    g_barraEncima = b;
    int antes = b->encima;
    b->encima = barraBotonEn(b, x, y);
    if (b->encima == antes) return;
    barraDibujar(b);
    ayudaOcultar();
    g_ayudaEn = b->encima >= 0 ? ahoraMs() + TIEMPO_AYUDA_MS : 0;
}

static void salirDeBarra(Barra *b)
{
    b->encima = -1;
    barraDibujar(b);
    ayudaOcultar();
    g_ayudaEn = 0;
    if (g_barraEncima == b) g_barraEncima = NULL;
}

int ejecutarVisor(LONG uid, const std::vector<int> &canales, const std::string &clave, const std::string &titulo)
{
    nCams = 0;
    for (size_t i = 0; i < canales.size() && nCams < MAX_CAMS; i++) {
        memset(&cams[nCams], 0, sizeof(Camara));
        cams[nCams++].canal = canales[i];
    }
    if (nCams == 0) return 1;
    camarasIniciar(uid, clave, cams, nCams);

    XSetErrorHandler(alErrorX);
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        printf("No se pudo abrir el display\n");
        return 1;
    }
    if (!controlesIniciar(dpy)) {
        printf("No se pudieron cargar las fuentes de X11\n");
        XCloseDisplay(dpy);
        return 1;
    }
    int scr = DefaultScreen(dpy);
    g_main = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, anchoMain, altoMain, 0, 0, 0x202020);
    XStoreName(dpy, g_main, titulo.c_str());
    iconoPonerX11(dpy, g_main);
    Atom cerrar = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, g_main, &cerrar, 1);
    XSelectInput(dpy, g_main, StructureNotifyMask | KeyPressMask);

    // la barra general y el tooltip se crean al final para quedar encima de las cámaras
    for (int i = 0; i < nCams; i++) crearVentanas(&cams[i]);
    barraCrear(&g_barraSup, g_main);
    ayudaCrear(g_main);

    // primero se acomoda y se muestra, después se arranca el video
    XMapWindow(dpy, g_main);
    while (1) {
        XEvent e;
        XNextEvent(dpy, &e);
        if (e.type == MapNotify && e.xmap.window == g_main) break;
    }
    acomodar();
    XSync(dpy, False);
    for (int i = 0; i < nCams; i++) iniciarCanal(&cams[i], STREAM_SUB);

    int arrastre = -1, inicioX = 0, inicioY = 0, offIniX = 0, offIniY = 0;
    int ultimaCam = -1;
    Time ultimoClic = 0;
    XEvent ev;
    bool salir = false;

    while (!salir) {
        siguienteEvento(&ev);
        switch (ev.type) {
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == cerrar) salir = true;
            break;

        case ConfigureNotify:
            if (ev.xconfigure.window == g_main &&
                (ev.xconfigure.width != anchoMain || ev.xconfigure.height != altoMain)) {
                anchoMain = ev.xconfigure.width;
                altoMain = ev.xconfigure.height;
                acomodar();
            }
            break;

        case Expose:
            if (ev.xexpose.count == 0) redibujar(ev.xexpose.window);
            break;

        case KeyPress:
            if (XLookupKeysym(&ev.xkey, 0) == XK_Escape && g_grande >= 0) volverAlMosaico();
            break;

        case LeaveNotify: {
            int cam;
            Barra *b = buscarBarra(ev.xcrossing.window, &cam);
            if (b) salirDeBarra(b);
            break;
        }

        case ButtonPress: {
            int cam;
            Barra *b = buscarBarra(ev.xbutton.window, &cam);
            if (b) {
                int k = barraBotonEn(b, ev.xbutton.x, ev.xbutton.y);
                if (ev.xbutton.button == Button1 && k >= 0) {
                    Boton *bt = &b->botones[k];
                    if (bt->accion > 0 && !bt->inactivo) {
                        ayudaOcultar();
                        g_ayudaEn = 0;
                        ejecutarAccion(bt->accion, cam);
                    }
                }
                break;
            }

            int i = buscarCamara(ev.xbutton.window);
            if (i < 0) break;
            Camara *c = &cams[i];
            int mx = ev.xbutton.x, my = ev.xbutton.y;
            posponerRecorrido();

            if (ev.xbutton.button == Button2) {
                reiniciarZoom(c);
                aplicarZoom(c);
            } else if (ev.xbutton.button == Button1) {
                // doble clic alterna entre mosaico y vista en grande
                if (i == ultimaCam && ev.xbutton.time - ultimoClic < DOBLE_CLIC_MS) {
                    arrastre = -1;
                    ultimaCam = -1;
                    ultimoClic = 0;
                    if (g_grande >= 0) volverAlMosaico();
                    else cambiarVista(i);
                    break;
                }
                ultimaCam = i;
                ultimoClic = ev.xbutton.time;
                arrastre = i;
                inicioX = ev.xbutton.x_root;
                inicioY = ev.xbutton.y_root;
                offIniX = c->offX;
                offIniY = c->offY;
            } else if (ev.xbutton.button == Button4 || ev.xbutton.button == Button5) {
                // zoom centrado en el cursor
                cambiarZoom(c, ev.xbutton.button == Button4 ? ZOOM_PASO : 1 / ZOOM_PASO, mx, my);
            }
            if (g_controles && g_camControles == i) colocarBarraCamara(i);
            break;
        }

        case MotionNotify: {
            // solo importa la última posición; se descartan los movimientos acumulados
            while (XCheckTypedWindowEvent(dpy, ev.xmotion.window, MotionNotify, &ev)) {}
            if (arrastre >= 0) {
                // al arrastrar se ocultan los controles para dejar libre la imagen
                Camara *c = &cams[arrastre];
                ocultarControles();
                posponerRecorrido();
                if (c->ocultarEtiquetaEn) ocultarEtiqueta(c);
                c->offX = offIniX + (ev.xmotion.x_root - inicioX);
                c->offY = offIniY + (ev.xmotion.y_root - inicioY);
                aplicarZoom(c);
                break;
            }
            int cam;
            Barra *b = buscarBarra(ev.xmotion.window, &cam);
            if (b) {
                mostrarControles(cam);
                moverSobreBarra(b, ev.xmotion.x, ev.xmotion.y);
                break;
            }
            int i = buscarCamara(ev.xmotion.window);
            if (i >= 0) mostrarControles(i);
            break;
        }

        case ButtonRelease:
            if (ev.xbutton.button == Button1) arrastre = -1;
            break;
        }
    }

    for (int i = 0; i < nCams; i++) detenerCanal(&cams[i]);
    camarasFinalizar();
    controlesFinalizar();
    XDestroyWindow(dpy, g_main);
    XCloseDisplay(dpy);
    return 0;
}
