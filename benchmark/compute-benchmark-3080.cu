/* Modified 2026-10-19/20 for Windows GPU UNLOCK; changes documented in CHANGES.md. */ // Reproducible ordinary Windows CUDA workload. No unlock, reset, or driver changes.
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <cublas_v2.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
static void cuda_check(cudaError_t e){if(e!=cudaSuccess){fprintf(stderr,"CUDA: %s\n",cudaGetErrorString(e));exit(1);}}
static void blas_check(cublasStatus_t e){if(e!=CUBLAS_STATUS_SUCCESS){fprintf(stderr,"cuBLAS error %d\n",(int)e);exit(1);}}
template<class T> __global__ void ones(T *p,size_t n){size_t i=blockIdx.x*(size_t)blockDim.x+threadIdx.x;if(i<n)p[i]=T(1.0f);}
static void measure(cublasHandle_t handle,int kind) {
  int n=4096;size_t count=(size_t)n*n;
  size_t input_bytes=count*(kind==0?sizeof(float):sizeof(__half));
  size_t output_bytes=count*sizeof(float);
  void *a,*b,*c;cuda_check(cudaMalloc(&a,input_bytes));cuda_check(cudaMalloc(&b,input_bytes));cuda_check(cudaMalloc(&c,output_bytes));
  unsigned blocks=(unsigned)((count+255)/256);
  if(kind==0){ones<<<blocks,256>>>((float *)a,count);ones<<<blocks,256>>>((float *)b,count);}
  else if(kind==1){ones<<<blocks,256>>>((__half *)a,count);ones<<<blocks,256>>>((__half *)b,count);}
  else {ones<<<blocks,256>>>((__nv_bfloat16 *)a,count);ones<<<blocks,256>>>((__nv_bfloat16 *)b,count);}
  cuda_check(cudaGetLastError());cuda_check(cudaDeviceSynchronize());
  blas_check(cublasSetMathMode(handle,kind?CUBLAS_TENSOR_OP_MATH:CUBLAS_PEDANTIC_MATH));
  auto gemm=[&](){
    float alpha=1,beta=0;
    if(kind==0)blas_check(cublasSgemm(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,(float *)a,n,(float *)b,n,&beta,(float *)c,n));
    else {cudaDataType_t type=kind==1?CUDA_R_16F:CUDA_R_16BF;
      blas_check(cublasGemmEx(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,a,type,n,b,type,n,&beta,c,CUDA_R_32F,n,CUBLAS_COMPUTE_32F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));}
  };
  auto warm_start=std::chrono::steady_clock::now();
  do {gemm();cuda_check(cudaDeviceSynchronize());}while(std::chrono::duration<double>(std::chrono::steady_clock::now()-warm_start).count()<2.0);
  cudaEvent_t start,stop;cuda_check(cudaEventCreate(&start));cuda_check(cudaEventCreate(&stop));
  std::vector<float> samples;
  for(int i=0;i<30;i++) {
    cuda_check(cudaEventRecord(start));gemm();cuda_check(cudaEventRecord(stop));cuda_check(cudaEventSynchronize(stop));
    float milliseconds;cuda_check(cudaEventElapsedTime(&milliseconds,start,stop));samples.push_back(milliseconds);
  }
  // Exact mathematical reference: every element of an all-ones product equals n.
  float first,last;cuda_check(cudaMemcpy(&first,c,4,cudaMemcpyDeviceToHost));cuda_check(cudaMemcpy(&last,(float *)c+count-1,4,cudaMemcpyDeviceToHost));if(first!=n || last!=n){fprintf(stderr,"Matrix result mismatch\n");exit(1);}
  std::sort(samples.begin(),samples.end());double median=(samples[14]+samples[15])*0.5;
  printf("%s{\"name\":\"%s\",\"n\":%d,\"samples\":30,\"warmup_seconds\":2,\"median_ms\":%.6f,\"min_ms\":%.6f,\"max_ms\":%.6f,\"gflops\":%.3f,\"reference_checked\":true}",kind?",":"",kind==0?"fp32_pedantic":kind==1?"fp16_tensor_fp32_accumulate":"bf16_tensor_fp32_accumulate",n,median,samples.front(),samples.back(),2.0*n*n*n/(median*1e6));
  cuda_check(cudaEventDestroy(start));cuda_check(cudaEventDestroy(stop));cuda_check(cudaFree(a));cuda_check(cudaFree(b));cuda_check(cudaFree(c));
}
int main(int argc, char **argv) {
  if(argc==2 && !strcmp(argv[1],"--inspect-only")) {
    int runtime=0,major=0,minor=0,patch=0;
    cuda_check(cudaRuntimeGetVersion(&runtime));
    blas_check(cublasGetProperty(MAJOR_VERSION,&major));
    blas_check(cublasGetProperty(MINOR_VERSION,&minor));
    blas_check(cublasGetProperty(PATCH_LEVEL,&patch));
    printf("{\"cuda_runtime\":%d,\"cublas_major\":%d,\"cublas_minor\":%d,\"cublas_patch\":%d,\"gpu_workload_run\":false}\n",runtime,major,minor,patch);
    return 0;
  }
  if(argc!=1){fprintf(stderr,"Only --inspect-only or no arguments are supported\n");return 2;}
  int devices=0;cuda_check(cudaGetDeviceCount(&devices));int selected=-1;cudaDeviceProp properties={};
  for(int i=0;i<devices;i++) {cudaDeviceProp p;cuda_check(cudaGetDeviceProperties(&p,i));
    if(!strcmp(p.name,"NVIDIA GeForce RTX 3080") && !p.pciDomainID && p.pciBusID==1 && !p.pciDeviceID){if(selected!=-1)return 2;selected=i;properties=p;}}
  if(selected<0){fprintf(stderr,"Exact RTX 3080 at 0000:01:00.0 not found\n");return 2;}
  cuda_check(cudaSetDevice(selected));cublasHandle_t handle;blas_check(cublasCreate(&handle));
  int runtime=0,driver=0,blas=0;cuda_check(cudaRuntimeGetVersion(&runtime));cuda_check(cudaDriverGetVersion(&driver));blas_check(cublasGetVersion(handle,&blas));
  printf("{\"gpu\":\"%s\",\"pci\":\"0000:01:00.0\",\"compute_capability\":\"%d.%d\",\"cuda_runtime\":%d,\"cuda_driver_api\":%d,\"cublas\":%d,\"benchmarks\":[",properties.name,properties.major,properties.minor,runtime,driver,blas);
  for(int kind=0;kind<3;kind++)measure(handle,kind);
  blas_check(cublasDestroy(handle));puts("]}");return 0;
}
