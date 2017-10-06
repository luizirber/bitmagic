

int BM_init(void*)
{
	return BM_OK;
}

const char* BM_version(unsigned* major, unsigned* minor, unsigned* patch)
{
    if (major)
        *major = bm::_copyright<true>::_v[0];
    if (minor)
        *minor = bm::_copyright<true>::_v[1];
    if (patch)
        *patch = bm::_copyright<true>::_v[2];
    
    return bm::_copyright<true>::_p;
}

const char* BM_error_msg(int errcode)
{
    switch (errcode)
    {
    case BM_OK:
        return "BM-00: All correct.";
    case BM_ERR_BADALLOC:
        return "BM-01: Allocation error.";
    case BM_ERR_BADARG:
        return "BM-02: Invalid or missing function argument.";
    }
    return 0;
}



int BM_bvector_construct(BM_BVHANDLE* h, unsigned int bv_max)
{
	TRY
	{
		void* mem = ::malloc(sizeof(TBM_bvector));
		if (mem == 0)
		{
			*h = 0;
			return BM_ERR_BADALLOC;
		}
        if (bv_max == 0)
        {
            bv_max = bm::id_max;
        }
		// placement new just to call the constructor
		TBM_bvector* bv = new(mem) TBM_bvector(bm::BM_BIT,
                                               bm::gap_len_table<true>::_len,
                                               bv_max,
                                               TBM_Alloc());
		*h = bv;
	}
	CATCH (BM_ERR_BADALLOC)
	{
		*h = 0;
		return BM_ERR_BADALLOC;
	}
	ETRY;

	return BM_OK;
}


int BM_bvector_free(BM_BVHANDLE h)
{
	if (!h)
		return BM_ERR_BADARG;
	TBM_bvector* bv = (TBM_bvector*)h;
	bv->~TBM_bvector();
	::free(h);

	return BM_OK;
}

int BM_bvector_set_bit(BM_BVHANDLE h, unsigned int i, unsigned int val)
{
	if (!h)
		return BM_ERR_BADARG;
	TRY
	{
        TBM_bvector* bv = (TBM_bvector*)h;
        bv->set(i, val);
	}
	CATCH (BM_ERR_BADALLOC)
	{
		return BM_ERR_BADALLOC;
	}
	ETRY;
	return BM_OK;
}

int BM_bvector_get_bit(BM_BVHANDLE h, unsigned int i, unsigned int* pval)
{
	if (!h || !pval)
		return BM_ERR_BADARG;
	TRY
	{
        const TBM_bvector* bv = (TBM_bvector*)h;
        *pval = bv->test(i);
	}
	CATCH (BM_ERR_BADALLOC)
	{
		return BM_ERR_BADALLOC;
	}
	ETRY;
	return BM_OK;
}


int BM_bvector_count(BM_BVHANDLE h, unsigned int* pcount)
{
	if (!h || !pcount)
		return BM_ERR_BADARG;
	TRY
	{
        const TBM_bvector* bv = (TBM_bvector*)h;
        *pcount = bv->count();
	}
	CATCH (BM_ERR_BADALLOC)
	{
		return BM_ERR_BADALLOC;
	}
	ETRY;
	return BM_OK;
}


