#ifndef LOPP_H_
#define LOPP_H_

#include <hwloc.h>

class Lopp {

public:
  Lopp() {
    hwloc_topology_init(&topology);
    hwloc_topology_load(topology);
    depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_CORE);
    ncores = hwloc_get_nbobjs_by_depth(topology, depth);
  }

  ~Lopp() {
    hwloc_topology_destroy(topology);
  }

  int getNumCores() {
    return ncores;
  }

  void bindThread(int id) {
    bind(HWLOC_CPUBIND_THREAD, id);
  }

  void bindProcess(int id) {
    bind(0, id);
  }

  void bind(int type, int id) {
    hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, depth, id);

    if (obj) {
      /* Get a copy of its cpuset that we may modify. */
      hwloc_cpuset_t cpuset = hwloc_bitmap_dup(obj->cpuset);
      /* Get only one logical processor (in case the core is SMT/hyperthreaded). */
      hwloc_bitmap_singlify(cpuset);
      /* And try to bind ourself there. */
      if (hwloc_set_cpubind(topology, cpuset, type)) {
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

private:
  hwloc_topology_t topology;
  int depth;
  int ncores;
};

#endif
