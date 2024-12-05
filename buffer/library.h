// library.h

#ifndef LIBRARY_H
#define LIBRARY_H

#include "buffer_cache.h"

void lib_read(int block_number, char *buffer);
void lib_write(int block_number, const char *buffer);
void lib_init(ReplacementPolicy policy);
void lib_destroy();

#endif // LIBRARY_H
