// CPU-only maintenance microbenchmark. Cache construction and touches are untimed.
#include "assets/idle_resource_cache.h"
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>

struct Resource {size_t id;};
int main() {
    using Clock=std::chrono::steady_clock;
    std::cout << "entries,victims,median_us,p95_us,checksum\n";
    for(const size_t n:{size_t(512),size_t(4096),size_t(32768)}) {
        std::vector<size_t> order(n);std::iota(order.begin(),order.end(),0);
        std::mt19937 random(3169);std::shuffle(order.begin(),order.end(),random);
        for(const size_t limit:{size_t(0),size_t(8),n}) {
            std::vector<double> times;uint64_t checksum=0;
            for(size_t trial=0;trial<103;++trial) {
                cgl::IdleResourceCache<size_t,Resource> cache;
                for(size_t i=0;i<n;++i) cache.insert(i,std::make_shared<Resource>(i),4);
                for(auto i:order) cache.find(i);
                size_t count=0;bool correct=true;
                const auto start=Clock::now();
                const auto bytes=cache.retireKeyed(0,[]{},[&](auto key,const auto&,uint64_t) {
                    correct=correct && count<limit && key==order[count];
                    checksum=checksum*131+key;++count;
                },0,limit);
                const auto end=Clock::now();
                if(!correct || count!=limit || bytes!=4*limit || cache.size()!=n-limit) return 1;
                if(trial>=2) times.push_back(std::chrono::duration<double,std::micro>(end-start).count());
            }
            std::sort(times.begin(),times.end());
            std::cout << n << ',' << limit << ',' << times[times.size()/2] << ','
                      << times[(times.size()-1)*95/100] << ',' << checksum << '\n';
        }
    }
}
