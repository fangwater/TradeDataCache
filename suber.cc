#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <folly/ProducerConsumerQueue.h>

constexpr size_t kQueueCapacity = 1024;

void consumer(folly::ProducerConsumerQueue<std::string>& queue) {
    while (true) {
        std::string message;
        while (!queue.read(message)) {
            // 如果队列为空，可以选择等待、继续尝试、或采取其他策略
        }

        // 处理消息
        std::cout << "Received: " << message << std::endl;
    }
}

int main() {
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    subscriber.connect("tcp://172.28.142.104:16688");
    subscriber.set(zmq::sockopt::subscribe, "");

    folly::ProducerConsumerQueue<std::string> message_queue(kQueueCapacity);

    std::thread consumer_thread(consumer, std::ref(message_queue));

    while (true) {
        zmq::message_t message;
        auto res_state = subscriber.recv(message, zmq::recv_flags::none);
        if(!res_state){
            std::cerr << "Failed to rec messgae" << std::endl;
        }
        std::string message_str(static_cast<char*>(message.data()), message.size());
        std::cout << "Received: " << message_str << std::endl;
        // 将消息推送到队列中
        while (!message_queue.write(std::move(message_str))) {
            // 如果队列已满，可以选择丢弃消息、等待、或采取其他策略
        }
    }

    consumer_thread.join();

    return 0;
}
