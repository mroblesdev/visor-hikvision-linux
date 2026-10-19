SDK ?= ./sdk
SDK_DIR := $(abspath $(SDK))
ICONO := $(abspath icono/visor-hik.svg)
DATOS := $(HOME)/.local/share
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-3.0)

CXXFLAGS ?= -O2 -Wall
CXXFLAGS += -isystem $(SDK_DIR)/incEn $(GTK_CFLAGS) -DSDK_LIB_DIR=\"$(SDK_DIR)/lib\" -DICONO_RUTA=\"$(ICONO)\"
LIBS = -L$(SDK_DIR)/lib -lhcnetsdk -lPlayCtrl -lAudioRender -lSuperRender \
       $(GTK_LIBS) -lX11 -lpthread -ldl \
       -Wl,--disable-new-dtags -Wl,-rpath,$(SDK_DIR)/lib -Wl,-rpath,$(SDK_DIR)/lib/HCNetSDKCom

SRC := $(wildcard src/*.cpp)
HDR := $(wildcard src/*.h)

visor-hik: $(SRC) $(HDR)
	@test -f $(SDK_DIR)/incEn/HCNetSDK.h || (echo "No se encontró el SDK en $(SDK_DIR). Revisa el README."; exit 1)
	$(CXX) $(CXXFLAGS) $(SRC) -o $@ $(LIBS)

# agrega el visor al menú de aplicaciones del usuario con su ícono
instalar: visor-hik
	install -Dm644 icono/visor-hik.svg $(DATOS)/icons/hicolor/scalable/apps/visor-hik.svg
	install -d $(DATOS)/applications
	sed 's|@RUTA@|$(abspath visor-hik)|' icono/visor-hik.desktop.in > $(DATOS)/applications/visor-hik.desktop

desinstalar:
	rm -f $(DATOS)/icons/hicolor/scalable/apps/visor-hik.svg $(DATOS)/applications/visor-hik.desktop

clean:
	rm -f visor-hik

.PHONY: clean instalar desinstalar
