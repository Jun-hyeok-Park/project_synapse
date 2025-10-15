#include "veh_can.hpp"
#include "config.hpp"
#include <iostream>
#include <cerrno>
#include <cstring>

CanInterface::CanInterface(const std::string& interface)
    : sock_fd(-1), if_name(interface) {}

CanInterface::~CanInterface() {
    close();
}

bool CanInterface::init() {
    struct ifreq ifr{};
    struct sockaddr_can addr{};

    // 1️. 소켓 생성 (RAW CAN)
    sock_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock_fd < 0) {
        std::cerr << "[CAN] Socket creation failed: " << strerror(errno) << std::endl;
        return false;
    }

    // 2️. 인터페이스 이름 설정 (e.g., can0)
    std::strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);
    if (ioctl(sock_fd, SIOCGIFINDEX, &ifr) < 0) {
        std::cerr << "[CAN] ioctl SIOCGIFINDEX failed: " << strerror(errno) << std::endl;
        ::close(sock_fd);
        sock_fd = -1;
        return false;
    }

    // 3️. 주소 설정 및 바인딩
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[CAN] Bind failed: " << strerror(errno) << std::endl;
        ::close(sock_fd);
        sock_fd = -1;
        return false;
    }

    LOG_INFO(Logger(veh::LOG_PATH_SERVER), "CAN interface initialized on " + if_name);
    return true;
}

void CanInterface::close() {
    if (sock_fd > 0) {
        ::close(sock_fd);
        sock_fd = -1;
        LOG_INFO(Logger(veh::LOG_PATH_SERVER), "CAN interface closed");
    }
}

bool CanInterface::sendFrame(const CanFrame& frame) {
    if (sock_fd < 0) {
        std::cerr << "[CAN] Socket not initialized" << std::endl;
        return false;
    }

    struct can_frame canMsg{};
    canMsg.can_id  = frame.id;
    canMsg.can_dlc = frame.dlc;
    std::memcpy(canMsg.data, frame.data.data(), frame.dlc);

    int nbytes = write(sock_fd, &canMsg, sizeof(struct can_frame));
    if (nbytes != sizeof(struct can_frame)) {
        std::cerr << "[CAN] Write failed: " << strerror(errno) << std::endl;
        return false;
    }

    std::ostringstream oss;
    oss << "CAN TX [0x" << std::hex << frame.id << "] : ";
    for (int i = 0; i < frame.dlc; ++i)
        oss << std::setw(2) << std::setfill('0') << std::hex << (int)frame.data[i] << " ";
    LOG_INFO(Logger(veh::LOG_PATH_SERVER), oss.str());

    return true;
}

bool CanInterface::receiveFrame(CanFrame& frame) {
    if (sock_fd < 0) return false;

    struct can_frame canMsg{};
    int nbytes = read(sock_fd, &canMsg, sizeof(struct can_frame));
    if (nbytes < 0) {
        std::cerr << "[CAN] Read failed: " << strerror(errno) << std::endl;
        return false;
    }

    frame.id  = canMsg.can_id;
    frame.dlc = canMsg.can_dlc;
    std::memcpy(frame.data.data(), canMsg.data, canMsg.can_dlc);

    std::ostringstream oss;
    oss << "CAN RX [0x" << std::hex << frame.id << "] : ";
    for (int i = 0; i < frame.dlc; ++i)
        oss << std::setw(2) << std::setfill('0') << std::hex << (int)frame.data[i] << " ";
    LOG_INFO(Logger(veh::LOG_PATH_SERVER), oss.str());

    return true;
}
