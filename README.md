# Project SYNAPSE
**vsomeip 기반 차량 통신 시스템 (Server ↔ Client)**  
라즈베리파이 2대 간 Service-Oriented Architecture(SOA) 통신 환경을 구축하여 차량 기능을 모듈화하고, 
AEB / OTA / Diagnostic 기능을 서비스 단위로 관리하는 프로젝트입니다.

---

## 🏗️ 구조
```
project_synapse/
├── CMakeLists.txt
├── README.md
├── .gitignore
│
├── common/
│ ├── CMakeLists.txt
│ ├── include/
│ │ └── common_defs.h
│ └── src/
│ └── logger.cpp
│
├── server/
│ ├── CMakeLists.txt
│ ├── include/
│ │ └── server_app.h
│ └── src/
│ └── main.cpp
│
└── client/
├── CMakeLists.txt
├── include/
│ └── client_app.h
└── src/
└── main.cpp
```
