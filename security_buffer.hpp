
#ifndef _SECURITYBUFFER_HPP
#define _SECURITYBUFFER_HPP
#include "tools.hpp"
#include <absl/container/flat_hash_map.h>
struct SecurityBuffer{
    mutable std::mutex mtx;
    std::unique_ptr<folly::ProducerConsumerQueue<std::shared_ptr<TickerInfo>>> buffer_ptr;
    SecurityBuffer(std::size_t queue_size){
        buffer_ptr = std::make_unique< folly::ProducerConsumerQueue<std::shared_ptr<TickerInfo>> >( queue_size );
    }
    SecurityBuffer(SecurityBuffer&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mtx);
        buffer_ptr = std::move(other.buffer_ptr);
    }

    //写可能并发,需要加锁
    bool appendTickerInfoSp(std::shared_ptr<TickerInfo> tick_info_sp){
        std::lock_guard<std::mutex> lock(mtx);
        if( !buffer_ptr->write(tick_info_sp) ){
            LOG(FATAL) << "Queue is out of size";
        }
        return true;
    };

    //读不会有多个线程,无需加锁
    std::shared_ptr<std::vector<std::shared_ptr<TickerInfo>>> flushByTimeThreshold(absl::Time time_threshold){
        std::shared_ptr<std::vector<std::shared_ptr<TickerInfo>>> flushed_tick_info_sp = std::make_shared<std::vector<std::shared_ptr<TickerInfo>>>();
        std::shared_ptr<TickerInfo> ticker_info_sp_before = nullptr;
        while(!buffer_ptr->isEmpty()){
            //getting all tick_info_ptr of this SecurityID
            if( ( *(buffer_ptr->frontPtr()) )->TradTime <= time_threshold){
            // only flush the ticker_info not in the same min of lastest ticker_info_sp
                buffer_ptr->read(ticker_info_sp_before);
                flushed_tick_info_sp->push_back(ticker_info_sp_before);
            }else{
                //ok, all tick_info in buffer before ticker_info_sp->TradTime has been flushed;
                break;
            }
        } 
        return flushed_tick_info_sp;
    }
};

class SecurityBufferMap{
public:
    absl::flat_hash_map<uint32_t, SecurityBuffer> SecurityBuffers;
public:
    SecurityBufferMap(std::vector<std::string>& security_id_strs, std::size_t buffer_size){
        SecurityBuffers.reserve(security_id_strs.size());
        for(auto& security_id_str : security_id_strs){
            uint32_t securityid;
            auto pos = security_id_str.find('.');
            if (pos != std::string_view::npos) {
                CHECK_RETURN_VALUE(absl::SimpleAtoi(security_id_str.substr(0, pos),&securityid),"Failed to convert security_id_str to security_id");
            }
            SecurityBuffers.insert( {securityid, SecurityBuffer(buffer_size)} );
        }
    }
    //appendTickerInfoSp
    bool insert(std::shared_ptr<TickerInfo> tick_sp){
        auto iter = SecurityBuffers.find(tick_sp->SecurityID);
        if(iter == SecurityBuffers.end()){
            LOG(INFO) << tick_sp->SecurityID;
            LOG(WARNING) << "this security_id is not included in the SecurityBufferMap";
            return false;
        }else{
            iter->second.appendTickerInfoSp(tick_sp);
            return true;
        }
    }
    //flushByTimeThreshold
};

#endif