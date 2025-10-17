<div align="center">

```
        ██████╗ ██ ██████╗      OS: D'O-CORE v1.0.0 "IRRIG Distro"
        ██╔══██╗ ██╔═══██╗      Host: ESP32 DevKit
        ██║  ██║ ██║   ██║      Kernel: ESP-IDF
        ██║  ██║ ██║   ██║      Shell: DORUS-CORE CLI
        ██████╔╝ ╚██████╔╝      Author: D'Orus Tsitera
        ╚═════╝  ╚═════╝        Embedded Systems Engineer
```

# 🌊 D'O-Core OS

**A Lightweight Real-Time Operating System for ESP32**  
*Designed for Industrial IoT & Smart Irrigation Systems*

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-1.0.0-orange.svg)](https://github.com/dorusrdt/DO-Core-Os)

</div>

---

## 📖 Table of Contents

- [Overview](#-overview)
- [Key Features](#-key-features)
- [Architecture](#-architecture)
- [Hardware Requirements](#-hardware-requirements)
- [Quick Start](#-quick-start)
- [System Components](#-system-components)
- [CLI Commands](#-cli-commands)
- [Application Framework](#-application-framework)
- [Visual Feedback](#-visual-feedback)
- [Network Services](#-network-services)
- [Project Structure](#-project-structure)
- [Development](#-development)
- [Contributing](#-contributing)
- [License](#-license)
- [Author](#-author)

---

## 🎯 Overview

**D'O-Core OS** is a custom-built, lightweight real-time operating system designed specifically for ESP32 microcontrollers. It provides a robust foundation for building industrial IoT applications with a focus on reliability, modularity, and ease of use.

The OS features a **microkernel architecture** with a modular application framework, making it ideal for distributed systems like smart irrigation, industrial automation, and sensor networks.

### 🌟 Why D'O-Core OS?

- **🔧 Modular Design**: Plug-and-play application architecture
- **⚡ Real-Time Performance**: FreeRTOS-based task management
- **🌐 Network-First**: Built-in WiFi, HTTP, NTP, and OTA support
- **📊 System Monitoring**: Real-time health checks and diagnostics
- **🛡️ Production-Ready**: Persistent configuration, error handling, and logging
- **💻 Developer-Friendly**: Interactive CLI with 100+ commands

---

## ✨ Key Features

### 🔹 Core Kernel

- **Task Manager**: Priority-based scheduling with FreeRTOS integration
- **Memory Manager**: Dynamic allocation tracking and leak detection
- **Log System**: Multi-level logging with circular buffer (1000+ messages)
- **System Monitor**: CPU, memory, and health monitoring
- **Error Handling**: Comprehensive error codes and recovery mechanisms

### 🔹 Hardware Abstraction Layer (HAL)

- **RTC Manager**: DS3231 real-time clock support
- **Time Sync**: Automatic NTP → RTC → System time synchronization
- **Heartbeat LED**: Visual system status indicator with 6 distinct patterns
- **GPIO Management**: Pin configuration and control

### 🔹 Network Stack

- **WiFi Manager**: Auto-connect, credential storage, supervision
- **HTTP Client**: Full REST API support (GET, POST, PUT, DELETE, PATCH)
- **NTP Manager**: Network time synchronization with timezone support
- **OTA Manager**: Over-The-Air firmware updates via web interface

### 🔹 Application Framework

- **Dynamic App Loading**: Register and manage multiple applications
- **State Management**: Start, stop, pause, resume applications
- **Inter-App Communication**: HTTP-based IPC for distributed systems
- **Resource Isolation**: Per-app memory and CPU tracking

### 🔹 Irrigation Distribution (IRRIG Distro)

Pre-built smart irrigation system with 3 specialized applications:

1. **Master Controller**: Orchestrates irrigation schedules and monitors zones
2. **Slave Sensors**: Reads soil moisture, temperature, humidity sensors
3. **Slave Relays**: Controls irrigation valves and pumps

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     USER APPLICATIONS                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Master App   │  │ Sensors App  │  │ Relays App   │      │
│  │ (ID: 1)      │  │ (ID: 2)      │  │ (ID: 3)      │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ App Manager API
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATION MANAGER                       │
│  • Dynamic Loading  • State Management  • IPC               │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ Kernel API
┌─────────────────────────────────────────────────────────────┐
│                      KERNEL SERVICES                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Task    │  │  Memory  │  │   Log    │  │  Monitor │   │
│  │ Manager  │  │ Manager  │  │  System  │  │  System  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ HAL API
┌─────────────────────────────────────────────────────────────┐
│              HARDWARE ABSTRACTION LAYER (HAL)                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   RTC    │  │   Time   │  │ Heartbeat│  │   GPIO   │   │
│  │ Manager  │  │   Sync   │  │   LED    │  │  Control │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │ Network API
┌─────────────────────────────────────────────────────────────┐
│                      NETWORK STACK                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   WiFi   │  │   HTTP   │  │   NTP    │  │   OTA    │   │
│  │ Manager  │  │  Client  │  │ Manager  │  │ Manager  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌─────────────────────────────────────────────────────────────┐
│                  ESP32 HARDWARE (ESP-IDF)                    │
│  • FreeRTOS  • WiFi  • NVS  • SPI  • I2C  • GPIO           │
└─────────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware Requirements

### Minimum Requirements

- **MCU**: ESP32 (any variant)
- **RAM**: 320KB (built-in)
- **Flash**: 4MB minimum
- **WiFi**: 2.4GHz 802.11 b/g/n

### Recommended Setup

- **Board**: ESP32 DevKit V1
- **RTC**: DS3231 (optional, for time persistence)
- **LED**: Built-in LED on GPIO 2 (heartbeat indicator)
- **Sensors**: Capacitive soil moisture sensors (for irrigation)
- **Relays**: 5V relay modules (for valve control)

### Pin Configuration

| Component | GPIO | Description |
|-----------|------|-------------|
| Heartbeat LED | 2 | System status indicator |
| I2C SDA | 21 | RTC communication |
| I2C SCL | 22 | RTC communication |
| Relay Zone 1 | 15 | Irrigation valve 1 |
| Relay Zone 2 | 4 | Irrigation valve 2 |
| Relay Zone 3 | 16 | Irrigation valve 3 |
| Relay Zone 4 | 17 | Irrigation valve 4 |

---

## 🚀 Quick Start

### 1. Prerequisites

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone https://github.com/dorusrdt/DO-Core-Os.git
cd DO-Core-Os
```

### 2. Build & Flash

```bash
# Build firmware
pio run

# Upload to ESP32
pio run --target upload --upload-port /dev/ttyUSB0

# Monitor serial output
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

### 3. First Boot

After flashing, you'll see the system logo:

```
        ██████╗ ██ ██████╗      OS: D'O-CORE v1.0.0 "IRRIG Distro"
        ██╔══██╗ ██╔═══██╗      Host: ESP32 DevKit
        ██║  ██║ ██║   ██║      Kernel: ESP-IDF
        ██║  ██║ ██║   ██║      Uptime: 0h 0m 15s
        ██████╔╝ ╚██████╔╝      Packages: 8 tasks
        ╚═════╝  ╚═════╝        Shell: DORUS-CORE CLI
                                CPU: 240MHz
                                Memory: 82MB / 320MB
                                System Health: HEALTHY
                                WiFi: DISCONNECTED
                                IP: N/A
                                Logs: 45/1000 messages
                                Local Time: 00:00:15

       v1.0.0 "IRRIG Distro OTA chg"

D'O-Core>
```

### 4. Configure WiFi

```bash
D'O-Core> wifi_save MySSID MyPassword
D'O-Core> reboot
```

### 5. Configure Device Role

```bash
# Set as Master controller
D'O-Core> irrig_set_role master
D'O-Core> irrig_config_save
D'O-Core> reboot
```

---

## 🧩 System Components

### Core Kernel

#### Task Manager
- **Priority Levels**: LOW, NORMAL, HIGH, CRITICAL
- **Stack Sizes**: SMALL (2KB), MEDIUM (4KB), LARGE (8KB)
- **Core Pinning**: Assign tasks to specific CPU cores
- **Statistics**: Track CPU time, memory usage per task

#### Memory Manager
- **Allocation Tracking**: Monitor all malloc/free operations
- **Leak Detection**: Identify memory leaks in real-time
- **Fragmentation Analysis**: Track heap fragmentation
- **Statistics**: Free heap, largest block, allocation count

#### Log System
- **Levels**: DEBUG, INFO, WARN, ERROR, CRITICAL
- **Circular Buffer**: 1000 messages with automatic rotation
- **Persistence**: Save logs to NVS flash
- **Filtering**: Query logs by level, time, or keyword

#### System Monitor
- **Health Checks**: CPU, memory, WiFi, RTC status
- **Uptime Tracking**: System runtime in seconds
- **Watchdog**: Automatic recovery from crashes
- **Alerts**: Configurable thresholds for warnings

---

## 💻 CLI Commands

The D'O-Core OS includes an interactive command-line interface with 100+ commands organized into categories:

### System Commands

```bash
help                    # Show all commands
system_info            # Display system information
system_health          # Check system health
reboot                 # Restart system
uptime                 # Show system uptime
```

### Task Management

```bash
task_list              # List all running tasks
task_info <id>         # Show task details
task_stats             # Task statistics
```

### Memory Management

```bash
mem_info               # Memory usage statistics
mem_stats              # Detailed memory analysis
heap_info              # Heap fragmentation info
```

### WiFi Management

```bash
wifi_scan              # Scan for networks
wifi_save <ssid> <pwd> # Save WiFi credentials
wifi_connect           # Connect to saved network
wifi_status            # Show WiFi status
wifi_ip                # Show IP address
```

### Application Management

```bash
app_list               # List registered apps
app_start <id>         # Start application
app_stop <id>          # Stop application
app_pause <id>         # Pause application
app_resume <id>        # Resume application
app_info <id>          # Show app details
```

### Irrigation Commands

```bash
irrig_set_role <role>  # Set device role (master/slave1/slave2)
irrig_activate_role    # Activate configured role
irrig_config_show      # Show irrigation config
irrig_config_save      # Save config to NVS
irrig_status           # Show irrigation status
```

### HTTP Client

```bash
http_get <url>         # HTTP GET request
http_post <url> <data> # HTTP POST request
http_stats             # HTTP statistics
```

### OTA Updates

```bash
ota_start              # Start OTA server
ota_stop               # Stop OTA server
ota_status             # OTA server status
ota_info               # Firmware version info
```

### Log System

```bash
log_show               # Display recent logs
log_clear              # Clear log buffer
log_save               # Save logs to NVS
log_stats              # Log statistics
```

### Time Management

```bash
time_show              # Show current time
time_sync              # Sync time (NTP/RTC)
time_set <timestamp>   # Set system time
rtc_read               # Read RTC time
rtc_write              # Write to RTC
```

---

## 📱 Application Framework

### Creating a Custom Application

```cpp
#include "kernel/app/app_manager.h"

// Application callbacks
void my_app_init(void) {
    // Initialize your app
}

void my_app_start(void) {
    // Start your app logic
}

void my_app_loop(void) {
    // Main loop (called repeatedly)
}

void my_app_stop(void) {
    // Cleanup on stop
}

// Register application
SysError_t register_my_app(void) {
    AppInfo_t info = {
        .app_id = 0,  // Auto-assigned
        .name = "MyApp",
        .description = "My custom application",
        .version = "1.0.0",
        .priority = PRIORITY_NORMAL,
        .stack_size = STACK_SIZE_MEDIUM,
        .auto_start = false
    };
    
    AppCallbacks_t callbacks = {
        .init = my_app_init,
        .start = my_app_start,
        .loop = my_app_loop,
        .stop = my_app_stop
    };
    
    return app_register(&info, &callbacks);
}
```

### Application Lifecycle

```
UNLOADED → LOADING → RUNNING → PAUSED → STOPPED
                         ↓
                      ERROR
```

---

## 💡 Visual Feedback

### Heartbeat LED Patterns

The built-in LED (GPIO 2) provides real-time system status:

| Pattern | Description | Meaning |
|---------|-------------|---------|
| 🔴 **Rapid (25ms)** | ▓░▓░▓░▓░▓░ | System booting |
| 🟢 **Slow (1s)** | ▓▓▓▓▓░░░░░ | Ready, WiFi connected |
| 🔵 **Double Pulse** | ▓░▓░░░░░░░ | Application running |
| 🟡 **Fast (100ms)** | ▓░▓░▓░▓░ | WiFi error |
| 🔴 **Solid ON** | ▓▓▓▓▓▓▓▓▓▓ | Critical error |
| ⚫ **OFF** | ░░░░░░░░░░ | System halted |

---

## 🌐 Network Services

### WiFi Manager

- **Auto-Connect**: Automatically connects to saved network on boot
- **Credential Storage**: Persistent WiFi credentials in NVS
- **Supervision**: Monitors connection and auto-reconnects
- **Signal Monitoring**: RSSI tracking and weak signal warnings

### HTTP Client

- **Full REST API**: GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
- **JSON Support**: Built-in ArduinoJson integration
- **Retry Logic**: Configurable retry count and delays
- **Statistics**: Track requests, failures, response times

### NTP Manager

- **Time Synchronization**: Automatic NTP sync on WiFi connect
- **Timezone Support**: Configurable timezone and DST
- **Business Hours**: Built-in business/night time detection
- **Fallback**: Uses RTC if NTP unavailable

### OTA Manager

- **Web Interface**: ElegantOTA web-based updater
- **Progress Tracking**: Real-time upload progress
- **Auto-Reboot**: Automatic restart after successful update
- **Rollback**: Keeps previous firmware for recovery

**Access OTA**: `http://<ESP32_IP>:3232/update`

---

## 📁 Project Structure

```
DO-Core-Os/
├── src/
│   ├── main.cpp                    # Main entry point
│   ├── kernel/
│   │   ├── core/                   # Core kernel services
│   │   │   ├── kernel.h            # System definitions
│   │   │   ├── task_manager.*      # Task management
│   │   │   ├── memory_manager.*    # Memory management
│   │   │   ├── log_system.*        # Logging system
│   │   │   └── system_monitor.*    # Health monitoring
│   │   ├── hal/                    # Hardware abstraction
│   │   │   ├── rtc_manager.*       # RTC DS3231
│   │   │   ├── time_sync_manager.* # Time synchronization
│   │   │   └── heartbeat_led.*     # LED status indicator
│   │   ├── network/                # Network stack
│   │   │   ├── wifi_manager.*      # WiFi management
│   │   │   ├── http_client.*       # HTTP client
│   │   │   ├── ntp_manager.*       # NTP client
│   │   │   └── ota_manager.*       # OTA updates
│   │   ├── app/                    # Application framework
│   │   │   └── app_manager.*       # App lifecycle
│   │   └── interface/              # CLI interface
│   │       └── interface.*         # Command processor
│   └── apps/                       # User applications
│       ├── irrig_app_master/       # Master controller
│       ├── irrig_app_slave_sensors/# Sensor reader
│       ├── irrig_app_slave_relays/ # Relay controller
│       └── irrig_common/           # Shared irrigation code
├── include/
│   └── config.h                    # System configuration
├── platformio.ini                  # PlatformIO config
├── README.md                       # This file
└── LICENSE                         # MIT License
```

---

## 🛠️ Development

### Building from Source

```bash
# Install dependencies
pio lib install

# Build
pio run

# Clean build
pio run --target clean
pio run
```

### Debugging

```bash
# Enable verbose logging
# In config.h, set:
#define LOG_LEVEL_DEFAULT LOG_LEVEL_DEBUG

# Monitor serial output
pio device monitor --baud 115200
```

### Adding a New Module

1. Create module files in appropriate directory
2. Add header include in `kernel.h`
3. Initialize in `main.cpp` setup()
4. Register CLI commands in `interface.cpp`

### Memory Optimization

```cpp
// Check memory usage
D'O-Core> mem_info
Free Heap: 245760 bytes
Largest Block: 110592 bytes
Allocations: 156

// Reduce stack sizes if needed
#define STACK_SIZE_SMALL  2048  // 2KB
#define STACK_SIZE_MEDIUM 4096  // 4KB
```

---

## 🤝 Contributing

Contributions are welcome! Please follow these guidelines:

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Code Style

- Use **4 spaces** for indentation
- Follow **C++11** standards
- Add **comments** for complex logic
- Update **documentation** for new features

---

## 📄 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 👨‍💻 Author

**D'Orus Tsitera**  
*Embedded Systems Engineer*

- 🌐 GitHub: [@dorusrdt](https://github.com/dorusrdt)
- 📧 Email: dorus.tsitera@example.com
- 💼 LinkedIn: [D'Orus Tsitera](https://linkedin.com/in/dorus-tsitera)

---

## 🙏 Acknowledgments

- **Espressif Systems** - ESP32 platform and ESP-IDF
- **Arduino Community** - Arduino framework for ESP32
- **FreeRTOS** - Real-time operating system kernel
- **PlatformIO** - Development platform
- **ElegantOTA** - Web-based OTA library

---

## 📊 Statistics

- **Lines of Code**: ~15,000
- **Modules**: 20+
- **CLI Commands**: 100+
- **Applications**: 3 (Irrigation Distribution)
- **Development Time**: 6+ months
- **Target Platform**: ESP32 (all variants)

---

<div align="center">

**⭐ Star this project if you find it useful!**

Made with ❤️ by D'Orus Tsitera

</div>
