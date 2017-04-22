#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <string.h>
#include <stdlib.h>



int main() {
	int fd = open("random.txt", 577, 384);
	if (fd == -1) {
		printf("Cant open file");
		return -1;
	}
	for (size_t i = 0; i < 10000; ++i) {
		std::vector<char> str;
		str.clear();
		size_t length = rand() % 4 + 1;
		for (size_t j = 0; j < length; ++j) {
			int d = rand() % 10000 + 1;
			printf("\nd = %d\n", d);
			char number[7];
			snprintf(number, 7," %d", d);
			printf("number: %s", number);
			str.insert(str.end(),
					   number,
					   number + strlen(number)
			);
		}
		str.insert(str.end(), '\0');
		str.erase(str.begin());
		size_t len = str.size();
		printf("\nlength = %lu", len);
		printf("\nvector: %s", &str[0]);  
		if (write(fd, &len, sizeof(len)) != sizeof(len)) {
			printf("\nCant write length\n");
			return -1;
		}
		if (write(fd, &str[0], len) != len) {
			printf("\nCant write str\n");
			return -1;
		}
	}
	return 0;
}
