#include "kv.hpp"
#include <chrono>
#include <iostream>
#include <thread>
using namespace kv; using clk = std::chrono::steady_clock;
int main(int argc, char** argv) {
    int threads = argc>1 ? atoi(argv[1]) : 4, per = 20000;
    std::filesystem::remove_all("/tmp/kvbench");
    Store s("/tmp/kvbench", 8);
    auto run = [&](const char* name, auto fn) {
        auto t0 = clk::now(); std::vector<std::thread> ts;
        for (int t=0;t<threads;++t) ts.emplace_back([&,t]{ for(int i=0;i<per;++i) fn(t,i); });
        for (auto& t: ts) t.join();
        double sec = std::chrono::duration<double>(clk::now()-t0).count();
        std::cout << name << ": " << (long)(threads*per/sec) << " ops/sec (" << threads << " threads)\n";
    };
    run("PUT", [&](int t,int i){ s.put("key"+std::to_string(t)+"_"+std::to_string(i), "value-payload-0123456789"); });
    run("GET", [&](int t,int i){ s.get("key"+std::to_string(t)+"_"+std::to_string(i)); });
}
