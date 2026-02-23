/**
 * @file tcp_service.cpp
 * @brief TCP 回传服务实现：监听 8000 端口，接收后原样回传。
 */

#include "servers/tcp_service.hpp"

#include <errno.h>
#include <zephyr/net/socket.h>

namespace {

/**
 * @brief 关闭 socket 并重置 fd。
 * @param fd 文件描述符引用。
 */
void close_fd(int& fd) {
  if (fd >= 0) {
    (void)zsock_close(fd);
    fd = -1;
  }
}

}  // namespace

namespace servers {

/**
 * @brief 线程入口静态适配函数。
 * @param p1 TcpService 对象指针。
 * @param p2 未使用。
 * @param p3 未使用。
 */
void TcpService::threadEntry(void* p1, void*, void*) { static_cast<TcpService*>(p1)->threads(); }

/**
 * @brief TCP 服务线程主循环。
 * @note 负责建链监听、接入客户端、接收并回传数据。
 */
void TcpService::threads() noexcept {
  /*
   * 执行步骤总览：
   * 1) 初始化本线程使用的监听/客户端 fd。
   * 2) 进入主循环并确保监听 socket 就绪。
   * 3) 轮询监听 socket，周期检查停止标志。
   * 4) 有新连接时 accept 客户端并配置收发超时。
   * 5) 在客户端会话内循环 recv -> send，完成 TCP 回传。
   * 6) 客户端断开或异常后关闭 client fd，回到主循环等待下一个连接。
   * 7) 收到停止请求后统一清理资源并更新服务状态。
   */

  /* 步骤 1：初始化线程内 socket 句柄。 */
  int listen_fd = -1;
  int client_fd = -1;

  log_.info("tcp service starting");

  /* 步骤 2：主循环，直到收到 stop 请求。 */
  while (atomic_get(&stop_requested_) == 0) {
    if (listen_fd < 0) {
      /* 步骤 2.1：监听 fd 未就绪时，创建并配置 0.0.0.0:8000。 */
      /* 监听端点固定为 0.0.0.0:8000。 */
      struct sockaddr_in addr = {};
      addr.sin_family = AF_INET;
      addr.sin_port = htons(kListenPort);
      addr.sin_addr.s_addr = htonl(INADDR_ANY);

      listen_fd = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (listen_fd < 0) {
        log_.error("tcp socket create failed", -errno);
        k_sleep(K_MSEC(1000));
        continue;
      }

      const int reuse_addr = 1;
      (void)zsock_setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));

      if (zsock_bind(listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log_.error("tcp bind failed", -errno);
        close_fd(listen_fd);
        k_sleep(K_MSEC(1000));
        continue;
      }

      if (zsock_listen(listen_fd, 1) < 0) {
        log_.error("tcp listen failed", -errno);
        close_fd(listen_fd);
        k_sleep(K_MSEC(1000));
        continue;
      }

      log_.info("tcp service listening on port 8000");
    }

    /* 步骤 3：轮询监听 socket，避免 accept 永久阻塞。 */
    struct zsock_pollfd pfd = {};
    pfd.fd = listen_fd;
    pfd.events = ZSOCK_POLLIN;

    const int poll_ret = zsock_poll(&pfd, 1, 1000);
    if (poll_ret < 0) {
      log_.error("tcp poll failed", -errno);
      close_fd(client_fd);
      close_fd(listen_fd);
      continue;
    }

    if (poll_ret == 0 || (pfd.revents & ZSOCK_POLLIN) == 0) {
      continue;
    }

    /* 步骤 4：接入一个客户端连接。 */
    client_fd = zsock_accept(listen_fd, nullptr, nullptr);
    if (client_fd < 0) {
      log_.error("tcp accept failed", -errno);
      continue;
    }

    log_.info("tcp client connected");

    /* 步骤 4.1：给收发设置超时，便于及时响应 stop。 */
    struct timeval timeout = {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    (void)zsock_setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)zsock_setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    /* 步骤 5：客户端会话循环，持续执行接收与回传。 */
    while (atomic_get(&stop_requested_) == 0) {
      uint8_t buf[256];
      const ssize_t recv_len = zsock_recv(client_fd, buf, sizeof(buf), 0);

      if (recv_len == 0) {
        log_.info("tcp client disconnected");
        break;
      }

      if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }

        log_.error("tcp recv failed", -errno);
        break;
      }

      /* 把本次收到的数据完整回传给客户端。 */
      size_t sent_total = 0U;
      /* 步骤 5.1：处理短写，确保本次 payload 全量发回。 */
      while (sent_total < static_cast<size_t>(recv_len)) {
        const ssize_t sent_len =
            zsock_send(client_fd, &buf[sent_total], static_cast<size_t>(recv_len) - sent_total, 0);
        if (sent_len < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
          }

          log_.error("tcp send failed", -errno);
          sent_total = 0U;
          break;
        }

        sent_total += static_cast<size_t>(sent_len);
      }

      if (sent_total == 0U) {
        break;
      }
    }

    /* 步骤 6：一次客户端会话结束，释放 client fd。 */
    close_fd(client_fd);
  }

  /* 步骤 7：线程退出前统一清理并更新服务状态。 */
  close_fd(client_fd);
  close_fd(listen_fd);

  atomic_set(&running_, 0);
  thread_id_ = nullptr;
  log_.info("tcp service stopped");
}

/**
 * @brief 请求停止 TCP 服务线程。
 * @note 只设置停止标志并尝试唤醒线程，不阻塞等待退出。
 */
void TcpService::stop() noexcept {
  if (atomic_get(&running_) == 0) {
    return;
  }

  atomic_set(&stop_requested_, 1);
  if (thread_id_ != nullptr) {
    k_wakeup(thread_id_);
  }
}

/**
 * @brief 启动 TCP 服务线程（幂等）。
 * @return 0 表示成功或已在运行；负值表示失败。
 */
int TcpService::run() noexcept {
  if (!atomic_cas(&running_, 0, 1)) {
    log_.info("tcp service already running");
    return 0;
  }

  atomic_set(&stop_requested_, 0);
  thread_id_ = k_thread_create(&thread_, stack_, K_THREAD_STACK_SIZEOF(stack_), threadEntry, this,
                               nullptr, nullptr, kPriority, 0, K_NO_WAIT);
  if (thread_id_ == nullptr) {
    atomic_set(&running_, 0);
    log_.error("failed to create tcp service thread", -1);
    return -1;
  }

  k_thread_name_set(thread_id_, "tcp_service");
  return 0;
}

}  // namespace servers
