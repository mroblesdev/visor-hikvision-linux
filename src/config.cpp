#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "config.h"
#include "llavero.h"

static std::string carpetaConfig()
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char *home = getenv("HOME");
        base = std::string(home ? home : ".") + "/.config";
    }
    return base + "/visor-hik";
}

std::string rutaConfig()
{
    return carpetaConfig() + "/config.ini";
}

static std::string recortar(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t");
    size_t b = s.find_last_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a, b - a + 1);
}

static bool guardarEnLlavero(const Config &cfg)
{
    return llaveroGuardar("password", "Visor HIK: contraseña de " + cfg.ip, cfg.password) &&
           llaveroGuardar("clave", "Visor HIK: clave de cifrado de " + cfg.ip, cfg.clave);
}

// escribe el archivo con permisos 600; sin llavero incluye la contraseña y la clave en texto plano
static bool escribirArchivo(const Config &cfg, bool enLlavero)
{
    std::string carpeta = carpetaConfig();
    std::string padre = carpeta.substr(0, carpeta.rfind('/'));
    mkdir(padre.c_str(), 0700);
    mkdir(carpeta.c_str(), 0700);

    // se crea con 600 desde el inicio; con fopen quedaría legible un momento según la umask
    std::string ruta = rutaConfig();
    int fd = open(ruta.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return false;
    fchmod(fd, 0600);
    FILE *f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        return false;
    }

    fprintf(f, "ip=%s\n", cfg.ip.c_str());
    fprintf(f, "puerto=%d\n", cfg.puerto);
    fprintf(f, "usuario=%s\n", cfg.usuario.c_str());
    if (enLlavero) {
        fprintf(f, "llavero=1\n");
    } else {
        fprintf(f, "password=%s\n", cfg.password.c_str());
        fprintf(f, "clave=%s\n", cfg.clave.c_str());
    }
    fprintf(f, "canales=%s\n", cfg.canales.c_str());
    fclose(f);
    return true;
}

// lee el archivo clave=valor; el valor se toma tal cual para no alterar contraseñas
bool cargarConfig(Config &cfg)
{
    FILE *f = fopen(rutaConfig().c_str(), "r");
    if (!f) return false;

    bool enLlavero = false;

    char linea[512];
    while (fgets(linea, sizeof(linea), f)) {
        std::string l = linea;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        size_t igual = l.find('=');
        if (igual == std::string::npos) continue;
        std::string clave = recortar(l.substr(0, igual));
        std::string valor = l.substr(igual + 1);

        if (clave == "ip") cfg.ip = recortar(valor);
        else if (clave == "puerto") cfg.puerto = atoi(valor.c_str());
        else if (clave == "usuario") cfg.usuario = valor;
        else if (clave == "password") cfg.password = valor;
        else if (clave == "clave") cfg.clave = valor;
        else if (clave == "llavero") enLlavero = recortar(valor) == "1";
        else if (clave == "canales") cfg.canales = recortar(valor);
    }
    fclose(f);

    if (cfg.puerto <= 0) cfg.puerto = 8000;
    if (cfg.ip.empty()) return false;

    if (enLlavero) {
        if (!llaveroLeer("password", cfg.password) || !llaveroLeer("clave", cfg.clave))
            printf("No se pudo leer la contraseña del llavero del sistema\n");
    } else if ((!cfg.password.empty() || !cfg.clave.empty()) && llaveroDisponible()) {
        // configuración anterior con la contraseña en texto plano: se pasa al llavero
        if (guardarEnLlavero(cfg) && escribirArchivo(cfg, true))
            printf("La contraseña y la clave se movieron al llavero del sistema\n");
    }
    return true;
}

// guarda la contraseña y la clave en el llavero; si no hay llavero van al archivo
bool guardarConfig(const Config &cfg)
{
    bool enLlavero = guardarEnLlavero(cfg);
    if (!enLlavero)
        printf("Llavero del sistema no disponible: la contraseña se guarda en texto plano en %s\n", rutaConfig().c_str());
    return escribirArchivo(cfg, enLlavero);
}

// convierte "1, 2 3" en {1, 2, 3}
std::vector<int> parsearCanales(const std::string &texto)
{
    std::vector<int> canales;
    std::string actual;
    for (size_t i = 0; i <= texto.size(); i++) {
        char c = i < texto.size() ? texto[i] : ' ';
        if (c >= '0' && c <= '9') {
            actual += c;
        } else if (!actual.empty()) {
            int n = atoi(actual.c_str());
            if (n > 0) canales.push_back(n);
            actual.clear();
        }
    }
    return canales;
}
