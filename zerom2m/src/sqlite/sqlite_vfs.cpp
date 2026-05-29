#include <zerom2m/sqlite/sqlite3.h>

extern "C" int sqlite3_os_init(void) {
    return SQLITE_OK;
}

extern "C" int sqlite3_os_end(void) {
    return SQLITE_OK;
}