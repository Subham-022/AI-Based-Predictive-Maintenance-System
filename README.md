# AI-Based-Predictive-Maintenance-System
AI-Based Predictive Maintenance System for Motor Health Monitoring .
<br>
This project presents a real-time, IoT-enabled predictive maintenance system designed to monitor and analyze the operational health of an electric motor using multiple sensors and intelligent scoring techniques.
<br>
Leveraging the capabilities of the ESP32 microcontroller, the system continuously collects and processes environmental and operational data—including temperature, humidity, current consumption, and vibration—to evaluate motor performance and detect potential faults before failure occurs.
<br>
Key Features:
<br>
📡 Real-Time Monitoring
Continuously captures and displays live motor parameters using an embedded web server on ESP32.
<br>
🧠 Intelligent Health Scoring
Computes a dynamic Motor Health Score (0–100) using weighted analysis of temperature, current, and vibration data.
<br>
🔗 Multi-Sensor Integration
Combines data from DHT22, ACS712, and MPU6050 for comprehensive system monitoring.
<br>
⚠️ Predictive Fault Detection
Identifies abnormal conditions early and triggers alerts to prevent system failure.
<br>
🖥️ Interactive Dashboard
User-friendly interface for real-time monitoring, motor control, and sensor calibration.
<br>
📊 Live Data Visualization
Displays dynamic trends for better analysis and decision-making.
<br>
🔔 Smart Alert System
Buzzer and LED indicators with intelligent mute and reset functionality.
<br>
⏱️ Runtime Monitoring
Tracks motor usage duration for maintenance planning.
<br>
⚡ Edge Computing Enabled
Performs all computations locally on ESP32 for faster response and reliability.
<br>
🔧 Scalable Architecture
Easily extendable with additional sensors, cloud integration, or AI models.
<br>
Technologies Used:
<br>
Hardware: ESP32, DHT22, ACS712, MPU6050, 300 RPM DC 12V Gear Motor, Buzzer, LED, L298N Motor Driver Module, 12V Adapter
Software: Arduino IDE 
Networking: Wi-Fi + HTTP Web Server
Frontend: HTML, CSS, JavaScript (embedded dashboard)
<br>
Applications:
<br>
Industrial motor monitoring
Smart factories (Industry 4.0)
Predictive maintenance systems
Remote equipment diagnostics
