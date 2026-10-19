#pragma once
#include <X11/Xlib.h>

// pone el ícono a las ventanas de GTK, como el formulario de conexión
void iconoIniciar();

// pone el ícono y la clase de la aplicación a una ventana de Xlib
void iconoPonerX11(Display *dpy, Window w);
