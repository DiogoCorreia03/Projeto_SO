#include "tecnicofs_client_api.h"
#include <stdlib.h> 
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

/* client session data */
static int session_id = -1;
static char c_pipe_path[PIPE_NAME_LENGTH];
static char s_pipe_path[PIPE_NAME_LENGTH];
static int fserv, fcli;

int destroy_session(){
    if (close(fserv) != 0) return -1;
	if (close(fcli) != 0) return -1;

	if (unlink(c_pipe_path) == -1)
		return -1;
    session_id = -1; // Reset the session_id
	return 0;
}

static void handler(int signum) {
	if (signum == SIGPIPE) {
		destroy_session();
	}
}

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
	signal(SIGPIPE, handler);
	/* create client pipe */
	unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0777) < 0) return -1;

	strcpy(c_pipe_path, client_pipe_path);
    strcpy(s_pipe_path, server_pipe_path);

	/* Send request to server */
	fserv = open(server_pipe_path, O_WRONLY);
	if(fserv == -1)
		return -1;

    char opcode = TFS_OP_CODE_MOUNT;
	size_t size = strlen(client_pipe_path);
    char buf[1+PIPE_NAME_LENGTH];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;

	memcpy(buf, &opcode, 1);
    memcpy(buf+1, client_pipe_path, size);
	memset(buf+1+size, '\0', PIPE_NAME_LENGTH-size);
    if(write(fserv, buf, 1+PIPE_NAME_LENGTH) == -1)
		return -1;

	/* Request sent */

	/* Receive answer from server */
	fcli = open(client_pipe_path, O_RDONLY);
    if (fcli == -1) 
		return -1;

    if(read(fcli, &session_id, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return 0;
}

int tfs_unmount() {
	signal(SIGPIPE, handler);
	/* Send request to server */
    char opcode = TFS_OP_CODE_UNMOUNT;
	char buf[1+sizeof(int)];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;
	memcpy(buf, &opcode, 1);
	memcpy(buf+1, &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1)
		return -1;
	/* Request sent */

	int ret;
	if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;

    return destroy_session();
}

int tfs_open(char const *name, int flags) {
	signal(SIGPIPE, handler);
	ssize_t written = -1;
	/* Send request to server */
	char buf[1+sizeof(int)+FILE_NAME_LENGTH+sizeof(int)];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;

    char opcode = TFS_OP_CODE_OPEN;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

    size_t size = strlen(name);
    memcpy(buf+1+sizeof(int), name, size);
	memset(buf+1+sizeof(int)+size, '\0', FILE_NAME_LENGTH-size);
	memcpy(buf+1+sizeof(int)+FILE_NAME_LENGTH, &flags, sizeof(int));
	written = write(fserv, buf, sizeof(buf));
    if(written == -1)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int fhandle;
    if(read(fcli, &fhandle, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return fhandle;
}

int tfs_close(int fhandle) {
	signal(SIGPIPE, handler);
	ssize_t written = -1;
	/* Send request to server */
	char buf[1+2*sizeof(int)];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;
    char opcode = TFS_OP_CODE_CLOSE;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));
	written = write(fserv, buf, sizeof(buf));
    if(written == -1)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
	signal(SIGPIPE, handler);
	/* Send request to server */
	char buf[1+2*sizeof(int)+sizeof(size_t)+len];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;

    char opcode = TFS_OP_CODE_WRITE;
    memcpy(buf, &opcode, 1);
	
	memcpy(buf+1, &session_id, sizeof(int));
    
	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));

	memcpy(buf+1+2*sizeof(int), &len, sizeof(size_t));

	memcpy(buf+1+2*sizeof(int)+sizeof(size_t), buffer, len);

	if(write(fserv, buf, sizeof(buf)) == -1)
		return -1;

	/* Request sent */

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

    return ret;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
	signal(SIGPIPE, handler);
	/* Send request to server */
	char buf[1+2*sizeof(int)+sizeof(size_t)];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;

    char opcode = TFS_OP_CODE_READ;
	memcpy(buf, &opcode, 1);
	
	memcpy(buf+1, &session_id, sizeof(int));
	
	memcpy(buf+1+sizeof(int), &fhandle, sizeof(int));
	
	memcpy(buf+1+2*sizeof(int), &len, sizeof(size_t));

    if(write(fserv, buf, sizeof(buf)) == -1)
		return -1;

	/* Request sent */

	/* Receive answer from server */
	int ret;
	if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;

    if(read(fcli, buffer, (size_t)ret) <= 0) // can we do this too?
		return -1;
	/* Answer received */

    return ret;
}

int tfs_shutdown_after_all_closed() {
	signal(SIGPIPE, handler);
	/* Send request to server */
    char buf[1+sizeof(int)];
	if(sizeof(buf) >= PIPE_BUF)
		return -1;
	char opcode = TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED;
	memcpy(buf, &opcode, 1);

	memcpy(buf+1, &session_id, sizeof(int));

    if(write(fserv, buf, sizeof(buf)) == -1)
		return -1;

	/* Request sent */
	if (close(fserv) != 0) return -1;

	/* Receive answer from server */
    int ret;
    if(read(fcli, &ret, sizeof(int)) <= 0)
		return -1;
	/* Answer received */

	if (close(fcli) != 0) return -1;

    session_id = -1; // Reset the session_id
	if (unlink(c_pipe_path) == -1)
		return -1;

    return ret;
}
