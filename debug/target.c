#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>


int add_nums(int x, int y) {

	int c;
	c = x + y;
	return c;
}


int main() {

	int svetas_x = 4;
	int svetas_y = 16;
	int total;

	while (1) {

		total = add_nums(svetas_x, svetas_y);
		printf("This is tid: %d, inject check: %d\n", gettid(), total);
		sleep(2);
	}

	return 0;
}


int thread_work() {

    while (1) {
        printf("This is tid: %d", gettid());
        sleep(2);
    }
}
