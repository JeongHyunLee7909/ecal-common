#pragma once

#include <map>
#include <queue>
#include <mutex>

#include <ecal_camera/CameraInterface.hpp>

#include <ecal/ecal.h>
#include <ecal/msg/capnproto/helper.h>
#include <ecal/msg/capnproto/subscriber.h>
#include <ecal/msg/capnproto/publisher.h>


#include <capnp/image.capnp.h>
#include <capnp/imu.capnp.h>

#include <capnp/cameracontrol.capnp.h>

namespace vk 
{

template <typename T>
class MessageSynchroniserExact {

  public:
    void init(size_t N, std::vector<std::string> &names = {}) {
        m_N = N;
        if (names.size()) {
            assert(names.size() == N);
            m_names = names;
        }
            
        m_queueMap.resize(m_N);
        m_lastTsMap.resize(m_N);
        m_lastSeqMap.resize(m_N);
    }

    void addMessage(size_t idx, std::uint64_t ts, std::uint64_t seq, T msg) {
        if (m_lastTsMap[idx] > 0) {
            if (m_lastTsMap[idx] > ts)
            std::cout << "warn: ts regression detected, from " << m_lastTsMap[idx] << " to " << ts << std::endl;
        }
        // else{
        //     std::cout << "first message received at synchroniser for " << m_names.size() ? m_names[idx] : idx << std::endl;
        // }

        {
            std::lock_guard<std::mutex> lock(m_mutexQueue);
            m_queueMap[idx].push(std::make_pair(ts, msg));
        }

        if (m_queueMap[idx].size() > 50) {
            if (m_names.size())
                std::cout << m_names[idx] << std::endl;
            throw std::runtime_error("too much message in the queue, sync msg is broken?");
        }

        m_lastTsMap[idx] = ts;
        m_lastSeqMap[idx] = seq;
    }

    std::vector<T> tryGet() {

        std::lock_guard<std::mutex> lock(m_mutexQueue);

        std::vector<T> ret;

        for (auto& queue : m_queueMap) {
            if (!queue.size())
                return ret; // some queues are empty
        }

        std::uint64_t minTs, maxTs;
        minTs = maxTs = m_queueMap[0].front().first;

        // calc min and max ts
        for (size_t i = 1; i < m_N; i++) {
            auto ts = m_queueMap[i].front().first;
            if (minTs > ts)
                minTs = ts;
            if (maxTs < ts)
                maxTs = ts;
        }

        if (minTs != maxTs) {
            // clean up all items smaller than maxTs
            for (size_t i = 0; i < m_N; i++) {
                auto ts = m_queueMap[i].front().first;
                if (ts < maxTs) {
                    m_queueMap[i].pop();
                    i--;
                    continue;
                }
            }
            return ret;

        }else{
            // sync found, returning and popping
            ret.resize(m_N);
            for (size_t i = 0; i < m_N; i++) {
                ret[i] = m_queueMap[i].front().second;
                m_queueMap[i].pop();
            }
            return ret;
        }
    }

  private:

    size_t m_N;
    std::vector<std::string> m_names;
    std::vector<std::queue<std::pair<std::uint64_t,T>>> m_queueMap;
    std::mutex m_mutexQueue;
    std::vector<std::uint64_t> m_lastTsMap;
    std::vector<std::uint64_t> m_lastSeqMap;

};

class CameraInternal : public CameraInterface {

  public:
    void init(const CameraParams &params);

    void registerSyncedCameraCallback(callbackCamera callback);
    void registerImuCallback(callbackImu callback);

    void sendCameraControl(const CameraControlData& data);
    
    void sendJsonIn(const std::string& topic, const std::string& content);

    ~CameraInternal();

  private:
    void run();

    void cameraCallbackInternal(const char* topic_name, ecal::Image::Reader msg, const long long ts, size_t idx);
    void imuCallbackInternal(const char* topic_name, ecal::Imu::Reader msg, const long long ts);

    std::map<std::string, eCAL::capnproto::CSubscriber<ecal::Image>> m_imageSubMap;
    std::map<std::string, eCAL::capnproto::CSubscriber<ecal::Imu>> m_imuSubMap;
    std::vector<std::string> m_idxMap;

    // regarding lastSeq
    std::map<size_t, std::uint64_t> m_lastSeqCameraFrameMap;
    std::uint64_t m_lastSeqImuFrameMap = 0;

    // regarding calibration
    std::map<size_t, CameraCalibration> m_cameraCalibrationMap;
    ImuCalibration m_imuCalibration;

    std::vector<callbackCamera> m_registeredImageCallbacks;
    std::vector<callbackImu> m_registeredImuCallbacks;

    MessageSynchroniserExact<CameraFrameData::Ptr> m_messageSyncHandler;
    ImuFrameData::Ptr m_imuMessage;

    // camera control
    std::shared_ptr<eCAL::capnproto::CPublisher<ecal::CameraControl>> m_cameraControlPub;

    bool m_ecalInitialisedOutside;

};

} // namespace vk

