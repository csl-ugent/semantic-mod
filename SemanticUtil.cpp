#include "SemanticUtil.h"
#include <stdlib.h> // rand
#include <time.h> // time

int random_0_to_n(int n) {

    /* initialize random seed */
    srand (time(NULL));

    // return a number between 0 and n-1.
    return rand() % n;
}

int factorial(int n)
{
  return (n == 1 || n == 0) ? 1 : factorial(n - 1) * n;
}
