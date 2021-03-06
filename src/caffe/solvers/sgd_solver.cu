#include "caffe/backend/device.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/sgd_solvers.hpp"

namespace caffe {

template<typename Dtype>
void SGDSolver<Dtype>::GenerateProgram() {
  this->device_program_ = this->device_->CreateProgram();
  stringstream ss;

  ss << this->device_program_->setup();
  ss << this->device_program_->template define_type<Dtype>("Dtype");

  KernelArgs args;
  args.push_back(this->device_program_->template create_kernel_arg<uint_tp>("n",
                    KERNEL_ARG_CONST));
  args.push_back(this->device_program_->template create_kernel_arg<Dtype>("g",
                    KERNEL_ARG_GLOBAL_MEM));
  args.push_back(this->device_program_->template create_kernel_arg<Dtype>("h",
                    KERNEL_ARG_GLOBAL_MEM));
  args.push_back(this->device_program_->template create_kernel_arg<Dtype>(
                    "momentum", KERNEL_ARG_CONST));
  args.push_back(this->device_program_->template create_kernel_arg<Dtype>(
                    "local_rate", KERNEL_ARG_CONST));
  ss << this->device_program_->function("SGDUpdate", args);
  ss << this->device_program_->kernel_loop("uint_tp", "i", "n");
  ss << "g[i] = h[i] = momentum * h[i] + local_rate * g[i];" << std::endl;
  ss << "}" << std::endl;
  ss << "}" << std::endl;

  this->device_program_->set_source(ss.str());
  this->device_program_->Compile(true, true);
}

template <typename Dtype>
void sgd_update_gpu(Device* dev, DeviceProgram* dev_prog,
                    uint_tp n, vptr<Dtype> g, vptr<Dtype> h,
                    Dtype momentum, Dtype local_rate) {
  shared_ptr<DeviceKernel> kernel = dev_prog->GetKernel("SGDUpdate");
  kernel->add_arg(&n);
  kernel->add_arg(&g);
  kernel->add_arg(&h);
  kernel->add_arg(&momentum);
  kernel->add_arg(&local_rate);

  vector<size_t> work_size(1, n);
  vector<size_t> group;
  vector<size_t> local;
  dev->get_threads(&work_size, &group, &local, kernel.get(), true);
  kernel->Execute(group, local);
}

#ifdef USE_HALF
template void sgd_update_gpu<half_fp>(Device* dev,
                 DeviceProgram* dev_prog, uint_tp n, vptr<half_fp> g,
                 vptr<half_fp> h, half_fp momentum,
                 half_fp local_rate);

#endif  // USE_HALF
#ifdef USE_SINGLE
template void sgd_update_gpu<float>(Device* dev,
                 DeviceProgram* dev_prog, uint_tp n, vptr<float> g,
                 vptr<float> h, float momentum,
                 float local_rate);
#endif  // USE_SINGLE
#ifdef USE_DOUBLE
template void sgd_update_gpu<double>(Device* dev,
                 DeviceProgram* dev_prog, uint_tp n, vptr<double> g,
                 vptr<double> h, double momentum,
                 double local_rate);
#endif  // USE_DOUBLE

INSTANTIATE_CLASS_1T_GUARDED(SGDSolver, (half_fp)(float)(double))

}  // namespace caffe
