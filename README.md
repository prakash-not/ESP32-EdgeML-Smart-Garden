# 🌱 ESP32 EdgeAI Smart Garden Monitor
> **Autonomous Plant Care System powered by Edge Machine Learning & IoT**

![Web Dashboard](diagrams/web_dashboard.png)

## 📖 Project Overview
This project is an advanced **AI-Driven Smart Garden** designed to automate the care of sensitive plants, utilizing the **Panda Plant** (*Kalanchoe tomentosa*) as a reference model.

Unlike standard irrigation systems that use fixed timers, this system utilizes a **Decision Tree Classifier** trained on real-world sensor data (`data/training_data.csv`). It runs purely on the **ESP32 microcontroller (Edge Computing)** to make real-time decisions about watering, lighting, and safety without relying on the cloud.

## 🚀 Key Features
* **🧠 Edge ML Engine:** Implements a C++ decision tree derived from Python analysis to classify plant health (Healthy, Thirsty, Root Rot Risk, etc.).
* **💧 Precision Auto-Watering:** Uses a "Pulse Dosing" algorithm to water only when soil is critically dry (succulent-specific logic: Soil > 3800).
* **🛡️ Hardware Safety Interlock:** Prevents water pump operation if the tank is empty (`Water Level < 1000`), protecting hardware from burnout.
* **⚠️ Root Rot Prevention:** Monitors Volatile Organic Compounds (VOCs) using an MQ-2 sensor to detect soil rot gases before visual symptoms appear.
* **🌐 Real-Time Telemetry:** Features a responsive, asynchronous (AJAX) web dashboard hosted directly on the ESP32.
* **☀️ Circadian Lighting:** Syncs with NTP (Internet Time) to provide grow lights during the day but ensure darkness at night.

## 📂 Repository Structure
```text
ESP32-EdgeAI-Garden-Monitor/
├── src/
│   └── SmartGarden_Final.ino      # Main C++ Firmware for ESP32
├── data/
│   └── training_data.csv          # Dataset used to train the ML model
├── scripts/
│   └── train_model.py             # Python script used to extract ML rules
├── diagrams/
│   ├── circuit_diagram.png        # Wiring connections
│   ├── ml_decision_tree.png       # Visual representation of AI logic
│   ├── web_dashboard.png          # UI Screenshot
│   └── hardware_setup.jpg         # Photo of the physical device
└── README.md                      # Project Documentation
