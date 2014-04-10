#include "lopp.h"

hwloc_topology_t lopp_topology;
int lopp_depth_core, lopp_depth_pu, lopp_depth_numa;
int lopp_ncores, lopp_npus, lopp_nnumas;

void lopp_init() {
  hwloc_topology_init(&lopp_topology);
  hwloc_topology_load(lopp_topology);

  lopp_depth_pu = hwloc_get_type_or_below_depth(lopp_topology, HWLOC_OBJ_PU);
  lopp_npus = hwloc_get_nbobjs_by_depth(lopp_topology, lopp_depth_pu);

  lopp_depth_core = hwloc_get_type_or_below_depth(lopp_topology, HWLOC_OBJ_CORE);
  lopp_ncores = hwloc_get_nbobjs_by_depth(lopp_topology, lopp_depth_core);

  lopp_depth_numa = hwloc_get_type_or_below_depth(lopp_topology, HWLOC_OBJ_NODE);
  lopp_nnumas = hwloc_get_nbobjs_by_depth(lopp_topology, lopp_depth_numa);
}

void lopp_finalize() {
  hwloc_topology_destroy(lopp_topology);
}

void lopp_bind(int type, int depth, int id) {
  hwloc_obj_t obj = hwloc_get_obj_by_depth(lopp_topology, depth, id);

  if (obj) {
    /* Get a copy of its cpuset that we may modify. */
    hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
    /* Get only one logical processor (in case the core is SMT/hyperthreaded). */
    //hwloc_bitmap_singlify(cpuset);
    /* And try to bind ourself there. */
    if (hwloc_set_cpubind(lopp_topology, cpuset, type)) {
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

void lopp_bindThreadToCore(int id) {
  lopp_bind(HWLOC_CPUBIND_THREAD, lopp_depth_core, (id*2) % lopp_ncores);
}

void lopp_bindThreadToPU(int id) {
  lopp_bind(HWLOC_CPUBIND_THREAD, lopp_depth_pu, id % lopp_npus);
}

void lopp_bindThreadToCoreRandom(int id) {
  int x = (rand() + id) % lopp_npus;
  lopp_bind(HWLOC_CPUBIND_THREAD, lopp_depth_core, x);
}

void lopp_bindThreadToNUMA(int id) {
  lopp_bind(HWLOC_CPUBIND_THREAD, lopp_depth_numa, id % lopp_nnumas);
}

void lopp_bindProcess(int id) {
  lopp_bind(0, lopp_depth_core, id);
}


