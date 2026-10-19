#pragma once
#include <string>
#include <vector>
#include "HCNetSDK.h"

bool dvrInicializar(const std::string &rutaLib);
void dvrFinalizar();
LONG dvrLogin(const std::string &ip, int puerto, const std::string &usuario,
              const std::string &password, NET_DVR_DEVICEINFO_V40 &info, std::string &error);
std::string dvrSerie(const NET_DVR_DEVICEINFO_V40 &info);
std::vector<int> dvrDetectarCanales(LONG uid, const NET_DVR_DEVICEINFO_V40 &info, bool debug);
