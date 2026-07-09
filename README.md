# S.A.L.A.M.I.N.-Spatiotemporal-Autonomous-Laboratory-Aggregating-Microplastic-Informatics-Network- (Incomplete)
S.A.L.A.M.I.N. is a self-driving, floating robotic laboratory that actively hunts for microplastics, measures 19 different environmental parameters, and streams that data to a 3D predictive web dashboard to prove how plastic pollution poisons the air and water.

1. The Autonomous Laboratory (The Hardware)
Instead of a human rowing a boat and scooping water into jars, you deploy an Unmanned Surface Vehicle (USV). Using a flight controller (like a Pixhawk or ESP32 with GPS), the boat drives itself back and forth across a river or lake in a precise grid pattern.

2. The Microplastic Assessment (The Edge AI)
As the boat drives, it acts as a mobile chemistry lab. A small pump pulls river water into a dark chamber inside the hull. An automated system injects a chemical dye (Nile Red), causing any microplastics to fluoresce (glow) under a UV light. A localized edge-computer (like a Raspberry Pi) uses OpenCV machine learning to take a picture, instantly count the glowing plastic particles, and flush the chamber for the next test.

3. Aggregating Informatics (Your 19-Sensor Array)
At the exact same moment the camera is counting the plastic, your massive sensor array is reading the environment:

//---The Hydro-Sensors (pH, TDS, Turbidity, Water Temp): These determine the toxicity and physical state of the water. The Aero-Sensors (BME680 - VOCs, Air Temp, Humidity, Pressure): Mounted on the roof of the boat, this acts as the "nose" of the system, sniffing the air right above the polluted water.

4. The Spatiotemporal Network (The 4D Digital Twin)
The boat packages the plastic count, the 19 environmental parameters, and its exact GPS location, beaming it via telemetry (LoRa or Wi-Fi) to your web backend (built on a fast stack like Next.js, React, and Supabase).

This is where the "Spatiotemporal 4D Twin" comes to life. The dashboard renders a 3D map of the river. As the boat moves, a colored heat map is drawn in real-time. Because it is 4D (incorporating time and physics), the algorithm uses the water current and BME680 wind data to predict where that toxic plastic plume will float tomorrow.


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


The project is being developed in five distinct phases. The physical sensor array and telemetry gateway have been successfully deployed. Active development is currently focused on building the 4D Spatiotemporal Digital Twin, followed by the Edge-AI vision systems and final system integration.

----- Phase 1: Core Telemetry & Offline Gateway (Completed) ------
The foundational hardware and data-routing pipeline. This ensures the unmanned laboratory can accurately read the environment and securely transmit data regardless of internet availability.

[x] 19-Parameter Environmental Array Integration: Hardware implementation of pH, TDS, Turbidity, and BME680 (VOC, Temp, Humidity, Pressure) sensors.

[x] LoRa Radio Telemetry Pipeline: Point-to-point data transmission between the Unmanned Surface Vehicle (USV) and the Ground Station.

[x] "Black Box" Offline Data Logging: Dynamic LittleFS memory partitioning for local CSV storage and generation of the offline HTML command center.

[x] Dual-Cloud Uplink: Asynchronous HTTP/HTTPS routing to Firebase Realtime Database (for the live dashboard) and Google Sheets (for the hard data archive).
Remarks: 


------ Phase 2: 4D Spatiotemporal Digital Twin (In Progress) -----

The development of the web-based command center to visualize the correlations between the physical and chemical data transmitted by the Ground Station.

[ ] Full-Stack Repository Setup: Initialization of the Next.js, React, and Tailwind CSS web environment.

[ ] Database Architecture: Structuring the Supabase backend to handle historical correlation data while listening to Firebase for live telemetry updates.

[ ] Spatiotemporal Data Visualization: Translating raw coordinate and telemetry data into a 3D mapping interface to track the USV's path.

[ ] Predictive Modeling Engine: Developing the correlation dashboard to actively track spikes in VOC off-gassing (BME680) against high-density microplastic zones.
Remarks:


----- Phase 3: Edge-AI Vision Systems (Upcoming) -----

The integration of computer vision to allow the system to actively navigate safely and hunt for microscopic plastics using a dual-camera architecture.

[ ] Mobile-Computed Surface Detection: Deployment of a smartphone acting as the primary forward-facing optical sensor. Utilizing onboard mobile processing to run object detection models (e.g., YOLOv8) for obstacle avoidance and macro-debris mapping.

[ ] Flow-Through Dark Chamber Engineering: Designing and sealing an internal, light-proof water sampling chamber equipped with an automated intake pump and a UV/Blue LED excitation light array.

[ ] Nile Red Fluorescence Capture: Integrating an internal secondary camera fitted with an emission filter to isolate and photograph Nile Red-stained microplastics in a controlled, zero-sunlight environment.

[ ] Edge-Computed Particle Counting: Training the local machine learning model to identify and count the fluorescing plastic particles from the dark chamber before flushing the sample.

[ ] Unified Payload Structuring: Merging the mobile object detection data and the internal microplastic count with the existing 19-parameter environmental JSON payload for LoRa transmission.
Remarks:

----- Phase 4: Overall System Interconnectivity & Code Revision (Upcoming) -----

The critical synchronization phase to ensure the hardware, edge-processing, and web dashboard communicate flawlessly before water deployment.

[ ] Subsystem Synchronization: Merging the Edge-AI vision data stream with the ESP32/LoRa environmental telemetry pipeline.

[ ] Latency & Payload Optimization: Refining the JSON payload size and transmission frequency to prevent LoRa bottlenecks or Firebase rate limits.

[ ] Fail-Safe Testing: Simulating connection drops to verify the Ground Station's offline LittleFS backup and auto-reconnect logic.

[ ] Code Refactoring & Cleanup: Finalizing comments, removing debug statements, and formatting the codebase for the final thesis repository.
Remarks:


----- Phase 5: Field Testing & Deployment (Future) ------

[ ] Controlled Pool Testing: Calibrating the vision system, sensor arrays, and boat thrusters in a closed, controlled water environment.

[ ] Live Riverine Deployment: Executing the autonomous grid-search algorithm in a live aquatic system.

[ ] Correlation Analysis & Defense Documentation: Extracting the Google Sheets data logs to statistically prove the Plastisphere Effect and VOC leaching for the final thesis defense.
Reamkes:
