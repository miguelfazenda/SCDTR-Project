#include "LastMinuteBuffer.h"

#include <iostream>

using namespace std;

void LastMinuteBuffer::addToLastMinuteBuffer(uint8_t nodeId, FrequentDataValues frequentDataValues)
{
    removeOldLastMinuteBufferEntries();
    time_t timeNow = time(0);

    //Note: buffer[nodeId] automatically inserts a new entry if key "nodeId" doesnt exist yet
    mtx.lock();
    //std::map<unsigned long, std::pair<uint8_t, float>> a = lastMinuteBuffer[nodeId];

    lastMinuteBuffer[nodeId][timeNow].push_back(frequentDataValues);
    mtx.unlock();
}

/**
 * Removes all the items from the last minute buffet where the time is older than 1 minute ago
 */
void LastMinuteBuffer::removeOldLastMinuteBufferEntries()
{
    mtx.lock();

    time_t timeNow = time(0);

    time_t timeThreshold = timeNow - 60;

    for (auto nodeBufferKeyValue : lastMinuteBuffer)
    {
        //For each pair nodeId,Buffer
        auto nodeBuffer = &nodeBufferKeyValue.second;

        //Find where an entry's time is older than 1 minute ago and erase
        auto item = begin(*nodeBuffer);
        while (item != end(*nodeBuffer))
        {
            if (item->first < timeThreshold)
            {
                item = nodeBuffer->erase(item);
            }
            else
            {
                //The first element to be found is the oldest, therefore no other element needs to be deleted
                break;
            }
        }
    }
    mtx.unlock();
}

/**
 * Prints a last minute buffer's contents
 * @param pwm : true: print PWM, false: print Iluminance
 * @param textOutputStream: where to print to. Can be &std::cout or a buffer to send to a client for ex.
 */
void LastMinuteBuffer::printLastMinuteBuffer(const uint8_t nodeId, const bool pwm, std::ostream* textOutputStream)
{
    //Prints header
    cout << "b " << (pwm ? 'd' : 'I') << ' ' << (int)nodeId << endl;


    removeOldLastMinuteBufferEntries();

    mtx.lock();
    if (lastMinuteBuffer.find(nodeId) == lastMinuteBuffer.end())
    {
        mtx.unlock();
        //Has no data for such nodeId
        return;
    }

    auto nodeBuffer = lastMinuteBuffer[nodeId];

    //Get last element pointer
    //auto lastElement = nodeBuffer.size() > 0 ? &nodeBuffer.back() : nullptr;

    for (auto bufForATime = nodeBuffer.begin(); bufForATime != nodeBuffer.end(); ++bufForATime)
    {
        //Each loop represents a different time
        for (auto entry = bufForATime->second.begin(); entry != bufForATime->second.end(); ++entry)
        {
            if (pwm)
            {
                *textOutputStream << entry->pwm * 100 / 255 << "%";
            }
            else
            {
                *textOutputStream << entry->iluminance;
            }


            //If it's the last element dont print the comma ','
            /*if (&(*entry) == lastElement)
                *textOutputStream << endl;
            else*/
            *textOutputStream << "," << endl;
        }
    }
    mtx.unlock();
}