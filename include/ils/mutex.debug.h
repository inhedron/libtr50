
#define _tr50_mutex_create(ptr) _tr50_mutex_create_internal((ptr), __FILE__, __LINE__)
#define _tr50_mutex_lock(ptr) _tr50_mutex_lock_internal((ptr), __FILE__, __LINE__)
#define _tr50_mutex_unlock(ptr) _tr50_mutex_unlock_internal((ptr), __FILE__, __LINE__)

int _tr50_mutex_create_internal(void **mux, const char *file, int line);
int _tr50_mutex_lock_internal(void *mux, const char *file, int line);
int _tr50_mutex_unlock_internal(void *mux, const char *file, int line);
int _tr50_mutex_delete(void *mux);
