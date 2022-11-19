#ifndef ASVGF_KERNEL_GLSL
#define ASVGF_KERNEL_GLSL

#ifdef __cplusplus

namespace foray::asvgf
{
    using uint = uint32_t;
#endif // __cplusplus
	const uint KERNEL_BOX3 = 0U;
	const uint KERNEL_BOX5 = 1U;
	const uint KERNEL_ATROUS = 2U;
	const uint KERNEL_SUBSAMPLED = 3U;


#ifdef __cplusplus
} // namespace foray::asvgf
#else

// 3x3 Box kernel
void box3(in RefPixelInfo ref, inout Sums sums)
{
	const int kernel = 1;
	for(int y = -kernel; y <= kernel; y++) {
		for(int x = -kernel; x <= kernel; x++) {
			if(x != 0 || y != 0) {
				tap(ivec2(x, y) * ref.StepSize, 1.f, ref, sums);
			}
		}
	}
}

// 5x5 Box kernel 
void box5(in RefPixelInfo ref, inout Sums sums)
{
	const int kernel = 2;
	for(int y = -kernel; y <= kernel; y++) {
		for(int x = -kernel; x <= kernel; x++) {
			if(x != 0 || y != 0) {
				tap(ivec2(x, y) * ref.StepSize, 1.f, ref, sums);
			}
		}
	}
}

// ATrous kernel
void atrous(in RefPixelInfo ref, inout Sums sums)
{
	const float kernel[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

	tap(ivec2( 1,  0) * ref.StepSize, 2.0 / 3.0, ref, sums);
	tap(ivec2( 0,  1) * ref.StepSize, 2.0 / 3.0, ref, sums);
	tap(ivec2(-1,  0) * ref.StepSize, 2.0 / 3.0, ref, sums);
	tap(ivec2( 0, -1) * ref.StepSize, 2.0 / 3.0, ref, sums);

	tap(ivec2( 2,  0) * ref.StepSize, 1.0 / 6.0, ref, sums);
	tap(ivec2( 0,  2) * ref.StepSize, 1.0 / 6.0, ref, sums);
	tap(ivec2(-2,  0) * ref.StepSize, 1.0 / 6.0, ref, sums);
	tap(ivec2( 0, -2) * ref.StepSize, 1.0 / 6.0, ref, sums);

	tap(ivec2( 1,  1) * ref.StepSize, 4.0 / 9.0, ref, sums);
	tap(ivec2(-1,  1) * ref.StepSize, 4.0 / 9.0, ref, sums);
	tap(ivec2(-1, -1) * ref.StepSize, 4.0 / 9.0, ref, sums);
	tap(ivec2( 1, -1) * ref.StepSize, 4.0 / 9.0, ref, sums);

	tap(ivec2( 1,  2) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2(-1,  2) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2(-1, -2) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2( 1, -2) * ref.StepSize, 1.0 / 9.0, ref, sums);

	tap(ivec2( 2,  1) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2(-2,  1) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2(-2, -1) * ref.StepSize, 1.0 / 9.0, ref, sums);
	tap(ivec2( 2, -1) * ref.StepSize, 1.0 / 9.0, ref, sums);

	tap(ivec2( 2,  2) * ref.StepSize, 1.0 / 36.0, ref, sums);
	tap(ivec2(-2,  2) * ref.StepSize, 1.0 / 36.0, ref, sums);
	tap(ivec2(-2, -2) * ref.StepSize, 1.0 / 36.0, ref, sums);
	tap(ivec2( 2, -2) * ref.StepSize, 1.0 / 36.0, ref, sums);
}


void subsampled(in RefPixelInfo ref, inout Sums sums)
{
	/*
	| | |x| | |
	| |x| |x| |
	|x| |x| |x|
	| |x| |x| |
	| | |x| | |
	*/

	if((PushC.IterationIdx & 1) == 0) {
		/*
		| | | | | |
		| |x| |x| |
		|x| |x| |x|
		| |x| |x| |
		| | | | | |
		*/
		tap(ivec2(-2,  0) * ref.StepSize, 1.0, ref, sums);
		tap(ivec2( 2,  0) * ref.StepSize, 1.0, ref, sums);
	}
	else {
		/*
		| | |x| | |
		| |x| |x| |
		| | |x| | |
		| |x| |x| |
		| | |x| | |
		*/
		tap(ivec2( 0, -2) * ref.StepSize, 1.0, ref, sums);
		tap(ivec2( 0,  2) * ref.StepSize, 1.0, ref, sums);
	}

	tap(ivec2(-1,  1) * ref.StepSize, 1.0, ref, sums);
	tap(ivec2( 1,  1) * ref.StepSize, 1.0, ref, sums);


	tap(ivec2(-1, -1) * ref.StepSize, 1.0, ref, sums);
	tap(ivec2( 1, -1) * ref.StepSize, 1.0, ref, sums);
}

void kernel(in RefPixelInfo ref, inout Sums sums, in uint KernelMode)
{
    if(KernelMode == KERNEL_BOX3) {
        box3(ref, sums);
    }
    else if(KernelMode == KERNEL_BOX5) {
        box5(ref, sums);
    }
    else if(KernelMode == KERNEL_ATROUS) {
        atrous(ref, sums);
    }
    else if(KernelMode == KERNEL_SUBSAMPLED) {
        subsampled(ref, sums);
    }
}

#endif // __cplusplus

#endif // ASVGF_KERNEL_GLSL