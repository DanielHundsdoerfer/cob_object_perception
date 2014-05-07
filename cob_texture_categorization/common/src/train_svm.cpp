#include "train_svm.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/ml/ml.hpp>


train_svm::train_svm()
{
}

void train_svm::run_training(std::string *trainingdata, std::string *traininglabel)
{


	cv::FileStorage fs("/home/rmb-dh/Test_dataset/training_data.yml", cv::FileStorage::READ);
	cv::Mat training_data;
	fs["Training_data"] >> training_data;
	std::cout<<training_data<<"Matrix"<<std::endl;

	cv::FileStorage fsl("/home/rmb-dh/Test_dataset/train_data_respons.yml", cv::FileStorage::READ);
	cv::Mat training_label;
	fsl["Training_label"] >> training_label;
	std::cout<<training_label<<"Matrix"<<std::endl;

	// Set up SVM's parameters
	CvSVMParams params;
	params.svm_type    = CvSVM::C_SVC;
	params.kernel_type = CvSVM::LINEAR;
	params.term_crit   = cvTermCriteria(CV_TERMCRIT_ITER, 100, 1e-6);

	CvSVM SVM;
	SVM.train(training_data, training_label, cv::Mat(), cv::Mat(), params);

//	cv::FileStorage fsvm("/home/rmb-dh/Test_dataset/svm.yml", cv::FileStorage::WRITE);
//	fs << "SVM_Model" << SVM;
	SVM.save("/home/rmb-dh/Test_dataset/svm.yml", "svm");

	std::cout<<"SVM training completed"<<std::endl;

}
