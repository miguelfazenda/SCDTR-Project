#ifndef _LAST_MINUTE_BUFFER_H
#define _LAST_MINUTE_BUFFER_H

#include <sstream>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

struct FrequentDataValues
{
    uint8_t pwm;
    float iluminance;
};

class LastMinuteBuffer
{
public:
    void addToLastMinuteBuffer(uint8_t nodeId, FrequentDataValues frequentDataValues);
    void removeOldLastMinuteBufferEntries();
    void printLastMinuteBuffer(const uint8_t nodeId, const bool pwm, std::ostream& textOutputStream);

    //Map: nodeId, vector
    //Vector: buffer containing pairs of <time, reading>
    //Reading: pair containing pwm(uint8_t) and iluminance(float)
    //std::map<uint8_t, std::vector<std::pair<unsigned long, std::pair<uint8_t, float>>>> lastMinuteBuffer;

    time_t lastTimeAddedEntry;

    std::map<uint8_t, std::map<time_t, std::vector<FrequentDataValues>>> lastMinuteBuffer;
private:
    std::mutex mtx;
};

#endif