#ifndef TRACK_SERVER_H
#define TRACK_SERVER_H
#include "train.h"
#define SWITCH_COUNT 18
#define SWITCH_MAX_count 255

// void set_switch(int track_server_tid, uint8_t sw_ind, char state);
// char get_switch(int track_server_tid, uint8_t sw_ind);


// a train can control the sensor direction

#define PREDICTNODECOL 130
#define TABLEROW 50
#define TABLECOL 50
#define SENSORROW 50
#define SENSORCOL 0
void get_sensor_pushed(int track_server_tid, struct track_node track[TRACK_MAX], int tracks_max);
char get_track_id(int track_server_tid);
void init_track(int track_server_tid, char track_id);
int getspeed_train(int track_server_tid, char train_no);
// void deregister_train(int track_server_tid, char train_no);
uint64_t await_sensor(int track_server_tid, char *ret);
uint64_t get_switches(int track_server_tid, char *sw_states, int sw_count);
// Processes
void track_server();
#endif