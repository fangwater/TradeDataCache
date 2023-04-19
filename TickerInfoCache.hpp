#ifndef _TICKINFOCACHE_HPP
#define _TICKINFOCACHE_HPP
#include <bits/stdc++.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/time/civil_time.h>
#include <absl/time/time.h>
#include <fmt/format.h>
#include <folly/MPMCPipeline.h>
#include <absl/container/flat_hash_map.h>
#include <folly/ProducerConsumerQueue.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <xsimd/xsimd.hpp>
#include <limits>
#include "tools.hpp"
#include "TickerInfoCache.hpp"
#include "security_buffer.hpp"
class TickerInfoCache;
class MessageParser{
protected:
    std::shared_ptr<TickerInfoCache> TickerInfoCache_sp;
public:
    MessageParser(std::shared_ptr<TickerInfoCache> cache_sp){
        TickerInfoCache_sp = cache_sp;
    }
    virtual void MessageProcess(std::string_view line) = 0;
};

class SZTickinfoParser;
class SHTickinfoParser;

class TickerInfoCache : public std::enable_shared_from_this<TickerInfoCache>{
public:
    std::unique_ptr< folly::MPMCPipeline<std::shared_ptr<TickerInfo>> > TickerInfoQueue_ptr;
    std::shared_ptr<SecurityBufferMap> security_buffer_map_sp;
    absl::Time today_start;
    std::vector<std::jthread> buffer_submitter;
public:
    TickerInfoCache(std::size_t queue_size, absl::CivilDay today){
        TickerInfoQueue_ptr = std::make_unique<folly::MPMCPipeline<std::shared_ptr<TickerInfo>>>(queue_size);
        today_start = absl::FromCivil(today,sh_tz.tz);
    }
    //put TickerInfo to MPSC queue after message parser
    void putTickerInfo(std::shared_ptr<TickerInfo> ticker_info_ptr) {
        if( !TickerInfoQueue_ptr->write(ticker_info_ptr) ){
            LOG(FATAL) << "TickerInfoQueue size not enough";
        }
        return;
    }

    //load TickerInfo from MPSC queue
    std::shared_ptr<TickerInfo> loadTickerInfo() {
        //此时队列可能为空
        std::shared_ptr<TickerInfo> ticker_info_ptr;
        if( !TickerInfoQueue_ptr->read(ticker_info_ptr) ){
            return nullptr; 
        }
        return ticker_info_ptr;
    }
    std::shared_ptr<MessageParser> MessagePaser_sp(std::string parser_type);
    //std::make_shared<SecurityBufferMap>(securitys,1024*8)
    std::shared_ptr<SecurityBufferMap> SecurityBufferMap_sp(std::vector<std::string>& security_id_strs, std::size_t buffer_size);
    
    void submitToBuffer(std::stop_token stoken){
        while(!stoken.stop_requested()){
            std::shared_ptr<TickerInfo> ticker_info_ptr = loadTickerInfo();
            if(ticker_info_ptr != nullptr){
                //success
                if(security_buffer_map_sp->insert(ticker_info_ptr)){
                    // if(ticker_info_ptr->SecurityID == 301314){
                    //     LOG(INFO) << ticker_info_ptr->to_string();
                    // }
                    //DLOG(INFO) << fmt::format("Insert: {}",ticker_info_ptr->SecurityID);
                }else{
                    //DLOG(WARNING) << fmt::format("Insert: {} failed",ticker_info_ptr->SecurityID);
                }
                //for test, pending here
                // DLOG(INFO) << std::this_thread::get_id() << " insert success, pending here"; 
            }else{
                std::this_thread::yield();
            }
        }
    }
    void startSubmitToBufferThreads(int num_threads){
        auto self = shared_from_this();
        for(int i = 0; i < num_threads; i++){
            buffer_submitter.emplace_back(
                [self](std::stop_token stoken){
                    self->submitToBuffer(stoken);
                }
            );
        }
        DLOG(INFO) << "success create threadpool";
    }

    ~TickerInfoCache(){
        for (auto& thread : buffer_submitter) {
            thread.request_stop();
        }
    }

};
std::shared_ptr<SecurityBufferMap> TickerInfoCache::SecurityBufferMap_sp(std::vector<std::string>& security_id_strs, std::size_t buffer_size){
    security_buffer_map_sp = std::make_shared<SecurityBufferMap>(security_id_strs, buffer_size);
    return security_buffer_map_sp; 
}



class SHTickinfoParser : public MessageParser{
public:
    using MessageParser::MessageParser;
    void MessageProcess(std::string_view line){
        std::vector<std::string_view> x = absl::StrSplit(line,",");
        if(x[10] != "N"){
            std::shared_ptr<TickerInfo> ticker_info_sp = std::make_shared<TickerInfo>();
            CHECK_RETURN_VALUE(absl::SimpleAtoi(x[3], &ticker_info_sp->SecurityID),"Failed to convert SecurityID");
            CHECK_RETURN_VALUE(absl::SimpleAtod(x[5],&ticker_info_sp->TradPrice),"Failed to convert TradPrice");
            CHECK_RETURN_VALUE(absl::SimpleAtod(x[6], &ticker_info_sp->TradVolume),"Failed to convert TradVolume");
            absl::Duration bias = convert_time_string_to_duration(x[4]);
            ticker_info_sp->TradTime = TickerInfoCache_sp->today_start + bias;
            ticker_info_sp->B_or_S = (x[10] == "B") ? 1 : -1;
            TickerInfoCache_sp->putTickerInfo(ticker_info_sp);
        }
        return;
    }
};

class SZTickinfoParser : public MessageParser{
public:
    using MessageParser::MessageParser;
    //深交所逐笔成交
    //2032,23622,011,23616,0,123120,102 ,0.0000,10,52,09:17:43.840,09:17:43.834,33193,
    void MessageProcess(std::string_view line){
        std::vector<std::string_view> x = absl::StrSplit(line,",");
        std::shared_ptr<TickerInfo> ticker_info_sp = std::make_shared<TickerInfo>();
        int ExecType;
        CHECK_RETURN_VALUE(absl::SimpleAtoi(x[9], &ExecType),"Failed to convert ExecType");
        if(ExecType == 70){
            absl::Duration bias = convert_time_string_to_duration(x[11]);
            std::shared_ptr<TickerInfo> ticker_info_sp = std::make_shared<TickerInfo>();
            CHECK_RETURN_VALUE(absl::SimpleAtoi(x[5], &ticker_info_sp->SecurityID),"Failed to convert SecurityID");
            CHECK_RETURN_VALUE(absl::SimpleAtod(x[7],&ticker_info_sp->TradPrice),"Failed to convert TradPrice");
            CHECK_RETURN_VALUE(absl::SimpleAtod(x[8], &ticker_info_sp->TradVolume),"Failed to convert TradVolume");
            ticker_info_sp->TradTime = TickerInfoCache_sp->today_start + bias;
            int64_t BidApplSeqNum;
            int64_t OfferApplSeqNum;
            CHECK_RETURN_VALUE(absl::SimpleAtoi(x[3],&BidApplSeqNum),"Failed to convert BidApplSeqNum");
            CHECK_RETURN_VALUE(absl::SimpleAtoi(x[4],&OfferApplSeqNum),"Failed to convert OfferApplSeqNum");
            ticker_info_sp->B_or_S = (BidApplSeqNum > OfferApplSeqNum) ? 1 : -1;
            TickerInfoCache_sp->putTickerInfo(ticker_info_sp);
        }
        return;
    }
};

std::shared_ptr<MessageParser> TickerInfoCache::MessagePaser_sp(std::string parser_type){
    if(parser_type == "SHTickinfo"){
        std::shared_ptr<SHTickinfoParser> parser_sp = std::make_shared<SHTickinfoParser>(shared_from_this());
        return std::dynamic_pointer_cast<MessageParser>(parser_sp);
    }else if(parser_type == "SZTickinfo"){
        std::shared_ptr<SZTickinfoParser> parser_sp = std::make_shared<SZTickinfoParser>(shared_from_this());
        return std::dynamic_pointer_cast<MessageParser>(parser_sp);
    }else{
        std::cerr << "Unknown parser_type" << std::endl;
        exit(0);
    }
};

#endif