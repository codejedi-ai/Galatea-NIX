#include "../tc1/train.h"
struct train_velocity {
  int dist;
  int time;
};
void init_stoppint_dist(int stopping_dist[TRAIN_MAX][SPEED_MAX]);
void init_vel(struct train_velocity vel_list[TRAIN_MAX][SPEED_MAX]);
int compute_time(int dist, struct train_velocity *speed);