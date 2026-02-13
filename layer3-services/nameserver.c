#include "processes.h"
#include "rpi.h"
#include "asm.h"
#include "syscall.h"
#include "util.h"
#include "nameserver.h"
#include "custstr.h"
#define DEBUG 0
void helper_parsestring( char *retarr, int size, char * str, int part) {
  int i = 1, j = 0;
  retarr[0] = 0;
  while (*str) {
    if (*str == ' ') {
      i++;
    }else if (part == i){
      if (j >= size - 1) {
        break;
      }
      retarr[j++] = *str;
      retarr[j] = 0;
    }
    str++;
  }
  
}
/*
These are the most essential terminal control sequences that you will need for your train program.

Code	Effect
"\033[2J"	Clear the screen.
"\033[H"	Move the cursor to the upper-left corner of the screen.
"\033[r;cH"	Move the cursor to row r, column c. Note that both the rows and columns are indexed starting at 1.
"\033[?25l"	Hide the cursor.
"\033[K"	Delete everything from the cursor to the end of the line.
These control sequences can help make your program's display more lively.

Code	Effect
"\033[0m"	Reset special formatting (such as colour).
"\033[30m"	Black text.
"\033[31m"	Red text.
"\033[32m"	Green text.
"\033[33m"	Yellow text.
"\033[34m"	Blue text.
"\033[35m"	Magenta text.
"\033[36m"	Cyan text.
"\033[37m"	White text.

*/

void nameserver(){
	char* pid_names[NUMPROCS][50];
	int tid;
	char msg[50];
	int msglen = 49;
	for (int i = 0; i < NUMPROCS; i++)
	{
		pid_names[i][0] = 0;
	}
	cust_strcpy(pid_names[1], 50, "nameserver", 50);
	cust_strcpy(pid_names[0], 50, "kernel", 50);
	while (1)
	{

		int recret = Receive(&tid, msg, msglen);
		// strflush(msg, 50);

		char command[50];
		char name[50];


		helper_parsestring(command, 50, msg, 1);
		helper_parsestring(name, 50 , msg, 2);


		// now print the compair
		char *command_cand = "REGISTER";
		int cmp;
		// strcmp_inpace(&cmp, command, command_cand);
		cmp = strcmp_ret(command, command_cand);

		if (cmp){
			cust_strcpy(pid_names[tid], 50, name, 50);
			// this registers a PID with a name
			// change font to blue
			# if DEBUG == 2
			uart_printf(CONSOLE, "\033[32m");
			uart_printf(CONSOLE, "REGISTERD PID: %u, Name: %s\r\n", tid, pid_names[tid]);
			uart_printf(CONSOLE, "\033[37m");
			# endif

			// change font to white
			int repret = 0;
			int repret_2 = Reply(tid, &repret, 0);
		}
		command_cand = "WHOIS";
		strcmp_inpace(&cmp, command, command_cand);

		if (cmp){

			// this registers a PID with a name
			int ret = 0;
			while (ret < NUMPROCS)
			{

				if (strcmp_ret(pid_names[ret], name))
				{
					int repret = Reply(tid, &ret, 4);
					break;
				}
				ret++;
			}
			if(ret >= NUMPROCS){
				ret = -1;
				int repret = Reply(tid, &ret, 4);
			}
		}
		command_cand = "DEREGISTER";
		strcmp_inpace(&cmp, command, command_cand);
		if (cmp){
			// this registers a PID with a name
			// print the deregistered in red
			# if DEBUG == 2
			uart_printf(CONSOLE, "\033[31m");
			uart_printf(CONSOLE, "DEREGISTERD PID: %u, Name: %s\r\n", tid, pid_names[tid]);
			uart_printf(CONSOLE, "\033[37m");
			# endif
			pid_names[tid][0] = 0;
			int repret = 0;
			int repret2 = Reply(tid, &repret, 0);
		}
		command_cand = "GETNAME";
		# if DEBUG == 2
		// print the registered table in green
		uart_printf(CONSOLE, "\033[34m");
		uart_printf(CONSOLE, "Registered Table:\r\n");
		for (int i = 0; i < NUMPROCS; i++)
		{
			
			uart_printf(CONSOLE, "PID: %u, Name: %s\r\n", i, pid_names[i]);
		}
		uart_printf(CONSOLE, "\033[37m");
		# endif
	}
	Exit();
}
// must return -1 if not found
int RegisterAs(const char *name){
	int rep = 0;
	char sendmsg[50] = "REGISTER ";
	//// strflush(command_cand, 6);
	int msgsz = 50;
	//msgsz = strcat_cust((char* )sendmsg, command_cand);
	//// strflush(sendmsg, msgsz);
	msgsz = strcat_cust((char* )sendmsg, name);
	//// strflush(sendmsg, msgsz);
	Send(1, sendmsg, 50, &rep, 4);
	return rep;
}
int Deregister(){
	int tid = MyTid();
	char rep[50];
	char sendmsg[50] = "DEREGISTER ";
	char tid_str[10] = "";
	i2a(tid, tid_str);
	int msgsz = 50;
	msgsz = strcat_cust((char* )sendmsg, (char *)tid_str);
	// strflush(sendmsg, msgsz);
	return Send(1, sendmsg, 50, rep, 50);
}
int WhoIs(const char *name){
	int rep = 0;
	char sendmsg[50] = "WHOIS ";

	//// strflush(command_cand, 6);
	int msgsz = 50;
	// msgsz = strcat_cust((char* )sendmsg, command_cand);
	//// strflush(sendmsg, msgsz);
	msgsz = strcat_cust((char* )sendmsg, name);
	// strflush(sendmsg, msgsz);
	
	//int ret_code = Send(tid, msg, msglen, msgreply, 25);
	Send(1, sendmsg, 50, &rep, 4);
	return rep;
}