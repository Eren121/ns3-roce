#include "rdma-random.h"
#include <random>

namespace ns3 {

int GenRandomInt(int min, int max)
{
    std::random_device rd;
    static std::mt19937 gen(rd());
    
    return std::uniform_int_distribution<int>(min, max)(gen);
}

int GenRandomInt(int max)
{
    return GenRandomInt(0, max);
}


double GenRandomDouble(double min, double max)
{
    std::random_device rd;
    static std::mt19937 gen(rd());
    
    return std::uniform_real_distribution<double>(min, max)(gen);
}

}