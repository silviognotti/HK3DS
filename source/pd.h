#pragma once
#include <stdbool.h>
#include <stdio.h>

bool pd_bool(const char *name);
void pd_set_bool(const char *name, bool v);
int  pd_int(const char *name);
void pd_set_int(const char *name, int v);
void pd_reset(void);
const char *pd_str(const char *name);       // "" when unset
void pd_set_str(const char *name, const char *v);
void pd_save(FILE *f);
bool pd_load_line(const char *line);
void pd_clear_prefix(const char *prefix);   // GameManager.ResetSemiPersistentObjects uses "pbs:"
bool pd_any_set(const char *prefix);         // any bool set whose name starts with prefix
