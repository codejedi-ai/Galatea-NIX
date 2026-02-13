#include "train.h"
#include "../rpi.h"
#include "../util.h"
#include "../nameserver.h"
#include "../ioserver.h"
#include "speed_measuring.h"
#include "../custstr.h"
#define SENSOR 0
#define TRAIN 1
#define SWITCH 2


int dist_to_node(struct track_node *start, struct track_node *end)
{
  struct track_node *queue[TRACK_MAX]; // this is the queue of the tracks

  int queue_size = 0;
  int queue_head = 0;
  int queue_tail = 0;

  int distance[TYPECOUNT][TRACK_MAX];

  for (int i = 0; i < TRACK_MAX; i++)
  {
    for (int j = 0; j < TYPECOUNT; j++)
    {
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
    if(current == end){
      return distance[current->type][current->num];
    }

    if (current->type == NODE_BRANCH)
    {
      // we have found a branch
      struct track_node *curved = current->edge[DIR_CURVED].dest;
      // we have not visited the curved branch
      if (distance[curved->type][curved->num] > distance[current->type][current->num] + current->edge[DIR_CURVED].dist)
      {
        // we have found a shorter path to the curved branch
        // prev[curved->type][curved->num] = current;
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
        // prev[straight->type][straight->num] = current;
        distance[straight->type][straight->num] = distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist;
        queue[queue_tail] = straight;
        queue_tail++;
        queue_size++;
      }
    }
    struct track_node *reverse = current->reverse;
    if (distance[reverse->type][reverse->num] > distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist)
    {
      // we have found a shorter path to the straight branch
      // prev[reverse->type][reverse->num] = current;
      distance[reverse->type][reverse->num] = distance[current->type][current->num] + current->edge[DIR_STRAIGHT].dist;
      queue[queue_tail] = reverse;
      queue_tail++;
      queue_size++;
    }
  }
  return (distance[end->type][end->num]);
}

void get_track_node_map(struct track_node *track, struct track_node *trackmap[20][20])
{
  // iterate through all the SENSOR_NODEs and find the one that matches the s88_id and sensor_no
  // the s88_id is the s88 that is triggered in alphabet A,B,C,D
  // the sensor_no is the sensor that is triggered in the s88 from 1 - 16
  // the naming convention is A1, A2, A3, A4, B1, B2, B3, B4, C1, C2, C3, C4, D1, D2, D3, D4.....
  for (int i = 0; i < TRACK_MAX; i++)
  {
    if (track[i].type == NODE_SENSOR)
    {
      // get name
      char *name = track[i].name;
      // get the s88_id the first character
      char s88_id = name[0] - 'A';
      // get the sensor_no the number that is after the first character
      int64_t sensor_no = atoi_64(&name[1]);
      // uart_printf(CONSOLE, "name:%s s88_id = %d, sensor_no = %d\r\n",name , s88_id, sensor_no);
      trackmap[s88_id][sensor_no] = &track[i];
    }
  }
}
// string -> node reverse name search
struct track_node* get_track_node_by_name(struct track_node *track, char* name){
  for(int i = 0; i < TRACK_MAX; i++){
    if(strcmp_ret(track[i].name, name)){
      return &track[i];
    }
  }
  return 0;
}
struct track_node *next_type_node(char *sw_states, int type, struct track_node *start_node, int *dist, int *isexit)
{
  // the current node is a sensor node then the for loop would not run
  struct track_node *current_node = start_node;
  int offset = 0;
  while ((current_node == start_node) || (current_node->type != type && current_node->type != NODE_EXIT))
  {
    offset++;
    // if the current position is a switch then we need to consult the switch table. Which is managed by the switch worker
    // uart_printf(CONSOLE, "type: %d next_sensor_node: current_node->type = %d, current_node = %s\n",type, current_node->type, current_node->name);
    // uart_getc(CONSOLE);
    // int buf;
    // scanf("%d", &buf);
    if (current_node->type == NODE_BRANCH)
    {
      char sw_state = sw_states[current_node->num];
      // this is a switch
      // check the switch table
      // if the switch is set to straight then we need to return the straight node
      if (sw_state == 'S')
      {

        *dist += current_node->edge[0].dist;
        current_node = current_node->edge[0].dest;
        // print the distance current_node->edge[0].dist
        // printf("next_sensor_node: next_node->type = %d, next_node = %s\n",current_node->type, current_node->name);
        // printf("current_node->edge[0].dist = %d\n", current_node->edge[0].dist);
      }
      // if the switch is set to curved then we need to return the curved node
      if (sw_state == 'C')
      {

        *dist += current_node->edge[1].dist;
        current_node = current_node->edge[1].dest;
        // printf("next_sensor_node: next_node->type = %d, next_node = %s\n",current_node->type, current_node->name);
        // printf("current_node->edge[1].dist = %d\n", current_node->edge[1].dist);
      }
    }
    else
    {

      *dist += current_node->edge[0].dist;
      current_node = current_node->edge[0].dest;
      // print the distance current_node->edge[0].dist
      // printf("next_sensor_node: next_node->type = %d, next_node = %s\n",current_node->type, current_node->name);
      // printf("current_node->edge[0].dist = %d\n", current_node->edge[0].dist);
    }
  }
  if (current_node->type == NODE_EXIT)
  {
    *isexit = 1;
  }
  // this node can be exit node or not
  return current_node;
}
