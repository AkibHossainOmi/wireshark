// Md. Akib Hossain Omi
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include <glib.h>

#ifdef _WIN32
  #define EXPORT_SYMBOL __declspec(dllexport)
#else
  #define EXPORT_SYMBOL __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

EXPORT_SYMBOL char* Tb_Main(char filename[1000]);
EXPORT_SYMBOL void freememory(char* str);

#ifdef __cplusplus
}
#endif
