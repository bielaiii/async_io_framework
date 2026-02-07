import socket
from time import sleep

HOST = '127.0.0.1'  # 本地地址
PORT = 7899        # 监听端口

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((HOST, PORT))
server_socket.listen(3) # 允许一个连接

print(f"TCP Echo Server 正在 {HOST}:{PORT} 监听...")

while True:
    conn, addr = server_socket.accept()
    print(f"连接来自 {addr}")
    try:
        while True:
            data = conn.recv(1024) # 接收数据
            if not data:
                break # 客户端断开连接
            print(f"收到: {data.decode()}")
            sleep(3)
            conn.sendall(data) # 原样返回 (ECHO)
            print(f"发送: {data.decode()}")
    except ConnectionResetError:
        print(f"连接 {addr} 被重置或中断。")
    finally:
        conn.close() # 关闭连接
