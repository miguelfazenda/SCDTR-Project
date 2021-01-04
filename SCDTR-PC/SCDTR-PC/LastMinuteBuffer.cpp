#include "LastMinuteBuffer.h"

#include <iostream>

using namespace std;

void LastMinuteBuffer::addToLastMinuteBuffer(uint8_t nodeId, uint8_t pwm, float iluminance)
{
    removeOldLastMinuteBufferEntries();
    time_t timeNow = time(0);

    //Note: buffer[nodeId] automatically inserts a new entry if key "nodeId" doesnt exist yet
    mtx.lock();
    lastMinuteBuffer[nodeId].push_back(make_pair((unsigned long)timeNow, make_pair(pwm, iluminance)));
    mtx.unlock();
}

/**
 * Removes all the items from the last minute buffet where the time is older than 1 minute ago
 */
void LastMinuteBuffer::removeOldLastMinuteBufferEntries()
{
    mtx.lock();
    unsigned long timeThreshold = (unsigned long)time(0) - 60;

    for (auto nodeBufferKeyValue : lastMinuteBuffer)
    {
        //For each pair nodeId,Buffer
        auto nodeBuffer = nodeBufferKeyValue.second;

        //Find where an entry's time is older than 1 minute ago and erase
        auto item = begin(nodeBuffer);
        while (item != end(nodeBuffer))
        {
            if (item->first < timeThreshold)
            {
                item = nodeBuffer.erase(item);
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
        //Has no data for such nodeId
        return;
    }

    auto nodeBuffer = lastMinuteBuffer[nodeId];

    //Get last element pointer
    auto lastElement = nodeBuffer.size() > 0 ? &nodeBuffer.back() : nullptr;

    for (auto entry = nodeBuffer.begin(); entry != nodeBuffer.end(); ++entry)
    {
        if (pwm)
        {
            *textOutputStream << entry->second.first * 100 / 255 << "%";
        }
        else
        {
            *textOutputStream << entry->second.second;
        }


        //If it's the last element dont print the comma ','
        if (&(*entry) == lastElement)
            *textOutputStream << endl;
        else
            *textOutputStream << "," << endl;
    }
    mtx.unlock();
}