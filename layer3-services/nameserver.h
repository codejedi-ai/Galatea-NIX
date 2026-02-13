/*
THIS WOULD ONLY WORK IF SEND TASKS IS IMPLEMENTED WHICH CURRENTLLY IS NOT
This section comprises the interface of the name server. Knowing how and where to start name resolution is generally referred to as closure mechanism and must be implicit. Therefore this interface does not include a task id.
These interface functions can be implemented as wrappers covering a Send to the name server.


registers the task id of the caller under the given name. On return without error it is guaranteed that all WhoIs() calls by any task will return the task id of the caller until the registration is overwritten. If another task has already registered with the given name, its registration is overwritten.
Return Value
0	success.
-1	unable to reach name server
*/
int RegisterAs(const char *name);
int Deregister();
/*
asks the name server for the task id of the task that is registered under the given name. Whether WhoIs() blocks waiting for a registration or returns with an error, if no task is registered under the given name, is implementation-dependent. There is guaranteed to be a unique task id associated with each registered name, but the registered task may change at any time after a call to WhoIs().
Return Value
tid	task id of the registered task.
-1	unable to reach name server
*/
int WhoIs(const char *name);

void nameserver(void);