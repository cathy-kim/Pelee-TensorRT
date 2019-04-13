#ifndef __PLUGIN_LAYER_H__
#define __PLUGIN_LAYER_H__
#include <memory>

#include <cassert>
#include <iostream>
#include <cudnn.h>
#include <cstring>
#include <cuda_runtime.h>
#include <cublas_v2.h>

#include "NvCaffeParser.h"
#include "NvInfer.h"
#include "NvInferPlugin.h"
#include "NvUtils.h"
//#include "fp16.h"

#define CHECK(status)                                                                                           \
    {                                                                                                                           \
        if (status != 0)                                                                                                \
        {                                                                                                                               \
            std::cout << "Cuda failure: " << cudaGetErrorString(status) \
                      << " at line " << __LINE__                                                        \
                      << std::endl;                                                                     \
            abort();                                                                                                    \
        }                                                                                                                               \
    }


using namespace nvinfer1;
using namespace nvcaffeparser1;
using namespace plugin;

static const int TIMING_ITERATIONS = 1000; 




enum FunctionType
{
    SELECT=0,
    SUMMARY
};

void cudaSoftmax(int n, int channels,  float* x, float*y);
//void cudaSoftmax(int n, int channels, __half* x, __half* y);



class bboxProfile {
public:
    bboxProfile(float4& p, int idx): pos(p), bboxNum(idx) {}

    float4 pos;
    int bboxNum = -1;
    int labelID = -1;

};

class tagProfile
{
public:
    tagProfile(int b, int l): bboxID(b), label(l) {}
    int bboxID;
    int label;
};

//SSD Reshape layer : shape{0,-1,21}
template<int OutC>
// @TODO: I think the OutC is the Out Channels and it is equal to 21.
class Reshape : public IPlugin
{
public:
    Reshape()
    {
    }
    Reshape(const void* buffer, size_t size)
    {
        assert(size == sizeof(mCopySize));
        mCopySize = *reinterpret_cast<const size_t*>(buffer);
    }
    int getNbOutputs() const override
    {
        return 1;
    }
    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) override
    {
        assert(nbInputDims == 1);
        assert(index == 0);
        assert(inputs[index].nbDims == 3);
        assert((inputs[0].d[0])*(inputs[0].d[1]) % OutC == 0);

        // @TODO: Understood this.
        return DimsCHW( inputs[0].d[0] * inputs[0].d[1] / OutC, OutC, inputs[0].d[2]);
    }

    int initialize() override { return 0; }
    void terminate() override {}

    size_t getWorkspaceSize(int) const override
    {
        // @TODO: 1 is the batch size.
        return mCopySize*1;
    }

    // currently it is not possible for a plugin to execute "in place". Therefore we memcpy the data from the input to the output buffer
    int enqueue(int batchSize, const void*const *inputs, void** outputs, void* workspace, cudaStream_t stream) override
    {  	
   	if(mDataType == DataType::kFLOAT){ // FP32 
	CHECK(cudaMemcpyAsync(outputs[0], inputs[0] , mCopySize * batchSize, cudaMemcpyDeviceToDevice, stream)); 
	} 
	else{  //FP16 
	CHECK(cudaMemcpyAsync(
		reinterpret_cast<__half*>(outputs[0]), 
		reinterpret_cast<const __half*>(inputs[0]), mCopySize * batchSize, 
		cudaMemcpyDeviceToDevice, stream)); 
	}
        //CHECK(cudaMemcpyAsync(outputs[0], inputs[0] , mCopySize * batchSize, cudaMemcpyDeviceToDevice, stream));
        return 0;
    }
    size_t getSerializationSize() override
    {
        return sizeof(mCopySize);
    }
    void serialize(void* buffer) override
    {
        *reinterpret_cast<size_t*>(buffer) = mCopySize;
    }
    void configure(const Dims*inputs, int nbInputs, const Dims* outputs, int nbOutputs, int)	override
    {
        mCopySize = inputs[0].d[0] * inputs[0].d[1] * inputs[0].d[2] * sizeof(float);
    }

protected:
    size_t mCopySize;
    DataType mDataType{DataType::kFLOAT};

};

//Softmax layer.TensorRT softmax only support cross channel
class SoftmaxPlugin : public IPlugin
{
    //You need to implement it when softmax parameter axis is 2.
public:
    int initialize() override { return 0; }
    inline void terminate() override {}

    SoftmaxPlugin(){}
    SoftmaxPlugin( const void* buffer, size_t size)
    {
        assert(size == sizeof(mCopySize));
        mCopySize = *reinterpret_cast<const size_t*>(buffer);
    }
    inline int getNbOutputs() const override
    {
        //@TODO:  As the number of outputs are only 1, because there is only layer in top.
        return 1;
    }
    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) override
    {
        assert(nbInputDims == 1);
        assert(index == 0);
        assert(inputs[index].nbDims == 3);
//        assert((inputs[0].d[0])*(inputs[0].d[1]) % OutC == 0);

        // @TODO: Understood this.
        return DimsCHW( inputs[0].d[0] , inputs[0].d[1] , inputs[0].d[2] );
    }

    size_t getWorkspaceSize(int) const override
    {
        // @TODO: 1 is the batch size.
        return mCopySize*1;
    }

    int enqueue(int batchSize, const void*const *inputs, void** outputs, void* workspace, cudaStream_t stream) override
    {
        //std::cout<<"flatten enqueue:"<<batchSize<<";"<< mCopySize<<std::endl;
//        CHECK(cudaMemcpyAsync(outputs[0],inputs[0],batchSize*mCopySize*sizeof(float),cudaMemcpyDeviceToDevice,stream));
        //@Seojin add fp16 inference code 
	//if(mDataType == DataType::kFLOAT){ //FP32 
	  cudaSoftmax( 8732*21, 21, (float *) *inputs, static_cast<float *>(*outputs));

	//}
	/*
	else{
	  cudaSoftmax( 8732*21, 21, (__half *) *inputs, static_cast<__half *>(*outputs));
	} 
	*/
	/*
	else{  //FP16 
	    cudaSoftmax( 8732*21, 21, 
		(__half *) *inputs, 
		static_cast<__half*>(*outputs));
	} */ 
        //cudaSoftmax( 8732*21, 21, (float *) *inputs, static_cast<float *>(*outputs));

        return 0;
    }

    size_t getSerializationSize() override
    {
        return sizeof(mCopySize);
    }
    void serialize(void* buffer) override
    {
        *reinterpret_cast<size_t*>(buffer) = mCopySize;
    }
    void configure(const Dims*inputs, int nbInputs, const Dims* outputs, int nbOutputs, int)	override
    {
        mCopySize = inputs[0].d[0] * inputs[0].d[1] * inputs[0].d[2] * sizeof(float);
    }

protected:
    size_t mCopySize;
    DataType mDataType{DataType::kFLOAT}; 

};


//SSD Flatten layer
class FlattenLayer : public IPlugin
{
public:

    FlattenLayer(){}
    FlattenLayer(const void* buffer, size_t size)
    {
        assert(size == 3 * sizeof(int));
        const int* d = reinterpret_cast<const int*>(buffer);
        _size = d[0] * d[1] * d[2];
        dimBottom = DimsCHW{d[0], d[1], d[2]};
    }

    inline int getNbOutputs() const override { return 1; };
    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) override
    {
        assert(1 == nbInputDims);
        assert(0 == index);
        assert(3 == inputs[index].nbDims);
        _size = inputs[0].d[0] * inputs[0].d[1] * inputs[0].d[2];
        return DimsCHW(_size, 1, 1);
    }

    int initialize() override
    {
        return 0;
    }
    inline void terminate() override {}

    inline size_t getWorkspaceSize(int) const override { return 0; }

    int enqueue(int batchSize, const void*const *inputs, void** outputs, void*, cudaStream_t stream) override
    {
        //std::cout<<"flatten enqueue:"<<batchSize<<";"<<_size<<std::endl;
        if(mDataType == DataType::kFLOAT){ //FP32 
	  CHECK(cudaMemcpyAsync(outputs[0],inputs[0],batchSize*_size*sizeof(float),cudaMemcpyDeviceToDevice,stream));
	} 
	else{ //FP16
	  CHECK(cudaMemcpyAsync(
		reinterpret_cast<__half*>(outputs[0]),
		reinterpret_cast<const __half*>(inputs[0]),
		batchSize*_size*sizeof(__half),
		cudaMemcpyDeviceToDevice,stream));
	}

	//CHECK(cudaMemcpyAsync(outputs[0],inputs[0],batchSize*_size*sizeof(float),cudaMemcpyDeviceToDevice,stream));
        return 0;
    }

    size_t getSerializationSize() override
    {
        return 3 * sizeof(int);
    }

    void serialize(void* buffer) override
    {
        int* d = reinterpret_cast<int*>(buffer);
        d[0] = dimBottom.c(); d[1] = dimBottom.h(); d[2] = dimBottom.w();
    }

    void configure(const Dims*inputs, int nbInputs, const Dims* outputs, int nbOutputs, int) override
    {
        dimBottom = DimsCHW(inputs[0].d[0], inputs[0].d[1], inputs[0].d[2]);
    }
protected: 
    DataType mDataType{DataType::kFLOAT}; 
    DimsCHW dimBottom;
    int _size;
};

/*
//Concat layer . TensorRT Concat only support cross channel
class ConcatPlugin : public IPlugin
{
public:
    ConcatPlugin(int axis){
     // _inputs_size = inputs_size;
     _axis = axis;  
        // ConcatPlugin(axis,flag);
    }; 
    ConcatPlugin(int axis, const void* buffer, size_t size);

    inline int getNbOutputs() const override {return 1;};
    Dims getOutputDimensions(int index, const Dims* inputs, int nbInputDims) override ;
    int initialize() override;
    inline void terminate() override;

    inline size_t getWorkspaceSize(int) const override { return 0; };
    int enqueue(int batchSize, const void*const *inputs, void** outputs, void*, cudaStream_t stream) override;

    size_t getSerializationSize() override;
    void serialize(void* buffer) override;

    void configure(const Dims*inputs, int nbInputs, const Dims* outputs, int nbOutputs, int) override;

protected:

    DimsCHW dimsConv4_3;
    DimsCHW dimsFc7;
    DimsCHW dimsConv6;
 
*/







class PluginFactory : public nvinfer1::IPluginFactory, public nvcaffeparser1::IPluginFactory
{
public:
    virtual nvinfer1::IPlugin* createPlugin(const char* layerName, const nvinfer1::Weights* weights, int nbWeights) override;
    IPlugin* createPlugin(const char* layerName, const void* serialData, size_t serialLength) override;

    void(*nvPluginDeleter)(INvPlugin*) { [](INvPlugin* ptr) {ptr->destroy(); } };

    bool isPlugin(const char* name) override;
    void destroyPlugin();

   
    //pelee
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm1_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm1_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm2_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm2_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm3_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm3_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm4_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm4_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm5_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm5_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm6_mbox_loc_perm_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm6_mbox_conf_perm_layer{ nullptr, nvPluginDeleter };

    //pelee 
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm1_mbox_priorbox_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm2_mbox_priorbox_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm3_mbox_priorbox_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm4_mbox_priorbox_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm5_mbox_priorbox_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mExt_pm6_mbox_priorbox_layer{ nullptr, nvPluginDeleter };

    //detection output layer
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mDetection_out{ nullptr, nvPluginDeleter };
    //pelee 
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStem_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage1_1_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage1_2_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage1_3_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage2_1_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage2_2_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage2_3_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage2_4_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_1_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_2_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_3_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_4_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_5_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_6_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_7_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage3_8_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_1_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_2_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_3_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_4_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_5_concat_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mStage4_6_concat_layer{ nullptr, nvPluginDeleter }; 

     std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mBox_loc_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mBox_conf_layer{ nullptr, nvPluginDeleter };
    std::unique_ptr<INvPlugin, decltype(nvPluginDeleter)> mBox_priorbox_layer{ nullptr, nvPluginDeleter };
   






    //reshape layer
    std::unique_ptr<Reshape<21>> mMbox_conf_reshape{ nullptr };
    //flatten layers
    //pelee 
    std::unique_ptr<FlattenLayer> mExt_pm1_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm1_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm2_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm2_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm3_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm3_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm4_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm4_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm5_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm5_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm6_mbox_loc_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mExt_pm6_mbox_conf_flat_layer{ nullptr };
    std::unique_ptr<FlattenLayer> mBox_conf_flat_layer{ nullptr };






    //softmax layer
    std::unique_ptr<SoftmaxPlugin> mPluginSoftmax{ nullptr };
    std::unique_ptr<FlattenLayer> mMbox_conf_flat_layer{ nullptr };


};

#endif
