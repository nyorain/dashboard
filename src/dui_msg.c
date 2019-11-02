#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, const char** argv) {
	const char* fifo_path = "/tmp/.dui-pipe";
	int fd = open(fifo_path, O_WRONLY);
	if(fd < 0) {
		printf("open on fifo failed: %s (%d)\n", strerror(errno), errno);
		return EXIT_FAILURE;
	}

	struct stat st;
	if(fstat(fd, &st) || !S_ISFIFO(st.st_mode)) {
		printf("%s isn't fifo\n", fifo_path);
		return EXIT_FAILURE;
	}

	// TODO: might interleave with other commands.
	// probably a fifo isn't the best for this
	for(int i = 1; i < argc; ++i) {
		if(i != 1) {
			write(fd, " ", 1);
		}
		write(fd, argv[i], strlen(argv[i]));
	}

	write(fd, "\n", 1);
	close(fd);
}
