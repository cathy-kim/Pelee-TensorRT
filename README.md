# Pelee-TensorRT

**Accelerate Pelee with tensorRT**
**TensorRT-Pelee can run 80fps on jetson tx2(FP32)**

---

**Performance:**
72 FPS, 13.8 ms (FP32)


**Requierments:**

1.TensorRT4

2.CUDA 9.0

3.cudnn7

4.Jetpack 3.3(on TX2) 

---

**Run:**

```shell
cmake .
make
./build/bin/pelee
```

---

**Reference:**

https://github.com/saikumarGadde/tensorrt-ssd-easy
https://github.com/chuanqi305/MobileNet-SSD 
https://github.com/Ghustwb/MobileNet-SSD-TensorRT

---

**TODO:**
- [ ] FP16 Implementation 
- [ ] Change Custom layers IPlugin to IPluginExt




**The bug has been fixed**

![image](testPic/test.png)
