import socket

RTSP_SERVER_IP = '127.0.0.1'
RTSP_SERVER_PORT = 7899 

# 添加Option请求
option_request = (
    'OPTION rtsp://{}:{}/video RTSP/1.0\r\n'
    'CSeq: 1\r\n'
    '\r\n'
).format(RTSP_SERVER_IP, RTSP_SERVER_PORT).encode('utf-8')

# Describe请求保持不变
describe_request = (
    'DESCRIBE rtsp://{}:{}/video RTSP/1.0\r\n' 
    'CSeq: 2\r\n'
    'Accept: application/sdp\r\n'
    '\r\n'
).format(RTSP_SERVER_IP, RTSP_SERVER_PORT).encode('utf-8')

# 添加Setup请求  
setup_request = (
    'SETUP rtsp://{}:{}/video RTSP/1.0\r\n'
    'CSeq: 3\r\n'
    'Transport: RTP/AVP;unicast;client_port=8000-8001\r\n' 
    '\r\n'
).format(RTSP_SERVER_IP, RTSP_SERVER_PORT).encode('utf-8')

# 添加Play请求
play_request = (
    'PLAY rtsp://{}:{}/video RTSP/1.0\r\n'
    'CSeq: 4\r\n'
    'Session: 1\r\n'
    '\r\n'  
).format(RTSP_SERVER_IP, RTSP_SERVER_PORT).encode('utf-8')

tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
tcp_socket.connect((RTSP_SERVER_IP, RTSP_SERVER_PORT))

# 发送数据
# message = "Hello, Server!"
# s.send(message.encode('utf-8')) 

# 发送请求
# tcp_socket.send(option_request)  
# option_response = tcp_socket.recv(4096)

# tcp_socket.send(describe_request)
# describe_response = tcp_socket.recv(4096)  

# tcp_socket.send(setup_request)
# setup_response = tcp_socket.recv(4096)

# tcp_socket.send(play_request) 
# play_response = tcp_socket.recv(4096)

# 打印响应
# print(option_response.decode('utf-8'))
# print(describe_response.decode('utf-8'))
# print(setup_response.decode('utf-8')) 
# print(play_response.decode('utf-8'))

tcp_socket.close()