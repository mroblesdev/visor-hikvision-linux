#pragma once
#include <string>

// guarda y lee secretos en el llavero del sistema (GNOME Keyring, KWallet) con libsecret
bool llaveroDisponible();
bool llaveroGuardar(const std::string &dato, const std::string &etiqueta, const std::string &valor);
bool llaveroLeer(const std::string &dato, std::string &valor);
