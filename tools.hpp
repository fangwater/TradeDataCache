
#ifndef _TOOLS_HPP
#define _TOOLS_HPP
#include <bits/stdc++.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/time/civil_time.h>
#include <absl/log/absl_log.h>
#include <absl/log/log.h>
#include <absl/time/time.h>
#include <fmt/format.h>
#include <folly/MPMCPipeline.h>
#include <absl/container/flat_hash_map.h>
#include <folly/ProducerConsumerQueue.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <xsimd/xsimd.hpp>
#include <limits>

struct ShanghaiTimeZone {
  absl::TimeZone tz;
  ShanghaiTimeZone() {
    absl::LoadTimeZone("Asia/Shanghai", &tz);
  }
}sh_tz;

std::function<absl::Duration(std::string_view time_str)> convert_time_string_to_duration = [](std::string_view time_str){
        //09:15:00.040
        //hh:mm:ss.xxx
        int hour_tens = time_str[0] - '0';       // 小时部分十位数
        int hour_ones = time_str[1] - '0';       // 小时部分个位数
        int minute_tens = time_str[3] - '0';     // 分钟部分十位数
        int minute_ones = time_str[4] - '0';     // 分钟部分个位数
        int second_tens = time_str[6] - '0';     // 秒数部分十位数
        int second_ones = time_str[7] - '0';     // 秒数部分个位数
        int millisecond_hundreds = time_str[9] - '0';   // 毫秒部分百位数
        int millisecond_tens = time_str[10] - '0';       // 毫秒部分十位数
        int millisecond_ones = time_str[11] - '0';       // 毫秒部分个位数
        //std::cout <<  fmt::format("{}{}:{}{}:{}{}.{}{}{}",hour_tens,hour_ones,minute_tens,minute_ones,second_tens,second_ones,millisecond_hundreds,millisecond_tens,millisecond_ones) << std::endl;        
        absl::Duration dur = absl::Milliseconds(millisecond_hundreds*100 + millisecond_tens*10 + millisecond_ones) + absl::Seconds(second_tens*10 + second_tens) + absl::Minutes(minute_tens*10 + minute_ones) + absl::Hours(hour_tens*10 + hour_ones);
        return dur;
};

inline void CHECK_RETURN_VALUE(bool res, std::string error_msg){
    if(!res){
        std::cerr << error_msg << std::endl;
    }
    return;
}


struct TickerInfo{
    uint32_t SecurityID;
    int32_t B_or_S;
    absl::Time TradTime;
    double TradPrice;
    double TradVolume;
    std::string to_string(){
        std::string trad_time_str = absl::FormatTime("%Y-%m-%d %H:%M:%S", TradTime, sh_tz.tz);
        return fmt::format("SecurityID: {}, B_or_S: {}, TradTime: {}, TradPrice: {:.2f}, TradVolume: {:.2f}",
                           SecurityID, B_or_S, trad_time_str, TradPrice, TradVolume);
    };
}__attribute__ ((aligned(8)));

std::function<bool(absl::Time&, absl::Time&)> is_same_civil_min = [](absl::Time& t1, absl::Time& t2)->bool {
  absl::CivilMinute minute1 = absl::ToCivilMinute(t1, sh_tz.tz);
  absl::CivilMinute minute2 = absl::ToCivilMinute(t2, sh_tz.tz);
  return minute1 == minute2;
};

std::function<bool(absl::Time&, absl::Time&)> is_next_civil_min = [](absl::Time& t1, absl::Time& t2)->bool {
  absl::CivilMinute minute1 = absl::ToCivilMinute(t1, sh_tz.tz);
  absl::CivilMinute minute2 = absl::ToCivilMinute(t2, sh_tz.tz);
  return (minute1+1) == minute2;
};

std::function<absl::Duration(std::string_view)> duration_from_index_quotes = [](std::string_view line)->absl::Duration{
    std::vector<std::string_view> x = absl::StrSplit(std::string_view(line),",");
    return convert_time_string_to_duration(x[0]);
};





template<typename T>
std::tuple<T, T, T, T> min_max_start_end_aligned(const std::unique_ptr<std::vector<T,xsimd::aligned_allocator<T>>>& input_ptr) {
    if (!input_ptr->size()) {
        //DLOG(INFO)<< "Input vector size is zero";
        return std::make_tuple(0, 0, 0, 0);
    }
    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;
    simd_type min_val = input_ptr->at(0), max_val = input_ptr->at(0);
    std::size_t i = 0;
    // for(int i = 0; i < input_ptr->size(); i++){
    //     std::cout << input_ptr->at(i) << ",";
    // }
    // std::cout << std::endl;

    for (; i + simd_size <= input_ptr->size(); i += simd_size) {
        simd_type chunk = xsimd::load_aligned(&input_ptr->at(i));

        min_val = xsimd::min(min_val, chunk);
        max_val = xsimd::max(max_val, chunk);
    }
    T min_value = xsimd::reduce_min(min_val);
    T max_value = xsimd::reduce_max(max_val);

    for (; i < input_ptr->size(); ++i) {
        min_value = std::min(min_value, input_ptr->at(i));
        max_value = std::max(max_value, input_ptr->at(i));
    }
    return std::make_tuple(min_value, max_value, input_ptr->front(), input_ptr->back());
}

template<typename T>
std::tuple<T, T, T, T> min_max_start_end(const std::unique_ptr<std::vector<T>>& input_ptr) {
    if (!input_ptr->size()) {
        throw std::invalid_argument("Input vector must not be empty.");
    }
    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;
    simd_type min_val = input_ptr->at(0), max_val = input_ptr->at(0);
    std::size_t i = 0;
    for (; i + simd_size <= input_ptr->size(); i += simd_size) {
        simd_type chunk = xsimd::load_aligned(&input_ptr->at(i));

        min_val = xsimd::min(min_val, chunk);
        max_val = xsimd::max(max_val, chunk);
    }
    T min_value = xsimd::reduce_min(min_val);
    T max_value = xsimd::reduce_max(max_val);

    for (; i < input_ptr->size(); ++i) {
        min_value = std::min(min_value, input_ptr->at(i));
        max_value = std::max(max_value, input_ptr->at(i));
    }
    return std::make_tuple(min_value, max_value, input_ptr->front(), input_ptr->back());
}




template<typename T>
T sum_aligned(const std::unique_ptr<std::vector<T,xsimd::aligned_allocator<T>>>& input_ptr) {
    if (!input_ptr->size()) {
        return static_cast<T>(0);
    }
    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;
    simd_type sum_val = 0;
    std::size_t i = 0;
    for (; i + simd_size <= input_ptr->size(); i += simd_size) {
        simd_type chunk = xsimd::load_aligned(&input_ptr->at(i));
        sum_val = sum_val + chunk;
    }
    T sum_value = xsimd::reduce_add(sum_val);

    for (; i < input_ptr->size(); ++i) {
        sum_value += input_ptr->at(i);
    }
    return sum_value;
}

template<typename T>
T sum_normal(const std::unique_ptr<std::vector<T>>& input_ptr) {
    if (!input_ptr->size()) {
        return static_cast<T>(0);
    }
    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;
    simd_type sum_val = 0;
    std::size_t i = 0;
    for (; i + simd_size <= input_ptr->size(); i += simd_size) {
        simd_type chunk = xsimd::load_unaligned(&input_ptr->at(i));
        sum_val = sum_val + chunk;
    }
    T sum_value = xsimd::reduce_add(sum_val);

    for (; i < input_ptr->size(); ++i) {
        sum_value += input_ptr->at(i);
    }
    return sum_value;
}


template<typename T>
std::unique_ptr<std::vector<T, xsimd::aligned_allocator<T>>> hadamard_product_aligned(const std::unique_ptr<std::vector<T, xsimd::aligned_allocator<T>>>& a_ptr, const std::unique_ptr<std::vector<T, xsimd::aligned_allocator<T>>>& b_ptr) {
    if (a_ptr->size() != b_ptr->size()) {
        throw std::invalid_argument("Input vectors must have the same size.");
    }

    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;

    std::unique_ptr<std::vector<T, xsimd::aligned_allocator<T>>> res_ptr = std::make_unique<std::vector<T, xsimd::aligned_allocator<T>>>(a_ptr->size());
    std::size_t i = 0;
    for (; i + simd_size <= a_ptr->size(); i += simd_size) {
        simd_type chunk_a = xsimd::load_aligned(&a_ptr->at(i));
        simd_type chunk_b = xsimd::load_aligned(&b_ptr->at(i));
        simd_type product_chunk = chunk_a * chunk_b;
        xsimd::store_aligned(&res_ptr->at(i), product_chunk);
    }

    // 对于未能对齐到SIMD宽度的剩余部分，我们使用普通循环处理
    for (; i < a_ptr->size(); ++i) {
        res_ptr->at(i) = a_ptr->at(i)* b_ptr->at(i);
    }
    return res_ptr;
}

template<typename T>
std::unique_ptr<std::vector<T>> hadamard_product_normal(const std::unique_ptr<std::vector<T>>& a_ptr, const std::unique_ptr<std::vector<T>>& b_ptr) {
    if (a_ptr->size() != b_ptr->size()) {
        throw std::invalid_argument("Input vectors must have the same size.");
    }

    using simd_type = xsimd::simd_type<T>;
    std::size_t simd_size = simd_type::size;

    std::unique_ptr<std::vector<T>> res_ptr = std::make_unique<std::vector<T>>(a_ptr->size());
    std::size_t i = 0;
    for (; i + simd_size <= a_ptr->size(); i += simd_size) {
        simd_type chunk_a = xsimd::load_unaligned(&a_ptr->at(i));
        simd_type chunk_b = xsimd::load_unaligned(&b_ptr->at(i));
        simd_type product_chunk = chunk_a * chunk_b;
        xsimd::store_aligned(&res_ptr->at(i), product_chunk);
    }

    // 对于未能对齐到SIMD宽度的剩余部分，我们使用普通循环处理
    for (; i < a_ptr->size(); ++i) {
        res_ptr->at(i) = a_ptr->at(i)* b_ptr->at(i);
    }
    return res_ptr;
}


#endif