#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(){
    if(malloc(100000000)==NULL){
        exit(123);
    }

    return 0;
}