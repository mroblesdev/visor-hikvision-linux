#include <stdio.h>
#include <dlfcn.h>
#include <glib.h>
#include "llavero.h"

// libsecret se carga con dlopen para no exigirla al compilar; las firmas son las de secret/secret-password.h
typedef void *(*FnSchemaNew)(const char *nombre, int flags, ...);
typedef gboolean (*FnStore)(void *schema, GHashTable *atributos, const gchar *coleccion, const gchar *etiqueta,
                            const gchar *password, void *cancelar, GError **error);
typedef gchar *(*FnLookup)(void *schema, GHashTable *atributos, void *cancelar, GError **error);
typedef gboolean (*FnClear)(void *schema, GHashTable *atributos, void *cancelar, GError **error);
typedef void (*FnFree)(gchar *password);

static FnStore fnStore = NULL;
static FnLookup fnLookup = NULL;
static FnClear fnClear = NULL;
static FnFree fnFree = NULL;
static void *schema = NULL;
static bool intentado = false;

// carga libsecret una sola vez y crea el esquema con el atributo "dato"
bool llaveroDisponible()
{
    if (intentado) return schema != NULL;
    intentado = true;

    void *lib = dlopen("libsecret-1.so.0", RTLD_NOW);
    if (!lib) return false;
    FnSchemaNew fnSchemaNew = (FnSchemaNew)dlsym(lib, "secret_schema_new");
    fnStore = (FnStore)dlsym(lib, "secret_password_storev_sync");
    fnLookup = (FnLookup)dlsym(lib, "secret_password_lookupv_sync");
    fnClear = (FnClear)dlsym(lib, "secret_password_clearv_sync");
    fnFree = (FnFree)dlsym(lib, "secret_password_free");
    if (!fnSchemaNew || !fnStore || !fnLookup || !fnClear || !fnFree) return false;

    // flags 0 = SECRET_SCHEMA_NONE, tipo 0 = SECRET_SCHEMA_ATTRIBUTE_STRING
    schema = fnSchemaNew("visor-hik.Credencial", 0, "dato", 0, NULL);
    return schema != NULL;
}

static GHashTable *atributos(const std::string &dato)
{
    GHashTable *t = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(t, (gpointer)"dato", (gpointer)dato.c_str());
    return t;
}

static void mostrarError(const char *accion, GError *error)
{
    if (!error) return;
    printf("Llavero: no se pudo %s: %s\n", accion, error->message);
    g_error_free(error);
}

// un valor vacío borra la entrada en lugar de guardar un secreto vacío
bool llaveroGuardar(const std::string &dato, const std::string &etiqueta, const std::string &valor)
{
    if (!llaveroDisponible()) return false;
    GHashTable *t = atributos(dato);
    GError *error = NULL;
    bool ok;
    if (valor.empty()) {
        fnClear(schema, t, NULL, &error);
        ok = error == NULL;
        mostrarError("borrar", error);
    } else {
        ok = fnStore(schema, t, NULL, etiqueta.c_str(), valor.c_str(), NULL, &error);
        mostrarError("guardar", error);
    }
    g_hash_table_unref(t);
    return ok;
}

// si la entrada no existe deja el valor vacío y regresa true
bool llaveroLeer(const std::string &dato, std::string &valor)
{
    valor.clear();
    if (!llaveroDisponible()) return false;
    GHashTable *t = atributos(dato);
    GError *error = NULL;
    gchar *secreto = fnLookup(schema, t, NULL, &error);
    g_hash_table_unref(t);
    if (error) {
        mostrarError("leer", error);
        return false;
    }
    if (secreto) {
        valor = secreto;
        fnFree(secreto);
    }
    return true;
}
