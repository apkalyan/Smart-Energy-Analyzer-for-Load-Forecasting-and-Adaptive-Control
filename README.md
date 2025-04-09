# Smart-Energy-Analyzer-for-Load-Forecasting-and-Adaptive-Control

The main objective of this project is to develop and deploy an intelligent real-time monitoring
and control system for domestic or small industrial power grids. The system should ensure
energy efficiency, load prioritization, and predictive control by integrating embedded
communication, edge computing, and Artificial Intelligence(AI)-based forecasting.
Key goals include:
1. Real-Time Power Monitoring:
Consistently monitor power usage from many end nodes and aggregate the readings at a
central edge node. Power readings will be sampled at regular intervals (every 5 seconds) to
provide high-resolution monitoring for sound decision-making.
2. Load Classification and Control:
Implement functionality to classify electrical loads as essential and non-essential.
Depending on the overall power consumption and a user-specified threshold, the system will
control relays for non-necessary loads automatically or manually to avoid overconsumption
or grid overload.
3. ESP-NOW for End-to-Edge Communication:
Use ESP-NOW, a low-latency and lightweight connectionless protocol, to enable effective
one-way or two-way communication between ESP-based end nodes and the central ESPbased edge node. The protocol is chosen following extensive testing that validated its
reliability, speed, and applicability for short-range, peer-to-peer communication in resourcelimited environments.
4. MQTT for Server Communication:
Employ the Message Queuing Telemetry Transport(MQTT) protocol to transmit aggregated
data from the edge node to a central server or application layer. MQTT also facilitates twoway communication for remote relay control, threshold setup, and system monitoring.
Topic-based publish-subscribe architecture ensures modularity and scalability of the
communication system.
5. Forecasting Power Consumption Using LSTM:
Long Short-Term Memory (LSTM) neural network to predict short-term future power
demand using recent historical data. The system periodically executes on the server to
predict the future load and invoke proactive load-shedding policies if forecasted power is
beyond safe limits.
6. Interactive User Interface(UI) Dashboard:
A user friendly PyQt-based dashboard to: Display real-time total and per-node power
consumption plots, Visualize LSTM-based predicted values, Offer relay control for every
node (manual and automatic), Display node essentiality status and enable user toggling,
Dynamically adjust the load threshold via a slider-based interface, Send alerts upon power
exceeding limits or upon load-shedding.
