/**
 * @file zephyr_ethernet.cpp
 * @brief 以太网接口初始化（Zephyr 原生网络栈）。
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_mgmt.h>

#include "platform/platform_ethernet.hpp"
#include "platform/platform_logger.hpp"

LOG_MODULE_REGISTER(sky_board_eth, LOG_LEVEL_INF);

namespace {

/** @brief IPv4 事件回调对象。 */
static struct net_mgmt_event_callback g_ipv4_event_cb;

/** @brief 回调注册标志，防止重复注册同一事件回调。 */
static bool g_ipv4_event_cb_registered;

/**
 * @brief IPv4 事件处理函数。
 * @param mgmt_event 事件类型，仅处理 DHCP 绑定与地址新增事件。
 * @param iface 触发事件的网络接口。
 */
static void on_ipv4_event(struct net_mgmt_event_callback*, uint64_t mgmt_event,
                          struct net_if* iface) {
  if ((mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) && (mgmt_event != NET_EVENT_IPV4_ADDR_ADD)) {
    return;
  }

  if (iface == nullptr) {
    return;
  }

  struct net_in_addr* addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
  if (addr == nullptr) {
    addr = net_if_ipv4_get_global_addr(iface, NET_ADDR_TENTATIVE);
  }
  if (addr == nullptr) {
    return;
  }

  char ip_buf[NET_IPV4_ADDR_LEN];
  if (net_addr_ntop(NET_AF_INET, addr, ip_buf, sizeof(ip_buf)) == nullptr) {
    return;
  }

  LOG_INF("eth ipv4 ready: %s", ip_buf);
}

}  // namespace

namespace platform {

/**
 * @brief 初始化以太网接口并启动 DHCPv4。
 * @return 0 表示成功；负值表示失败。
 */
int ethernet_init() noexcept {
  struct net_if* iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
  if (iface == nullptr) {
    logger().error("ethernet interface not found", -ENODEV);
    return -ENODEV;
  }

  if (!g_ipv4_event_cb_registered) {
    net_mgmt_init_event_callback(&g_ipv4_event_cb, on_ipv4_event,
                                 NET_EVENT_IPV4_DHCP_BOUND | NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&g_ipv4_event_cb);
    g_ipv4_event_cb_registered = true;
  }

  const int ret = net_if_up(iface);
  if (ret < 0 && ret != -EALREADY) {
    logger().error("failed to bring ethernet up", ret);
    return ret;
  }

  logger().info("ethernet interface up");
  net_dhcpv4_start(iface);
  logger().info("ethernet dhcpv4 started");

  return 0;
}

}  // namespace platform
