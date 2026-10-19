#include <stdio.h>
#include <vector>
#include <gtk/gtk.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "icono.h"

#ifndef ICONO_RUTA
#define ICONO_RUTA "./icono/visor-hik.svg"
#endif

static const int TAMANOS[] = {16, 32, 48, 64, 128};
#define N_TAMANOS 5

void iconoIniciar()
{
    GError *error = NULL;
    if (!gtk_window_set_default_icon_from_file(ICONO_RUTA, &error)) {
        printf("No se pudo cargar el ícono %s: %s\n", ICONO_RUTA, error->message);
        g_error_free(error);
    }
}

void iconoPonerX11(Display *dpy, Window w)
{
    // la clase debe coincidir con StartupWMClass del .desktop para que el dock use el ícono
    XClassHint clase;
    clase.res_name = (char *)"visor-hik";
    clase.res_class = (char *)"Visor-hik";
    XSetClassHint(dpy, w, &clase);

    // _NET_WM_ICON lleva ancho, alto y pixeles ARGB de cada tamaño; con formato 32 cada valor es un long
    std::vector<unsigned long> datos;
    for (int i = 0; i < N_TAMANOS; i++) {
        GdkPixbuf *img = gdk_pixbuf_new_from_file_at_size(ICONO_RUTA, TAMANOS[i], TAMANOS[i], NULL);
        if (!img) continue;
        if (!gdk_pixbuf_get_has_alpha(img)) {
            GdkPixbuf *conAlfa = gdk_pixbuf_add_alpha(img, FALSE, 0, 0, 0);
            g_object_unref(img);
            img = conAlfa;
        }
        int ancho = gdk_pixbuf_get_width(img);
        int alto = gdk_pixbuf_get_height(img);
        int fila = gdk_pixbuf_get_rowstride(img);
        const guchar *px = gdk_pixbuf_get_pixels(img);
        datos.push_back(ancho);
        datos.push_back(alto);
        for (int y = 0; y < alto; y++) {
            for (int x = 0; x < ancho; x++) {
                const guchar *p = px + y * fila + x * 4;
                datos.push_back((unsigned long)p[3] << 24 | p[0] << 16 | p[1] << 8 | p[2]);
            }
        }
        g_object_unref(img);
    }
    if (datos.empty()) return;
    Atom icono = XInternAtom(dpy, "_NET_WM_ICON", False);
    XChangeProperty(dpy, w, icono, XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)datos.data(), datos.size());
}
