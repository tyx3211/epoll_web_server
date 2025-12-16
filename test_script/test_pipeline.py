import socket
import time
from http.client import HTTPResponse
from io import BytesIO

class FakeSocket:
    """将 BytesIO 伪装成 socket 供 HTTPResponse 使用"""
    def __init__(self, response_bytes):
        self._file = BytesIO(response_bytes)
    
    def makefile(self, mode):
        return self._file

def test_pipeline():
    host = '127.0.0.1'
    port = 8888

    print(f"Connecting to {host}:{port}...")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((host, port))
    except Exception as e:
        print(f"Connection failed: {e}")
        return

    # 构造 3 个 Pipelined 请求
    payload = (
        "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
    )

    print("Sending 3 pipelined requests (GET /) in a single write...")
    start_ts = time.time()
    s.sendall(payload.encode())

    # 接收响应
    response_data = b""
    while True:
        try:
            chunk = s.recv(4096)
            if not chunk:
                break
            response_data += chunk
        except socket.error as e:
            print(f"Socket error: {e}")
            break
            
    end_ts = time.time()
    s.close()
    
    print(f"\nReceived total {len(response_data)} bytes in {end_ts - start_ts:.4f}s")
    
    # 使用标准库解析 Pipeline 响应
    responses = []
    buffer = response_data
    
    while buffer:
        # 创建 FakeSocket 并用 HTTPResponse 解析
        fake_sock = FakeSocket(buffer)
        try:
            response = HTTPResponse(fake_sock)
            response.begin()
            
            # 读取响应数据
            status_code = response.status
            reason = response.reason
            headers = dict(response.headers)
            body = response.read()
            
            responses.append({
                "status": f"HTTP/1.1 {status_code} {reason}",
                "headers": headers,
                "body_len": len(body),
                "body_snippet": body[:50].decode(errors='ignore').replace("\n", " ") + "..." if len(body) > 0 else ""
            })
            
            # 计算已消耗的字节数，准备处理下一个响应
            consumed = len(response_data) - len(buffer) + len(response.headers.as_bytes()) + len(body) + 4  # 4 for \r\n\r\n
            # 更简单的方法：直接计算剩余字节
            # HTTPResponse 会告诉我们读了多少
            # 但因为我们用的 BytesIO，需要手动切割
            
            # 重新计算：找到当前响应的总长度
            status_line_end = buffer.find(b"\r\n")
            headers_end = buffer.find(b"\r\n\r\n")
            content_length = int(headers.get('Content-Length', 0))
            
            total_consumed = headers_end + 4 + content_length
            buffer = buffer[total_consumed:]
            
        except Exception as e:
            print(f"Parse error: {e}")
            break

    print("-" * 50)
    print(f"Parsed {len(responses)} distinct responses from stream.")
    
    if len(responses) == 3:
        print("✅ SUCCESS: Pipeline working perfectly! Server handled 3 requests back-to-back.")
        for i, resp in enumerate(responses):
            print(f"\n[Response {i+1}]")
            print(f"  Status:      {resp['status']}")
            print(f"  Content-Length: {resp['headers'].get('Content-Length', 'N/A')}")
            print(f"  Body Size:   {resp['body_len']} bytes")
            print(f"  Snippet:     {resp['body_snippet']}")
    else:
        print(f"❌ FAILURE: Expected 3 responses, got {len(responses)}.")

if __name__ == "__main__":
    test_pipeline()