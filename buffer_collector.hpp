#ifndef _BUFFER_COLLECTOR_HPP
#define _BUFFER_COLLECTOR_HPP
#include "tools.hpp"
#include "security_buffer.hpp"
#include "process_1min_ticker.hpp"
class SecurityBufferMapCollect{
public:
    mutable std::mutex mtx;
    std::shared_ptr<SecurityBufferMap> security_buffer_map_sp;
    std::vector<uint32_t> security_ids;

    mutable std::mutex mtx_for_collection_sig;
    mutable std::mutex mtx_for_last_update_time;
    absl::Time last_update_time;
    
public:
    SecurityBufferMapCollect() = delete;
    SecurityBufferMapCollect(std::shared_ptr<SecurityBufferMap> buffer_map_sp, std::vector<std::string>& ids, absl::Time start_tp)
        : security_buffer_map_sp(buffer_map_sp), last_update_time(start_tp){
            for(auto& security_id_str : ids){
                uint32_t securityid;
                auto pos = security_id_str.find('.');
                if (pos != std::string_view::npos) {
                    CHECK_RETURN_VALUE(absl::SimpleAtoi(security_id_str.substr(0, pos),&securityid),"Failed to convert security_id_str to security_id");
                }
                security_ids.emplace_back(securityid);
            }
        }
    void change_last_update_time(absl::Time time){
        std::lock_guard<std::mutex> lock(mtx_for_last_update_time);
        // DLOG(INFO) << "update_last_update_time";
        last_update_time = ( last_update_time < time ) ? time : last_update_time;
    }
    void updateTimeForTest(absl::Time time){
        std::unique_lock<std::mutex> all_mtx(mtx);
        if( is_next_civil_min(last_update_time, time) ){
            //涉及驱动信号，加锁
            change_last_update_time(time);
            all_mtx.unlock();
            LOG(INFO) << "Start to process 1 min";
            std::unique_lock<std::mutex> sig_mtx(mtx_for_collection_sig);
            std::this_thread::sleep_for(std::chrono::seconds(3));
            //修正为整分钟
            absl::CivilMinute truncated_civil = absl::ToCivilMinute(time,sh_tz.tz);
            absl::Time truncated_time = absl::FromCivil(truncated_civil,sh_tz.tz);
            if(sigHandler(truncated_time)){
                LOG(INFO) << "Success process 1 min";
            }
            else{
                LOG(FATAL) << "Failed to retrieval this min trade data";
            }
            sig_mtx.unlock();
        }else{
            //不涉及驱动信号，解锁
            all_mtx.unlock();
            change_last_update_time(time);  
        }
    };
    std::shared_ptr<std::vector<std::shared_ptr<TickerInfo>>> collectBySecurityidTickerinfo(uint32_t security_id, absl::Time time){
        auto iter = security_buffer_map_sp->SecurityBuffers.find(security_id);
        if( iter != security_buffer_map_sp->SecurityBuffers.end() ){
            auto ticker_1min_buffer_sp = iter->second.flushByTimeThreshold(time);
            return ticker_1min_buffer_sp;
        }else{
            LOG(WARNING) << fmt::format(" {} is untracked security_id ",security_id);
            return nullptr;
        }
    };
    bool sigHandler(absl::Time time){
        //parallel 
        time += absl::Minutes(1);
        for(int i = 0; i < security_ids.size(); i++){
            auto ticker_1min_buffer_sp = collectBySecurityidTickerinfo( security_ids[i], time);
            if( ticker_1min_buffer_sp->size() ){
                if(security_ids[i] == 301314){
                    auto [feature_3,feature_10] =  process_1min_ticker( ticker_1min_buffer_sp );
                    //LOG(INFO) <<  "ticker_1min_buffer_sp is nullptr"; 
                    LOG(INFO) << fmt::format("{}:",301314);
                    LOG(INFO) << fmt::format("cjbs: {}, bcjbs: {}, scjbs:{}",feature_3.at(0),feature_3.at(1),feature_3.at(2));
                    LOG(INFO) << fmt::format("volume_nfq: {:.2f}, bvolume_nfq:{:.2f}, svolume_nfq:{:.2f}",feature_10.at(0),feature_10.at(1),feature_10.at(2));
                    LOG(INFO) << fmt::format("amount: {:.2f}, bamount:{:.2f}, samount:{:.2f}",feature_10.at(3),feature_10.at(4),feature_10.at(5));
                    LOG(INFO) << fmt::format("closeprice_nfq: {:.2f}, openprice_nfq:{:.2f}, highprice_nfq:{:.2f}, lowprice_nfq:{:.2f}",feature_10.at(9),feature_10.at(8),feature_10.at(7),feature_10.at(6));
                }
            }else{
                //LOG(INFO) <<  "ticker_1min_buffer_sp is empty"; 
            }
        }
        return true;
    };
};
#endif