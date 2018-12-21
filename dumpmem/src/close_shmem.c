#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static const char *g_LogFileName = "newcomair_123456789";

int main() {

	int fd = shm_open(g_LogFileName, O_RDWR, 07777);
	if (fd == -1) {
		fprintf(stderr, "shm_open failed: %s\n", strerror(errno));
        exit(-1);
	}

	shm_unlink(g_LogFileName);
	close(fd);
}
