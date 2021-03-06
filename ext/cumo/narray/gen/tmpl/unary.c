<% if type_name == 'robject' || name == 'map' %>
<% else %>
void <%="cumo_#{c_iter}_index_index_kernel_launch"%>(char *p1, char *p2, size_t *idx1, size_t *idx2, uint64_t n);
void <%="cumo_#{c_iter}_index_stride_kernel_launch"%>(char *p1, char *p2, size_t *idx1, ssize_t s2, uint64_t n);
void <%="cumo_#{c_iter}_stride_index_kernel_launch"%>(char *p1, char *p2, ssize_t s1, size_t *idx2, uint64_t n);
void <%="cumo_#{c_iter}_stride_stride_kernel_launch"%>(char *p1, char *p2, ssize_t s1, ssize_t s2, uint64_t n);
void <%="cumo_#{c_iter}_contiguous_kernel_launch"%>(char *p1, char *p2, uint64_t n);
<% end %>

static void
<%=c_iter%>(cumo_na_loop_t *const lp)
{
    size_t  n;
    char   *p1, *p2;
    ssize_t s1, s2;
    size_t *idx1, *idx2;

    CUMO_INIT_COUNTER(lp, n);
    CUMO_INIT_PTR_IDX(lp, 0, p1, s1, idx1);
    CUMO_INIT_PTR_IDX(lp, 1, p2, s2, idx2);

    <% if type_name == 'robject' || name == 'map' %>
    {
        size_t i;
        dtype x;
        CUMO_SHOW_SYNCHRONIZE_FIXME_WARNING_ONCE("<%=name%>", "<%=type_name%>");
        if (idx1) {
            if (idx2) {
                for (i=0; i<n; i++) {
                    CUMO_GET_DATA_INDEX(p1,idx1,dtype,x);
                    x = m_<%=name%>(x);
                    CUMO_SET_DATA_INDEX(p2,idx2,dtype,x);
                }
            } else {
                for (i=0; i<n; i++) {
                    CUMO_GET_DATA_INDEX(p1,idx1,dtype,x);
                    x = m_<%=name%>(x);
                    CUMO_SET_DATA_STRIDE(p2,s2,dtype,x);
                }
            }
        } else {
            if (idx2) {
                for (i=0; i<n; i++) {
                    CUMO_GET_DATA_STRIDE(p1,s1,dtype,x);
                    x = m_<%=name%>(x);
                    CUMO_SET_DATA_INDEX(p2,idx2,dtype,x);
                }
            } else {
                //<% if need_align %>
                if (cumo_is_aligned(p1,sizeof(dtype)) &&
                    cumo_is_aligned(p2,sizeof(dtype)) ) {
                    if (s1 == sizeof(dtype) &&
                        s2 == sizeof(dtype) ) {
                        for (i=0; i<n; i++) {
                            ((dtype*)p2)[i] = m_<%=name%>(((dtype*)p1)[i]);
                        }
                        return;
                    }
                    if (cumo_is_aligned_step(s1,sizeof(dtype)) &&
                        cumo_is_aligned_step(s2,sizeof(dtype)) ) {
                        //<% end %>
                        for (i=0; i<n; i++) {
                            *(dtype*)p2 = m_<%=name%>(*(dtype*)p1);
                            p1 += s1;
                            p2 += s2;
                        }
                        return;
                        //<% if need_align %>
                    }
                }
                for (i=0; i<n; i++) {
                    CUMO_GET_DATA_STRIDE(p1,s1,dtype,x);
                    x = m_<%=name%>(x);
                    CUMO_SET_DATA_STRIDE(p2,s2,dtype,x);
                }
                //<% end %>
            }
        }
    }
    <% else %>
    {
        if (idx1) {
            if (idx2) {
                <%="cumo_#{c_iter}_index_index_kernel_launch"%>(p1,p2,idx1,idx2,n);
            } else {
                <%="cumo_#{c_iter}_index_stride_kernel_launch"%>(p1,p2,idx1,s2,n);
            }
        } else {
            if (idx2) {
                <%="cumo_#{c_iter}_stride_index_kernel_launch"%>(p1,p2,s1,idx2,n);
            } else {
                //<% if need_align %>
                if (cumo_is_aligned(p1,sizeof(dtype)) &&
                    cumo_is_aligned(p2,sizeof(dtype)) ) {
                    if (s1 == sizeof(dtype) &&
                        s2 == sizeof(dtype) ) {
                        <%="cumo_#{c_iter}_contiguous_kernel_launch"%>(p1,p2,n);
                        return;
                    }
                    if (cumo_is_aligned_step(s1,sizeof(dtype)) &&
                        cumo_is_aligned_step(s2,sizeof(dtype)) ) {
                        //<% end %>
                        <%="cumo_#{c_iter}_stride_stride_kernel_launch"%>(p1,p2,s1,s2,n);
                        return;
                        //<% if need_align %>
                    }
                }
                <%="cumo_#{c_iter}_stride_stride_kernel_launch"%>(p1,p2,s1,s2,n);
                //<% end %>
            }
        }
    }
    <% end %>
}

/*
  Unary <%=name%>.
  @overload <%=op_map%>
  @return [Cumo::<%=class_name%>] <%=name%> of self.
*/
static VALUE
<%=c_func(0)%>(VALUE self)
{
    cumo_ndfunc_arg_in_t ain[1] = {{cT,0}};
    cumo_ndfunc_arg_out_t aout[1] = {{cT,0}};
    cumo_ndfunc_t ndf = {<%=c_iter%>, CUMO_FULL_LOOP, 1,1, ain,aout};

    <% if name == 'map' %>
    cumo_cuda_runtime_check_status(cudaDeviceSynchronize());
    <% end %>
    return cumo_na_ndloop(&ndf, 1, self);
}
