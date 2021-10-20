#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include "ngtdm.h"

const double BAD_ROI_FVAL = 0.0;
const double EPS = 2.2e-16;

void NGTDM_features::initialize(int minI, int maxI, const ImageMatrix& im)
{
	//==== Check if the ROI is degenerate (equal intensity)
	if (minI == maxI)
	{
		bad_roi_data = true;
		return;
	}

	//==== Make a list of intensity clusters (zones)
	using AveNeighborhoodInte = std::pair<PixIntens, double>;	// Pairs of (intensity, average intensity of all 8 neighbors)
	std::vector<AveNeighborhoodInte> Z;

	//==== While scanning clusters, learn unique intensities 
	std::unordered_set<PixIntens> U;

	const pixData& D = im.ReadablePixels();

	// Gather zones
	for (int row = 0; row < D.height(); row++)
		for (int col = 0; col < D.width(); col++)
		{
			// Find a non-blank pixel
			PixIntens pi = D(row, col);
			if (pi == 0)
				continue;

			// Evaluate the neighborhood
			PixIntens neigsI = 0;

			int nd = 0;	// Number of dependencies

			if (D.safe(row - 1, col))	// North
			{
				neigsI += D(row - 1, col);
				nd++;
			}
			if (D.safe(row - 1, col + 1))	// North-East
			{
				neigsI += D(row - 1, col + 1);
				nd++;
			}
			if (D.safe(row, col + 1))	// East
			{
				neigsI += D(row, col + 1);
				nd++;
			}
			if (D.safe(row + 1, col + 1))	// South-East
			{
				neigsI += D(row + 1, col + 1);
				nd++;
			}
			if (D.safe(row + 1, col))	// South
			{
				neigsI += D(row + 1, col);
				nd++;
			}
			if (D.safe(row + 1, col - 1))	// South-West
			{
				neigsI += D(row + 1, col - 1);
				nd++;
			}
			if (D.safe(row, col - 1))	// West
			{
				neigsI += D(row, col - 1);
				nd++;
			}
			if (D.safe(row - 1, col - 1))	// North-West
			{
				neigsI += D(row - 1, col - 1);
				nd++;
			}

			// Save the intensity's average neigborhood intensity
			neigsI /= nd;
			AveNeighborhoodInte z = { pi, neigsI };
			Z.push_back(z);

			// Update unique intensities
			U.insert(pi);
		}

	//==== Fill the matrix

	Ng = (decltype(Ng))U.size();
	Ngp = Ng;

	// --allocate the matrix
	P.resize(Ng, 0);
	S.resize(Ng, 0);
	N.resize(Ng, 0);

	// --Set to vector to be able to know each intensity's index
	std::vector<PixIntens> I(U.begin(), U.end());
	std::sort(I.begin(), I.end());	// Optional

	// --Calculate N and S
	for (auto& z : Z)
	{
		// row
		auto iter = std::find(I.begin(), I.end(), z.first);
		int row = int(iter - I.begin());
		// col
		int col = z.second;	// 1-based
		// increment
		N[row]++;
		// --S
		PixIntens pi = z.first;
		double aveNeigI = z.second;
		S[row] += std::abs(pi - aveNeigI);
		// --Nvp
		if (aveNeigI > 0.0)
			Nvp++;
	}

	// --Calculate P
	for (int i = 0; i < Ng; i++)
		P[i] = double(Ng) / (im.height * im.width);
}

// Coarseness
double NGTDM_features::calc_Coarseness()
{
	// Prevent using bad data 
	if (bad_roi_data)
		return BAD_ROI_FVAL;

	// Calculate the feature
	double sum = 0.0;
	for (int i = 0; i < Ng; i++)
		sum += P[i] * S[i];
	double retval = 1.0 / sum;
	return retval;
}

// Contrast
double NGTDM_features::calc_Contrast()
{
	// Prevent using bad data 
	if (bad_roi_data)
		return BAD_ROI_FVAL;

	// Calculate the feature
	// --term 1
	double sum = 0.0;
	for (int i=0; i<Ng; i++)
		for (int j = 0; j < Ng; j++)
		{
			double tmp = P[i] * P[j] * ((i+1) - (j+1)) * ((i+1) - (j+1));
			sum += tmp;
		}
	int Ngp_p2 = Ngp > 1 ? Ngp * (Ngp - 1) : Ngp;
	double term1 = sum / double(Ngp_p2);

	// --term 2
	sum = 0.0;
	for (int i = 0; i < Ng; i++)
		sum += S[i];
	double term2 = sum / double(Ngp);

	double retval = term1 * term2;
	return retval;
}

// Busyness
double NGTDM_features::calc_Busyness()
{
	// Prevent using bad data 
	if (bad_roi_data)
		return BAD_ROI_FVAL;

	// Trivial case?
	if (Ngp == 1)
		return 0.0;

	// Calculate the feature
	double sum1 = 0.0;
	for (int i = 0; i < Ng; i++)
		sum1 += P[i] * S[i];

	double sum2 = 0.0;
	for (int i = 0; i < Ng; i++)
		for (int j = 0; j < Ng; j++)
		{
			double tmp = P[i] * double(i) - P[j] * double(j);
			sum2 += std::abs (tmp);
		}

	double retval = sum1 / sum2;
	return retval;
}

// Complexity
double NGTDM_features::calc_Complexity()
{
	// Prevent using bad data 
	if (bad_roi_data)
		return BAD_ROI_FVAL;

	// Calculate the feature
	double sum = 0.0;
	for (int i = 0; i < Ng; i++)
		for (int j = 0; j < Ng; j++)
		{
			double tmp = std::abs((i+1)-(j+1)) * (P[i]*S[i] + P[j]*S[j]) / (P[i]+P[j]) ;
			sum += tmp;
		}

	double retval = sum / double(Nvp);
	return retval;
}

// Strength
double NGTDM_features::calc_Strength()
{
	// Prevent using bad data 
	if (bad_roi_data)
		return BAD_ROI_FVAL;

	// Calculate the feature
	double sum1 = 0.0;
	for (int i = 0; i < Ng; i++)
		for (int j = 0; j < Ng; j++)
		{
			double tmp = (P[i] + P[j]) * ((i+1) - (j+1)) * ((i+1) - (j+1));
			sum1 += tmp;
		}

	double sum2 = 0.0;
	for (int i = 0; i < Ng; i++)
		sum2 += S[i];

	double retval = sum1 / sum2;
	return retval;
}
