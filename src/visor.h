#pragma once
#include <string>
#include <vector>
#include "HCNetSDK.h"

// abre la ventana con el mosaico y regresa al cerrarla
int ejecutarVisor(LONG uid, const std::vector<int> &canales, const std::string &clave, const std::string &titulo);
