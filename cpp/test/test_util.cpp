#include "mental1104/util.h"
#include <iostream>
#include <map>

int add(int a, int b) {
    return a + b;
}

void complicated_operation(){
    std::map<int, int> temp;
    for (int i = 0; i < 100000; i++){
        temp[i] = i;
    }
}


int main() {
    mental1104::make_timed(complicated_operation, "complicated_operation")();
    auto temp = mental1104::make_timed(add, "add")(1, 2);
}
