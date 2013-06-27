/*
 * ProbabilisticRvmClassifier.cpp
 *
 *  Created on: 14.06.2013
 *      Author: Patrik Huber
 */

#include "classification/ProbabilisticRvmClassifier.hpp"
#include "classification/RvmClassifier.hpp"
#include "mat.h"
#ifdef WIN32
	#define BOOST_ALL_DYN_LINK	// Link against the dynamic boost lib. Seems to be necessary because we use /MD, i.e. link to the dynamic CRT.
	#define BOOST_ALL_NO_LIB	// Don't use the automatic library linking by boost with VS2010 (#pragma ...). Instead, we specify everything in cmake.
#endif
#include "boost/filesystem/path.hpp"
#include <iostream>
#include <stdexcept>

using boost::filesystem::path;
using std::make_pair;
using std::make_shared;
using std::invalid_argument;
using std::runtime_error;
using std::logic_error;

namespace classification {

ProbabilisticRvmClassifier::ProbabilisticRvmClassifier() :
		svm(), logisticA(0), logisticB(0) {}

ProbabilisticRvmClassifier::ProbabilisticRvmClassifier(shared_ptr<RvmClassifier> svm, double logisticA, double logisticB) :
		svm(svm), logisticA(logisticA), logisticB(logisticB) {}

ProbabilisticRvmClassifier::~ProbabilisticRvmClassifier() {}

pair<bool, double> ProbabilisticRvmClassifier::classify(const Mat& featureVector) const {
	double hyperplaneDistance = svm->computeHyperplaneDistance(featureVector);
	double probability = 1.0f / (1.0f + exp(logisticA + logisticB * hyperplaneDistance));
	return make_pair(svm->classify(hyperplaneDistance), probability);
}

void ProbabilisticRvmClassifier::setLogisticParameters(double logisticA, double logisticB) {
	this->logisticA = logisticA;
	this->logisticB = logisticB;
}

/*
shared_ptr<ProbabilisticRvmClassifier> ProbabilisticRvmClassifier::loadMatlab(const string& classifierFilename, const string& logisticFilename)
{
	// Load sigmoid stuff:
	double logisticA, logisticB;
	MATFile *pmatfile;
	mxArray *pmxarray; // =mat
	pmatfile = matOpen(logisticFilename.c_str(), "r");
	if (pmatfile == 0) {
		throw invalid_argument("ProbabilisticRvmClassifier: Unable to open the thresholds-file: " + logisticFilename);
	} 

	//TODO is there a case (when svm+wvm from same trainingdata) when there exists only a posterior_svm, and I should use this here?
	pmxarray = matGetVariable(pmatfile, "posterior_wrvm");
	if (pmxarray == 0) {
		throw runtime_error("ProbabilisticRvmClassifier: Unable to find the vector posterior_wrvm. If you don't want probabilistic output, don't use a probabilistic classifier.");
		//logisticA = logisticB = 0; // TODO: Does it make sense to continue?
	} else {
		double* matdata = mxGetPr(pmxarray);
		const mwSize *dim = mxGetDimensions(pmxarray);
		if (dim[1] != 2) {
			throw runtime_error("ProbabilisticRvmClassifier: Size of vector posterior_wrvm !=2. If you don't want probabilistic output, don't use a probabilistic classifier.");
			//logisticA = logisticB = 0; // TODO: Does it make sense to continue?
		} else {
			logisticB = matdata[0];
			logisticA = matdata[1];
		}
		mxDestroyArray(pmxarray);
	}
	matClose(pmatfile);

	// Load the detector and thresholds:
	shared_ptr<RvmClassifier> svm = RvmClassifier::loadMatlab(classifierFilename);
	return make_shared<ProbabilisticRvmClassifier>(svm, logisticA, logisticB);
}
*/

shared_ptr<ProbabilisticRvmClassifier> ProbabilisticRvmClassifier::loadConfig(const ptree& subtree)
{
	path classifierFile = subtree.get<path>("classifierFile");
	string a = classifierFile.extension().string();
	if (classifierFile.extension() == ".mat") {
		//return loadMatlab(classifierFile.string(), subtree.get<string>("thresholdsFile"));
		throw logic_error("ProbabilisticRvmClassifier: Loading of .mat RVM's is not yet supported!");
	} else {
		double logisticA = 0;
		double logisticB = 0;
		shared_ptr<RvmClassifier> svm = RvmClassifier::loadText(classifierFile.string());
		//return make_shared<ProbabilisticSvmClassifier>(svm, logisticA, logisticB);
		return make_shared<ProbabilisticRvmClassifier>(svm); // default values for a, b
	}

}

} /* namespace classification */
