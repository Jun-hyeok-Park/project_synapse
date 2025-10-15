#include "veh_can.hpp"
#include "veh_logger.hpp"
#include "config.hpp"

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <cstring>

// SocketCAN 송신 구현

veh::Logger can_logger("logs/veh_can.log");

CanInterface::CanInterface(const std::string& ifname) : sock_fd_(-1), if_name_(ifname) {}

CanInterface::~CanInterface() {
    close();
}

bool CanInterface::init() {
    sock_fd_ = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock_fd_ < 0) {
        LOG_ERROR(can_logger, "socket() failed.");
        return false;
    }

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, if_name_.c_str(), IFNAMSIZ - 1);
    if (ioctl(sock_fd_, SIOCGIFINDEX, &ifr) < 0) {
        LOG_ERROR(can_logger, "ioctl() failed: " + if_name_);
        return false;
    }

    struct sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR(can_logger, "bind() failed.");
        return false;
    }

    LOG_INFO(can_logger, "CAN interface opened on " + if_name_);
    return true;
}

void CanInterface::close() {
    if (sock_fd_ >= 0) {
        ::close(sock_fd_);
        sock_fd_ = -1;
        LOG_INFO(can_logger, "CAN socket closed.");
    }
}

bool CanInterface::isOpen() const {
    return sock_fd_ >= 0;
}

bool CanInterface::sendFrame(const CanFrame& frame) {
    if (sock_fd_ < 0) return false;

    struct can_frame f {};
    f.can_id = frame.id;
    f.can_dlc = frame.dlc;
    std::memcpy(f.data, frame.data.data(), frame.dlc);

    int nbytes = ::write(sock_fd_, &f, sizeof(f));
    if (nbytes != sizeof(f)) {
        LOG_ERROR(can_logger, "CAN TX failed.");
        return false;
    }
    LOG_INFO(can_logger, "CAN TX ID=0x" + std::to_string(frame.id) + " DLC=" + std::to_string(frame.dlc));
    return true;
}

bool CanInterface::receiveFrame(CanFrame& frame) {
    if (sock_fd_ < 0) return false;

    struct can_frame f {};
    int nbytes = ::read(sock_fd_, &f, sizeof(f));
    if (nbytes < 0) return false;

    frame.id = f.can_id;
    frame.dlc = f.can_dlc;
    std::memcpy(frame.data.data(), f.data, f.can_dlc);
    return true;
}
