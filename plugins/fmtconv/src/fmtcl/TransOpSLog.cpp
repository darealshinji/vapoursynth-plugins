/*****************************************************************************

        TransOpSLog.cpp
        Author: Laurent de Soras, 2015

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "fmtcl/TransOpSLog.h"

#include <algorithm>

#include <cassert>
#include <cmath>



namespace fmtcl
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



TransOpSLog::TransOpSLog (bool inv_flag, bool slog2_flag)
:	_inv_flag (inv_flag)
,	_slog2_flag (slog2_flag)
{
	// Nothing
}



// 1 lin is reference white, peak white at 10 lin.
double	TransOpSLog::operator () (double x) const
{
	static const double  a  = 0.037584;
	static const double  b  = 0.432699;
	static const double  c1 = 0.616596;
	static const double  c2 = 0.03;
	static const double  c  = c1 + c2;
	static const double  s2 = 219.0 / 155.0;

	double         y = x;
	if (_inv_flag)
	{
		if (x < c2)
		{
			y = (x - c2) / 5.0;
		}
		else
		{
			y = pow (10, (y - c) / b) - a;
		}
		if (_slog2_flag)
		{
			y *= s2;
		}
	}
	else
	{
		if (_slog2_flag)
		{
			y /= s2;
		}
		if (x < 0)
		{
			y = x * 5 + c2;
		}
		else
		{
			y = b * log10 (x + a) + c;
		}
	}

	return (y);
}



double	TransOpSLog::get_max () const
{
	return (_slog2_flag) ? 10.0 * 219 / 155 : 10.0;
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}	// namespace fmtcl



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
