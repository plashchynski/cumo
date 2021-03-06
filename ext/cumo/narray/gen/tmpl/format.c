static VALUE
format_<%=type_name%>(VALUE fmt, dtype* x)
{
    // fix-me
    char s[48];
    int n;

    if (NIL_P(fmt)) {
        n = m_sprintf(s,*x);
        return rb_str_new(s,n);
    }
    return rb_funcall(fmt, '%', 1, m_data_to_num(*x));
}

static void
<%=c_iter%>(cumo_na_loop_t *const lp)
{
    size_t  i;
    char   *p1, *p2;
    ssize_t s1, s2;
    size_t *idx1;
    dtype *x;
    VALUE y;
    VALUE fmt = lp->option;
    CUMO_INIT_COUNTER(lp, i);
    CUMO_INIT_PTR_IDX(lp, 0, p1, s1, idx1);
    CUMO_INIT_PTR(lp, 1, p2, s2);
    //CUMO_SHOW_SYNCHRONIZE_FIXME_WARNING_ONCE("<%=name%>", "<%=type_name%>");
    if (idx1) {
        for (; i--;) {
            x = (dtype*)(p1+*idx1); idx1++;
            y = format_<%=type_name%>(fmt, x);
            CUMO_SET_DATA_STRIDE(p2, s2, VALUE, y);
        }
    } else {
        for (; i--;) {
            x = (dtype*)p1;         p1+=s1;
            y = format_<%=type_name%>(fmt, x);
            CUMO_SET_DATA_STRIDE(p2, s2, VALUE, y);
        }
    }
}

/*
  Format elements into strings.
  @overload <%=name%> format
  @param [String] format
  @return [Cumo::RObject] array of formated strings.
*/
static VALUE
<%=c_func(-1)%>(int argc, VALUE *argv, VALUE self)
{
    VALUE fmt=Qnil;

    cumo_ndfunc_arg_in_t ain[2] = {{Qnil,0},{cumo_sym_option}};
    cumo_ndfunc_arg_out_t aout[1] = {{cumo_cRObject,0}};
    cumo_ndfunc_t ndf = { <%=c_iter%>, CUMO_FULL_LOOP_NIP, 2, 1, ain, aout };

    rb_scan_args(argc, argv, "01", &fmt);
    cumo_cuda_runtime_check_status(cudaDeviceSynchronize());
    return cumo_na_ndloop(&ndf, 2, self, fmt);
}
