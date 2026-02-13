#include "../rpi.h"
#include "../clockserver.h"
#include "track_data_new.h"
#include "marklin_worker.h"
#include "train_control.h"
#include "train_velocity.h"
// Delay until interrupt for stop
void delay_until_stop_task(){
    int tid;
    int delay_since_interrupt = 0;
    Receive(&tid, &delay_since_interrupt, 4);
    Reply(tid, &tid, 0);
    char train_id = 0x01;
    Receive(&tid, &train_id, 1);
    Reply(tid, &train_id, 0);
    char ret[4];
    // there is a sensor that is recentlly triggered
    // await for the interrupt from the task server
    uint32_t time = await_sensor(WhoIs("track_server"), ret);

    char s88_id = ret[0];
    char sensor_no = ret[1];
    char is_released = ret[2];
    while (!is_released)
    {
      /* code */
      time = await_sensor(WhoIs("track_server"), ret);
      s88_id = ret[0];
      sensor_no = ret[1];
      is_released = ret[2];
    }
    

    int clock_server_tid = WhoIs("clock_server");
    int track_server_tid = WhoIs("track_server");
    int marklin_worker_tid = WhoIs("marklin_worker");



    DelayUntil(clock_server_tid, time + delay_since_interrupt);
    set_train_state(marklin_worker_tid, train_id, 0);
    
}
void delay_until_stop(int delaytime, char train_id){
    int tid = Create(1, &delay_until_stop_task);
    Send(tid, &delaytime, 4, NULL, 0);
    Send(tid, &train_id, 1, NULL, 0);
}

void execute_reverse_command(){  // Binary: 00000001)
// void set_train_state(int marklin_worker_tid, uint8_t train_ind, char speed);
    
    char train_id = 0x01;
    char speed = 0x00;
    int tid;
    Receive(&tid, &train_id, 1);
    Reply(tid, NULL, 0);
    Receive(&tid, &speed, 1);
    Reply(tid, NULL, 0);
    /*
      command_wrapper(0, id);
      command_wrapper(15, id);
      command_wrapper(speed, id);
*/
    int marklin_worker_tid = WhoIs("marklin_worker");
    int clock_server_tid = WhoIs("clock_server");
    //print on the column 200
    int rev_delay = 100;

    uart_printf(CONSOLE, "\033[%d;%dH", 9, 200);
    uart_printf(CONSOLE, "execute_reverse_command\r\n");
    uart_printf(CONSOLE, "\033[%d;%dH", 10, 200);
    // clear the line
    uart_printf(CONSOLE, "\033[K");
    uart_printf(CONSOLE, "\033[%d;%dH", 11, 200);
    // clear the line
    uart_printf(CONSOLE, "\033[K");
    uart_printf(CONSOLE, "\033[%d;%dH", 12, 200);
    // clear the line
    uart_printf(CONSOLE, "\033[K");
    uart_printf(CONSOLE, "\033[%d;%dH", 13, 200);


    uart_printf(CONSOLE, "\033[%d;%dH", 10, 200);
    uart_printf(CONSOLE, "Stopping train ....%d\r\n", train_id);
    set_train_state(marklin_worker_tid, train_id, 0);
    Delay(clock_server_tid, rev_delay);
    uart_printf(CONSOLE, "\033[%d;%dH", 11, 200);
    uart_printf(CONSOLE, "Reversing train ....%d\r\n", train_id);
    set_train_state(marklin_worker_tid, train_id, 15);
    Delay(clock_server_tid, rev_delay);
    uart_printf(CONSOLE, "\033[%d;%dH", 12, 200);
    uart_printf(CONSOLE, "Setting speed to %d ....%d\r\n", speed, train_id);
    set_train_state(marklin_worker_tid, train_id, speed);
    uart_printf(CONSOLE, "\033[%d;%dH", 13, 200);
    uart_printf(CONSOLE, "Reversed train %d\r\n", train_id);
    
    Exit();
}
void reverse(char train_id, char speed){
    int tid = Create(1, execute_reverse_command);
    Send(tid, &train_id, 1, NULL, 0);
    Send(tid, &speed, 1, NULL, 0);
}

// this is the polling loop for each individual train task
void sensor_stop_task(){
  // get the trainID and the speed to run at
  int tid;
  char train_id = 0x01;
  Receive(&tid, &train_id, 1);
  Reply(tid, NULL, 0);
  // get the track server tid
  int track_server_tid = WhoIs("track_server");
  // get the clock server tid
  int clock_server_tid = WhoIs("clock_server");
  // get the marklin worker tid
  int marklin_worker_tid = WhoIs("marklin_worker");
  // get the sensor server tid
  int sensor_server_tid = WhoIs("sensor_server");
  // wait for a sensor to be triggered
  char ret[4];
  uint32_t time = await_sensor(track_server_tid, ret);
  // get the sensor id
  char s88_id = ret[0];
  char sensor_no = ret[1];
  char is_released = ret[2];
  // must be a pressed sensor if it is released it does not count
  while (!is_released)
  {
    /* code */
    time = await_sensor(track_server_tid, ret);
    s88_id = ret[0];
    sensor_no = ret[1];
    is_released = ret[2];
  }
  // move to TC_ROW and TC_COL
  uart_printf(CONSOLE, "\033[%d;%dH", TC_ROW, TC_COL);
  // clear the line
  uart_printf(CONSOLE, "\033[K");
  uart_printf(CONSOLE, "Sensor triggered: ");
  uart_putc(CONSOLE, s88_id + 'A');
  uart_printf(CONSOLE, "%d\r\n", sensor_no);
  // now stop the train
  set_train_state(marklin_worker_tid, train_id, 0);
  Exit();
}
void sensor_stop(trainid){
  // initialize the task
  int tid = Create(1, sensor_stop_task);
  // send the trainid and speed to the task
  Send(tid, &trainid, 1, NULL, 0);
}
void sensor_delay_stop_task(){
    // get the trainID and the speed to run at
  int tid, delay_since_interrupt;
  char train_id = 0x01;
  Receive(&tid, &train_id, 1);
  Reply(tid, NULL, 0);
  Receive(&tid, &delay_since_interrupt, 4);
  Reply(tid, NULL, 0);
  // get the track server tid
  int track_server_tid = WhoIs("track_server");
  // get the clock server tid
  int clock_server_tid = WhoIs("clock_server");
  // get the marklin worker tid
  int marklin_worker_tid = WhoIs("marklin_worker");
  // get the sensor server tid
  int sensor_server_tid = WhoIs("sensor_server");
  // wait for a sensor to be triggered
  char ret[4];
  uint32_t time = await_sensor(track_server_tid, ret);
  // get the sensor id
  char s88_id = ret[0];
  char sensor_no = ret[1];
  char is_released = ret[2];
  // must be a pressed sensor if it is released it does not count
  while (!is_released)
  {
    /* code */
    time = await_sensor(track_server_tid, ret);
    s88_id = ret[0];
    sensor_no = ret[1];
    is_released = ret[2];
  }
  // move to TC_ROW and TC_COL
  uart_printf(CONSOLE, "\033[%d;%dH", TC_ROW, TC_COL);
  // clear the line
  uart_printf(CONSOLE, "\033[K");
  uart_printf(CONSOLE, "Sensor triggered: ");
  uart_putc(CONSOLE, s88_id + 'A');
  uart_printf(CONSOLE, "%d\r\n", sensor_no);
  // now stop the train
  delay_until_stop(delay_since_interrupt, train_id);
  Exit();

}


// at this point we need to know the value of delay_since_interrupt
void sensor_delay_stop(int trainid, int delay_since_interrupt){
  // initialize the task
  int tid = Create(1, sensor_delay_stop_task);
  // send the trainid and speed to the task
  Send(tid, &trainid, 1, NULL, 0);
  Send(tid, &delay_since_interrupt, 4, NULL, 0);
}
