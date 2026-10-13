# 3D Printed Mechanical Mounts and Enclosures

This folder contains 3D-printable CAD files (`.stl`) and dimensional drawings for the mechanical fixtures on the fluid-handling rig.

## 1. Component Index

| Component | STL File | Quantity | Key Dimensions | Purpose |
|---|---|:---:|---|---|
| SMPS Clamp | [`stl/smps-clamp.stl`](stl/smps-clamp.stl) | 1 | ~200 × 100 mm, R10 fillets, 17×3 mm slots | Secures 12 V SMPS with ventilation windows |
| PCB Standoff Foot | [`stl/pcb-foot.stl`](stl/pcb-foot.stl) | 4 | Ø7.0 mm OD pillar, Ø3.0 mm bore, 7.0 mm base | Mounts and isolates the control circuit board |
| Pipe Guide Clamp | [`stl/pipe-clamp.stl`](stl/pipe-clamp.stl) | Multiple | 8–10 mm bore, 90° mounting bracket | Secures 8 mm fluid tubing along chassis |

## 2. Technical Drawings and Renders

Drawings and visual renders are located in [`images/`](images/).

- **SMPS Clamp Drawing:** [`images/smps_clamp_dimensions.png`](images/smps_clamp_dimensions.png)
- **SMPS Clamp 3D Model:** [`images/smps_clamp_cad_3d.png`](images/smps_clamp_cad_3d.png)
- **PCB Standoff Drawing:** [`images/pcb_clamp_dimensions.png`](images/pcb_clamp_dimensions.png)
- **PCB Standoff 3D Model:** [`images/pcb_clamp_cad_3d.png`](images/pcb_clamp_cad_3d.png)
- **Pipe Clamp Model:** [`images/pipe_clamp_cad_3d.png`](images/pipe_clamp_cad_3d.png)

## 3. Slicer and 3D Print Settings

| Parameter | Recommended Value | Engineering Reason |
|---|---|---|
| Material | PLA or PETG | PETG provides resistance to liquid splashes |
| Layer Height | 0.20 mm | Balanced speed and dimensional accuracy |
| Infill Density | 30% to 45% | Grid or Gyroid pattern for rigidity |
| Wall Perimeters | 3 to 4 perimeters | Prevents cracking when torquing M3 screws |
| Support Material | None | Orient parts flat on build plate |
