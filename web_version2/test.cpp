#include "server.h"
#include <thread>
#include <iostream>

void print(){
    printf("done\n");
}

using namespace std;
int main(){
    Time_heap timer(100);

    
    for(int i = 0; i < 10; ++i){
        thread([&](){
            int t = rand() % 500;
            cout << this_thread::get_id() << " sleep " 
            << t << "ms\n";
            usleep(t*1000);
            auto print = [](int t) ->void{
                printf("%d\n", t);
            };
            timer.push({Clock::now(), bind(print, t), 1, 1});
        }).detach();
    }
    

    usleep(500*1000);
    while(1){
        if(timer.empty()) break;
        timer.tick();
    }
    printf("tick!\n");
    sleep(5);
    return 0;
}