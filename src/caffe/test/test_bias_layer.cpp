#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layers/bias_layer.hpp"

#include "caffe/test/test_caffe_main.hpp"
#include "caffe/test/test_gradient_check_util.hpp"

namespace caffe {

template <typename TypeParam>
class BiasLayerTest : public MultiDeviceTest<TypeParam> {
  typedef typename TypeParam::Dtype Dtype;

 protected:
  BiasLayerTest()
      : blob_bottom_(new Blob<Dtype>(2, 3, 4, 5)),
        blob_bottom_eltwise_(new Blob<Dtype>(2, 3, 4, 5)),
        blob_bottom_broadcast_0_(new Blob<Dtype>()),
        blob_bottom_broadcast_1_(new Blob<Dtype>()),
        blob_bottom_broadcast_2_(new Blob<Dtype>()),
        blob_bottom_bias_(new Blob<Dtype>()),
        blob_top_(new Blob<Dtype>()) {
    Caffe::set_random_seed(1701, Caffe::GetDefaultDevice());
    vector<int_tp> broadcast_shape(2);
    broadcast_shape[0] = 2; broadcast_shape[1] = 3;
    this->blob_bottom_broadcast_0_->Reshape(broadcast_shape);
    broadcast_shape[0] = 3; broadcast_shape[1] = 4;
    this->blob_bottom_broadcast_1_->Reshape(broadcast_shape);
    broadcast_shape[0] = 4; broadcast_shape[1] = 5;
    this->blob_bottom_broadcast_2_->Reshape(broadcast_shape);
    FillerParameter filler_param;
    filler_param.set_min(1);
    filler_param.set_max(10);
    shared_ptr<UniformFiller<Dtype> > filler =
        std::make_shared<UniformFiller<Dtype> >(filler_param);
    this->filler_ = filler;
    this->filler_->Fill(this->blob_bottom_);
    this->filler_->Fill(this->blob_bottom_eltwise_);
    this->filler_->Fill(this->blob_bottom_broadcast_0_);
    this->filler_->Fill(this->blob_bottom_broadcast_1_);
    this->filler_->Fill(this->blob_bottom_broadcast_2_);
    blob_bottom_vec_.push_back(blob_bottom_);
    blob_top_vec_.push_back(blob_top_);
  }
  virtual ~BiasLayerTest() {
    delete blob_bottom_;
    delete blob_bottom_eltwise_;
    delete blob_bottom_broadcast_0_;
    delete blob_bottom_broadcast_1_;
    delete blob_bottom_broadcast_2_;
    delete blob_bottom_bias_;
    delete blob_top_;
  }
  shared_ptr<UniformFiller<Dtype> > filler_;
  Blob<Dtype>* const blob_bottom_;
  Blob<Dtype>* const blob_bottom_eltwise_;
  Blob<Dtype>* const blob_bottom_broadcast_0_;
  Blob<Dtype>* const blob_bottom_broadcast_1_;
  Blob<Dtype>* const blob_bottom_broadcast_2_;
  Blob<Dtype>* const blob_bottom_bias_;
  Blob<Dtype>* const blob_top_;
  vector<Blob<Dtype>*> blob_bottom_vec_;
  vector<Blob<Dtype>*> blob_top_vec_;
};

TYPED_TEST_CASE(BiasLayerTest, TestDtypesAndDevices);

TYPED_TEST(BiasLayerTest, TestForwardEltwise) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_eltwise_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype* data = this->blob_top_->cpu_data();
  const int_tp count = this->blob_top_->count();
  const Dtype* in_data_a = this->blob_bottom_->cpu_data();
  const Dtype* in_data_b = this->blob_bottom_eltwise_->cpu_data();
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < count; ++i) {
    EXPECT_NEAR(data[i], in_data_a[i] + in_data_b[i],
                delta * fabs(in_data_a[i] + in_data_b[i]));
  }
}

TYPED_TEST(BiasLayerTest, TestForwardEltwiseInPlace) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_top_vec_[0] = this->blob_bottom_;  // in-place computation
  Blob<Dtype> orig_bottom(this->blob_bottom_->shape());
  orig_bottom.CopyFrom(*this->blob_bottom_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_eltwise_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype* data = this->blob_bottom_->cpu_data();
  const int_tp count = this->blob_bottom_->count();
  const Dtype* in_data_a = orig_bottom.cpu_data();
  const Dtype* in_data_b = this->blob_bottom_eltwise_->cpu_data();
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < count; ++i) {
    EXPECT_NEAR(data[i], in_data_a[i] + in_data_b[i],
                delta * fabs(in_data_a[i] + in_data_b[i]));
  }
}

TYPED_TEST(BiasLayerTest, TestBackwardEltwiseInPlace) {
  typedef typename TypeParam::Dtype Dtype;
  Blob<Dtype> orig_bottom(this->blob_bottom_->shape());
  orig_bottom.CopyFrom(*this->blob_bottom_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_eltwise_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  Blob<Dtype> top_diff(this->blob_bottom_->shape());
  FillerParameter filler_param;
  filler_param.set_type("gaussian");
  filler_param.set_std(1);
  GaussianFiller<Dtype> filler(filler_param);
  filler.Fill(&top_diff);
  vector<bool> propagate_down(2, true);
  // Run forward + backward without in-place computation;
  // save resulting bottom diffs.
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  caffe_copy(top_diff.count(), top_diff.cpu_data(),
             this->blob_top_->mutable_cpu_diff());
  layer->Backward(this->blob_top_vec_, propagate_down, this->blob_bottom_vec_);
  const bool kReshape = true;
  const bool kCopyDiff = true;
  Blob<Dtype> orig_bottom_diff;
  orig_bottom_diff.CopyFrom(*this->blob_bottom_, kCopyDiff, kReshape);
  Blob<Dtype> orig_bias_diff;
  orig_bias_diff.CopyFrom(*this->blob_bottom_eltwise_,
                            kCopyDiff, kReshape);
  // Rerun forward + backward with in-place computation;
  // check that resulting bottom diffs are the same.
  this->blob_top_vec_[0] = this->blob_bottom_;  // in-place computation
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  caffe_copy(top_diff.count(), top_diff.cpu_data(),
             this->blob_bottom_->mutable_cpu_diff());
  layer->Backward(this->blob_top_vec_, propagate_down, this->blob_bottom_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < this->blob_bottom_->count(); ++i) {
    EXPECT_NEAR(orig_bottom_diff.cpu_diff()[i],
                this->blob_bottom_->cpu_diff()[i], delta);
  }
  for (int_tp i = 0; i < this->blob_bottom_eltwise_->count(); ++i) {
    EXPECT_NEAR(orig_bias_diff.cpu_diff()[i],
                this->blob_bottom_eltwise_->cpu_diff()[i], delta);
  }
}

TYPED_TEST(BiasLayerTest, TestForwardEltwiseWithParam) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  BiasParameter* bias_param = layer_param.mutable_bias_param();
  bias_param->set_axis(0);
  bias_param->set_num_axes(-1);
  bias_param->mutable_filler()->set_type("gaussian");
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype* data = this->blob_top_->cpu_data();
  const int_tp count = this->blob_top_->count();
  const Dtype* in_data_a = this->blob_bottom_->cpu_data();
  const Dtype* in_data_b = layer->blobs()[0]->cpu_data();
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < count; ++i) {
    EXPECT_NEAR(data[i], in_data_a[i] + in_data_b[i], delta);
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBroadcastBegin) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_0_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int_tp c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int_tp h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int_tp w = 0; w < this->blob_bottom_->width(); ++w) {
          EXPECT_NEAR(this->blob_top_->data_at(n, c, h, w),
                      this->blob_bottom_->data_at(n, c, h, w) +
                      this->blob_bottom_broadcast_0_->data_at(n, c, 0, 0),
                      delta * fabs(this->blob_top_->data_at(n, c, h, w)));
        }
      }
    }
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBroadcastMiddle) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_1_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(1);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int_tp c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int_tp h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int_tp w = 0; w < this->blob_bottom_->width(); ++w) {
          EXPECT_NEAR(this->blob_top_->data_at(n, c, h, w),
                      this->blob_bottom_->data_at(n, c, h, w) +
                      this->blob_bottom_broadcast_1_->data_at(c, h, 0, 0),
                      delta * fabs(this->blob_top_->data_at(n, c, h, w)));
        }
      }
    }
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBroadcastMiddleInPlace) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_top_vec_[0] = this->blob_bottom_;  // in-place computation
  Blob<Dtype> orig_bottom(this->blob_bottom_->shape());
  orig_bottom.CopyFrom(*this->blob_bottom_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_1_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(1);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int_tp c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int_tp h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int_tp w = 0; w < this->blob_bottom_->width(); ++w) {
          EXPECT_NEAR(this->blob_bottom_->data_at(n, c, h, w),
                      orig_bottom.data_at(n, c, h, w) +
                      this->blob_bottom_broadcast_1_->data_at(c, h, 0, 0),
                      delta);
        }
      }
    }
  }
}

TYPED_TEST(BiasLayerTest, TestBackwardBroadcastMiddleInPlace) {
  typedef typename TypeParam::Dtype Dtype;
  Blob<Dtype> orig_bottom(this->blob_bottom_->shape());
  orig_bottom.CopyFrom(*this->blob_bottom_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_1_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(1);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  Blob<Dtype> top_diff(this->blob_bottom_->shape());
  FillerParameter filler_param;
  filler_param.set_type("gaussian");
  filler_param.set_std(1);
  GaussianFiller<Dtype> filler(filler_param);
  filler.Fill(&top_diff);
  vector<bool> propagate_down(2, true);
  // Run forward + backward without in-place computation;
  // save resulting bottom diffs.
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  caffe_copy(top_diff.count(), top_diff.cpu_data(),
             this->blob_top_->mutable_cpu_diff());
  layer->Backward(this->blob_top_vec_, propagate_down, this->blob_bottom_vec_);
  const bool kReshape = true;
  const bool kCopyDiff = true;
  Blob<Dtype> orig_bottom_diff;
  orig_bottom_diff.CopyFrom(*this->blob_bottom_, kCopyDiff, kReshape);
  Blob<Dtype> orig_bias_diff;
  orig_bias_diff.CopyFrom(*this->blob_bottom_broadcast_1_,
                            kCopyDiff, kReshape);
  // Rerun forward + backward with in-place computation;
  // check that resulting bottom diffs are the same.
  this->blob_top_vec_[0] = this->blob_bottom_;  // in-place computation
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  caffe_copy(top_diff.count(), top_diff.cpu_data(),
             this->blob_bottom_->mutable_cpu_diff());
  layer->Backward(this->blob_top_vec_, propagate_down, this->blob_bottom_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < this->blob_bottom_->count(); ++i) {
    EXPECT_NEAR(orig_bottom_diff.cpu_diff()[i],
                this->blob_bottom_->cpu_diff()[i], delta);
  }
  for (int_tp i = 0; i < this->blob_bottom_broadcast_1_->count(); ++i) {
    EXPECT_NEAR(orig_bias_diff.cpu_diff()[i],
                this->blob_bottom_broadcast_1_->cpu_diff()[i], delta);
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBroadcastMiddleWithParam) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  BiasParameter* bias_param = layer_param.mutable_bias_param();
  bias_param->set_axis(1);
  bias_param->set_num_axes(2);
  bias_param->mutable_filler()->set_type("gaussian");
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int_tp c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int_tp h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int_tp w = 0; w < this->blob_bottom_->width(); ++w) {
          EXPECT_NEAR(this->blob_top_->data_at(n, c, h, w),
                      this->blob_bottom_->data_at(n, c, h, w) +
                      layer->blobs()[0]->data_at(c, h, 0, 0), delta);
        }
      }
    }
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBroadcastEnd) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_2_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(2);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> > layer(
      new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp n = 0; n < this->blob_bottom_->num(); ++n) {
    for (int_tp c = 0; c < this->blob_bottom_->channels(); ++c) {
      for (int_tp h = 0; h < this->blob_bottom_->height(); ++h) {
        for (int_tp w = 0; w < this->blob_bottom_->width(); ++w) {
          EXPECT_NEAR(this->blob_top_->data_at(n, c, h, w),
                      this->blob_bottom_->data_at(n, c, h, w) +
                      this->blob_bottom_broadcast_2_->data_at(h, w, 0, 0),
                      delta);
        }
      }
    }
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBias) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_bias_->Reshape(std::vector<int_tp>(1,
      this->blob_bottom_vec_[0]->shape(1)));
  this->filler_->Fill(this->blob_bottom_bias_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_bias_);
  LayerParameter layer_param;
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> >
    layer(new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype* data = this->blob_top_->cpu_data();
  const int_tp count = this->blob_top_->count();
  const Dtype* in_data = this->blob_bottom_->cpu_data();
  const Dtype* bias = this->blob_bottom_bias_->cpu_data();
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < count; ++i) {
    int_tp j = (i / this->blob_bottom_vec_[0]->count(2))
        %  this->blob_bottom_vec_[0]->shape(1);
    EXPECT_NEAR(data[i], in_data[i] + bias[j], delta);
  }
}

TYPED_TEST(BiasLayerTest, TestForwardBiasAxis2) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_bias_->Reshape(std::vector<int_tp>(1,
      this->blob_bottom_vec_[0]->shape(2)));
  this->filler_->Fill(this->blob_bottom_bias_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_bias_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(2);
  shared_ptr<BiasLayer<Dtype, Dtype, Dtype> >
    layer(new BiasLayer<Dtype, Dtype, Dtype>(layer_param));
  layer->SetUp(this->blob_bottom_vec_, this->blob_top_vec_);
  ASSERT_EQ(this->blob_bottom_->shape(), this->blob_top_->shape());
  layer->Forward(this->blob_bottom_vec_, this->blob_top_vec_);
  const Dtype* data = this->blob_top_->cpu_data();
  const int_tp count = this->blob_top_->count();
  const Dtype* in_data = this->blob_bottom_->cpu_data();
  const Dtype* bias = this->blob_bottom_bias_->cpu_data();
  const Dtype delta = std::is_same<Dtype, half_fp>::value ?
                      1e-2 : 1e-5;
  for (int_tp i = 0; i < count; ++i) {
    int_tp j = (i / this->blob_bottom_vec_[0]->count(3))
        %  this->blob_bottom_vec_[0]->shape(2);
    EXPECT_NEAR(data[i], in_data[i] + bias[j], delta);
  }
}

TYPED_TEST(BiasLayerTest, TestGradientEltwise) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_eltwise_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientEltwise(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientEltwiseWithParam) {
  typedef typename TypeParam::Dtype Dtype;
  LayerParameter layer_param;
  BiasParameter* bias_param = layer_param.mutable_bias_param();
  bias_param->set_axis(0);
  bias_param->set_num_axes(-1);
  bias_param->mutable_filler()->set_type("gaussian");
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBroadcastBegin) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_0_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(0);
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBroadcastMiddle) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_1_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(1);
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBroadcastMiddleWithParam) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_1_);
  LayerParameter layer_param;
  BiasParameter* bias_param = layer_param.mutable_bias_param();
  bias_param->set_axis(1);
  bias_param->set_num_axes(2);
  bias_param->mutable_filler()->set_type("gaussian");
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBroadcastEnd) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_vec_.push_back(this->blob_bottom_broadcast_2_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(2);
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBias) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_bias_->Reshape(std::vector<int_tp>(1,
      this->blob_bottom_vec_[0]->shape(1)));
  this->filler_->Fill(this->blob_bottom_bias_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_bias_);
  LayerParameter layer_param;
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

TYPED_TEST(BiasLayerTest, TestGradientBiasAxis2) {
  typedef typename TypeParam::Dtype Dtype;
  this->blob_bottom_bias_->Reshape(std::vector<int_tp>(1,
      this->blob_bottom_vec_[0]->shape(2)));
  this->filler_->Fill(this->blob_bottom_bias_);
  this->blob_bottom_vec_.push_back(this->blob_bottom_bias_);
  LayerParameter layer_param;
  layer_param.mutable_bias_param()->set_axis(2);
  BiasLayer<Dtype, Dtype, Dtype> layer(layer_param);
  GradientChecker<Dtype> checker(1e-2, 1e-3);
  checker.CheckGradientExhaustive(&layer, this->blob_bottom_vec_,
      this->blob_top_vec_);
}

}  // namespace caffe
