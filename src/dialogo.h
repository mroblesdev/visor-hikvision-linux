#pragma once
#include <string>
#include "config.h"

// muestra el formulario de conexión; regresa false si el usuario cancela
bool mostrarDialogo(Config &cfg, const std::string &mensaje);
