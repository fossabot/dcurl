/*
 * Copyright (C) 2018 dcurl Developers.
 * Copyright (C) 2017 IOTA AS, IOTA Foundation and Developers.
 * Use of this source code is governed by MIT license that can be
 * found in the LICENSE file.
 */

#include "clcontext.h"
#include <stdio.h>
#include "pearl.cl.h"

static int init_cl_devices(CLContext *ctx)
{
    cl_uint num_platform = 0;
    cl_int errno;
    cl_platform_id *platform;

    errno = clGetPlatformIDs(0, NULL, &num_platform);

    if (errno != CL_SUCCESS)
        return 0; /* Cannot get # of OpenCL platform */

    /* We only need one Platform */
    platform = (cl_platform_id *) malloc(sizeof(cl_platform_id) * num_platform);
    clGetPlatformIDs(num_platform, platform, NULL);

    /* Get Device IDs */
    cl_uint platform_num_device;
    if (clGetDeviceIDs(platform[0], CL_DEVICE_TYPE_GPU, 1, &ctx->device,
                       &platform_num_device) != CL_SUCCESS)
        return 0; /* Failed to get OpenCL Device IDs in platform */

    /* Create OpenCL context */
    ctx->context =
        (cl_context) clCreateContext(NULL, 1, &ctx->device, NULL, NULL, &errno);
    if (errno != CL_SUCCESS)
        return 0; /* Failed to create OpenCL Context */

    /* Get Device Info (num_cores) */
    if (CL_SUCCESS != clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_COMPUTE_UNITS,
                                      sizeof(cl_uint), &ctx->num_cores, NULL))
        return 0; /* Failed to get num_cores of GPU */

    /* Get Device Info (max_memory) */
    if (CL_SUCCESS != clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_MEM_ALLOC_SIZE,
                                      sizeof(cl_ulong), &ctx->max_memory, NULL))
        return 0; /* Failed to get Max memory of GPU */

    /* Get Device Info (num work group) */
    if (CL_SUCCESS !=
        clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                        sizeof(size_t), &ctx->num_work_group, NULL))
        ctx->num_work_group = 1;

    /* Create Command Queue */
    ctx->cmdq = clCreateCommandQueue(ctx->context, ctx->device, 0, &errno);
    if (errno != CL_SUCCESS)
        return 0; /* Failed to create command queue */

    free(platform);
    return 1;
}


static int init_cl_program(CLContext *ctx)
{
    unsigned char *source_str = pearl_cl;
    size_t source_size = pearl_cl_len;
    cl_int errno;

    ctx->program = clCreateProgramWithSource(
        ctx->context, ctx->kernel_info.num_src, (const char **) &source_str,
        (const size_t *) &source_size, &errno);
    if (CL_SUCCESS != errno)
        return 0; /* Failed to create OpenCL program */

    errno =
        clBuildProgram(ctx->program, 1, &ctx->device, "-Werror", NULL, NULL);
    if (CL_SUCCESS != errno)
        return 0; /* Failed to build OpenCL program */

    return 1;
}

int init_cl_kernel(CLContext *ctx, char **kernel_name)
{
    cl_int errno;

    for (int i = 0; i < ctx->kernel_info.num_kernels; i++) {
        ctx->kernel[i] = clCreateKernel(ctx->program, kernel_name[i], &errno);
        if (CL_SUCCESS != errno)
            return 0; /* Failed to create OpenCL kernel */
    }
    return 1;
}

int init_cl_buffer(CLContext *ctx)
{
    cl_ulong mem = 0, max_mem = 0;
    cl_int errno;

    for (int i = 0; i < ctx->kernel_info.num_buffers; i++) {
        mem = ctx->kernel_info.buffer_info[i].size;
        if (ctx->kernel_info.buffer_info[i].init_flags & 2) {
            mem *= ctx->num_cores * ctx->num_work_group;
            if (mem > ctx->max_memory) {
                int temp =
                    ctx->max_memory / ctx->kernel_info.buffer_info[i].size;
                ctx->num_cores = temp;
                mem = temp * ctx->kernel_info.buffer_info[i].size;
            }
        }
        /* Check Memory bound */
        max_mem += mem;
        if (max_mem >= ctx->max_memory)
            return 0; /* GPU Memory is not enough */

        /* Create OpenCL Buffer */
        ctx->buffer[i] =
            clCreateBuffer(ctx->context, ctx->kernel_info.buffer_info[i].flags,
                           mem, NULL, &errno);
        if (CL_SUCCESS != errno)
            return 0; /* Failed to create OpenCL Memory Buffer */

        /* Set Kernel Arguments */
        for (int j = 0; j < ctx->kernel_info.num_kernels; j++) {
            if (CL_SUCCESS != clSetKernelArg(ctx->kernel[j], i, sizeof(cl_mem),
                                             (void *) &ctx->buffer[i]))
                return 0; /* Failed to set OpenCL kernel arguments */
        }
    }
    return 1;
}

int init_clcontext(CLContext **ctx)
{
    *ctx = (CLContext *) malloc(sizeof(CLContext));

    if (!(*ctx))
        return 0;

    (*ctx)->kernel_info.num_buffers = 9;
    (*ctx)->kernel_info.num_kernels = 3;
    (*ctx)->kernel_info.num_src = 1;

    return init_cl_devices(*ctx) && init_cl_program(*ctx);
}
