# Snapdragon 662 / Hexagon V66 support plan

## Executive summary

The Redmi 9T's Snapdragon 662 (SM6115) contains a Hexagon V66 **DSP**. V66 is not an HTP generation. The current PocketPal Hexagon path is an HTP backend for newer Qualcomm devices, so identifying the SM6115 as "Hexagon supported" and selecting HTP would be incorrect.

The first engineering task is to discover which Android NNAPI devices the installed ROM exposes. On older V66 consumer phones, NNAPI may be the only vendor-authorized way for third-party apps to reach the accelerator. Device discovery is not proof of successful DSP execution; a model must be compiled for a specific accelerator and run without CPU fallback to prove that.

## Verified hardware facts

- Xiaomi lists Redmi 9T configurations with 4 GB or 6 GB LPDDR4X, Snapdragon 662, Kryo 260 CPU and Adreno 610.
- Qualcomm's supported-device table maps Snapdragon 662 / SM6115 to Hexagon architecture V66.
- Qualcomm's current QAIRT setup table distinguishes **DSP V66** from HTP and specifies Hexagon SDK 4.1.0 plus Hexagon Tools 8.4.06 for V66. The SDK 6.x / Tools 19.x workflow in this repository targets newer HTP generations, not V66.
- Qualcomm's Snapdragon 662 product brief lists OpenCL 2.0 and Vulkan 1.1 for Adreno 610. That describes API capability, not proof that a particular Android driver works with llama.cpp.
- Upstream llama.cpp's OpenCL documentation warns that Adreno A6xx GPUs in phones are often limited by old drivers/compilers. The actual Redmi 9T driver must be tested.

References:
- https://www.mi.com/global/product/redmi-9t/specs/
- https://docs.qualcomm.com/bundle/publicresource/topics/80-63442-10/linux_setup.html
- https://www.qualcomm.com/content/dam/qcomm-martech/dm-assets/documents/snapdragon-662-mobile-platform-product-brief.pdf
- https://github.com/ggml-org/llama.cpp/blob/master/docs/backend/OPENCL.md

## Important Android firmware limitation

Qualcomm's AI Hub Apps issue #46 contains reports from Snapdragon 662 / V66 phones (Moto G Power and Galaxy Tab A7) where QNN DSP delegate initialization failed while loading the V66 skeleton library. A Qualcomm engineer explained that older consumer Android vendors commonly authorize NNAPI access but restrict direct QNN access; the application developer cannot necessarily override that vendor policy.

This does **not** prove that every Redmi 9T ROM blocks QNN. It means we must test the actual ROM and must not ship a workaround that assumes direct FastRPC/QNN access.

Reference:
- https://github.com/qualcomm/ai-hub-apps/issues/46

## Audit of this fork at the time this plan was written

The `main` branch was identical to upstream PocketPal at the inspected commit. The existing Hexagon implementation:

- discovers llama.cpp devices whose names start with `HTP`;
- only maps modern HTP generations (V69/V73/V75/V79/V81);
- packages HTP kernel assets for V73/V75/V79/V81/V85;
- builds the Hexagon-enabled llama.rn variant using ARMv8.2 + dotprod + i8mm;
- does not contain a V66 DSP kernel, V66 QAIC interface, or NNAPI backend for GGUF.

Therefore Snapdragon 662 must **not** be offered the existing HTP device option. GGUF loading through llama.cpp CPU remains separate from Hexagon acceleration.

## Implementation sequence

### Phase 1 — inspect the exact Android runtime (safe, first)

1. Enumerate NNAPI devices using the public Android NDK device-discovery API (Android 10 / API 29+).
2. Report each device's exact name, type, driver version and feature level.
3. Keep discovery separate from backend selection. A Qualcomm accelerator entry is only a lead; it does not prove which DSP domain or operations it supports.
4. Capture the installed ROM build, Android API level, SoC identifier, CPU instruction flags, GPU renderer and NNAPI inventory.

Android's device-discovery APIs are documented at:
- https://developer.android.com/ndk/guides/neuralnetworks#device-discovery-and-assignment

### Phase 2 — prove DSP execution with a small test graph

1. Build a small quantized matrix-multiply / fully-connected NNAPI test.
2. Query `ANeuralNetworksModel_getSupportedOperationsForDevices`.
3. Compile explicitly for the candidate Qualcomm accelerator with `ANeuralNetworksCompilation_createForDevices`, excluding the NNAPI reference CPU.
4. Run repeated inferences and collect Android `ExecutionPlan` logs, latency and power/thermal observations.
5. If explicit accelerator compilation or execution fails, record the exact status/log and keep the app on CPU/GPU. Never report CPU fallback as DSP success.

### Phase 3 — determine whether GGUF can benefit

NNAPI does not load GGUF. PocketPal's LLM path is llama.cpp/GGML, so a real solution needs an adapter/backend:

- either lower selected GGML operations (initially quantized linear/matmul) into NNAPI subgraphs, with persistent weights/KV state and explicit accelerator assignment;
- or implement a V66-specific GGML backend using the V66 DSP toolchain, HVX kernels and FastRPC, if the firmware permits the required DSP process/skeleton loading.

Both approaches need on-device performance measurements. Transferring activations between CPU and DSP can erase the benefit, and LLM attention/KV-cache operations are not a simple image-model graph. A one-off TFLite benchmark is not proof that full Qwen/Llama inference can be accelerated.

### Phase 4 — use the rest of the SoC efficiently

- CPU: retain a safe ARMv8-A baseline and dispatch optional CPU kernels only when `/proc/cpuinfo` confirms their feature bits. Do not compile the baseline with dotprod or i8mm assumptions.
- Adreno 610: probe the actual OpenCL platform/device/version and Vulkan driver. Only expose GPU inference after a GGML smoke test succeeds on this exact driver. Qualcomm's API-version claim alone is insufficient.
- Memory: select context length, batch size, mmap and model quantization using the actual 4 GB / 6 GB SKU and current available memory.
- Thermals: benchmark sustained speed as well as a short peak; do not keep all CPU cores at maximum continuously by default.

## Success criteria

A Redmi 9T build is not "Hexagon accelerated" until a repeatable test demonstrates:
- the selected NNAPI/QNN device is the Qualcomm accelerator, not `nnapi-reference` or CPU;
- supported model operations are explicitly assigned to that accelerator;
- the logs show the target device executing the graph;
- output matches a CPU reference within the chosen quantization tolerance;
- end-to-end latency is better than the CPU/GPU baseline after transfer and setup overhead.

Until those conditions pass, PocketPal should label V66 as **hardware present / acceleration unverified**, not as a supported GGUF backend.
