// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2017, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.

#include "op_halide.hpp"

namespace cv
{
namespace dnn
{

#ifdef HAVE_HALIDE
Halide::Buffer<float> wrapToHalideBuffer(const Mat& mat)
{
    int n, c, w, h;
    getCanonicalSize(mat.size, &w, &h, &c, &n);
    return wrapToHalideBuffer(mat, {w, h, c, n});
}

Halide::Buffer<float> wrapToHalideBuffer(const Mat& mat,
                                         const std::vector<int>& sizes)
{
    Halide::Buffer<float> buffer((float*)mat.data, sizes);
    buffer.set_host_dirty();  // Indicate that data is on CPU.
    return buffer;
}

Halide::Buffer<> halideBuffer(const Ptr<BackendWrapper>& ptr)
{
    CV_Assert(!ptr.empty());
    return ptr.dynamicCast<HalideBackendWrapper>()->buffer;
}

std::vector<Halide::Buffer<> > halideBuffers(const std::vector<Ptr<BackendWrapper> >& ptrs)
{
    std::vector<Halide::Buffer<> > vec;
    vec.reserve(ptrs.size());
    for (const Ptr<BackendWrapper>& ptr : ptrs)
    {
        vec.push_back(halideBuffer(ptr));
    }
    return vec;
}

void getCanonicalSize(const Halide::Buffer<>& buffer, int* width, int* height,
                      int* channels, int* batch)
{
    CV_Assert(buffer.dimensions() == 4);
    *width = buffer.extent(0);
    *height = buffer.extent(1);
    *channels = buffer.extent(2);
    *batch = buffer.extent(3);
}

HalideBackendNode::HalideBackendNode(const Halide::Func& func)
    : BackendNode(DNN_BACKEND_HALIDE), funcs(1, func) {}

HalideBackendNode::HalideBackendNode(const std::vector<Halide::Func>& funcs)
    : BackendNode(DNN_BACKEND_HALIDE), funcs(funcs) {}

HalideBackendNode::HalideBackendNode(const Ptr<HalideBackendNode>& base,
                                     const Halide::Func& top)
    : BackendNode(DNN_BACKEND_HALIDE), funcs(base->funcs)
{
    funcs.back() = top;
}

HalideBackendWrapper::HalideBackendWrapper(int targetId, const cv::Mat& m)
    : BackendWrapper(DNN_BACKEND_HALIDE, targetId)
{
    buffer = wrapToHalideBuffer(m);
    if (targetId != DNN_TARGET_CPU)
        CV_Error(Error::StsNotImplemented, "Unknown target identifier");
}

HalideBackendWrapper::HalideBackendWrapper(const Ptr<BackendWrapper>& base,
                                           const MatShape& shape)
    : BackendWrapper(DNN_BACKEND_HALIDE, base->targetId)
{
    if (base->targetId != DNN_TARGET_CPU)
        CV_Error(Error::StsNotImplemented, "Unknown target identifier");

    int w, h, c, n;
    getCanonicalSize(shape, &w, &h, &c, &n);
    Halide::Buffer<float> baseBuffer = halideBuffer(base);
    buffer = Halide::Buffer<float>((float*)baseBuffer.raw_buffer()->host,
                                   {w, h, c, n});
    buffer.set_host_dirty();  // Indicate that data is on CPU.
}
#endif  // HAVE_HALIDE

void getCanonicalSize(const MatSize& size, int* width, int* height,
                      int* channels, int* batch)
{
    const int dims = size.p[-1];
    CV_Assert(dims == 2 || dims == 4);
    *batch = size[0];
    *channels = size[1];
    if (dims == 4)
    {
        *width = size[3];
        *height = size[2];
    }
    else
    {
        *width = 1;
        *height = 1;
    }
}

void getCanonicalSize(const MatShape& shape, int* width, int* height,
                      int* channels, int* batch)
{
    const int dims = shape.size();
    CV_Assert(dims == 2 || dims == 4);
    *batch = shape[0];
    *channels = shape[1];
    if (dims == 4)
    {
        *width = shape[3];
        *height = shape[2];
    }
    else
    {
        *width = 1;
        *height = 1;
    }
}

void compileHalide(std::vector<Mat> &outputs, Ptr<BackendNode>& node, int targetId)
{
#ifdef HAVE_HALIDE
    CV_Assert(!node.empty());
    Halide::Func& top = node.dynamicCast<HalideBackendNode>()->funcs.back();

    int outW, outH, outC, outN;
    Halide::Var x("x"), y("y"), c("c"), n("n");
    getCanonicalSize(outputs[0].size, &outW, &outH, &outC, &outN);
    top.bound(x, 0, outW).bound(y, 0, outH)
       .bound(c, 0, outC).bound(n, 0, outN);

    Halide::Target target = Halide::get_host_target();
    target.set_feature(Halide::Target::NoAsserts);
    top.compile_jit(target);
#endif  // HAVE_HALIDE
}

void forwardHalide(std::vector<Ptr<BackendWrapper> > &outputs,
                   const Ptr<BackendNode>& node)
{
#ifdef HAVE_HALIDE
    CV_Assert(!node.empty());
    Halide::Func& top = node.dynamicCast<HalideBackendNode>()->funcs.back();
    auto outputBuffers = halideBuffers(outputs);
    top.realize(Halide::Realization(outputBuffers));
#endif  // HAVE_HALIDE
}

bool haveHalide()
{
#ifdef HAVE_HALIDE
    return true;
#else
    return false;
#endif  // HAVE_HALIDE
}

}  // namespace dnn
}  // namespace cv