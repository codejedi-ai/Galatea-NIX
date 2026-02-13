#ifndef TRAIN_CONTROL_H
#define TRAIN_CONTROL_H

void reverse(char train_id, char speed);
void sensor_stop(trainid);
void sensor_delay_stop(int trainid, int delay_since_interrupt);
#define TC_ROW 9
#define TC_COL 200
#endif