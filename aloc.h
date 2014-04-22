#ifndef aloc_H_
#define aloc_H_

#include <hwloc.h>

void aloc_init(char _auto);
void aloc_finalize();
void aloc_bind(int type, int depth, int tid, int id);
void aloc_bindThread(int id);
void aloc_bindThreadStride(int id, int stride);
void aloc_bindThreadGroup(int id, int group_size);

void aloc_bindProcess(int id);
void aloc_bindThreadToNUMA(int id);
void aloc_bindThreadToPU(int id);
void aloc_bindThreadToCoreRandom(int id);

int aloc_getNumCores();

#endif
