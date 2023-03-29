#include <zmq.hpp>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // 1. 初始化ZeroMQ上下文
    zmq::context_t context(1);

    // 2. 创建ZeroMQ套接字并将其设置为发布者类型
    zmq::socket_t publisher(context, zmq::socket_type::pub);

    // 3. 绑定套接字到特定的端点，例如：TCP端口5555
    publisher.bind("tcp://172.28.142.104:16688");

    // 4. 持续发布数据
    int counter = 0;
    while (true) {
        // 5. 准备要发布的消息
        std::string msg = "Hello, this is message number " + std::to_string(counter++);

        // 6. 发送消息
        zmq::message_t zmq_msg(msg.begin(), msg.end());
        publisher.send(zmq_msg, zmq::send_flags::none);

        // 7. 休眠一段时间，例如：1秒
        std::cout << msg << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}