////////////////////////////////////////////////////////////////////////////////
//
// Filename: 	filtertb.cpp
//
// Project:	DSP Filtering Example Project
//
// Purpose:	A generic filter testbench class
//
// Creator:	Dan Gisselquist, Ph.D.
//		Gisselquist Technology, LLC
//
////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2017, Gisselquist Technology, LLC
//
// This program is free software (firmware): you can redistribute it and/or
// modify it under the terms of the GNU General Public License as published
// by the Free Software Foundation, either version 3 of the License, or (at
// your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTIBILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program.  (It's in the $(ROOT)/doc directory.  Run make with no
// target there if the PDF file isn't present.)  If not, see
// <http://www.gnu.org/licenses/> for a copy.
//
// License:	GPL, v3, as defined and found on www.gnu.org,
//		http://www.gnu.org/licenses/gpl.html
//
//
////////////////////////////////////////////////////////////////////////////////
//
//
#include "filtertb.h"

static int	sbits(uint32_t val, int b) {
	int	s;

	s = (val << (sizeof(int)*8-b));
	s >>= (sizeof(int)*8-b);
	return	s;
}

static unsigned	ubits(uint32_t val, int b) {
	return	val &= (1<<b)-1;
}

template<class VFLTR> void	FILTERTB<VFLTR>::tick(void) {
	bool	ce;
	int	vec[2];

	ce = (TESTB<VFLTR>::m_core->i_ce);
	vec[0] = sbits(TESTB<VFLTR>::m_core->i_sample, IW());

	TESTB<VFLTR>::tick();

	vec[1] = sbits(TESTB<VFLTR>::m_core->o_result, OW());

	if ((ce)&&(result_fp))
		fwrite(vec, sizeof(int), 2, result_fp);
}

template<class VFLTR> void	FILTERTB<VFLTR>::reset(void) {
	TESTB<VFLTR>::m_core->i_tap   = 0;
	TESTB<VFLTR>::m_core->i_sample= 0;
	TESTB<VFLTR>::m_core->i_ce    = 0;
	TESTB<VFLTR>::m_core->i_tap_wr= 0;

	TESTB<VFLTR>::reset();

	TESTB<VFLTR>::m_core->i_reset = 0;
}

template<class VFLTR> void	FILTERTB<VFLTR>::apply(int nlen, int *data) {
// printf("FILTERTB::apply(%d, ...)\n", nlen);
	TESTB<VFLTR>::m_core->i_reset  = 0;
	TESTB<VFLTR>::m_core->i_tap_wr = 0;
	TESTB<VFLTR>::m_core->i_ce     = 0;
	tick();
	for(int i=0; i<nlen; i++) {
		// Make sure the CE line is high
		TESTB<VFLTR>::m_core->i_ce     = 1;

		// Strip off any excess bits
		TESTB<VFLTR>::m_core->i_sample= ubits(data[i], IW());

		// Apply the filter
		tick();

		// Sign extend the result
		data[i] = sbits(TESTB<VFLTR>::m_core->o_result, OW());

		if (m_nclks > 1) {
			TESTB<VFLTR>::m_core->i_ce     = 0;
			for(int k=1; k<m_nclks; k++)
				tick();
		}
	}
	TESTB<VFLTR>::m_core->i_ce     = 0;
}

template<class VFLTR> void	FILTERTB<VFLTR>::load(int  ntaps,  int *data) {
// printf("FILTERTB::load(%d, ...)\n", ntaps);
	TESTB<VFLTR>::m_core->i_reset = 0;
	TESTB<VFLTR>::m_core->i_ce    = 0;
	TESTB<VFLTR>::m_core->i_tap_wr= 1;
	for(int i=0; i<ntaps; i++) {
		// Strip off any excess bits
		TESTB<VFLTR>::m_core->i_tap = ubits(data[i], TW());

		// Apply the filter
		tick();
	}
	TESTB<VFLTR>::m_core->i_tap_wr= 0;

	clear_cache();
}

template<class VFLTR> void	FILTERTB<VFLTR>::test(int  nlen, int *data) {
	const	bool	debug = false;
	assert(nlen > 0);

	reset();

	TESTB<VFLTR>::m_core->i_reset  = 0;
	TESTB<VFLTR>::m_core->i_tap_wr = 0;
	TESTB<VFLTR>::m_core->i_ce = 1;

	int	tstcounts = nlen+DELAY();
	for(int i=0; i<tstcounts; i++) {
		int	v;

		TESTB<VFLTR>::m_core->i_ce = 1;
		// Strip off any excess bits
		if (i >= nlen)
			TESTB<VFLTR>::m_core->i_sample = v = 0;
		else
			TESTB<VFLTR>::m_core->i_sample = ubits(data[i], IW());

		if (debug)
			printf("%3d, %3d, %d : Input : %5d[%6x] ", i, DELAY(), nlen, v,v );

		// Apply the filter
		tick();

		// Sign extend the result
		v = sbits(TESTB<VFLTR>::m_core->o_result, OW());

		if (i >= DELAY()) {
			if (debug) printf("Read    :%8d[%8x]\n", v, v);
			data[i-DELAY()] = v;
		} else if (debug)
			printf("Discard : %2d\n", v);

		// Deal with any filters requiring multiple clocks
		TESTB<VFLTR>::m_core->i_ce = 0;
		for(int k=1; k<m_nclks; k++)
			tick();
	}
	TESTB<VFLTR>::m_core->i_ce = 0;
}

template<class VFLTR> int	FILTERTB<VFLTR>::operator[](const int tap) {

	if ((tap < 0)||(tap >= 2*NTAPS()))
		return 0;
	else if (!m_hk) {
		int	nlen = 2*NTAPS();
		m_hk = new int[nlen];

		// Create an input vector with a single impulse in it
		for(int i=0; i<nlen; i++)
			m_hk[i] = 0;
		m_hk[0] = -(1<<(IW()-1));

		// Apply the filter to the impulse vector
		test(nlen, m_hk);

		// Set our m_hk vector based upon the results
		for(int i=0; i<nlen; i++) {
			int	shift;
			shift = IW()-1;
			m_hk[i] >>= shift;
			m_hk[i] = -m_hk[i];
		}
	}

	// if (m_hk[tap] != 0) printf("Hk[%4d] = %8d\n", tap, m_hk[tap]);
	return m_hk[tap];
}

template<class VFLTR> void	FILTERTB<VFLTR>::testload(int nlen, int *data) {
// printf("FILTERTB::testload(%d, ...)\n", nlen);
	load(nlen, data);
	reset();

	for(int k=0; k<nlen; k++) {
		int	m = (*this)[k];
		if (data[k] != m)
			printf("Data[k] = %d != (*this)[k] = %d\n", data[k], m);
		assert(data[k] == m);
	}
	for(int k=nlen; k<2*DELAY(); k++)
		assert(0 == (*this)[k]);
}

template<class VFLTR> bool	FILTERTB<VFLTR>::test_overflow(void) {
// printf("TESTING-BIBO\n");
	int	nlen = 2*NTAPS();
	int	*input  = new int[nlen],
		*output = new int[nlen];
	int	maxv = (1<<(IW()-1))-1;
	bool	pass = true, tested = false;

	// maxv = 1;

	for(int k=0; k<nlen; k++) {
		// input[v] * (*this)[(NTAPS-1)-v]
		if ((*this)[NTAPS()-1-k] < 0)
			input[k] = -maxv;
		else
			input[k] =  maxv;
		output[k]= input[k];
	}

	test(nlen, output);

	for(int k=0; k<nlen; k++) {
		long	acc = 0;
		bool	all = true;
		for(int v = 0; v<NTAPS(); v++) {
			if (k-v >= 0) {
				acc += input[k-v] * (*this)[v];
				if (acc < 0)
					all = false;
			} else
				all = false;
		}

		if (all)
			tested = true;

		pass = (pass)&&(output[k] == acc);
		assert(output[k] == acc);
	}

	delete[] input;
	delete[] output;
	return (pass)&&(tested);
}

template<class VFLTR> void	FILTERTB<VFLTR>::response(int nfreq,
		COMPLEX *rvec, double mag) {
	int	nlen = NTAPS();
	int	dlen = nlen + DELAY() + 2*NTAPS(), doffset = DELAY()+NTAPS();
	int	*data = new int[dlen],
		*input= new int[dlen];

	// Nh tap filter
	// Nv length vector
	// Nh+Nv-1 length output
	// But we want our output length to not include any runups
	//
	// Nh runup + Nv + Nh rundown + delay
	mag = mag * ((1<<(IW()-1))-1);

	for(int i=0; i<nfreq; i++) {
		double	dtheta = 2.0 * M_PI * i / (double)nfreq / 2.0,
			theta=0.;
		COMPLEX	acc = 0.;

		theta = 0;
		for(int j=0; j<dlen; j++) {
			double	dv = mag * cos(theta);

			theta += dtheta;
			data[j] = dv;
			input[j] = dv;
		}

		apply(dlen, data);

		theta = 0.0;
		for(int j=0; j<nlen; j++) {
			double	cs = cos(theta) / mag,
				sn = sin(theta) / mag;

			theta -= dtheta;

			real(acc) += cs * data[j+doffset];
			imag(acc) += sn * data[j+doffset];
		}

		// Repeat what should produce the same response, but using
		// a 90 degree phase offset.  Do this for all but the zero
		// frequency
		if (i > 0) {
			theta = 0.0;
			for(int j=0; j<dlen; j++) {
				double	dv = mag * sin(theta);

				theta += dtheta;
				data[j] = dv;
				input[j] = dv;
			}

			apply(dlen, data);

			theta = 0.0;
			for(int j=0; j<nlen; j++) {
				double	cs = cos(theta) / mag,
					sn = sin(theta) / mag;

				theta -= dtheta;

				real(acc) -= sn * data[j+doffset];
				imag(acc) += cs * data[j+doffset];
			}

		} rvec[i] = acc * (1./ nlen);

		printf("RSP[%4d / %4d] = %10.1f + %10.1f\n",
			i, nfreq, real(acc), imag(acc));
	}

	delete[] data;
	delete[] input;

	{
		FILE* fp;
		fp = fopen("filter_tb.dbl","w");
		fwrite(rvec, sizeof(COMPLEX), nfreq, fp);
		fclose(fp);
	}
}

template<class VFLTR> void FILTERTB<VFLTR>::measure_lowpass(double &fp, double &fs,
			double &depth, double &ripple) {
	const	int	NLEN = 16*NTAPS();
	COMPLEX	*data = new COMPLEX[NLEN];
	double	*magv = new double[NLEN];
	double	dc, maxpass, minpass, maxstop;
	int	midcut;
	bool	passband_ripple = false;

	response(NLEN, data);

	for(int k=0; k<NLEN; k++) {
		magv[k]= norm(data[k]);
	}

	midcut = NLEN-1;
	dc = magv[0];
	for(int k=0; k<NLEN; k++)
		if (magv[k] < 0.25 * dc) {
			midcut = k;
			break;
		}
	maxpass = dc;
	minpass = dc;
	for(int k=midcut; k>= 0; k--)
		if (magv[k] > maxpass)
			maxpass = magv[k];
	for(int k=midcut; k>= 0; k--)
		if ((magv[k] < minpass)&&(magv[k+1] > magv[k])) {
			minpass = magv[k];
			passband_ripple = true;
		}
	if (!passband_ripple)
		minpass = maxpass / sqrt(2.0);
	for(int k=midcut; k>= 0; k--)
		if (magv[k] > minpass) {
			fp = k;
			break;
		}

	maxstop = magv[NLEN-1];
	for(int k=midcut; k<NLEN; k++)
		if ((fabs(magv[k]) > fabs(magv[k-1]))&&(fabs(magv[k])> maxstop))
			maxstop = fabs(magv[k]);
	for(int k=midcut; k<NLEN; k++)
		if (magv[k] <= maxstop) {
			fs = k;
			break;
		}

printf("MAXPASS= %f\n", maxpass);
printf("MINPASS= %f\n", minpass);
printf("FP     = %f\n", fp);
printf("FS     = %f\n", fs);
printf("DC     = %f\n", dc);
printf("--------\n");

	ripple = 2.0 * (maxpass - minpass)/(maxpass + minpass);
	depth  = 10.0*log(maxstop/dc)/log(10.0);
	fs = fs / NLEN / 2.;
	fp = fp / NLEN / 2.;
	delete[]	data;
}