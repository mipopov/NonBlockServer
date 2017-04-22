#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


int main() {
	int fd = open("random.txt", O_RDONLY);
	if (fd == -1)
		return -1;
	size_t len;
	for (;;) {
		char *buf = new char;
		if (read(fd, &len, sizeof(len)) != sizeof(len))
			return -1;
		if (read(fd, buf, len) != len)
			return -1;
		puts(buf);
		delete buf;
	}
	return 0;
}
