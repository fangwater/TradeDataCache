#ifndef _PROCESS_1MIN_TICKER_HPP
#define _PROCESS_1MIN_TICKER_HPP
#include "tools.hpp"
std::tuple<std::array<uint64_t,3>, std::array<double,10>> process_1min_ticker(std::shared_ptr<std::vector<std::shared_ptr<TickerInfo>>> ticker_1min_buffer_sp) {
    auto aa_tradp = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>(ticker_1min_buffer_sp->size());
    auto aa_S_tradp = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>();
    auto aa_B_tradp = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>();
    

    auto aa_tradv = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>(ticker_1min_buffer_sp->size());
    auto aa_S_tradv = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>();
    auto aa_B_tradv = std::make_unique<std::vector<double,xsimd::aligned_allocator<double>>>();
    for(int i = 0; i < ticker_1min_buffer_sp->size(); i++){
        std::shared_ptr<TickerInfo> ticker_sp = ticker_1min_buffer_sp->at(i);
        if(ticker_sp->TradPrice > 0){
            aa_tradp->at(i) = ticker_sp->TradPrice;
            aa_tradv->at(i) = ticker_sp->TradVolume;
            if( ticker_sp->B_or_S == 1 ){
                aa_B_tradp->emplace_back(ticker_sp->TradPrice);
                aa_B_tradv->emplace_back(ticker_sp->TradVolume);
            }else{
                aa_S_tradp->emplace_back(ticker_sp->TradPrice);
                aa_S_tradv->emplace_back(ticker_sp->TradVolume);  
            }   
        }
    }
    std::cout << std::endl;
    //cjbs
    uint64_t cjb = aa_tradp->size();
    //bcjbs
    uint64_t bcjb = aa_B_tradp->size();
    //scjbs
    uint64_t scjb = aa_S_tradp->size();

    auto aa_tradmt = hadamard_product_aligned<double>(aa_tradp,aa_tradv);
    auto aa_B_tradmt = hadamard_product_aligned<double>(aa_B_tradp,aa_B_tradv);
    auto aa_S_tradmt = hadamard_product_aligned<double>(aa_S_tradp,aa_S_tradv);

    //volume_nfq
    auto volume_nfq = sum_aligned<double>(aa_tradv);
    //bvolume_nfq
    auto bvolume_nfq = sum_aligned<double>(aa_B_tradv);
    //svolume_nfq
    auto svolume_nfq = sum_aligned<double>(aa_S_tradv);
    //amount
    auto amount = sum_aligned<double>(aa_tradmt);
    //bamount
    auto bamount = sum_aligned<double>(aa_B_tradmt);
    //samount
    auto samount = sum_aligned<double>(aa_S_tradmt);
    
    auto [lowprice_nfq, highprice_nfq, openprice_nfq, closeprice_nfq] = min_max_start_end_aligned<double>(aa_tradp);
    
    return std::tuple<std::array<uint64_t,3>, std::array<double,10>>({cjb,bcjb,scjb},{volume_nfq,bvolume_nfq,svolume_nfq,amount,bamount,samount,lowprice_nfq,highprice_nfq,openprice_nfq,closeprice_nfq});
}

#endif