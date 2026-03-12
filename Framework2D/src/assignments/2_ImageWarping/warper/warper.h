// HW2_TODO: Please implement the abstract class Warper
// 1. The Warper class should abstract the **mathematical mapping** involved in
// the warping problem, **independent of image**.
// 2. The Warper class should have a virtual function warp(...) to be called in
// our image warping application.
//    - You should design the inputs and outputs of warp(...) according to the
//    mathematical abstraction discussed in class.
//    - Generally, the warping map should map one input point to another place.
// 3. Subclasses of Warper, IDWWarper and RBFWarper, should implement the
// warp(...) function to perform the actual warping.
#pragma once

namespace USTC_CG
{
class Warper
{
   public:

    // HW2_TODO: A virtual function warp(...)
    //src是 dst 
    virtual void warp(const unsigned char* src, unsigned char* dst, int width, int height);
    
    // HW2_TODO: other functions or variables if you need
};
}  // namespace USTC_CG