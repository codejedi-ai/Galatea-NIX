#include "tc1tests.h"
#include "../rpi.h"
#include "../syscall.h"
#include "../nameserver.h"
#include "../custstr.h"
#include "../systimer.h"
#include "../clockserver.h"
#include "../tc1/marklin_worker.h"
#include "../tc1/train_control.h"
#include "../tc1/track_data_new.h"
#include "../tc1/goto.h"
#include "../ioserver.h"
#define delay 0
#define DJIKSTRAS_ROW 1
#define DJIKSTRAS_COL 1
// Train functions Begin
/*

code functions on
64 all off
65 #1
66 #2
67 #1,#2
68 #3
69 #3,#1
70 #3,#2
71 #3,#2,#1
72 #4
73 #4,#1
74 #4,#2
75 #4,#2,#1
76 #4,#3
77 #4,#3,#1
78 #4,#3,#2
79 #4,#3,#2,#1
*/


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
#define PRINT_ROW 1
#define PRINT_COL 1


int tc1ExecuteCommands(char *command, char **num, int command_part_count){
  // if command is in the form of tc trainid speed

  int marklin_worker_tid = WhoIs("marklin_worker");
  if (strcmp_ret(command, "tr")){
    int trainid = atoi_64(num[1]);
    int speed = atoi_64(num[2]);
    if (trainid < 1 || trainid > 80 || speed < 0 || speed > 14){
      // set the position of cursor to DJIKSTRAS_ROW + offset and DJIKSTRAS_COL
      
      uart_printf(CONSOLE, "Invalid trainid or speed\r\n");
      return 1;
    }
    // set the speed of the train
    set_train_state(marklin_worker_tid, trainid, speed);
    return 0;
  }
  // if the command is in the form of sw switchid direction
  if (strcmp_ret(command, "sw")){
    int switchid = atoi_64(num[1]);
    int direction = num[2][0];
    if (switchid < 1 || switchid > 18 || (direction != 'C' && direction != 'S')){
      uart_printf(CONSOLE, "Invalid switchid or direction\r\n");
      return 1;
    }
    // set the direction of the switch
    set_solonoid(marklin_worker_tid, switchid, direction);
    return 0;
  }
  // if the command is in the form of rv trainid
  if (strcmp_ret(command, "rv")){
    int trainid = atoi_64(num[1]);
    int speed = atoi_64(num[2]);
    if (trainid < 1 || trainid > 80){
      uart_printf(CONSOLE, "Invalid trainid\r\n");
      return 1;
    }
    uart_printf(CONSOLE, "Reversing train ....%d\r\n", trainid);
    reverse(trainid, speed);
    
    return 0;
  }
  // testing stopping distance. If this is executed the marklin would make a task that would execute the stop commmand when the train has hit a sensor node
  if (strcmp_ret(command, "tcstd")){
    int trainid = atoi_64(num[1]);
    int speed = atoi_64(num[2]);
    if (trainid < 1 || trainid > 80){
      uart_printf(CONSOLE, "Invalid trainid\r\n");
      return 1;
    }
    uart_printf(CONSOLE, "NOT IMPLEMENTED: Testing stopping distance for train %d\r\n", trainid);
    return 0;
  }
  // void delay_until_stop(int delaytime, char train_id)
  if(strcmp_ret(command, "delaystop")){
    int trainid = atoi_64(num[1]);
    int delaytime = atoi_64(num[2]);
    if (trainid < 1 || trainid > 80){
      uart_printf(CONSOLE, "Invalid trainid\r\n");
      return 1;
    }
    uart_printf(CONSOLE, "Delaying train %d until it stops\r\n", trainid);
    delay_until_stop(delaytime, trainid);
    return 0;
  }
  // add a stop at a sensor command
  if (strcmp_ret(command, "interruptstop")){
    int trainid = atoi_64(num[1]);
    if (trainid < 1 || trainid > 80){
      uart_printf(CONSOLE, "Invalid trainid\r\n");
      return 1;
    }
    uart_printf(CONSOLE, "Stopping train %d\r\n", trainid);
    sensor_stop(trainid);
    return 0;
  }
  // add one for sensor_delay_stop
  if (strcmp_ret(command, "sensordelaystop")){
    int trainid = atoi_64(num[1]);
    int delaytime = atoi_64(num[3]);
    if (trainid < 1 || trainid > 80){
      uart_printf(CONSOLE, "Invalid trainid\r\n");
      return 1;
    }
    uart_printf(CONSOLE, "Stopping train %d\r\n", trainid);
    sensor_delay_stop(trainid, delaytime);
    return 0;
  }
  // void path_switch(char* start_str, char* end_str);
  if (strcmp_ret(command, "ps")){
    char* start_str = num[1];
    char* end_str = num[2];
    path_switch(start_str, end_str);
    return 0;
  }
  // stop_at void stop_at(int trainid, char *dest)
  if (strcmp_ret(command, "stopa")){
    int trainid = atoi_64(num[1]);
    char* dest = num[2];
    stop_at(trainid, dest);
    return 0;
  }
  return 2;
}