
#define SWITCH_COUNT 18
#define SWITCH_MAX_count 255
// void set_switch(int sw_server_tid, uint8_t sw_ind, char state);
// char get_switch(int sw_server_tid, uint8_t sw_ind);

// a train can control the sensor direction
#define PREDICTNODECOL 130
#define TABLEROW 50
#define TABLECOL 50
#define SENSORROW 50
#define SENSORCOL 0
// look ahead should be done by the shell as it also involves displaying stuff on the screen

void get_track_node_map(struct track_node *track, struct track_node *trackmap[20][20]);
struct track_node* get_track_node_by_name(struct track_node *track, char* name);
struct track_node *next_type_node(char *sw_states, int type, struct track_node *start_node, int *dist, int *isexit);
int dist_to_node(struct track_node *start, struct track_node *end);