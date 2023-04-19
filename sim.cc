#include <bits/stdc++.h>
#include <zmq.hpp>
#include "tools.hpp"
#include "TickerInfoCache.hpp"
#include "security_buffer.hpp"
#include "buffer_collector.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

void Tickerinfo_Zmq_Recvier(std::stop_token stoken, std::shared_ptr<MessageParser> parser_sp, std::string bind_to){
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    //172.16.30.12 13002 sz_trade
    subscriber.set(zmq::sockopt::rcvbuf, 1024 * 1024);
    subscriber.set(zmq::sockopt::sndhwm, 0); 
    subscriber.set(zmq::sockopt::subscribe,"");
    subscriber.connect(bind_to);
    // subscriber.set(zmq::sockopt::subscribe,"");
    while (true) {
        zmq::message_t message;
        auto res_state = subscriber.recv(message, zmq::recv_flags::none);
        if(!res_state){
            std::cerr << "Failed to rec messgae" << std::endl;
        }
        std::string message_str(static_cast<char*>(message.data()), message.size());
        parser_sp->MessageProcess(std::string_view(message_str));
        if(stoken.stop_requested()){
            LOG(INFO) << "cancel recvier";
            break;
        }
    }
}


void SH_Indexinfo_Zmq_Recvier(std::stop_token stoken, std::string bind_to, std::shared_ptr<SecurityBufferMapCollect> collector_sp){
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    //172.16.30.12 13002 sz_trade
    subscriber.set(zmq::sockopt::rcvbuf, 1024 * 1024);
    subscriber.set(zmq::sockopt::sndhwm, 0); 
    subscriber.set(zmq::sockopt::subscribe,"");
    subscriber.connect(bind_to);
    absl::CivilDay today(2023,4,6);
    absl::Time today_start = absl::FromCivil(today,sh_tz.tz);
    while (true) {
        zmq::message_t message;
        auto res_state = subscriber.recv(message, zmq::recv_flags::none);
        if(!res_state){
            std::cerr << "Failed to rec messgae" << std::endl;
        }
        std::string message_str(static_cast<char*>(message.data()), message.size());
        std::vector<std::string_view> x = absl::StrSplit(message_str,",");
        absl::Duration bias = convert_time_string_to_duration(x[4]);
        absl::Time t = today_start + bias;
        std::jthread async_update_time(&SecurityBufferMapCollect::updateTimeForTest,collector_sp,t);
        async_update_time.detach();
        if(stoken.stop_requested()){
            LOG(INFO) << "cancel recvier";
            break;
        }
    }
}


void SZ_Indexinfo_Zmq_Recvier(std::stop_token stoken, std::string bind_to, std::shared_ptr<SecurityBufferMapCollect> collector_sp){
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    //172.16.30.12 13002 sz_trade
    subscriber.set(zmq::sockopt::rcvbuf, 1024 * 1024);
    subscriber.set(zmq::sockopt::sndhwm, 0); 
    subscriber.set(zmq::sockopt::subscribe,"");
    subscriber.connect(bind_to);
    absl::CivilDay today(2023,4,6);
    absl::Time today_start = absl::FromCivil(today,sh_tz.tz);
    while (true) {
        zmq::message_t message;
        auto res_state = subscriber.recv(message, zmq::recv_flags::none);
        if(!res_state){
            std::cerr << "Failed to rec messgae" << std::endl;
        }
        std::string message_str(static_cast<char*>(message.data()), message.size());
        std::vector<std::string_view> x = absl::StrSplit(message_str,",");
        absl::Duration bias = convert_time_string_to_duration(x[11]);
        absl::Time t = today_start + bias;
        std::jthread async_update_time(&SecurityBufferMapCollect::updateTimeForTest,collector_sp,t);
        async_update_time.detach();
        if(stoken.stop_requested()){
            LOG(INFO) << "cancel recvier";
            break;
        }
    }
}



int main()
{
    std::string file_path = "/home/fanghz/project/workload_sim/recv.json";
    std::ifstream file_stream(file_path);
    if (!file_stream.is_open()) {
        std::cerr << "Error: Could not open the file." << std::endl;
        return 1;
    }
    std::string sz_bind_addr,sh_bind_addr;
    json config;
    try {
        file_stream >> config;
        std::string ip_sz = config["sz_ip"];
        int port_sz = config["sz_port"];
        std::string ip_sh = config["sh_ip"];
        int port_sh = config["sh_port"];
        sz_bind_addr = fmt::format("tcp://{}:{}",ip_sz,port_sz);
        sh_bind_addr = fmt::format("tcp://{}:{}",ip_sh,port_sh);
        std::cout << sz_bind_addr << std::endl;
        std::cout << sh_bind_addr << std::endl;
        std::cout << "Parsed JSON successfully." << std::endl;
    } catch (const json::parse_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    std::shared_ptr<TickerInfoCache> ticker_info_cache_sp = std::make_shared<TickerInfoCache>(10e6, absl::CivilDay(2023,4,6));
    auto sh_ticker_parser_sp = ticker_info_cache_sp->MessagePaser_sp("SHTickinfo");
    auto sz_ticker_parser_sp = ticker_info_cache_sp->MessagePaser_sp("SZTickinfo");
    std::jthread SH_rec_sim(Tickerinfo_Zmq_Recvier,sh_ticker_parser_sp,sh_bind_addr);
    std::jthread SZ_rec_sim(Tickerinfo_Zmq_Recvier,sz_ticker_parser_sp,sz_bind_addr);

    json SecurityGroup = json::parse(std::ifstream("../output.json"));
    std::vector<std::string> sz_securitys = SecurityGroup["sz"];
    std::vector<std::string> sh_securitys = SecurityGroup["sh"];

    std::vector<std::string> securitys(sz_securitys); 
    securitys.insert(securitys.begin(), sh_securitys.begin(), sh_securitys.end());
    auto security_buffer_map_sp = ticker_info_cache_sp->SecurityBufferMap_sp(securitys,1024*8*8*2);

    //启动多个线程，持续从ticker_info_cache 中将 tickinfo 写入到security_buffer_map
    ticker_info_cache_sp->startSubmitToBufferThreads(6);

    //对于深圳和上海，启动器是独立运行的，各自驱动
    absl::CivilDay today(2023,4,6);
    absl::Time today_start = absl::FromCivil(today,sh_tz.tz) + absl::Hours(9) + absl::Minutes(29);
    //SecurityBufferMapCollect(std::shared_ptr<SecurityBufferMap> buffer_map_sp, std::vector<uint32_t> ids, absl::Time start_tp)

    std::shared_ptr<SecurityBufferMapCollect> sz_collector_sp = std::make_shared<SecurityBufferMapCollect>(security_buffer_map_sp, sz_securitys, today_start);
    std::shared_ptr<SecurityBufferMapCollect> sh_collector_sp = std::make_shared<SecurityBufferMapCollect>(security_buffer_map_sp, sh_securitys, today_start);
    std::jthread SH_time_sig(SH_Indexinfo_Zmq_Recvier,sh_bind_addr,sh_collector_sp);
    std::jthread SZ_time_sig(SZ_Indexinfo_Zmq_Recvier,sz_bind_addr,sz_collector_sp);
    
    //一般的，驱动应该根据行情时间，但由于模拟环境的发送只依赖于读取速度，因此时间信号只能依赖于 分钟首次出现 + delay的方式模拟
    //此处简化实现，由于一天的交易时间为 9:30 - 11:30 13:00 - 15:00 因此驱动可以直接根据时间 + delay 产生
    //为了方针均匀，接受行情的rec实际上也是接受ticker， 但处理方式不同
    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "main running..." << std::endl;
    }
    //通过时间，触发采样信号，进行收割
    return 0;
}