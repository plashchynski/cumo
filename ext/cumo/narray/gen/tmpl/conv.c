<%
  cudnn_dtype =
    case type_name
    when 'sfloat'
      'CUDNN_DATA_FLOAT'
    when 'dfloat'
      'CUDNN_DATA_DOUBLE'
    else
      # CUDNN_DATA_HALF
      raise 'not supported'
    end
%>

#define CHECK_NARRAY_TYPE(x,t)                                 \
    if (rb_obj_class(x)!=(t)) {                                \
        rb_raise(rb_eTypeError,"invalid NArray type (class)"); \
    }

#define CHECK_DIM_EQ(nd1,nd2)                        \
    if ((nd1) != (nd2)) {                            \
        rb_raise(cumo_na_eShapeError,                \
                 "dimention mismatch: %d != %d",     \
                 (int)(nd1), (int)(nd2));            \
    }

static VALUE
option_value(VALUE value, VALUE default_value)
{
    switch(TYPE(value)) {
    case T_NIL:
    case T_UNDEF:
        return default_value;
    }
    return value;
}

static size_t kDefaultMaxWorkspaceSize = 8 * 1024 * 1024;

// VALUE is Ruby Array
static void
get_int_ary(int* int_ary, VALUE ary, size_t ndim, int default_value)
{
    if (ary == Qnil) {
        // default to 1
        for (size_t idim = 0; idim < ndim; ++idim) {
            int_ary[idim] = default_value;
        }
    } else if (TYPE(ary) == T_FIXNUM) {
        for (size_t idim = 0; idim < ndim; ++idim) {
            int_ary[idim] = NUM2INT(ary);
        }
    } else {
        Check_Type(ary, T_ARRAY);
        CHECK_DIM_EQ((size_t)(RARRAY_LEN(ary)), ndim);
        for (size_t idim = 0; idim < ndim; ++idim) {
            int_ary[idim] = NUM2INT(rb_ary_entry(ary, idim));
        }
    }
}

static VALUE
cumo_na_as_contiguous_array(VALUE a)
{
    return cumo_na_check_contiguous(a) == Qtrue ? a : rb_funcall(a, rb_intern("dup"), 0);
}

static char*
cumo_na_get_offset_pointer_for_read(VALUE a)
{
    return cumo_na_get_pointer_for_read(a) + cumo_na_get_offset(a);
}

static char*
cumo_na_get_offset_pointer_for_write(VALUE a)
{
    return cumo_na_get_pointer_for_write(a) + cumo_na_get_offset(a);
}

// cover_all is not supported with CuDNN
// x.conv(w, stride:, pad:, b: nil, y: nil)
static VALUE
<%=c_func(-1)%>(int argc, VALUE argv[], VALUE self)
{
    cudnnDataType_t cudnn_dtype = <%= cudnn_dtype %>;
    cudnnStatus_t status = 0;
    cudnnHandle_t handle = 0;
    dtype alpha = 1;
    dtype beta = 0;

    VALUE x=self, w, b, stride, pad, y;
    VALUE kw_hash = Qnil;
    ID kw_table[4] = {rb_intern("stride"), rb_intern("pad"), rb_intern("b"), rb_intern("y")};
    VALUE opts[4] = {Qundef, Qundef, Qundef, Qundef};

    size_t ndim;
    cumo_narray_t *nx, *nw;
    size_t *x_shape, *w_shape;
    size_t out_channels, batch_size;

    VALUE x_cont, w_cont;
    cudnnTensorDescriptor_t x_desc = 0;
    cudnnTensorDescriptor_t y_desc = 0;
    cudnnTensorDescriptor_t b_desc = 0;
    cudnnFilterDescriptor_t w_desc = 0;
    cudnnConvolutionDescriptor_t conv_desc = 0;
    char *x_cont_ptr, *w_cont_ptr, *y_ptr;

    cudnnConvolutionFwdAlgoPerf_t perf_result;
    cudnnConvolutionFwdAlgo_t algo;
    size_t max_workspace_size = kDefaultMaxWorkspaceSize;
    size_t workspace_size;
    char* workspace = 0;

    int int_stride[CUMO_NA_MAX_DIMENSION];
    int int_pad[CUMO_NA_MAX_DIMENSION];

    rb_scan_args(argc, argv, "1:", &w, &kw_hash);
    rb_get_kwargs(kw_hash, kw_table, 0, 4, opts);
    stride = option_value(opts[0], Qnil);
    pad = option_value(opts[1], Qnil);
    b = option_value(opts[2], Qnil);
    y = option_value(opts[3], Qnil);

    CumoGetNArray(x, nx);
    CumoGetNArray(w, nw);

    CHECK_DIM_EQ(nx->ndim, nw->ndim);
    CHECK_NARRAY_TYPE(x, cT);
    CHECK_NARRAY_TYPE(w, cT);
    if (nx->ndim - 2 < 2) {
        rb_raise(cumo_na_eShapeError, "CuDNN convolution requires number of spatial "
                "dimensions to be greater than or equal to 2, but %d", nx->ndim - 2);
    }
    ndim = nx->ndim - 2;  // Number of spatial dimensions

    get_int_ary(int_stride, stride, ndim, 1);
    get_int_ary(int_pad, pad, ndim, 0);

    x_shape = nx->shape;
    w_shape = nw->shape;
    batch_size = x_shape[0]; // x_shape = (batch_size, in_channels, d_1, d_2, ..., d_N)
    out_channels = w_shape[0]; // w.shape = (out_channels, _, k_1, k_2, ..., k_N)

    if (y != Qnil) {
        CHECK_NARRAY_TYPE(y, cT);
    }
    else {
        size_t *y_shape = ALLOCA_N(size_t, ndim + 2);
        // out_shape = (batch_size, out_channels, out_1, out_2, ..., out_N)
        y_shape[0] = batch_size;
        y_shape[1] = out_channels;
        for (size_t i = 0; i < ndim; ++i) {
            y_shape[i + 2] = cumo_cuda_cudnn_GetConvOutDim(
                    x_shape[i + 2], w_shape[i + 2], int_stride[i], int_pad[i]);
        }
        y = cumo_na_new(cT, ndim + 2, y_shape);
    }

    x_cont = cumo_na_as_contiguous_array(x);
    w_cont = cumo_na_as_contiguous_array(w);

    x_cont_ptr = cumo_na_get_offset_pointer_for_read(x_cont);
    w_cont_ptr = cumo_na_get_offset_pointer_for_read(w_cont);
    y_ptr = cumo_na_get_offset_pointer_for_write(y);

    status = cumo_cuda_cudnn_CreateTensorDescriptor(&x_desc, x_cont, cudnn_dtype);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;
    status = cumo_cuda_cudnn_CreateTensorDescriptor(&y_desc, y, cudnn_dtype);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;
    status = cumo_cuda_cudnn_CreateFilterDescriptor(&w_desc, w_cont, cudnn_dtype);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;
    status = cumo_cuda_cudnn_CreateConvolutionDescriptor(&conv_desc, ndim, int_stride, int_pad, cudnn_dtype);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;

    handle = cumo_cuda_cudnn_handle();

    // auto tune
    status = cumo_cuda_cudnn_FindConvolutionForwardAlgorithm(
            &perf_result,
            handle,
            x_desc,
            x_cont,
            w_desc,
            w_cont,
            conv_desc,
            y_desc,
            y,
            max_workspace_size,
            int_stride,
            int_pad,
            ndim,
            cudnn_dtype);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;
    algo = perf_result.algo;
    workspace_size = perf_result.memory;

    workspace = cumo_cuda_runtime_malloc(max_workspace_size);
    status = cudnnConvolutionForward(
            handle,
            (void*)&alpha,
            x_desc,
            (void*)x_cont_ptr,
            w_desc,
            (void*)w_cont_ptr,
            conv_desc,
            algo,
            (void*)workspace,
            workspace_size,
            (void*)&beta,
            y_desc,
            (void*)y_ptr);
    if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;

    if (b != Qnil) {
        size_t b_shape[CUMO_NA_MAX_DIMENSION];
        VALUE b_cont;
        char* b_cont_ptr;
        cumo_narray_t *nb, *nb_cont;

        CHECK_NARRAY_TYPE(b, cT);
        CumoGetNArray(b, nb);
        b_shape[0] = 1;
        b_shape[1] = nb->size;
        for (size_t i = 0; i < ndim; ++i) {
            b_shape[i + 2] = 1;
        }
        b_cont =  cumo_na_as_contiguous_array(b);
        b_cont_ptr = cumo_na_get_offset_pointer_for_read(b_cont);
        CumoGetNArray(b_cont, nb_cont);
        cumo_na_setup_shape(nb_cont, ndim + 2, b_shape);
        status = cumo_cuda_cudnn_CreateTensorDescriptor(&b_desc, b_cont, cudnn_dtype);
        if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;

        status = cudnnAddTensor(
                    handle,
                    (void*)&alpha,
                    b_desc,
                    (void*)b_cont_ptr,
                    (void*)&alpha,
                    y_desc,
                    (void*)y_ptr);
        if (status != CUDNN_STATUS_SUCCESS) goto CONV_ERROR;
    }

CONV_ERROR:
    if (x_desc) cudnnDestroyTensorDescriptor(x_desc);
    if (y_desc) cudnnDestroyTensorDescriptor(y_desc);
    if (b_desc) cudnnDestroyTensorDescriptor(b_desc);
    if (w_desc) cudnnDestroyFilterDescriptor(w_desc);
    if (conv_desc) cudnnDestroyConvolutionDescriptor(conv_desc);
    if (workspace) cumo_cuda_runtime_free(workspace);
    cumo_cuda_cudnn_check_status(status);

    return y;
}

#undef CHECK_NARRAY_TYPE
#undef CHECK_DIM_EQ
