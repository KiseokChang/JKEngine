#include <stdio.h>
#include <stdlib.h>


int16 sum(int16 a,int16 b)
{
	return a+b;
}

void main()
{
	int16 a=5,b=7;
	printf("%d\n",sum(a,b));
}