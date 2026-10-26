<div align="center">

  ### beach weather probe
  Use a kite to measure **wind force**, instead of using anemometer.

</div>

-----
### 📜️ how it works

```mermaid
flowchart TB
    A[load cell] --> B[XIAO esp32c3]
    B -.->|wifi| C[access point]
    C -.->|4g/5g| D[**home server**]
    D -->|influxdb| E[grafana]
```

### 📷️ screenshots
<img width="3264" height="2448" alt="IMG_20261026_102735" src="https://github.com/user-attachments/assets/d6d34d07-9a1a-4be6-8280-7fd15b1a3e33" />
<img width="3264" height="2448" alt="IMG_20261026_102744" src="https://github.com/user-attachments/assets/42877aef-34af-4656-b591-ae310f658faf" />
<img width="2448" height="3264" alt="IMG_20261026_102817" src="https://github.com/user-attachments/assets/dc87108b-4b9c-4fe7-bd8a-c458077c4d63" />
