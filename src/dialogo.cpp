#include <stdio.h>
#include <gtk/gtk.h>
#include "dialogo.h"
#include "dvr.h"

struct Campos {
    GtkWidget *ip;
    GtkWidget *puerto;
    GtkWidget *usuario;
    GtkWidget *password;
    GtkWidget *clave;
    GtkWidget *canales;
    GtkWidget *estado;
};

static void procesarEventos()
{
    while (gtk_events_pending()) gtk_main_iteration();
}

static void mostrarEstado(Campos *f, const char *texto, bool error)
{
    char *markup = g_markup_printf_escaped(error ? "<span foreground=\"#c62828\">%s</span>" : "%s", texto);
    gtk_label_set_markup(GTK_LABEL(f->estado), markup);
    g_free(markup);
}

static void leerCampos(Campos *f, Config &cfg)
{
    cfg.ip = gtk_entry_get_text(GTK_ENTRY(f->ip));
    cfg.puerto = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(f->puerto));
    cfg.usuario = gtk_entry_get_text(GTK_ENTRY(f->usuario));
    cfg.password = gtk_entry_get_text(GTK_ENTRY(f->password));
    cfg.clave = gtk_entry_get_text(GTK_ENTRY(f->clave));
    cfg.canales = gtk_entry_get_text(GTK_ENTRY(f->canales));
}

// intenta conectar con los datos del formulario y muestra cuántas cámaras encontró
static void alProbar(GtkButton *boton, gpointer datos)
{
    Campos *f = (Campos *)datos;
    Config cfg;
    leerCampos(f, cfg);
    if (cfg.ip.empty()) {
        mostrarEstado(f, "Escribe la IP del equipo", true);
        return;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(boton), FALSE);
    mostrarEstado(f, "Conectando...", false);
    procesarEventos();

    NET_DVR_DEVICEINFO_V40 info;
    std::string error;
    LONG uid = dvrLogin(cfg.ip, cfg.puerto, cfg.usuario, cfg.password, info, error);
    if (uid < 0) {
        mostrarEstado(f, error.c_str(), true);
    } else {
        mostrarEstado(f, "Conectado, buscando cámaras...", false);
        procesarEventos();
        std::vector<int> canales = dvrDetectarCanales(uid, info, false);
        NET_DVR_Logout(uid);
        char texto[256];
        snprintf(texto, sizeof(texto), "Conectado a %s\n%d cámara(s) encontradas",
                 dvrSerie(info).c_str(), (int)canales.size());
        mostrarEstado(f, texto, canales.empty());
    }
    gtk_widget_set_sensitive(GTK_WIDGET(boton), TRUE);
}

static void alMostrarClaves(GtkToggleButton *check, gpointer datos)
{
    Campos *f = (Campos *)datos;
    gboolean ver = gtk_toggle_button_get_active(check);
    gtk_entry_set_visibility(GTK_ENTRY(f->password), ver);
    gtk_entry_set_visibility(GTK_ENTRY(f->clave), ver);
}

static GtkWidget *agregarFila(GtkWidget *grid, int fila, const char *etiqueta, const std::string &valor)
{
    GtkWidget *label = gtk_label_new(etiqueta);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), valor.c_str());
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 0, fila, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, fila, 1, 1);
    return entry;
}

bool mostrarDialogo(Config &cfg, const std::string &mensaje)
{
    GtkWidget *dialogo = gtk_dialog_new_with_buttons(
        "Visor HIK - Conexión", NULL, GTK_DIALOG_MODAL,
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Guardar y abrir", GTK_RESPONSE_ACCEPT,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialogo), GTK_RESPONSE_ACCEPT);
    gtk_window_set_default_size(GTK_WINDOW(dialogo), 420, -1);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 16);

    Campos f;
    f.ip = agregarFila(grid, 0, "IP del DVR/NVR", cfg.ip);
    gtk_entry_set_placeholder_text(GTK_ENTRY(f.ip), "192.168.1.64");

    GtkWidget *lblPuerto = gtk_label_new("Puerto SDK");
    gtk_widget_set_halign(lblPuerto, GTK_ALIGN_END);
    f.puerto = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(f.puerto), cfg.puerto);
    gtk_grid_attach(GTK_GRID(grid), lblPuerto, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), f.puerto, 1, 1, 1, 1);

    f.usuario = agregarFila(grid, 2, "Usuario", cfg.usuario);
    f.password = agregarFila(grid, 3, "Contraseña", cfg.password);
    gtk_entry_set_visibility(GTK_ENTRY(f.password), FALSE);
    f.clave = agregarFila(grid, 4, "Clave de cifrado", cfg.clave);
    gtk_entry_set_visibility(GTK_ENTRY(f.clave), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(f.clave), "Vacío si no usas cifrado");
    f.canales = agregarFila(grid, 5, "Canales", cfg.canales);
    gtk_entry_set_placeholder_text(GTK_ENTRY(f.canales), "Vacío = todos, ej. 1,2,3");

    GtkWidget *check = gtk_check_button_new_with_label("Mostrar contraseña y clave");
    gtk_grid_attach(GTK_GRID(grid), check, 1, 6, 1, 1);
    g_signal_connect(check, "toggled", G_CALLBACK(alMostrarClaves), &f);

    GtkWidget *probar = gtk_button_new_with_label("Probar conexión");
    gtk_grid_attach(GTK_GRID(grid), probar, 1, 7, 1, 1);
    g_signal_connect(probar, "clicked", G_CALLBACK(alProbar), &f);

    f.estado = gtk_label_new("");
    gtk_label_set_line_wrap(GTK_LABEL(f.estado), TRUE);
    gtk_label_set_xalign(GTK_LABEL(f.estado), 0);
    gtk_grid_attach(GTK_GRID(grid), f.estado, 0, 8, 2, 1);
    if (!mensaje.empty()) mostrarEstado(&f, mensaje.c_str(), true);

    GtkWidget *nota = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(nota),
        "<small>La clave de cifrado es el código de verificación de Hik-Connect.\n"
        "La configuración se guarda en ~/.config/visor-hik/config.ini</small>");
    gtk_label_set_xalign(GTK_LABEL(nota), 0);
    gtk_grid_attach(GTK_GRID(grid), nota, 0, 9, 2, 1);

    GtkWidget *contenido = gtk_dialog_get_content_area(GTK_DIALOG(dialogo));
    gtk_container_add(GTK_CONTAINER(contenido), grid);
    gtk_widget_show_all(dialogo);

    bool aceptado = false;
    while (true) {
        if (gtk_dialog_run(GTK_DIALOG(dialogo)) != GTK_RESPONSE_ACCEPT) break;
        Config nuevo;
        leerCampos(&f, nuevo);
        if (nuevo.ip.empty() || nuevo.usuario.empty()) {
            mostrarEstado(&f, "La IP y el usuario son obligatorios", true);
            continue;
        }
        cfg = nuevo;
        aceptado = true;
        break;
    }

    gtk_widget_destroy(dialogo);
    procesarEventos();
    return aceptado;
}
