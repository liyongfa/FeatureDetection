/*
 * train-frame-extract-nnet.cpp
 *
 *  Created on: 11.09.2014
 *      Author: Patrik Huber
 *
 * Ideally we'd use video, match against highres stills? (and not the lowres). Because if still are lowres/bad, we could match a
 * good frame against a bad gallery, which would give a bad score, but it shouldn't, because the frame is good.
 * Do we have labels for this?
 * Maybe "sensor_id","stage_id","env_id","illuminant_id" in the files emailed by Ross.
 *
 * Example:
 * train-frame-extract-nnet ...
 *   
 */

#include <chrono>
#include <memory>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <random>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

#ifdef WIN32
	#define BOOST_ALL_DYN_LINK	// Link against the dynamic boost lib. Seems to be necessary because we use /MD, i.e. link to the dynamic CRT.
	#define BOOST_ALL_NO_LIB	// Don't use the automatic library linking by boost with VS2010 (#pragma ...). Instead, we specify everything in cmake.
#endif
#include "boost/program_options.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/filesystem.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/archive/text_iarchive.hpp"

#include "tiny_cnn.h"

#include <frsdk/config.h>
#include <frsdk/enroll.h>
#include <frsdk/match.h>

#include "facerecognition/pasc.hpp"
#include "facerecognition/utils.hpp"

#include "logging/LoggerFactory.hpp"

namespace po = boost::program_options;
using logging::Logger;
using logging::LoggerFactory;
using logging::LogLevel;
using cv::Mat;
using boost::filesystem::path;
using boost::lexical_cast;
using std::cout;
using std::endl;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using std::string;

// for the CoutEnrol
std::ostream&
operator<<(std::ostream& o, const FRsdk::Position& p)
{
	o << "[" << p.x() << ", " << p.y() << "]";
	return o;
}

// templates instead of inheritance? (compile-time fine, don't need runtime selection)
class FaceVacsEngine
{
public:
	// tempDirectory a path where temp files stored... only FIRs? More? firDir?
	FaceVacsEngine(path frsdkConfig, path tempDirectory) : tempDir(tempDirectory) {
		// initialize and resource allocation
		cfg = std::make_unique<FRsdk::Configuration>(frsdkConfig.string());
		firBuilder = std::make_unique<FRsdk::FIRBuilder>(*cfg.get());
		// initialize matching facility
		me = std::make_unique<FRsdk::FacialMatchingEngine>(*cfg.get());
		// load the fir population (gallery)
		population = std::make_unique<FRsdk::Population>(*cfg.get());

		// Add somewhere: 
		//catch (const FRsdk::FeatureDisabled& e) 
		//catch (const FRsdk::LicenseSignatureMismatch& e)
	};

	// because it often stays the same
	void enrollGallery(std::vector<facerecognition::FaceRecord> galleryRecords, path databasePath)
	{

		// We first enroll the whole gallery:
		
		cout << "Loading the input images..." << endl;
		FRsdk::SampleSet enrollmentImages;
		FRsdk::Image img(FRsdk::ImageIO::load(R"(C:\Users\Patrik\Documents\GitHub\aaatmp\04261d1822.jpg)"));
		enrollmentImages.push_back(FRsdk::Sample(img));
		// create an enrollment processor
		FRsdk::Enrollment::Processor proc(*cfg);
		// create the needed interaction instances
		FRsdk::Enrollment::Feedback feedback(new EnrolCoutFeedback("bla.fir"));

		// do the enrollment    
		proc.process(enrollmentImages.begin(), enrollmentImages.end(), feedback);


		for (auto& r : galleryRecords) {
			path imagePath(R"(C:\Users\Patrik\Documents\GitHub\aaatmp\04261d1822.jpg)"); //databasePath / r.dataPath;
			std::ifstream firIn(imagePath.string(), std::ios::in | std::ios::binary);
			if (firIn.is_open() && firIn.good()) {
				auto fir = firBuilder->build(firIn);
				population->append(fir, imagePath.string());
			}
		}
	};

	// matches a pair of images, Cog LMs
	double match(cv::Mat first, cv::Mat second)
	{
		
		return 0.0;
	};
	// matches a pair of images, given LMs as Cog init
	// match(fn, fn, ...)
	// match(Mat, Mat, ...)

private:
	path tempDir;
	std::unique_ptr<FRsdk::Configuration> cfg; // static? (recommendation by fvsdk doc)
	std::unique_ptr<FRsdk::FIRBuilder> firBuilder;
	std::unique_ptr<FRsdk::FacialMatchingEngine> me;
	std::unique_ptr<FRsdk::Population> population;

	class EnrolCoutFeedback : public FRsdk::Enrollment::FeedbackBody
	{
	public:
		EnrolCoutFeedback(const std::string& firFilename)
			: firFN(firFilename), firvalid(false) { }
		~EnrolCoutFeedback() {}

		void start() {
			firvalid = false;
			std::cout << "start" << std::endl;
		}

		void processingImage(const FRsdk::Image& img)
		{
			std::cout << "processing image[" << img.name() << "]" << std::endl;
		}

		void eyesFound(const FRsdk::Eyes::Location& eyeLoc)
		{
			std::cout << "found eyes at [" << eyeLoc.first
				<< " " << eyeLoc.second << "; confidences: "
				<< eyeLoc.firstConfidence << " "
				<< eyeLoc.secondConfidence << "]" << std::endl;
		}

		void eyesNotFound()
		{
			std::cout << "eyes not found" << std::endl;
		}

		void sampleQualityTooLow() {
			std::cout << "sampleQualityTooLow" << std::endl;
		}


		void sampleQuality(const float& f) {
			std::cout << "Sample Quality: " << f << std::endl;
		}

		void success(const FRsdk::FIR& fir_)
		{
			fir = new FRsdk::FIR(fir_);
			std::cout
				<< "successful enrollment";
			if (firFN != std::string("")) {

				std::cout << " FIR[filename,id,size] = [\""
					<< firFN.c_str() << "\",\"" << (fir->version()).c_str() << "\","
					<< fir->size() << "]";
				// write the fir
				std::ofstream firOut(firFN.c_str(),
					std::ios::binary | std::ios::out | std::ios::trunc);
				firOut << *fir;
			}
			firvalid = true;
			std::cout << std::endl;
		}

		void failure() { std::cout << "failure" << std::endl; }

		void end() { std::cout << "end" << std::endl; }

		const FRsdk::FIR& getFir() const {
			// call only if success() has been invoked    
			if (!firvalid)
				throw InvalidFIRAccessError();

			return *fir;
		}

		bool firValid() const {
			return firvalid;
		}

	private:
		FRsdk::CountedPtr<FRsdk::FIR> fir;
		std::string firFN;
		bool firvalid;
	};

	class InvalidFIRAccessError : public std::exception
	{
	public:
		InvalidFIRAccessError() throw() :
			msg("Trying to access invalid FIR") {}
		~InvalidFIRAccessError() throw() { }
		const char* what() const throw() { return msg.c_str(); }
	private:
		std::string msg;
	};

};

// Caution: This will eat a lot of RAM, 1-2 GB for 600 RGB frames at 720p
vector<Mat> getFrames(path videoFilename)
{
	vector<Mat> frames;

	cv::VideoCapture cap(videoFilename.string());
	if (!cap.isOpened())
		throw("Couldn't open video file.");

	Mat img;
	while (cap.read(img)) {
		frames.emplace_back(img.clone()); // we need to clone, otherwise we'd just get a reference to the same 'img' instance
	}

	return frames;
}

// pascFrameNumber starts with 1. Your counting might start with 0, so add 1 to it before passing it here.
std::string getPascFrameName(path videoFilename, int pascFrameNumber)
{
	std::ostringstream ss;
	ss << std::setw(3) << std::setfill('0') << pascFrameNumber;
	return videoFilename.stem().string() + "/" + videoFilename.stem().string() + "-" + ss.str() + ".jpg";
}

int main(int argc, char *argv[])
{
	#ifdef WIN32
	//_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF ); // dump leaks at return
	//_CrtSetBreakAlloc(287);
	#endif
	
	string verboseLevelConsole;
	path inputDirectoryVideos, inputDirectoryStills;
	path inputLandmarks;
	path outputPath;

	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h",
				"produce help message")
			("verbose,v", po::value<string>(&verboseLevelConsole)->implicit_value("DEBUG")->default_value("INFO","show messages with INFO loglevel or below."),
				  "specify the verbosity of the console output: PANIC, ERROR, WARN, INFO, DEBUG or TRACE")
			("input-videos,i", po::value<path>(&inputDirectoryVideos)->required(),
				"input folder with training videos")
			("input-stills,j", po::value<path>(&inputDirectoryStills)->required(),
				"input folder with training videos")
			("landmarks,l", po::value<path>(&inputLandmarks)->required(),
				"input landmarks in boost::serialization text format")
			("output,o", po::value<path>(&outputPath)->default_value("."),
				"path to an output folder")
		;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm); // style(po::command_line_style::unix_style | po::command_line_style::allow_long_disguise)
		if (vm.count("help")) {
			cout << "Usage: train-frame-extract-nnet [options]\n";
			cout << desc;
			return EXIT_SUCCESS;
		}
		po::notify(vm);

	}
	catch (po::error& e) {
		cout << "Error while parsing command-line arguments: " << e.what() << endl;
		cout << "Use --help to display a list of options." << endl;
		return EXIT_SUCCESS;
	}

	LogLevel logLevel;
	if(boost::iequals(verboseLevelConsole, "PANIC")) logLevel = LogLevel::Panic;
	else if(boost::iequals(verboseLevelConsole, "ERROR")) logLevel = LogLevel::Error;
	else if(boost::iequals(verboseLevelConsole, "WARN")) logLevel = LogLevel::Warn;
	else if(boost::iequals(verboseLevelConsole, "INFO")) logLevel = LogLevel::Info;
	else if(boost::iequals(verboseLevelConsole, "DEBUG")) logLevel = LogLevel::Debug;
	else if(boost::iequals(verboseLevelConsole, "TRACE")) logLevel = LogLevel::Trace;
	else {
		cout << "Error: Invalid LogLevel." << endl;
		return EXIT_FAILURE;
	}
	
	Loggers->getLogger("imageio").addAppender(make_shared<logging::ConsoleAppender>(logLevel));
	Loggers->getLogger("train-frame-extract-nnet").addAppender(make_shared<logging::ConsoleAppender>(logLevel));
	Logger appLogger = Loggers->getLogger("train-frame-extract-nnet");

	appLogger.debug("Verbose level for console output: " + logging::logLevelToString(logLevel));

	// Read the video detections metadata (eyes, face-coords):
	vector<facerecognition::PascVideoDetection> pascVideoDetections;
	{
		std::ifstream ifs(inputLandmarks.string()); // ("pasc.bin", std::ios::binary | std::ios::in)
		boost::archive::text_iarchive ia(ifs); // binary_iarchive
		ia >> pascVideoDetections;
	} // archive and stream closed when destructors are called

	// We would read the still images detection metadata (eyes, face-coords) here, but it's not provided with PaSC. Emailed Ross.
	// TODO!

	// Read the training-video xml sigset and the training-still sigset to get the subject-id metadata:
	auto videoQuerySet = facerecognition::utils::readPascSigset(R"(C:\Users\Patrik\Documents\GitHub\data\PaSC\Web\nd1Fall2010VideoPaSCTrainingSet.xml)", true);
	auto stillTargetSet = facerecognition::utils::readPascSigset(R"(C:\Users\Patrik\Documents\GitHub\data\PaSC\Web\nd1Fall2010PaSCamerasStillTrainingSet.xml)", true);

	// Create the output directory if it doesn't exist yet:
	if (!boost::filesystem::exists(outputPath)) {
		boost::filesystem::create_directory(outputPath);
	}
	
	// Read all videos:
	if (!boost::filesystem::exists(inputDirectoryVideos)) {
		appLogger.error("The given input files directory doesn't exist. Aborting.");
		return EXIT_FAILURE;
	}

	/*
	vector<path> trainingVideos;
	try {
		copy(boost::filesystem::directory_iterator(inputDirectoryVideos), boost::filesystem::directory_iterator(), back_inserter(trainingVideos));
	}
	catch (boost::filesystem::filesystem_error& e) {
		string errorMsg("Error while loading the video files from the given input directory: " + string(e.what()));
		appLogger.error(errorMsg);
		return EXIT_FAILURE;
	}*/

	FaceVacsEngine faceRecEngine(R"(C:\FVSDK_8_9_5\etc\frsdk.cfg)", R"(C:\Users\Patrik\Documents\GitHub\aaatmp)");
	faceRecEngine.enrollGallery(stillTargetSet, inputDirectoryStills);

	std::random_device rd;
	auto videosSeed = rd();
	auto framesSeed = rd();
	auto posTargetsSeed = rd();
	auto negTargetsSeed = rd();
	std::mt19937 rndGenVideos(videosSeed);
	std::mt19937 rndGenFrames(framesSeed);
	std::mt19937 rndGenPosTargets(posTargetsSeed);
	std::mt19937 rndGenNegTargets(negTargetsSeed);
	std::uniform_int_distribution<> rndVidDistr(0, videoQuerySet.size() - 1);
	auto randomVideo = std::bind(rndVidDistr, rndGenVideos);
	
	// The training data:
	vector<Mat> trainingFrames;
	vector<float> labels; // the score difference to the value we would optimally like
						  // I.e. if it's a positive pair, the label is the difference to 1.0
						  // In case of a negative pair, the label is the difference to 0.0

	// Select random subset of videos: (Note: This means we may select the same video twice - not so optimal?)
	int numVideosToTrain = 10;
	int numFramesPerVideo = 3;
	int numPositivePairsPerFrame = 1;
	int numNegativePairsPerFrame = 1;
	for (int i = 0; i < numVideosToTrain; ++i) {
		auto queryVideo = videoQuerySet[randomVideo()];
		auto frames = getFrames(inputDirectoryVideos / queryVideo.dataPath);
		// For the currently selected video, partition the target set. The distributions don't change each frame, whole video has the same FaceRecord.
		auto bound = std::partition(begin(stillTargetSet), end(stillTargetSet), [queryVideo](facerecognition::FaceRecord& target) { return target.subjectId == queryVideo.subjectId; });
		// begin to bound = positive pairs, rest = negative
		auto numPositivePairs = std::distance(begin(stillTargetSet), bound);
		auto numNegativePairs = std::distance(bound, end(stillTargetSet));
		std::uniform_int_distribution<> rndPositiveDistr(0, numPositivePairs - 1); // -1 because all vector indices start with 0
		std::uniform_int_distribution<> rndNegativeDistr(numPositivePairs, stillTargetSet.size() - 1);
		// Select random subset of frames:
		std::uniform_int_distribution<> rndFrameDistr(0, frames.size() - 1);
		for (int j = 0; j < numFramesPerVideo; ++j) {
			int frameNum = rndFrameDistr(rndGenFrames);
			auto frame = frames[frameNum];
			// Get the landmarks for this frame:
			string frameName = getPascFrameName(queryVideo.dataPath, frameNum + 1);
			auto landmarks = std::find_if(begin(pascVideoDetections), end(pascVideoDetections), [frameName](const facerecognition::PascVideoDetection& d) { return (d.frame_id == frameName); });
			// Use facebox (later: or eyes) to run the engine
			if (landmarks == std::end(pascVideoDetections)) {
				appLogger.info("Chose a frame but could not find a corresponding entry in the metadata file - skipping it.");
				continue; // instead, do a while-loop and count the number of frames with landmarks (so we don't skip videos if we draw bad values)
				// We throw away the frames with no landmarks. This basically means our algorithm will only be trained on frames where PittPatt succeeds, and
				// frames where it doesn't are unknown data to our nnet. I think we should try including these frames as well, e.g. with an error/label of 1.0.
			}
			// Choose one random positive and one random negative pair (we could use more, this is just the first attempt):
			// Actually with the Cog engine we could enrol the whole gallery and get all the scores in one go, should be much faster
			for (int k = 0; k < numPositivePairsPerFrame; ++k) { // we can also get twice (or more) times the same, but doesn't matter for now
				auto targetStill = stillTargetSet[rndPositiveDistr(rndGenPosTargets)];
				// TODO get LMs for targetStill from PaSC - see further up, email Ross


				// match (targetStill (FaceRecord), LMS TODO ) against ('frame' (Mat), queryVideo (FaceRecord), landmarks)

			}
			for (int k = 0; k < numNegativePairsPerFrame; ++k) {

			}

			// Run the engine:
			// in: frame, eye/face coords, plus one positive image from the still-sigset with its eye/face coords
			// out: score
			// Later: Include several positive scores, and also negative pairs
			// I.e. enroll the whole gallery once, then match the query frame and get all scores?

			// From this pair:
			// resulting score difference to class label = label, facebox = input, resize it
			trainingFrames.emplace_back(frame); // store only the resized frames!
			labels.emplace_back(1.0f);
		}
	}

	// Engine:
	// libFaceRecognition: CMake-Option for dependency on FaceVACS
	// Class Engine;
	// FaceVacsExecutableRunner : Engine; C'tor: Path to directory with binaries or individual binaries?
	// FaceVacs : Engine; C'tor - nothing? FV-Config?

	// Or: All the FaceVacs stuff in libFaceVacsWrapper. Direct-calls and exe-runner. With CMake-Option.
	
	
	// Train NN:
	// trainingFrames, labels


	// Save it:

	// Test it:
	// - Error on train-set?
	// - Split the train set in train/test
	// - Error on 'real' PaSC part? (i.e. the validation set)

	return EXIT_SUCCESS;
}
