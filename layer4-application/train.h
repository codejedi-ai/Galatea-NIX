#ifndef TRAIN_H
#define TRAIN_H
#include "../rpi.h"
#include "../rpi.h"
#include "track_data_new.h"
#define TRAIN_MAX 80
#define SPEED_MAX 16
struct train
{
    /* data */
    int sensor_pushed_count;
    track_node *position;
    track_node *predict_sensor;
    uint32_t sensor_time;
    uint32_t dist;
    int speed;
    int train_num;
    int dist_to_next_sensor;
    uint8_t light;
    
};
#endif