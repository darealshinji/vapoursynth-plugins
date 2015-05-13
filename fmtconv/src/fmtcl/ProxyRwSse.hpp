/*****************************************************************************

        ProxyRwSse.hpp
        Author: Laurent de Soras, 2012

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (fmtcl_ProxyRwSse_CODEHEADER_INCLUDED)
#define	fmtcl_ProxyRwSse_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "fstb/ToolsSse2.h"



namespace fmtcl
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



void	ProxyRwSse <SplFmt_FLOAT>::read (const PtrConst::Type &ptr, __m128 &src0, __m128 &src1, const __m128i &/*zero*/)
{
	src0 = _mm_loadu_ps (ptr    );
	src1 = _mm_loadu_ps (ptr + 4);
}

void	ProxyRwSse <SplFmt_FLOAT>::write (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &/*mask_lsb*/, const __m128i &/*sign_bit*/, const __m128 &/*offset*/)
{
	_mm_store_ps (ptr,     src0);
	_mm_store_ps (ptr + 4, src1);
}

void	ProxyRwSse <SplFmt_FLOAT>::write_partial (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &/*mask_lsb*/, const __m128i &/*sign_bit*/, const __m128 &/*offset*/, const __m128i &/*mask_store*/)
{
	_mm_store_ps (ptr,     src0);
	_mm_store_ps (ptr + 4, src1);
}



// const __m128i	zero = _mm_setzero_si128 ();
void	ProxyRwSse <SplFmt_INT8>::read (const PtrConst::Type &ptr, __m128 &src0, __m128 &src1, const __m128i &zero)
{
	const __m128i	src      = fstb::ToolsSse2::load_8_16l (ptr, zero);
	const __m128i	src_03   = _mm_unpacklo_epi16 (src, zero);
	const __m128i	src_47   = _mm_unpackhi_epi16 (src, zero);
	src0 = _mm_cvtepi32_ps (src_03);
	src1 = _mm_cvtepi32_ps (src_47);
}

__m128i	ProxyRwSse <SplFmt_INT8>::read_i16 (const PtrConst::Type &ptr, const __m128i &zero)
{
	return (fstb::ToolsSse2::load_8_16l (ptr, zero));
}

//	const __m128i	mask_lsb = _mm_set1_epi16 (0x00FF);
//	const __m128i	sign_bit = _mm_set1_epi16 (-0x8000);
//	const __m128	offset   = _mm_set1_ps (-32768);
void	ProxyRwSse <SplFmt_INT8>::write (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &mask_lsb, const __m128i &sign_bit, const __m128 &offset)
{
	const __m128i  val = prepare_write (src0, src1, sign_bit, offset);
	fstb::ToolsSse2::store_8_16m (ptr, val, mask_lsb);
}

void	ProxyRwSse <SplFmt_INT8>::write_partial (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &mask_lsb, const __m128i &sign_bit, const __m128 &offset, const __m128i &mask_store)
{
	const __m128i  val = prepare_write (src0, src1, sign_bit, offset);
	fstb::ToolsSse2::store_8_16m_partial (ptr, val, mask_lsb, mask_store);
}

__m128i	ProxyRwSse <SplFmt_INT8>::prepare_write (const __m128 &src0, const __m128 &src1, const __m128i &sign_bit, const __m128 &offset)
{
	__m128			val_03_f = _mm_add_ps (src0, offset);
	__m128			val_47_f = _mm_add_ps (src1, offset);

	const __m128i	val_03 = _mm_cvtps_epi32 (val_03_f);
	const __m128i	val_47 = _mm_cvtps_epi32 (val_47_f);

	__m128i			val = _mm_packs_epi32 (val_03, val_47);
	val = _mm_xor_si128 (val, sign_bit);

	return (val);
}

void	ProxyRwSse <SplFmt_INT8>::write_i16 (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb)
{
	fstb::ToolsSse2::store_8_16l (ptr, src, mask_lsb);
}

void	ProxyRwSse <SplFmt_INT8>::write_i16_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mask_store)
{
	fstb::ToolsSse2::store_8_16l_partial (ptr, src, mask_lsb, mask_store);
}



// Sign is ignored here
template <bool CLIP_FLAG, bool SIGN_FLAG>
__m128i	ProxyRwSse <SplFmt_INT8>::S16 <CLIP_FLAG, SIGN_FLAG>::read (const PtrConst::Type &ptr, const __m128i &zero, const __m128i &sign_bit)
{
	return (fstb::ToolsSse2::load_8_16l (ptr, zero));
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_INT8>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	fstb::ToolsSse2::store_8_16l (ptr, val, mask_lsb);
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_INT8>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit, const __m128i &mask_store)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	fstb::ToolsSse2::store_8_16l_partial (ptr, val, mask_lsb, mask_store);
}



// const __m128i	zero = _mm_setzero_si128 ();
void	ProxyRwSse <SplFmt_INT16>::read (const PtrConst::Type &ptr, __m128 &src0, __m128 &src1, const __m128i &zero)
{
	const __m128i	src =
		_mm_loadu_si128 (reinterpret_cast <const __m128i *> (ptr));
	const __m128i	src_03   = _mm_unpacklo_epi16 (src, zero);
	const __m128i	src_47   = _mm_unpackhi_epi16 (src, zero);
	src0 = _mm_cvtepi32_ps (src_03);
	src1 = _mm_cvtepi32_ps (src_47);
}

__m128i	ProxyRwSse <SplFmt_INT16>::read_i16 (const PtrConst::Type &ptr, const __m128i &/*zero*/)
{
	return (_mm_loadu_si128 (reinterpret_cast <const __m128i *> (ptr)));
}

//	const __m128i	mask_lsb = _mm_set1_epi16 (0x00FF);
//	const __m128i	sign_bit = _mm_set1_epi16 (-0x8000);
//	const __m128	offset   = _mm_set1_ps (-32768);
void	ProxyRwSse <SplFmt_INT16>::write (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &/*mask_lsb*/, const __m128i &sign_bit, const __m128 &offset)
{
	__m128			val_03_f = _mm_add_ps (src0, offset);
	__m128			val_47_f = _mm_add_ps (src1, offset);

	const __m128i	val_03 = _mm_cvtps_epi32 (val_03_f);
	const __m128i	val_47 = _mm_cvtps_epi32 (val_47_f);

	__m128i			val = _mm_packs_epi32 (val_03, val_47);
	val = _mm_xor_si128 (val, sign_bit);

	_mm_storeu_si128 (reinterpret_cast <__m128i *> (ptr), val);
}

void	ProxyRwSse <SplFmt_INT16>::write_partial (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &/*mask_lsb*/, const __m128i &sign_bit, const __m128 &offset, const __m128i &mask_store)
{
	__m128			val_03_f = _mm_add_ps (src0, offset);
	__m128			val_47_f = _mm_add_ps (src1, offset);

	const __m128i	val_03 = _mm_cvtps_epi32 (val_03_f);
	const __m128i	val_47 = _mm_cvtps_epi32 (val_47_f);

	__m128i			val = _mm_packs_epi32 (val_03, val_47);
	val = _mm_xor_si128 (val, sign_bit);

	_mm_maskmoveu_si128 (val, mask_store, reinterpret_cast <char *> (ptr));
}

void	ProxyRwSse <SplFmt_INT16>::write_i16 (const Ptr::Type &ptr, const __m128i &src, const __m128i &/*mask_lsb*/)
{
	_mm_storeu_si128 (reinterpret_cast <__m128i *> (ptr), src);
}

void	ProxyRwSse <SplFmt_INT16>::write_i16_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &/*mask_lsb*/, const __m128i &mask_store)
{
	_mm_maskmoveu_si128 (src, mask_store, reinterpret_cast <char *> (ptr));
}



template <bool CLIP_FLAG, bool SIGN_FLAG>
__m128i	ProxyRwSse <SplFmt_INT16>::S16 <CLIP_FLAG, SIGN_FLAG>::read (const PtrConst::Type &ptr, const __m128i &/*zero*/, const __m128i &sign_bit)
{
	__m128i        val = _mm_loadu_si128 (reinterpret_cast <const __m128i *> (ptr));
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}

	return (val);
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_INT16>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip (const Ptr::Type &ptr, const __m128i &src, const __m128i &/*mask_lsb*/, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	_mm_storeu_si128 (reinterpret_cast <__m128i *> (ptr), val);
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_INT16>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &/*mask_lsb*/, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit, const __m128i &mask_store)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	_mm_maskmoveu_si128 (val, mask_store, reinterpret_cast <char *> (ptr));
}



// const __m128i	zero = _mm_setzero_si128 ();
void	ProxyRwSse <SplFmt_STACK16>::read (const PtrConst::Type &ptr, __m128 &src0, __m128 &src1, const __m128i &zero)
{
	const __m128i	src =
		fstb::ToolsSse2::load_8_16ml (ptr._msb_ptr, ptr._lsb_ptr);
	const __m128i	src_03   = _mm_unpacklo_epi16 (src, zero);
	const __m128i	src_47   = _mm_unpackhi_epi16 (src, zero);
	src0 = _mm_cvtepi32_ps (src_03);
	src1 = _mm_cvtepi32_ps (src_47);
}

__m128i	ProxyRwSse <SplFmt_STACK16>::read_i16 (const PtrConst::Type &ptr, const __m128i &/*zero*/)
{
	return (fstb::ToolsSse2::load_8_16ml (ptr._msb_ptr, ptr._lsb_ptr));
}

//	const __m128i	mask_lsb = _mm_set1_epi16 (0x00FF);
//	const __m128i	sign_bit = _mm_set1_epi16 (-0x8000);
//	const __m128	offset   = _mm_set1_ps (-32768);
void	ProxyRwSse <SplFmt_STACK16>::write (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &mask_lsb, const __m128i &sign_bit, const __m128 &offset)
{
	__m128			val_03_f = _mm_add_ps (src0, offset);
	__m128			val_47_f = _mm_add_ps (src1, offset);

	const __m128i	val_03 = _mm_cvtps_epi32 (val_03_f);
	const __m128i	val_47 = _mm_cvtps_epi32 (val_47_f);

	__m128i			val = _mm_packs_epi32 (val_03, val_47);
	val = _mm_xor_si128 (val, sign_bit);

	fstb::ToolsSse2::store_8_16ml (
		ptr._msb_ptr,
		ptr._lsb_ptr,
		val,
		mask_lsb
	);
}

void	ProxyRwSse <SplFmt_STACK16>::write_partial (const Ptr::Type &ptr, const __m128 &src0, const __m128 &src1, const __m128i &mask_lsb, const __m128i &sign_bit, const __m128 &offset, const __m128i &mask_store)
{
	__m128			val_03_f = _mm_add_ps (src0, offset);
	__m128			val_47_f = _mm_add_ps (src1, offset);

	const __m128i	val_03 = _mm_cvtps_epi32 (val_03_f);
	const __m128i	val_47 = _mm_cvtps_epi32 (val_47_f);

	__m128i			val = _mm_packs_epi32 (val_03, val_47);
	val = _mm_xor_si128 (val, sign_bit);

	fstb::ToolsSse2::store_8_16ml_partial (
		ptr._msb_ptr,
		ptr._lsb_ptr,
		val,
		mask_lsb,
		mask_store
	);
}

void	ProxyRwSse <SplFmt_STACK16>::write_i16 (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb)
{
	fstb::ToolsSse2::store_8_16ml (ptr._msb_ptr, ptr._lsb_ptr, src, mask_lsb);
}

void	ProxyRwSse <SplFmt_STACK16>::write_i16_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mask_store)
{
	fstb::ToolsSse2::store_8_16ml_partial (
		ptr._msb_ptr,
		ptr._lsb_ptr,
		src,
		mask_lsb,
		mask_store
	);
}



template <bool CLIP_FLAG, bool SIGN_FLAG>
__m128i	ProxyRwSse <SplFmt_STACK16>::S16 <CLIP_FLAG, SIGN_FLAG>::read (const PtrConst::Type &ptr, const __m128i &/*zero*/, const __m128i &sign_bit)
{
	__m128i        val = fstb::ToolsSse2::load_8_16ml (ptr._msb_ptr, ptr._lsb_ptr);
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}

	return (val);
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_STACK16>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	fstb::ToolsSse2::store_8_16ml (ptr._msb_ptr, ptr._lsb_ptr, val, mask_lsb);
}

template <bool CLIP_FLAG, bool SIGN_FLAG>
void	ProxyRwSse <SplFmt_STACK16>::S16 <CLIP_FLAG, SIGN_FLAG>::write_clip_partial (const Ptr::Type &ptr, const __m128i &src, const __m128i &mask_lsb, const __m128i &mi, const __m128i &ma, const __m128i &sign_bit, const __m128i &mask_store)
{
	__m128i        val = src;
	if (CLIP_FLAG)
	{
		val = _mm_min_epi16 (val, ma);
		val = _mm_max_epi16 (val, mi);
	}
	if (SIGN_FLAG)
	{
		val = _mm_xor_si128 (val, sign_bit);
	}
	fstb::ToolsSse2::store_8_16ml_partial (ptr._msb_ptr, ptr._lsb_ptr, val, mask_lsb, mask_store);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace fmtcl



#endif	// fmtcl_ProxyRwSse_CODEHEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
