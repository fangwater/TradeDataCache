import zmq

def main():
    # 创建一个 ZeroMQ 上下文
    context = zmq.Context()

    # 创建一个 SUB 类型的套接字
    socket = context.socket(zmq.SUB)

    # 连接到发布者的地址
    publisher_address = "tcp://172.29.209.51:10086"
    socket.connect(publisher_address)
    socket.setsockopt(zmq.RCVBUF, 1024 * 1024 * 8)
    socket.setsockopt(zmq.RCVHWM, 0)
    # 订阅的主题，这里设置为全部订阅
    topic = ""
    socket.setsockopt_string(zmq.SUBSCRIBE, topic)

    print(f"已订阅：{publisher_address}，主题：{topic}")

    try:
        while True:
            # 接收消息，使用 recv_string 方法接收字符串格式的消息
            message = socket.recv_string()
            if(message.split(',')[5] == "301314"):
                print(f"{message}")
    except KeyboardInterrupt:
        print("程序已中断。")
    finally:
        # 关闭套接字和上下文
        socket.close()
        context.term()

if __name__ == "__main__":
    main()