#include "kv.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <fstream>
using namespace kv;
namespace fs = std::filesystem;
#define CHECK(c) do{ if(!(c)){ std::cerr<<"FAIL line "<<__LINE__<<": "#c"\n"; return 1; } }while(0)

int main() {
    fs::remove_all("/tmp/kvtest");
    {   Store s("/tmp/kvtest", 4);
        s.put("a","1"); s.put("b","2"); s.put("a","3");
        CHECK(*s.get("a")=="3"); CHECK(s.get("zz")==std::nullopt);
        CHECK(s.del("b")); CHECK(!s.del("b")); CHECK(s.size()==1);
        s.put("bin", std::string("x\ny z\0w",7));          // binary/whitespace-safe values
    }
    {   Store s("/tmp/kvtest", 4);                          // restart -> recovered from WAL
        CHECK(*s.get("a")=="3"); CHECK(s.get("b")==std::nullopt);
        CHECK(*s.get("bin")==std::string("x\ny z\0w",7));
        s.compact();
    }
    {   Store s("/tmp/kvtest", 4);                          // still correct after compaction
        CHECK(s.size()==2); CHECK(*s.get("a")=="3");
    }
    {   // crash simulation: truncated last record must be ignored, not corrupt recovery
        Store s("/tmp/kvtest", 1); s.put("k","v");
        std::ofstream("/tmp/kvtest/shard0.wal", std::ios::app) << "P 3 10\nabc12";
        Store r("/tmp/kvtest", 1); CHECK(*r.get("k")=="v"); CHECK(r.get("abc")==std::nullopt);
    }
    {   // distribution + concurrency
        fs::remove_all("/tmp/kvtest2");
        Store s("/tmp/kvtest2", 4);
        std::vector<std::thread> ts;
        for (int t=0;t<8;++t) ts.emplace_back([&,t]{ for(int i=0;i<500;++i) s.put("k"+std::to_string(t)+"_"+std::to_string(i),"v"); });
        for (auto& t: ts) t.join();
        CHECK(s.size()==4000);
        for (int i=0;i<4;++i) CHECK(s.shard_size(i) > 400);   // reasonably balanced
    }
    std::cout << "ALL TESTS PASSED\n";
}
