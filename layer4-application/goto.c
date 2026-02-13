#include "../rpi.h"
#include "track_server.h"
#include "marklin_worker.h"
#include "train_control.h"
#include "track_data_new.h"
#include "train_velocity.h"
#include "speed_measuring.h"
#include "goto.h"
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
// find_all_ahead_sensors returns a list of sensors that are ahead of the train
// they may be hit by the train
int find_all_ahead_sensors(const struct track_node *start,
                           const struct track_node *track,
                           const struct track_node **sensors, // it is an array
                           int *sensors_len,
                           int distance[TYPECOUNT][TRACK_MAX])
{
  struct track_node *queue[TRACK_MAX]; // this is the queue of the tracks
  int queue_size = 0;
  int queue_head = 0;
  int queue_tail = 0;
  *sensors_len = 0;
  int visited[TYPECOUNT][TRACK_MAX];
  for (int i = 0; i < TRACK_MAX; i++)
  {
    for (int j = 0; j < TYPECOUNT; j++)
    {
      visited[j][i] = 0;
      distance[j][i] = 9999999;
    }
  }
  distance[start->type][start->num] = 0;
  queue[queue_tail] = start; // start from one track node
  queue_tail++;
  queue_size++;
  // start from one track node
  while (queue_size > 0)
  {

    struct track_node *current = queue[queue_head];
    queue_head++;
    queue_size--;

    if (current->type == NODE_SENSOR)
    {
      // DO NOT ENQUEUE THE STRAIGHT NODE IT IS THE END IT IS TO BE ADDED TO THE LIST
      sensors[*sensors_len] = current;
      *sensors_len = *sensors_len + 1;
      continue;
    }
    struct track_node *reverse = current->reverse;
    if (current->type == NODE_BRANCH)
    {
      // we have found a branch
      struct track_node *curved = current->edge[DIR_CURVED].dest;
      // we have not visited the curved branch
      if (distance[curved->type][curved->num] > distance[current->type][current->num] + current->edge[DIR_CURVED].dist)
      {
        // we have found a shorter path to the curved branch
        distance[curved->type][curved->num] = distance[current->type][current->num] + current->edge[DIR_CURVED].dist;
        queue[queue_tail] = curved;
        queue_tail++;
        queue_size++;
      }
    }
    if (current->type != NODE_EXIT)
    {
      struct track_node *straight = current->edge[DIR_STRAIGHT].dest;
      if (distance[straight->type][straight->num] > distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist)
      {
        // we have found a shorter path to the straight branch
        distance[straight->type][straight->num] = distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist;
        queue[queue_tail] = straight;
        queue_tail++;
        queue_size++;
      }
    }
  }
  return 0;
}
// path find returns the distance from the start to the end
int Pathfind(const struct track_node *start, const struct track_node *end,
             const struct track_node *track,
             struct track_node *prev[TYPECOUNT][TRACK_MAX],
             int distance[TYPECOUNT][TRACK_MAX])
{
  struct track_node *queue[TRACK_MAX]; // this is the queue of the tracks
  int queue_size = 0;
  int queue_head = 0;
  int queue_tail = 0;
  int visited[TYPECOUNT][TRACK_MAX];
  for (int i = 0; i < TRACK_MAX; i++)
  {
    for (int j = 0; j < TYPECOUNT; j++)
    {
      visited[j][i] = 0;
      distance[j][i] = 9999999;
    }
  }
  distance[start->type][start->num] = 0;
  queue[queue_tail] = start; // start from one track node
  queue_tail++;
  queue_size++;
  // start from one track node
  while (queue_size > 0)
  {
    struct track_node *current = queue[queue_head];
    queue_head++;
    queue_size--;
    //
    struct track_node *reverse = current->reverse;
    if (current->type == NODE_BRANCH)
    {
      // we have found a branch
      struct track_node *curved = current->edge[DIR_CURVED].dest;
      // we have not visited the curved branch
      if (distance[curved->type][curved->num] > distance[current->type][current->num] + current->edge[DIR_CURVED].dist)
      {
        // we have found a shorter path to the curved branch
        prev[curved->type][curved->num] = current;
        distance[curved->type][curved->num] = distance[current->type][current->num] + current->edge[DIR_CURVED].dist;
        queue[queue_tail] = curved;
        queue_tail++;
        queue_size++;
      }
    }
    if (current->type != NODE_EXIT)
    {
      struct track_node *straight = current->edge[DIR_STRAIGHT].dest;
      if (distance[straight->type][straight->num] > distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist)
      {
        // we have found a shorter path to the straight branch
        prev[straight->type][straight->num] = current;
        distance[straight->type][straight->num] = distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist;
        queue[queue_tail] = straight;
        queue_tail++;
        queue_size++;
      }
    }
    if (current->type == NODE_SENSOR)
    {
      if (distance[reverse->type][reverse->num] > distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist)
      {
        // we have found a shorter path to the straight branch
        prev[reverse->type][reverse->num] = current;
        distance[reverse->type][reverse->num] = distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist;
        queue[queue_tail] = reverse;
        queue_tail++;
        queue_size++;
      }
    }
  }
  return (distance[end->type][end->num]);
}
// Train functions Begin
int parse_path(struct track_node *track,
               struct track_node *start,
               struct track_node *end,
               struct track_node **branches,
               int *branches_len,
               char *mode,
               int *mode_len,
               struct track_node **sen_list,
               int *sen_list_sz,
               struct track_node **revlist,
               int *revlist_len,
               int distance[TYPECOUNT][TRACK_MAX])
{
  struct track_node *previouse_node[TYPECOUNT][TRACK_MAX];
  Pathfind(start, end, track, previouse_node, distance);
  struct track_node *start_node = start;
  struct track_node *end_node = end;
  // get the path in two char arrays
  *branches_len = 0;
  *mode_len = 0;
  *sen_list_sz = 0;
  // compute the path
  while (end_node != start_node)
  {
    //
    struct track_node *prev_node = previouse_node[end_node->type][end_node->num];
    if (prev_node->type == NODE_BRANCH)
    {
      branches[*branches_len] = prev_node;
      *branches_len = *branches_len + 1;
      if (prev_node->edge[DIR_CURVED].dest == end_node)
      {
        // we have to switch the curved branch
        mode[*(mode_len)] = 'C';
        *(mode_len) = *(mode_len) + 1;
      }
      else
      {
        // we have to switch the straight branch
        mode[*(mode_len)] = 'S';
        *(mode_len) = *(mode_len) + 1;
      }
    }
    if (prev_node->type == NODE_SENSOR)
    {
      // we have to switch the straight branch
      sen_list[*(sen_list_sz)] = prev_node;
      *(sen_list_sz) = *(sen_list_sz) + 1;
    }
    // if it is reverse, we need to reverse the train
    if (0) //(prev_node->reverse == end_node)
    {
      revlist[(*revlist_len)] = prev_node;
      (*revlist_len)++;
    }
    end_node = previouse_node[end_node->type][end_node->num];
  }
  // print total distance
  return 0;
}
void solonoid_switches_helper(char track_id, char *start_str, char *end_str)
{

  struct track_node track[TRACK_MAX];
  struct track_node *revlist[TRACK_MAX];
  int revlist_len;
  if (track_id == 'a')
  {
    init_tracka(track);
  }
  else
  {
    init_trackb(track);
  }
  struct track_node *banches[TRACK_MAX];
  int branches_len;
  char mode[TRACK_MAX];
  int mode_len;
  struct track_node *sen_list[TRACK_MAX];
  int sen_list_sz;
  int distance[TYPECOUNT][TRACK_MAX];
  struct track_node *start = get_track_node_by_name(track, start_str);
  struct track_node *end = get_track_node_by_name(track, end_str);
  parse_path(track, start, end,
             banches, &branches_len,
             mode, &mode_len,
             sen_list, &sen_list_sz,
             revlist, &revlist_len,
             distance);
  // iterate through all the branch nodes
  // solonoid command time get from the marklin_worker
  // set the cursor to it's approperate location

  // print the nodes in string format
  int marklin_worker_tid = WhoIs("marklin_worker");
  for (int i = 0; i < branches_len; i++)
  {
    // solonoid(banches[i]->num, mode[i]);
    // print name of the branch node

    set_solonoid(marklin_worker_tid, banches[i]->num, mode[i]);
  }
}
int lookahead(char track_id, char *nodename, struct track_node *nodes[TRACK_MAX], int nodes_len, int distance[TYPECOUNT][TRACK_MAX])
{
  // init start node
  struct track_node track[TRACK_MAX];
  if (track_id == 'a')
  {
    init_tracka(track);
  }
  else
  {
    init_trackb(track);
  }
  struct track_node *start = get_track_node_by_name(track, "BR3");
  // init a list of nodes pointers

  // init a list of nodes pointers
  // find_all_ahead_sensors

  find_all_ahead_sensors(start, track, nodes, &nodes_len, distance);
  for (int i = 0; i < nodes_len; i++)
  {
  }
  return 0;
}
int solonoid_switches_task()
{
  char start_str[10];
  char end_str[10];
  char track_id;
  int tid;
  int track_server_tid = WhoIs("track_server");
  track_id = get_track_id(track_server_tid);
  Receive(&tid, start_str, 10);
  Reply(tid, start_str, 0);
  Receive(&tid, end_str, 10);
  Reply(tid, end_str, 0);
  solonoid_switches_helper(track_id, start_str, end_str);
  return 0;
}
// working on STOP AT
// in which the train need to stop at a location. This requires delicate tuning of delay_until_stop and sensor stop
void path_switch(char *start_str, char *end_str)
{
  int tid = Create(1, solonoid_switches_task);
  Send(tid, start_str, 10, NULL, 0);
  Send(tid, end_str, 10, NULL, 0);
}

void stop_at_task()
{
  int offset = 0;
  // need to make sure that the distance to next node is smaller than stopping distance
  int tid;
  char trainid;
  Receive(&tid, &trainid, 1);
  Reply(tid, NULL, 0);
  char dest[10];
  Receive(&tid, dest, 10);
  Reply(tid, NULL, 0);
  char speed = getspeed_train(WhoIs("track_server"), trainid);

  int stopping_dist[TRAIN_MAX][SPEED_MAX];
  struct train_velocity vel_list[TRAIN_MAX][SPEED_MAX];

  init_stoppint_dist(stopping_dist);
  init_vel(vel_list);
  // get current sensor location

  char train_id = 0x01;

  // get the track server tid
  int track_server_tid = WhoIs("track_server");
  int clock_server_tid = WhoIs("clock_server");
  int marklin_worker_tid = WhoIs("marklin_worker");
  // instantiate track
  // get the track from marklin_worker_tid

  struct track_node *trackmap[20][20];
  struct track_node track[TRACK_MAX];
  get_track_node_map(track, trackmap);

  if (get_track_id(track_server_tid) == 'a')
  {
    init_tracka(track);
  }
  else
  {
    init_trackb(track);
  }
  // get the switch states
  char sw_states[SWITCH_COUNT];
  get_switches(track_server_tid, sw_states, SWITCH_COUNT);
  // get the sensor server tid
  // next_type_node
  int dist = 0;
  int isexit = 0;
  // this is where most errors may occure

  uint32_t time;
  // get the sensor id
  char s88_id = 0;
  char sensor_no = 0;
  char is_released = 0;
  char ret[4];
  time = await_sensor(track_server_tid, ret);
  s88_id = ret[0];
  sensor_no = ret[1];
  is_released = ret[2];
  /**/
  // clear the line
  // print on row and col

  // PRINTSWITCHROW + offset, PRINTSWITCHCOL
  uart_printf(CONSOLE, "\033[%d;%dH", GOTC_ROW + offset, GOTC_COL);
  uart_printf(CONSOLE, "\033[32m");
  uart_printf(CONSOLE, "stop a executed %d%d\r\n", s88_id, sensor_no);
  uart_printf(CONSOLE, "\033[37m");
  offset++;
  // must be a pressed sensor if it is released it does not count
  // struct track_node *next_type_node(char *sw_states, int type, struct track_node *start_node, int *dist, int *isexit)
  // this cannot yeild a segmentation fault
  struct track_node *start_node = trackmap[s88_id][sensor_no];
  // det end node
  struct track_node *end_node = get_track_node_by_name(track, dest);
  int dist_to_next_node = dist_to_node(start_node, end_node);
  // this is here to get the critical node
  struct track_node *curnode;
  // print the start and end node names

  /*
 offset++;
 while (dist_to_next_node > stopping_dist[trainid][speed])
 {
   time = await_sensor(track_server_tid, ret);
   s88_id = ret[0];
   sensor_no = ret[1];
   is_released = ret[2];
   if (is_released)
   {
     continue;
   }

   // get next node
   // get curnode with trackmap
   curnode = trackmap[s88_id][sensor_no];
   struct track_node *next_node = next_type_node(sw_states, NODE_SENSOR, start_node, &dist, &isexit);
   // print the name of the nodes curnode and next_node and end_node
   offset++;
   // struct track_node *next_type_node(char *sw_states, int type, struct track_node *start_node, int *dist, int *isexit)
   // int dist_to_node(struct track_node *start, struct track_node *end)
   dist_to_next_node = dist_to_node(next_node, end_node);
   // get the distance to next node
   // get the velocity
 }
 // move t
 // int compute_time(dist, struct train_velocity *speed)
 //vel_list[trainid][speed]
 // int compute_time(dist, struct train_velocity *speed)
 uint32_t time_to_stop = compute_time(dist, &vel_list[trainid][speed]);
 // get the current time
 DelayUntil(time_to_stop + time);
 // set the train to stop
 set_train_state(marklin_worker_tid, train_id, 0);
 */
  // delay until time +
  Exit();
}
void stop_at(int trainid, char *dest)
{
  // initialize the task
  int tid = Create(1, stop_at_task);
  // send the trainid and speed to the task
  Send(tid, &trainid, 1, NULL, 0);
  Send(tid, &dest, 10, NULL, 0);
}