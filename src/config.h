#pragma once
#include <string>
#include <vector>

struct Config {
    std::string ip;
    int puerto = 8000;
    std::string usuario = "admin";
    std::string password;
    std::string clave;
    std::string canales;
};

std::string rutaConfig();
bool cargarConfig(Config &cfg);
bool guardarConfig(const Config &cfg);
std::vector<int> parsearCanales(const std::string &texto);
