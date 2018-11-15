static void
<%=c_iter%>(cumo_na_loop_t *const lp)
{
    size_t i, s1;
    char *p1;
    size_t *idx1;
    dtype x;
    volatile VALUE a, y;

    CUMO_INIT_COUNTER(lp, i);
    CUMO_INIT_PTR_IDX(lp, 0, p1, s1, idx1);
    a = rb_ary_new2(i);
    rb_ary_push(lp->args[1].value, a);
    if (idx1) {
        for (; i--;) {
            CUMO_GET_DATA_INDEX(p1,idx1,dtype,x);
            y = m_data_to_num(x);
            rb_ary_push(a,y);
        }
    } else {
        for (; i--;) {
            CUMO_GET_DATA_STRIDE(p1,s1,dtype,x);
            y = m_data_to_num(x);
            rb_ary_push(a,y);
        }
    }
}

/*
  Convert self to Array.
  @overload <%=name%>
  @return [Array]
*/
static VALUE
<%=c_func(0)%>(VALUE self)
{
    cumo_ndfunc_arg_in_t ain[3] = {{Qnil,0},{cumo_sym_loop_opt},{cumo_sym_option}};
    cumo_ndfunc_arg_out_t aout[1] = {{rb_cArray,0}}; // dummy?
    cumo_ndfunc_t ndf = { <%=c_iter%>, CUMO_FULL_LOOP_NIP, 3, 1, ain, aout };
    CUMO_SHOW_SYNCHRONIZE_WARNING_ONCE("<%=name%>", "<%=type_name%>");
    cumo_cuda_runtime_check_status(cudaDeviceSynchronize());
    return cumo_na_ndloop_cast_narray_to_rarray(&ndf, self, Qnil);
}
