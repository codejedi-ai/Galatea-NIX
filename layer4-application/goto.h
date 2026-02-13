#include "track_server.h"
#include "marklin_worker.h"
#include "train_control.h"
#include "track_data_new.h"
/*
Track node.c
typedef enum {
  NODE_NONE,
  NODE_SENSOR,
  NODE_BRANCH,
  NODE_MERGE,
  NODE_ENTER,
  NODE_EXIT,
} node_type;
*/
void path_switch(char* start_str, char* end_str);
#define GOTC_ROW 28
#define GOTC_COL 30