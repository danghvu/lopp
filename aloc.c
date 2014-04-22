#include "aloc.h"
#include <unistd.h>
#include <libconfig.h>


hwloc_topology_t aloc_topology;
int aloc_depth_core, aloc_depth_pu, aloc_depth_numa;
int aloc_ncores, aloc_npus, aloc_nnumas;

typedef struct {
  int type;
  int cid;
  int tid;
} bind_t;

typedef struct node {
  bind_t *item;
  struct node *next;
} node_t;

node_t *start_node;
node_t *current_node;

void aloc_init(char _auto) {
  hwloc_topology_init(&aloc_topology);
  hwloc_topology_load(aloc_topology);

  if (_auto) return;
  // expect no additional call to aloc, so probably do not need
  // the following lines

  aloc_depth_pu = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_PU);
  aloc_npus = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_pu);

  aloc_depth_core = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_CORE);
  aloc_ncores = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_core);

  aloc_depth_numa = hwloc_get_type_or_below_depth(aloc_topology, HWLOC_OBJ_NODE);
  aloc_nnumas = hwloc_get_nbobjs_by_depth(aloc_topology, aloc_depth_numa);

  start_node = (node_t *) malloc (sizeof(node_t));
  start_node->item = NULL;
  start_node->next = NULL;

  current_node = start_node;

  // TODO: getting previous setting if exists
}

void aloc_finalize() {
  hwloc_topology_destroy(aloc_topology);

  // initialize configuration file
  config_t config;
  config_init(&config);
  config_setting_t *root = config_root_setting(&config);

  // now record the binding
  int i;
  int cores[] = {1,2,3,4};
  int numas[] = {1,2};

  // initialize a group
  config_setting_t *group = config_setting_add(root, "AUTOLOC", CONFIG_TYPE_GROUP);

  // write mapped core
  config_setting_t *core = config_setting_add(group, "core", CONFIG_TYPE_ARRAY);
  for (i=0; i<4; i++) {
    config_setting_t *s = config_setting_add(core, NULL, CONFIG_TYPE_INT);
    config_setting_set_int(s, cores[i]);
  }

  // write mapped numa
  config_setting_t *numa = config_setting_add(group, "numa", CONFIG_TYPE_ARRAY);
  for (i=0; i<2; i++) {
    config_setting_t *s = config_setting_add(numa, NULL, CONFIG_TYPE_INT);
    config_setting_set_int(s, numas[i]);
  }

  // write the resulting config to file
  pid_t pid = getpid();
  char *filename;
  asprintf(&filename, "%d.cfg", pid);
  config_write_file(&config, filename);
  config_destroy(&config);
}

int aloc_getNumCores() { return aloc_ncores; }

void aloc_bind(int type, int depth, int tid, int id) {
  hwloc_obj_t obj = hwloc_get_obj_by_depth(aloc_topology, depth, id);

  current_node->item = (bind_t *) malloc (sizeof(bind_t));
  current_node->next = (node_t *) malloc (sizeof(node_t));

  current_node->item->type = type;
  current_node->item->tid = tid;
  current_node->item->cid = id;

  current_node = current_node->next;

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
  aloc_bind(HWLOC_CPUBIND_THREAD, depth, id, id % group_size);
}

void aloc_bindThreadStride(int id, int stride) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, id, (id * stride) % aloc_ncores);
}

void aloc_bindThread(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, id, id % aloc_ncores);
}

void aloc_bindThreadToPU(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_pu, id, id % aloc_npus);
}

void aloc_bindThreadToCoreRandom(int id) {
  int x = (rand() + id) % aloc_npus;
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_core, id, x);
}

void aloc_bindThreadToNUMA(int id) {
  aloc_bind(HWLOC_CPUBIND_THREAD, aloc_depth_numa, id, id % aloc_nnumas);
}

void aloc_bindProcess(int id) {
  aloc_bind(0, aloc_depth_core, id, id);
}
