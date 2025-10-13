#include "veh_server_common.hpp"

// 7개 서비스 헤더
#include "veh_drive_server.hpp"
#include "veh_aeb_server.hpp"
#include "veh_fcw_server.hpp"
#include "veh_autopark_server.hpp"
#include "veh_auth_server.hpp"
#include "veh_tof_server.hpp"
#include "veh_fault_server.hpp"

int main() {
    LOG_SERVER("Init", "Unified veh_server starting...");

    // 단일 애플리케이션
    auto app = vsomeip::runtime::get()->create_application("veh-server");
    if (!app->init()) {
        LOG_SERVER("Error", "Initialization failed");
        return -1;
    }

    // 서비스 인스턴스
    auto drive    = std::make_shared<VehDriveServer>(app);
    auto aeb      = std::make_shared<VehAebServer>(app);
    auto fcw      = std::make_shared<VehFcwServer>(app);
    auto autopark = std::make_shared<VehAutoparkServer>(app);
    auto auth     = std::make_shared<VehAuthServer>(app);
    auto tof      = std::make_shared<VehTofServer>(app);
    auto fault    = std::make_shared<VehFaultServer>(app);

    // 서비스 Offer (각 JSON에서 쓰던 포트맵은 통합 JSON에서 지정)
    offer_service(app, VEH_DRIVE_SERVICE_ID, VEH_INSTANCE_ID);
    offer_service(app, VEH_AEB_SERVICE_ID,   VEH_INSTANCE_ID);
    offer_service(app, VEH_FCW_SERVICE_ID,   VEH_INSTANCE_ID);
    offer_service(app, VEH_AP_SERVICE_ID,    VEH_INSTANCE_ID);
    offer_service(app, VEH_AUTH_SERVICE_ID,  VEH_INSTANCE_ID);
    offer_service(app, VEH_TOF_SERVICE_ID,   VEH_INSTANCE_ID);
    offer_service(app, VEH_FAULT_SERVICE_ID, VEH_INSTANCE_ID);

    // 핸들러/이벤트 초기화
    drive->init();
    aeb->init();
    fcw->init();
    autopark->init();
    auth->init();
    tof->init();
    fault->init();

    LOG_SERVER("Main", "All services initialized. Starting app...");
    app->start();
    return 0;
}
