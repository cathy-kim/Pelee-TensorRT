# Pelee-TensorRT
**To accelerate Pelee with tensorRT**

**TensorRT-Pelee can run 80fps on jetson tx2(FP32)**

---

**Requierments:**

1.tensorRT4

2.cudnn7

3.Opencv

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



*If want to decrease the time cost of "imread",you could rebuild OpenCV[https://github.com/jetsonhacks/buildOpenCVTX2]*

*Added producer-consumer*



**The bug has been fixed**

![image](testPic/test1.png)
