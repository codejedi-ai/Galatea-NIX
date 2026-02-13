#include "train_velocity.h"
void init_stoppint_dist(int stopping_dist[TRAIN_MAX][SPEED_MAX]){
    stopping_dist[77][10] = 660;
}
void init_vel(struct train_velocity vel_list[TRAIN_MAX][SPEED_MAX]){
    vel_list[77][10].dist = 4;
    vel_list[77][10].time = 1;
}
int compute_time(int dist, struct train_velocity *speed)
{
  int time = (dist / speed->dist) * speed->time;
  return time;
}