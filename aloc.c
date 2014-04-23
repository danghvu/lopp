#include "aloc.h"
#include <unistd.h>
#include <libconfig.h>


hwloc_topology_t aloc_topology;
int aloc_depth_core, aloc_depth_pu, aloc_depth_numa;
int aloc_ncores, aloc_npus, aloc_nnumas;
pthread_mutex_t aloc_mutex     = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  int depth;
  int cid;
  int tid;
} bind_t;

typedef struct node {
  bind_t *item;
  struct node *next;
} node_t;

node_t *start_node;
node_t *current_node;

config_t aloc_config;
char config_found = 0;

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
  FILE *fconfig;
  if ((fconfig = fopen("aloc.cfg", "r")) == NULL) {
    // ignore the file
  } else {
    // read config file here
    // initialize configuration file
    config_init(&aloc_config);
    if ( config_read_file(&aloc_config, "aloc.cfg") != CONFIG_TRUE ) {
      char *s = config_error_text(&aloc_config);
      printf("%s\n", s);
    }
    fclose(fconfig);

    config_setting_t *root = config_root_setting(&aloc_config);
    config_setting_t *group = config_setting_get_member(root, "AUTOLOC-NEXT");

    fprintf(stderr, "**WARNING: found aloc configuration, all binding will be overrided by the given setting unless unable to**\n");
    config_found = 1;
  }
}

void aloc_finalize() {
  hwloc_topology_destroy(aloc_topology);

  // initialize configuration file
  config_t config;
  config_init(&config);
  config_setting_t *root = config_root_setting(&config);

  int i;

  current_node = start_node;

  // initialize a group
  config_setting_t *group = config_setting_add(root, "AUTOLOC-SUMMARY", CONFIG_TYPE_GROUP);

  config_setting_t *d = config_setting_add(group, "core_depth", CONFIG_TYPE_INT);
  config_setting_set_int(d, aloc_depth_core);

  d = config_setting_add(group, "numa_depth", CONFIG_TYPE_INT);
  config_setting_set_int(d, aloc_depth_numa);


  // write mapped core
  config_setting_t *core = config_setting_add(group, "core", CONFIG_TYPE_LIST);
  for (i=0; i<aloc_ncores; i++) {
    config_setting_t *s = config_setting_add(core, NULL, CONFIG_TYPE_LIST);
  }

  // write mapped numa
  config_setting_t *numa = config_setting_add(group, "numa", CONFIG_TYPE_LIST);
  assert(numa);
  for (i=0; i<aloc_nnumas; i++) {
    config_setting_t *s = config_setting_add(numa, NULL, CONFIG_TYPE_LIST);
  }

  while (current_node->item != NULL) {
    bind_t *item = current_node->item;
    // printf("%d %d %d\n", item->depth, item->tid, item->cid);
    if (item->depth == aloc_depth_core) {
      config_setting_t *core_item = config_setting_get_elem(core, item->cid);
      config_setting_set_int_elem(core_item, -1, item->tid);
    }
    if (item->depth == aloc_depth_numa) {
      config_setting_t *numa_item = config_setting_get_elem(numa, item->cid);
      if (!numa_item) {
        fprintf(stderr, "ALOC ERROR: %d of numa is not available\n", item->cid);
        exit(-1);
      }
      config_setting_set_int_elem(numa_item, -1, item->tid);
    }

    current_node = current_node->next;
  }

  // now create setting for the reverse
  group = config_setting_add(root, "AUTOLOC-DETAIL", CONFIG_TYPE_GROUP);
  assert(group);
  current_node = start_node;

  while (current_node->item != NULL) {
    bind_t *item = current_node->item;
    char *gname;
    asprintf(&gname, "T%d", item->tid);
    config_setting_t *entry = config_setting_add(group, gname, CONFIG_TYPE_ARRAY);
    config_setting_set_int_elem(entry, -1, item->cid);
    config_setting_set_int_elem(entry, -1, item->depth);
    current_node = current_node->next;
  }

  // write the resulting config to file
  pid_t pid = getpid();
  char *filename;
  asprintf(&filename, "%d.cfg", pid);
  fprintf(stderr, "generating ... %s\n", filename);
  config_write_file(&config, filename);
  config_destroy(&config);
}

int aloc_getNumCores() { return aloc_ncores; }

void aloc_bind(int type, int depth, int tid, int id) {
  if (config_found == 1) {
    char *member;
    config_setting_t *root = config_root_setting(&aloc_config);
    config_setting_t *group = config_setting_get_member(root, "AUTOLOC-DETAIL");
    if (!group) {
      fprintf(stderr, "ALOC ERROR: configuration file needs AUTOLOC-DETAIL node\n");
      exit(-1);
    }

    char *gname;
    asprintf(&gname, "T%d", tid);
    config_setting_t *s = config_setting_get_member(group, gname);
    if (s) {
      //override
      id = config_setting_get_int_elem(s, 0);
      depth = config_setting_get_int_elem(s, 1);
    }
  }
  hwloc_obj_t obj = hwloc_get_obj_by_depth(aloc_topology, depth, id);

  bind_t *b = (bind_t *) malloc (sizeof(bind_t));
  node_t *n = (node_t *) malloc (sizeof(node_t));

  b->depth = depth;
  b->tid = tid;
  b->cid = id;
  n->next = NULL;
  n->item = NULL;

  // sync since many threads can access this
  pthread_mutex_lock(&aloc_mutex);
  current_node->item = b;
  current_node->next = n;
  current_node = n;
  pthread_mutex_unlock(&aloc_mutex);

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
