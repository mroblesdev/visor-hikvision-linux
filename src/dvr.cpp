#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dvr.h"

#define CANALES_POR_GRUPO 64

// indica al SDK dónde están sus componentes para poder ejecutar desde cualquier carpeta
bool dvrInicializar(const std::string &rutaLib)
{
    static char rutaCrypto[512];
    static char rutaSsl[512];
    snprintf(rutaCrypto, sizeof(rutaCrypto), "%s/libcrypto.so.1.1", rutaLib.c_str());
    snprintf(rutaSsl, sizeof(rutaSsl), "%s/libssl.so.1.1", rutaLib.c_str());
    if (access(rutaCrypto, F_OK) == 0) NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_LIBEAY_PATH, rutaCrypto);
    if (access(rutaSsl, F_OK) == 0) NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SSLEAY_PATH, rutaSsl);

    NET_DVR_LOCAL_SDK_PATH ruta;
    memset(&ruta, 0, sizeof(ruta));
    snprintf(ruta.sPath, sizeof(ruta.sPath), "%s/", rutaLib.c_str());
    NET_DVR_SetSDKInitCfg(NET_SDK_INIT_CFG_SDK_PATH, &ruta);

    if (!NET_DVR_Init()) return false;
    NET_DVR_SetConnectTime(3000, 3);
    NET_DVR_SetReconnect(5000, TRUE);
    return true;
}

void dvrFinalizar()
{
    NET_DVR_Cleanup();
}

static std::string mensajeError(DWORD codigo)
{
    switch (codigo) {
    case 1: return "Usuario o contraseña incorrectos";
    case 7: return "No se pudo conectar. Revisa la IP y el puerto del SDK (8000 por defecto)";
    case 10: return "El equipo no respondió a tiempo";
    case 153: return "Usuario bloqueado por intentos fallidos, espera unos minutos";
    }
    LONG c = (LONG)codigo;
    char texto[256];
    snprintf(texto, sizeof(texto), "Error %u del SDK: %s", codigo, NET_DVR_GetErrorMsg(&c));
    return texto;
}

LONG dvrLogin(const std::string &ip, int puerto, const std::string &usuario,
              const std::string &password, NET_DVR_DEVICEINFO_V40 &info, std::string &error)
{
    NET_DVR_USER_LOGIN_INFO login;
    memset(&login, 0, sizeof(login));
    memset(&info, 0, sizeof(info));
    strncpy(login.sDeviceAddress, ip.c_str(), sizeof(login.sDeviceAddress) - 1);
    strncpy(login.sUserName, usuario.c_str(), sizeof(login.sUserName) - 1);
    strncpy(login.sPassword, password.c_str(), sizeof(login.sPassword) - 1);
    login.wPort = (WORD)puerto;
    login.bUseAsynLogin = 0;

    LONG uid = NET_DVR_Login_V40(&login, &info);
    if (uid < 0) error = mensajeError(NET_DVR_GetLastError());
    return uid;
}

// el número de serie de Hikvision empieza con el modelo, ej. DS-7204HQHI-K1...
std::string dvrSerie(const NET_DVR_DEVICEINFO_V40 &info)
{
    const char *s = (const char *)info.struDeviceV30.sSerialNumber;
    return std::string(s, strnlen(s, SERIALNO_LEN));
}

// confirma que el canal entrega video, primero con substream y luego con el principal
static bool probarCanal(LONG uid, int canal)
{
    for (int stream = 1; stream >= 0; stream--) {
        NET_DVR_PREVIEWINFO pv;
        memset(&pv, 0, sizeof(pv));
        pv.lChannel = canal;
        pv.dwStreamType = stream;
        pv.bBlocked = 1;
        LONG h = NET_DVR_RealPlay_V40(uid, &pv, NULL, NULL);
        if (h >= 0) {
            NET_DVR_StopRealPlay(h);
            return true;
        }
    }
    return false;
}

// arma la lista de canales con cámara: analógicos más los IP que el equipo reporta en línea
std::vector<int> dvrDetectarCanales(LONG uid, const NET_DVR_DEVICEINFO_V40 &info, bool debug)
{
    const NET_DVR_DEVICEINFO_V30 &d = info.struDeviceV30;
    int nIP = d.byIPChanNum + d.byHighDChanNum * 256;

    if (debug) {
        printf("Equipo: %s\n", dvrSerie(info).c_str());
        printf("Tipo: %d, analógicos: %d desde %d, IP: %d desde %d\n",
               d.byDVRType, d.byChanNum, d.byStartChan, nIP, d.byStartDChan);
    }

    std::vector<int> candidatos;
    for (int i = 0; i < d.byChanNum; i++)
        candidatos.push_back(d.byStartChan + i);

    if (nIP > 0) {
        bool consultado = false;
        NET_DVR_IPPARACFG_V40 *cfg = new NET_DVR_IPPARACFG_V40;
        int grupos = (nIP + CANALES_POR_GRUPO - 1) / CANALES_POR_GRUPO;

        for (int g = 0; g < grupos; g++) {
            memset(cfg, 0, sizeof(*cfg));
            DWORD recibido = 0;
            if (!NET_DVR_GetDVRConfig(uid, NET_DVR_GET_IPPARACFG_V40, g, cfg, sizeof(*cfg), &recibido)) {
                if (debug) printf("IPPARACFG_V40 grupo %d no disponible: %u\n", g, NET_DVR_GetLastError());
                break;
            }
            consultado = true;
            int enGrupo = nIP - g * CANALES_POR_GRUPO;
            if (enGrupo > CANALES_POR_GRUPO) enGrupo = CANALES_POR_GRUPO;

            for (int i = 0; i < enGrupo; i++) {
                int canal = d.byStartDChan + g * CANALES_POR_GRUPO + i;
                bool enLinea = cfg->struStreamMode[i].byGetStreamType == 0 &&
                               cfg->struStreamMode[i].uGetStream.struChanInfo.byEnable;
                if (debug) printf("Canal IP %d: %s\n", canal, enLinea ? "en línea" : "sin cámara");
                if (enLinea) candidatos.push_back(canal);
            }
        }
        delete cfg;

        // si el equipo no responde la consulta se prueban todos
        if (!consultado) {
            for (int i = 0; i < nIP; i++)
                candidatos.push_back(d.byStartDChan + i);
        }
    }

    std::vector<int> canales;
    for (int canal : candidatos) {
        bool ok = probarCanal(uid, canal);
        if (debug) printf("Canal %d: %s\n", canal, ok ? "con video" : "sin video");
        if (ok) canales.push_back(canal);
    }
    return canales;
}
