# S.A.L.A.M.I.N.-Spatiotemporal-Autonomous-Laboratory-Aggregating-Microplastic-Informatics-Network-
S.A.L.A.M.I.N. is a self-driving, floating robotic laboratory that actively hunts for microplastics, measures 19 different environmental parameters, and streams that data to a 3D predictive web dashboard to prove how plastic pollution poisons the air and water.
1. The Autonomous Laboratory (The Hardware)
Instead of a human rowing a boat and scooping water into jars, you deploy an Unmanned Surface Vehicle (USV). Using a flight controller (like a Pixhawk or ESP32 with GPS), the boat drives itself back and forth across a river or lake in a precise grid pattern.
2. The Microplastic Assessment (The Edge AI)
As the boat drives, it acts as a mobile chemistry lab. A small pump pulls river water into a dark chamber inside the hull. An automated system injects a chemical dye (Nile Red), causing any microplastics to fluoresce (glow) under a UV light. A localized edge-computer (like a Raspberry Pi) uses OpenCV machine learning to take a picture, instantly count the glowing plastic particles, and flush the chamber for the next test.
3. Aggregating Informatics (Your 19-Sensor Array)
At the exact same moment the camera is counting the plastic, your massive sensor array is reading the environment:

The Hydro-Sensors (pH, TDS, Turbidity, Water Temp): These determine the toxicity and physical state of the water.

The Aero-Sensors (BME680 - VOCs, Air Temp, Humidity, Pressure): Mounted on the roof of the boat, this acts as the "nose" of the system, sniffing the air right above the polluted water.
4. The Spatiotemporal Network (The 4D Digital Twin)
The boat packages the plastic count, the 19 environmental parameters, and its exact GPS location, beaming it via telemetry (LoRa or Wi-Fi) to your web backend (built on a fast stack like Next.js, React, and Supabase).

This is where the "Spatiotemporal 4D Twin" comes to life. The dashboard renders a 3D map of the river. As the boat moves, a colored heat map is drawn in real-time. Because it is 4D (incorporating time and physics), the algorithm uses the water current and BME680 wind data to predict where that toxic plastic plume will float tomorrow.
