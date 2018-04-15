/*****************************************************************************
 * box_type.h
 *****************************************************************************
 * Copyright (C) 2017 L-SMASH project
 *
 * Authors: Yusuke Nakamura <muken.the.vfrmaniac@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *****************************************************************************/

/* This file is available under an ISC license. */

/* Box types */
#ifndef LSMASH_INITIALIZE_BOX_TYPE_HERE
    #define DEFINE_BOX_TYPE( BOX_TYPE, FOURCC, UUID_TYPE ) \
        extern const lsmash_box_type_t BOX_TYPE
#else
    #define DEFINE_BOX_TYPE( BOX_TYPE, FOURCC, UUID_TYPE ) \
        const lsmash_box_type_t BOX_TYPE = LSMASH_##UUID_TYPE##_BOX_TYPE_INITIALIZER( FOURCC )
#endif

DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ID32, LSMASH_4CC( 'I', 'D', '3', '2' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ALBM, LSMASH_4CC( 'a', 'l', 'b', 'm' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_AUTH, LSMASH_4CC( 'a', 'u', 't', 'h' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_BPCC, LSMASH_4CC( 'b', 'p', 'c', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_BUFF, LSMASH_4CC( 'b', 'u', 'f', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_BXML, LSMASH_4CC( 'b', 'x', 'm', 'l' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CCID, LSMASH_4CC( 'c', 'c', 'i', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CDEF, LSMASH_4CC( 'c', 'd', 'e', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CLSF, LSMASH_4CC( 'c', 'l', 's', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CMAP, LSMASH_4CC( 'c', 'm', 'a', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CO64, LSMASH_4CC( 'c', 'o', '6', '4' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_COLR, LSMASH_4CC( 'c', 'o', 'l', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CPRT, LSMASH_4CC( 'c', 'p', 'r', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CSLG, LSMASH_4CC( 'c', 's', 'l', 'g' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CTTS, LSMASH_4CC( 'c', 't', 't', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CVRU, LSMASH_4CC( 'c', 'v', 'r', 'u' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DCFD, LSMASH_4CC( 'd', 'c', 'f', 'D' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DINF, LSMASH_4CC( 'd', 'i', 'n', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DREF, LSMASH_4CC( 'd', 'r', 'e', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DSCP, LSMASH_4CC( 'd', 's', 'c', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DSGD, LSMASH_4CC( 'd', 's', 'g', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DSTG, LSMASH_4CC( 'd', 's', 't', 'g' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_EDTS, LSMASH_4CC( 'e', 'd', 't', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ELST, LSMASH_4CC( 'e', 'l', 's', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FECI, LSMASH_4CC( 'f', 'e', 'c', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FECR, LSMASH_4CC( 'f', 'e', 'c', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FIIN, LSMASH_4CC( 'f', 'i', 'i', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FIRE, LSMASH_4CC( 'f', 'i', 'r', 'e' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FPAR, LSMASH_4CC( 'f', 'p', 'a', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FREE, LSMASH_4CC( 'f', 'r', 'e', 'e' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FRMA, LSMASH_4CC( 'f', 'r', 'm', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FTYP, LSMASH_4CC( 'f', 't', 'y', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_GITN, LSMASH_4CC( 'g', 'i', 't', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_GNRE, LSMASH_4CC( 'g', 'n', 'r', 'e' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_GRPI, LSMASH_4CC( 'g', 'r', 'p', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_HDLR, LSMASH_4CC( 'h', 'd', 'l', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_HMHD, LSMASH_4CC( 'h', 'm', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ICNU, LSMASH_4CC( 'i', 'c', 'n', 'u' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IDAT, LSMASH_4CC( 'i', 'd', 'a', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IHDR, LSMASH_4CC( 'i', 'h', 'd', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IINF, LSMASH_4CC( 'i', 'i', 'n', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ILOC, LSMASH_4CC( 'i', 'l', 'o', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IMIF, LSMASH_4CC( 'i', 'm', 'i', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_INFU, LSMASH_4CC( 'i', 'n', 'f', 'u' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IODS, LSMASH_4CC( 'i', 'o', 'd', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IPHD, LSMASH_4CC( 'i', 'p', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IPMC, LSMASH_4CC( 'i', 'p', 'm', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IPRO, LSMASH_4CC( 'i', 'p', 'r', 'o' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_IREF, LSMASH_4CC( 'i', 'r', 'e', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_JP  , LSMASH_4CC( 'j', 'p', ' ', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_JP2C, LSMASH_4CC( 'j', 'p', '2', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_JP2H, LSMASH_4CC( 'j', 'p', '2', 'h' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_JP2I, LSMASH_4CC( 'j', 'p', '2', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_KYWD, LSMASH_4CC( 'k', 'y', 'w', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_LOCI, LSMASH_4CC( 'l', 'o', 'c', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_LRCU, LSMASH_4CC( 'l', 'r', 'c', 'u' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MDAT, LSMASH_4CC( 'm', 'd', 'a', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MDHD, LSMASH_4CC( 'm', 'd', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MDIA, LSMASH_4CC( 'm', 'd', 'i', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MDRI, LSMASH_4CC( 'm', 'd', 'r', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MECO, LSMASH_4CC( 'm', 'e', 'c', 'o' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MEHD, LSMASH_4CC( 'm', 'e', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_M7HD, LSMASH_4CC( 'm', '7', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MERE, LSMASH_4CC( 'm', 'e', 'r', 'e' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_META, LSMASH_4CC( 'm', 'e', 't', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MFHD, LSMASH_4CC( 'm', 'f', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MFRA, LSMASH_4CC( 'm', 'f', 'r', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MFRO, LSMASH_4CC( 'm', 'f', 'r', 'o' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MINF, LSMASH_4CC( 'm', 'i', 'n', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MJHD, LSMASH_4CC( 'm', 'j', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MOOF, LSMASH_4CC( 'm', 'o', 'o', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MOOV, LSMASH_4CC( 'm', 'o', 'o', 'v' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MVCG, LSMASH_4CC( 'm', 'v', 'c', 'g' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MVCI, LSMASH_4CC( 'm', 'v', 'c', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MVEX, LSMASH_4CC( 'm', 'v', 'e', 'x' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MVHD, LSMASH_4CC( 'm', 'v', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MVRA, LSMASH_4CC( 'm', 'v', 'r', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_NMHD, LSMASH_4CC( 'n', 'm', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_OCHD, LSMASH_4CC( 'o', 'c', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODAF, LSMASH_4CC( 'o', 'd', 'a', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODDA, LSMASH_4CC( 'o', 'd', 'd', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODHD, LSMASH_4CC( 'o', 'd', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODHE, LSMASH_4CC( 'o', 'd', 'h', 'e' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODRB, LSMASH_4CC( 'o', 'd', 'r', 'b' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODRM, LSMASH_4CC( 'o', 'd', 'r', 'm' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ODTT, LSMASH_4CC( 'o', 'd', 't', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_OHDR, LSMASH_4CC( 'o', 'h', 'd', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PADB, LSMASH_4CC( 'p', 'a', 'd', 'b' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PAEN, LSMASH_4CC( 'p', 'a', 'e', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PCLR, LSMASH_4CC( 'p', 'c', 'l', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PDIN, LSMASH_4CC( 'p', 'd', 'i', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PERF, LSMASH_4CC( 'p', 'e', 'r', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PITM, LSMASH_4CC( 'p', 'i', 't', 'm' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_RES , LSMASH_4CC( 'r', 'e', 's', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_RESC, LSMASH_4CC( 'r', 'e', 's', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_RESD, LSMASH_4CC( 'r', 'e', 's', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_RTNG, LSMASH_4CC( 'r', 't', 'n', 'g' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SBGP, LSMASH_4CC( 's', 'b', 'g', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SCHI, LSMASH_4CC( 's', 'c', 'h', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SCHM, LSMASH_4CC( 's', 'c', 'h', 'm' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SDEP, LSMASH_4CC( 's', 'd', 'e', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SDHD, LSMASH_4CC( 's', 'd', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SDTP, LSMASH_4CC( 's', 'd', 't', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SDVP, LSMASH_4CC( 's', 'd', 'v', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SEGR, LSMASH_4CC( 's', 'e', 'g', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SGPD, LSMASH_4CC( 's', 'g', 'p', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SIDX, LSMASH_4CC( 's', 'i', 'd', 'x' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SINF, LSMASH_4CC( 's', 'i', 'n', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SKIP, LSMASH_4CC( 's', 'k', 'i', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SMHD, LSMASH_4CC( 's', 'm', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SRMB, LSMASH_4CC( 's', 'r', 'm', 'b' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SRMC, LSMASH_4CC( 's', 'r', 'm', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SRPP, LSMASH_4CC( 's', 'r', 'p', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STBL, LSMASH_4CC( 's', 't', 'b', 'l' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STCO, LSMASH_4CC( 's', 't', 'c', 'o' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STDP, LSMASH_4CC( 's', 't', 'd', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSC, LSMASH_4CC( 's', 't', 's', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSD, LSMASH_4CC( 's', 't', 's', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSH, LSMASH_4CC( 's', 't', 's', 'h' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSS, LSMASH_4CC( 's', 't', 's', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSZ, LSMASH_4CC( 's', 't', 's', 'z' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STTS, LSMASH_4CC( 's', 't', 't', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STYP, LSMASH_4CC( 's', 't', 'y', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STZ2, LSMASH_4CC( 's', 't', 'z', '2' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SUBS, LSMASH_4CC( 's', 'u', 'b', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SWTC, LSMASH_4CC( 's', 'w', 't', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TFHD, LSMASH_4CC( 't', 'f', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TFDT, LSMASH_4CC( 't', 'f', 'd', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TFRA, LSMASH_4CC( 't', 'f', 'r', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TIBR, LSMASH_4CC( 't', 'i', 'b', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TIRI, LSMASH_4CC( 't', 'i', 'r', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TITL, LSMASH_4CC( 't', 'i', 't', 'l' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TKHD, LSMASH_4CC( 't', 'k', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TRAF, LSMASH_4CC( 't', 'r', 'a', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TRAK, LSMASH_4CC( 't', 'r', 'a', 'k' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TREF, LSMASH_4CC( 't', 'r', 'e', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TREX, LSMASH_4CC( 't', 'r', 'e', 'x' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TRGR, LSMASH_4CC( 't', 'r', 'g', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TRUN, LSMASH_4CC( 't', 'r', 'u', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TSEL, LSMASH_4CC( 't', 's', 'e', 'l' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_UDTA, LSMASH_4CC( 'u', 'd', 't', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_UINF, LSMASH_4CC( 'u', 'i', 'n', 'f' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ULST, LSMASH_4CC( 'u', 'l', 's', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_URL , LSMASH_4CC( 'u', 'r', 'l', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_URN , LSMASH_4CC( 'u', 'r', 'n', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_UUID, LSMASH_4CC( 'u', 'u', 'i', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_VMHD, LSMASH_4CC( 'v', 'm', 'h', 'd' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_VWDI, LSMASH_4CC( 'v', 'w', 'd', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_XML , LSMASH_4CC( 'x', 'm', 'l', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_YRRC, LSMASH_4CC( 'y', 'r', 'r', 'c' ), ISO );

DEFINE_BOX_TYPE( ISOM_BOX_TYPE_BTRT, LSMASH_4CC( 'b', 't', 'r', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CLAP, LSMASH_4CC( 'c', 'l', 'a', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_PASP, LSMASH_4CC( 'p', 'a', 's', 'p' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SRAT, LSMASH_4CC( 's', 'r', 'a', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_STSL, LSMASH_4CC( 's', 't', 's', 'l' ), ISO );

DEFINE_BOX_TYPE( ISOM_BOX_TYPE_FTAB, LSMASH_4CC( 'f', 't', 'a', 'b' ), ISO );

DEFINE_BOX_TYPE( ISOM_BOX_TYPE_HNTI, LSMASH_4CC( 'h', 'n', 't', 'i' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_RTP , LSMASH_4CC( 'r', 't', 'p', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_SDP , LSMASH_4CC( 's', 'd', 'p', ' ' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TIMS, LSMASH_4CC( 't', 'i', 'm', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TSRO, LSMASH_4CC( 't', 's', 'r', 'o' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_TSSY, LSMASH_4CC( 't', 's', 's', 'y' ), ISO );

/* iTunes Metadata */
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DATA, LSMASH_4CC( 'd', 'a', 't', 'a' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ILST, LSMASH_4CC( 'i', 'l', 's', 't' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_MEAN, LSMASH_4CC( 'm', 'e', 'a', 'n' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_NAME, LSMASH_4CC( 'n', 'a', 'm', 'e' ), ISO );

/* Tyrant extension */
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_CHPL, LSMASH_4CC( 'c', 'h', 'p', 'l' ), ISO );

/* Decoder Specific Info */
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ALAC, LSMASH_4CC( 'a', 'l', 'a', 'c' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_AVCC, LSMASH_4CC( 'a', 'v', 'c', 'C' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DAC3, LSMASH_4CC( 'd', 'a', 'c', '3' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DAMR, LSMASH_4CC( 'd', 'a', 'm', 'r' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DDTS, LSMASH_4CC( 'd', 'd', 't', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DEC3, LSMASH_4CC( 'd', 'e', 'c', '3' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_DVC1, LSMASH_4CC( 'd', 'v', 'c', '1' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_ESDS, LSMASH_4CC( 'e', 's', 'd', 's' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_HVCC, LSMASH_4CC( 'h', 'v', 'c', 'C' ), ISO );
DEFINE_BOX_TYPE( ISOM_BOX_TYPE_WFEX, LSMASH_4CC( 'w', 'f', 'e', 'x' ), ISO );

DEFINE_BOX_TYPE( QT_BOX_TYPE_ALLF, LSMASH_4CC( 'A', 'l', 'l', 'F' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CLEF, LSMASH_4CC( 'c', 'l', 'e', 'f' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CLIP, LSMASH_4CC( 'c', 'l', 'i', 'p' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CLLI, LSMASH_4CC( 'c', 'l', 'l', 'i' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CRGN, LSMASH_4CC( 'c', 'r', 'g', 'n' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CTAB, LSMASH_4CC( 'c', 't', 'a', 'b' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_ENOF, LSMASH_4CC( 'e', 'n', 'o', 'f' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_GMHD, LSMASH_4CC( 'g', 'm', 'h', 'd' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_GMIN, LSMASH_4CC( 'g', 'm', 'i', 'n' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_ILST, LSMASH_4CC( 'i', 'l', 's', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_IMAP, LSMASH_4CC( 'i', 'm', 'a', 'p' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_KEYS, LSMASH_4CC( 'k', 'e', 'y', 's' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_KMAT, LSMASH_4CC( 'k', 'm', 'a', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_LOAD, LSMASH_4CC( 'l', 'o', 'a', 'd' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_LOOP, LSMASH_4CC( 'L', 'O', 'O', 'P' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_MATT, LSMASH_4CC( 'm', 'a', 't', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_MDCV, LSMASH_4CC( 'm', 'd', 'c', 'v' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_META, LSMASH_4CC( 'm', 'e', 't', 'a' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_PNOT, LSMASH_4CC( 'p', 'n', 'o', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_PROF, LSMASH_4CC( 'p', 'r', 'o', 'f' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_SELO, LSMASH_4CC( 'S', 'e', 'l', 'O' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_STPS, LSMASH_4CC( 's', 't', 'p', 's' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_TAPT, LSMASH_4CC( 't', 'a', 'p', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_TEXT, LSMASH_4CC( 't', 'e', 'x', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_WLOC, LSMASH_4CC( 'W', 'L', 'O', 'C' ), QTFF );

DEFINE_BOX_TYPE( QT_BOX_TYPE_ALIS, LSMASH_4CC( 'a', 'l', 'i', 's' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_RSRC, LSMASH_4CC( 'r', 's', 'r', 'c' ), QTFF );

DEFINE_BOX_TYPE( QT_BOX_TYPE_CHAN, LSMASH_4CC( 'c', 'h', 'a', 'n' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_COLR, LSMASH_4CC( 'c', 'o', 'l', 'r' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_CSPC, LSMASH_4CC( 'c', 's', 'p', 'c' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_ENDA, LSMASH_4CC( 'e', 'n', 'd', 'a' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_FIEL, LSMASH_4CC( 'f', 'i', 'e', 'l' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_FRMA, LSMASH_4CC( 'f', 'r', 'm', 'a' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_GAMA, LSMASH_4CC( 'g', 'a', 'm', 'a' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_SGBT, LSMASH_4CC( 's', 'g', 'b', 't' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_WAVE, LSMASH_4CC( 'w', 'a', 'v', 'e' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_TERMINATOR, 0x00000000, QTFF );

/* Decoder Specific Info */
DEFINE_BOX_TYPE( QT_BOX_TYPE_ALAC, LSMASH_4CC( 'a', 'l', 'a', 'c' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_ESDS, LSMASH_4CC( 'e', 's', 'd', 's' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_GLBL, LSMASH_4CC( 'g', 'l', 'b', 'l' ), QTFF );
DEFINE_BOX_TYPE( QT_BOX_TYPE_MP4A, LSMASH_4CC( 'm', 'p', '4', 'a' ), QTFF );
