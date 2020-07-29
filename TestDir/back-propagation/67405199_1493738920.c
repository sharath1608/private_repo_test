#include <stdio.h>
#include <math.h>


double sigmoid(const double x) {
	return 1.0/(1.0 + exp(-x));
}

double sigprime(const double x) {
	return 1.0 - sigmoid(x);
}


main () {

double x=.75;
double y,z;
y=sigmoid(x);
z=sigprime(x);
printf("test x=%f y=%f z=%f\n",x,y,z);

return 0;
}
