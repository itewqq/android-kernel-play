#include <unistd.h>
#include <sys/syscall.h>

#include <stdio.h>

#define __NR_justatest 600

int main(){
	int a,b,c;
	a=1337, b=42;
	c=syscall(__NR_justatest, a, b);
	printf("Result from the kernel: %d * %d = %d\n", a, b, c);
	return 0;
}
