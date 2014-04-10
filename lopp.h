#ifndef LOPP_H_
#define LOPP_H_

#include <hwloc.h>

void lopp_init();
void lopp_finalize();
void lopp_bind(int type, int depth, int id);
void lopp_bindThreadToCore(int id);
void lopp_bindProcess(int id);
void lopp_bindThreadToNUMA(int id);
void lopp_bindThreadToPU(int id);
void lopp_bindThreadToCoreRandom(int id);

#endif
