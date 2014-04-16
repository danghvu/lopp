#include "aloc.h"

hwloc_topology_t aloc_topology;
int aloc_depth_core, aloc_depth_pu, aloc_depth_numa;
int aloc_ncores, aloc_npus, aloc_nnumas;

void aloc_init() {
  hwloc_topology_init(&aloc_topology);
  hwloc_topology_load(aloc_topology);

  aloc_depth_pu = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_PU);
  aloc_npus = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_pu);

  aloc_depth_core = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_CORE);
  aloc_ncores = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_core);

  aloc_depth_numa = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_NODE);
  aloc_nnumas = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_numa);
}

void aloc_finalize() {
  hwloc_topology_destroy(aloc_topology);
}

void aloc_bind(int type, int depth, int id) {
  hwloc_obj_t obj = hwloc_get_obj_by_depth(aloc_topology, depth, id);

  if (obj) {
    /* Get a copy of its cpuset that we may modify. */
    hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
    /* Get only one logical processor (in case the core is SMT/hyperthreaded). */
    //hwloc_bitmap_singlify(cpuset);
    /* And try to bind ourself there. */
    if (hwloc_set_cpubind(aloc_topology, cpuset, type)) {
      char *str;
      int error = errno;
      hwloc_bitmap_asprintf(&str, obj->cpuset);
      printf("Couldn't bind to cpuset %s: %s\n", str, strerror(error));
      free(str);
    }
    /* Free our cpuset copy */
    hwloc_bitmap_free(cpuset);
  }
}

void aloc_bindThreadGroup(int id, int group_size) {
  if (group_size == 1) return;
  int depth = aloc_depth_core;
  if (group_size < aloc_nnumas) depth = aloc_depth_numa;
  else if (group_size < aloc_ncores) depth = aloc_depth_core;
  else if (group_size < aloc_npus) depth = aloc_depth_pu;

  aloc_bind(HWLOC_CPUBIND_THREAD, depth, id % group_size);
}

void aloc_bindThreadStride(int id, int stride) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, (id * stride) % aloc_ncores);
}

void aloc_bindThread(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, id % aloc_ncores);
}

void aloc_bindThreadToPU(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_pu, id % aloc_npus);
}

void aloc_bindThreadToCoreRandom(int id) {
  int x = (rand() + id) % aloc_npus;
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, x);
}

void aloc_bindThreadToNUMA(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_numa, id % aloc_nnumas);
}

void aloc_bindProcess(int id) {
  aloc_bind(0, aloc_depth_core, id);
}
