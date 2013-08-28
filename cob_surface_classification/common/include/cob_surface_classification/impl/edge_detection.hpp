/*
 * edge_detection.hpp
 *
 *  Created on: May 17, 2013
 *      Author: rmb-ce
 */

#ifndef IMPL_EDGE_DETECTION_HPP_
#define IMPL_EDGE_DETECTION_HPP_

#define DECIDE_CURV 	false	//mark points on SURFACES (not only edges) as concave or convex, using scalarproduct
#define COMP_SVD 		true	//approximate lines using SVD. otherwise line approximation using only two points.


template <typename PointInT> void
EdgeDetection<PointInT>::sobelLaplace(cv::Mat& color_image, cv::Mat& depth_image)
{
	//smooth image
	cv::Mat depth_image_smooth = depth_image.clone();
	//cv::GaussianBlur( depth_image, depth_image_smooth, cv::Size(13,13), -1, -1);

	//first derivatives with Sobel operator
	//-----------------------------------------------------------------------
	cv::Mat x_deriv;
	cv::Mat y_deriv;


	int scale = 1;
	int delta = 0;
	int ddepth = CV_32F;//CV_16S; //depth (format) of output data
	//x-derivative:
	cv::Sobel(depth_image_smooth,x_deriv,ddepth,1,0,11,scale,delta);	//order of derivative in x-direction: 1, in y-direction: 0

	//y-derivative
	cv::Sobel(depth_image_smooth,y_deriv,ddepth,0,1,11,scale,delta);	//order of derivative in y-direction: 1, in x-direction: 0

	//compute gradient magnitude
	cv::Mat  x_pow2, y_pow2, grad;
	//cv::magnitude(x_deriv,y_deriv,grad);
	cv::pow(x_deriv,2.0, x_pow2);
	cv::pow(y_deriv,2.0, y_pow2);
	cv::sqrt(x_pow2 + y_pow2, grad);

	cv::Mat depth_im_show;
	cv::normalize(depth_image_smooth,depth_im_show,0,1,cv::NORM_MINMAX);
	cv::imshow("depth image", depth_im_show);
	cv::waitKey(10);

	/*	cv::imshow("x gradient",x_deriv);
		cv::waitKey(10);

		cv::imshow("y gradient",y_deriv);
		cv::waitKey(10);*/


	cv::normalize(grad,grad,0,1,cv::NORM_MINMAX);
	cv::imshow("Sobel", grad);
	cv::waitKey(10);



	//second derivatives with Laplace operator
	//------------------------------------------------------------------------
	cv::Mat deriv_2,temp;
	int kernel_size = 7;

	//cv::Laplacian( depth_image_smooth, temp, ddepth, kernel_size, scale, delta );
	//cv::normalize(deriv_2,deriv_2,-1,2,cv::NORM_MINMAX);

/*	cv::Mat kernel = (cv::Mat_<float>(5,5) <<  0,          0,          1,          0,          0,
			0,          2,         -8,         2,          0,
			1,         -8,         20,         -8,          1,
			0,          2,         -8,          2,          0,
			0,          0,          1,          0,          0);*/


	cv::Mat kernel = (cv::Mat_< float >(7,7) << 0.1, 0.2, 0.5, 0.8, 0.5, 0.2, 0.1,
			0.2, 1.1, 2.5, 2.7, 2.5, 1.1, 0.2,
			0.5, 2.5, 0, -6.1, 0, 2.5, 0.5,
			0.8, 2.7, -6.1, -20, -6.1, 2.7, 0.8,
			0.5, 2.5, 0, -6.1, 0, 2.5, 0.5,
			0.2, 1.1, 2.5, 2.7, 2.5, 1.1, 0.2,
			0.1, 0.2, 0.5, 0.8, 0.5, 0.2, 0.1);
	//LoG (Laplacian and Gaussian) with sigma = 1.4
	/*cv::Mat kernel = (cv::Mat_< float >(9,9) << 0,0,3,2, 2, 2,3,0,0,
													0,2,3,5, 5, 5,3,2,0,
													3,3,5,3, 0, 3,5,3,3,
													2,5,3,-12, -23, -12,3,5,2,
													2,5,0,-23, -40, -23,0,5,2,
													2,5,3,-12, -23, -12,3,5,2,
													3,3,5,3, 0, 3,5,3,3,
													0,2,3,5, 5, 5,3,2,0,
													0,0,3,2, 2, 2,3,0,0);*/


	cv::filter2D(depth_image_smooth, temp, ddepth, kernel);

	float sat = 0.5;

	//Bild immer zeilenweise durchgehen
	for(int v=0; v<temp.rows; v++ )
	{
		for(int u=0; u< temp.cols; u++)
		{
			if(temp.at<float>(v,u) < -sat)
			{
				temp.at<float>(v,u) = - sat;
			}
			else if(temp.at<float>(v,u) > sat)
			{
				temp.at<float>(v,u) =  sat;
			}
		}
	}
	//normalize values to range between 0 and 1, so that all values can be depicted in an image
	cv::normalize(temp,deriv_2,0,1,cv::NORM_MINMAX);

	cv::imshow("Laplacian", deriv_2);
	cv::waitKey(10);
}




template <typename PointInT> void
EdgeDetection<PointInT>::coordinatesMat
(cv::Mat& depth_image, PointCloudInConstPtr pointcloud, cv::Point2f dotIni, cv::Point2f dotEnd, cv::Mat& coordinates, bool& step)
{
	/* consider depth coordinates along the line between dotIni and dotEnd
	 * write them into a matrix
	 * first column: coordinate on the line w, second column: depth z
	 * format of one row: [coordinate along the line (w), depth coordinate (z) , 1]
	 * --------------------------------------------------------------------------------*/

	//dotIni is origin of local coordinate system, dotEnd the end point of the line
	int xDist = dotEnd.x - dotIni.x;
	int yDist = dotEnd.y - dotIni.y;
	//include both points -> one coordinate more than distance
	int lineLength = std::max(std::abs(xDist),std::abs(yDist)) + 1;
	int sign = 1;	//iteration in positive or negative direction
	int xIter[lineLength];
	int yIter[lineLength];

	//iterate from dotIni to dotEnd. Set up indices for iteration:
	if(xDist != 0)
	{
		//line in x-direction
		if(xDist <0)
			sign = -1;

		for(int i=0; i<lineLength; i ++)
		{
			xIter[i] = dotIni.x + i * sign;
			yIter[i] = dotIni.y;
		}
	}
	else
	{
		//line in y-direction
		if(yDist <0)
			sign = -1;

		for(int i=0; i<lineLength; i ++)
		{
			xIter[i] = dotIni.x;
			yIter[i] = dotIni.y + i*sign;
		}
	}


	float x0,y0; //coordinates of reference point


	int iCoord;

	/*timer.start();
		for(int itim=0;itim<10000;itim++)
		{*/

	coordinates = cv::Mat::zeros(lineLength,3,CV_32FC1);
	iCoord = 0;
	int iX = 0;
	int iY = 0;
	bool first = true;



	while(iX < lineLength && iY < lineLength)
	{
		//später nicht einfach x bzw y-Koordinate betrachten, sondern (x-x0)²+(y-y0)² als w nehmen


		//don't save points with nan-entries (no data available)
		if(!std::isnan(pointcloud->at(xIter[iX],yIter[iY]).x))
		{

			if(first)
			{
				//origin of local coordinate system is the first point with valid data. Express coordinates relatively:
				x0 = pointcloud->at(xIter[iX],yIter[iY]).x;
				y0 = pointcloud->at(xIter[iX],yIter[iY]).y;
				first = false;
			}
			coordinates.at<float>(iCoord,0) = (pointcloud->at(xIter[iX],yIter[iY]).x - x0)
															+ (pointcloud->at(xIter[iX],yIter[iY]).y - y0);
			coordinates.at<float>(iCoord,1) = depth_image.at<float>(yIter[iY], xIter[iX]);// - z0;
			coordinates.at<float>(iCoord,2) = 1.0;


			//detect steps in depth coordinates (not directly at the center)
			//if(iCoord != 0 && std::abs(coordinates.at<float>(iCoord,1) - coordinates.at<float>(iCoord-1,1)) > 0.05)
			if(iCoord  > 1 && abs(coordinates.at<float>(iCoord,1) - coordinates.at<float>(iCoord-1,1)) > 0.05)
			{
				step = true;
			}

			iCoord++;
		}
		iX++;
		iY++;
	}

	if(iCoord != 0 && iCoord<lineLength)
	{
		coordinates.resize(iCoord);
	}



}


//-------------------------------------------------------------------------------------------------------------------------



template <typename PointInT> void
EdgeDetection<PointInT>::approximateLine
(cv::Mat& depth_image, PointCloudInConstPtr pointcloud, cv::Point2f dotIni, cv::Point2f dotEnd, cv::Mat& abc, cv::Mat& n, cv::Mat& coordinates, bool& step)
{
	//Achtung: coordinates-Matrix wird durch SVD verändert

	/* consider depth coordinates between dotIni and dotEnd.
	 *  linear regression via least squares minimisation (-> SVD)
	 *   parameters of the approximated line: aw+bz+1 = 0
	 * ----------------------------------------------------------*/

	Timer timer;


	//set up coordinates matrix for points between dotIni and dotEnd
	coordinatesMat(depth_image,  pointcloud,  dotIni,  dotEnd, coordinates, step);

	//if(iCoord == 0)
	//if(coordinates.rows == 0)
	if(coordinates.rows <= 2)
		//no valid data
		abc = cv::Mat::zeros(1,3,CV_32FC1);
	else
	{
		/*
		//-------------------------------------------------------
		//SVD
		//-------------------------------------------------------

		cv::Mat sv;	//singular values
		cv::Mat u;	//left singular vectors
		cv::Mat vt;	//right singular vectors, transposed, 3x3

		//if there have been valid coordinates available, perform SVD for linear regression of coordinates
		//cv::SVD::compute(coordinates,sv,u,vt,cv::SVD::MODIFY_A);
		cv::SVD::compute(coordinates,sv,u,vt);
		abc =  vt.row(2);*/

		//------------------------------------------------------
		//QR
		//------------------------------------------------------

		//y = mx + n
		//b=1; a= -m; c= -n
		//E = [x 1 ; ...]
		//d = [y ; ...]
		//E* mn - d -> 0

		cv::Mat E (cv::Mat::zeros(coordinates.rows,2,CV_32FC1));
		for(unsigned int i=0; i<E.rows; ++i)
		{
			E.at<float>(i,0) = coordinates.at<float>(i,0);
			E.at<float>(i,1) = 1;
		}

		cv::Mat d = coordinates.col(1);
		cv::Mat mn (cv::Mat::zeros(2,1,CV_32FC1));

		cv::solve(E,d,mn, cv::DECOMP_QR);

		abc.at<float>(0) = -mn.at<float>(0);
		abc.at<float>(1) = 1;
		abc.at<float>(2) = -mn.at<float>(1);

	}
	/*}
		timer.stop();
		std::cout << timer.getElapsedTimeInMilliSec() /10000<< " ms for setting up coordinates-matrix + SVD (averaged over 10000 iterations)\n";*/



	/*

		//---------------------------------------------------------------------------------------------------
		//PCA version
		//---------------------------------------------------------------------------------------------------


		//timer.start();
		//for(int itim=0;itim<10000;itim++)
		//{

		iCoord = 0;
		int iX = 0;
		int iY = 0;
		bool first = true;
		coordinates = cv::Mat::zeros(lineLength,2,CV_32FC1);
		cv::PCA pca;


		while(iX < lineLength && iY < lineLength)
		{
			//später nicht einfach x bzw y-Koordinate betrachten, sondern (x-x0)²+(y-y0)² als w nehmen


			//don't save points with nan-entries (no data available)
			if(!std::isnan(pointcloud->at(xIter[iX],yIter[iY]).x))
			{

				if(first)
				{
					//origin of local coordinate system is the first point with valid data. Express coordinates relatively:
					x0 = pointcloud->at(xIter[iX],yIter[iY]).x;		//x0 = pointcloud->at(xIter.pos().x,xIter.pos().y).x;
					y0 = pointcloud->at(xIter[iX],yIter[iY]).y;
					first = false;
				}
				coordinates.at<float>(iCoord,0) = (pointcloud->at(xIter[iX],yIter[iY]).x - x0)
																			+ (pointcloud->at(xIter[iX],yIter[iY]).y - y0);
				coordinates.at<float>(iCoord,1) = depth_image.at<float>(yIter[iY], xIter[iX]);	//depth coordinate
				//coordinates.at<float>(iCoord,2) = 1.0;


				//detect steps in depth coordinates
				if(iCoord != 0 && std::abs(coordinates.at<float>(iCoord,1) - coordinates.at<float>(iCoord-1,1)) > 0.05)
				{
					step = true;
				}

				iCoord++;
			}
			iX++;
			iY++;
		}

		if(iCoord == 0)
			//no valid data
			n = cv::Mat::zeros(1,2,CV_32FC1);
		else if (iCoord == 1)
		{
			//coordinates already is its eigenvector
			n = coordinates;
		}
		else
		{
			//std::cout << "iCoord: " <<iCoord << "\n";
			if(iCoord<lineLength)
			{
				coordinates.resize(iCoord);
			}


			//PCA
			pca(coordinates, // pass the data
					cv::Mat(), // there is no pre-computed mean vector,
					// so let the PCA engine to compute it
					CV_PCA_DATA_AS_ROW, // input samples are stored as matrix rows
					1 // retain only the first principal component (the eigenvector to the largest eigenvalue of the covariance matrix)
			);
			n = pca.eigenvectors.row(0); 	//eigenvector to largest eigenvalue

			//directional vector should point from dotIni to dotEnd
			if(coordinates.at<float>(1,0) < 0)
				n *= -1;

		}

	 */
	/*}
		timer.stop();
		std::cout << timer.getElapsedTimeInMilliSec() /10000<< " ms for setting up coordinates-matrix + PCA (averaged over 10000 iterations)\n";*/

	//std::cout << "eigenvectors:\n"<< pca.eigenvectors <<"\n";
}

//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------





template <typename PointInT> void
EdgeDetection<PointInT>::approximateLine
(cv::Mat& depth_image, PointCloudInConstPtr pointcloud, cv::Point2f dotIni, cv::Point2f dotEnd, cv::Mat& abc)
{

	//step-detection in den Koordinaten hier nicht mehr nötig, da durch Vergleichen der zwei Geraden für jede Seite schon auffällt, ob man auf Kante oder direkt daneben ist (?)

	/*Timer timer;
	timer.start();
			for(int itim=0;itim<10000;itim++)
			{*/

	/* approximate depth coordinates between dotIni and dotEnd as a line */
	/* linear approximation using only the two points
	 * line equation: a*w + b*z + c =  0  (w refers to coordinate on the line between dotIni and dotEnd, z to depth coordinate) */
	/* -------------------------------------------------------------------------------------------------------------------------*/

	abc.at<float>(1) = 1;		//b=1
	float z1 = depth_image.at<float>(dotIni.y, dotIni.x);
	float z2 = depth_image.at<float>(dotEnd.y, dotEnd.x);


	// c = -z1 -a*w1   with w1=0
	abc.at<float>(2) = -z1;

	float w2;

	//don't save points with nan-entries (no data available)
	if(std::isnan(pointcloud->at(dotIni.x,dotIni.y).z))
	{
		//mark as nan if dot at center is nan
		abc.at<float>(0) =abc.at<float>(1) =abc.at<float>(2)   = std::numeric_limits<float>::quiet_NaN();

	}
	else if(std::isnan(pointcloud->at(dotEnd.x,dotEnd.y).z))
	{
		//mark as "no decision possible"
		abc = cv::Mat::zeros(1,3,CV_32FC1);
	}
	else
	{
		float x0 = pointcloud->at(dotIni.x,dotIni.y).x;
		float y0 = pointcloud->at(dotIni.x,dotIni.y).y;
		//dotIni and dotEnd lie on line in either x- or y-direction
		w2 = (pointcloud->at(dotEnd.x,dotEnd.y).x - x0) + (pointcloud->at(dotEnd.x,dotEnd.y).y - y0);
		if(w2 != 0)
		{
			// a = -(z2-z1) /(w2-w1);
			// with w1=0
			abc.at<float>(0) = (z1-z2) /w2;
		}
		else
		{
			std::cout << "dotIni and dotEnd are the same!\n";
			abc = cv::Mat::zeros(1,3,CV_32FC1);
		}
	}
	/*}
			timer.stop();
			std::cout << timer.getElapsedTimeInMilliSec() /10000<< " ms for approximating one line using 2 points  (averaged over 10000 iterations)\n";*/

}

//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------




template <typename PointInT> void
EdgeDetection<PointInT>::scalarProduct(cv::Mat& abc1,cv::Mat& abc2,float& scalarProduct, int& concaveConvex, bool& step)
{
	/*Timer timer;
	timer.start();
	for(int itim=0;itim<10000;itim++)
	{*/


	//detect steps
	//------------------------------------------------------------

	if(step)
	{
		//step in neighbourhood -> approximation inaccurate
		scalarProduct = -1;
	}
	else
	{
		//compute z-value at w =0
		//compare value on left side of w=0 to value on right side
		// P[0, -c/b]
		float zLeft = -abc1.at<float>(2)/abc1.at<float>(1);
		float zRight = -abc2.at<float>(2)/abc2.at<float>(1);

		//there truly is a step at the central point only if there is none in the coordinates to its left and right
		if((zRight - zLeft) > stepThreshold_ || (zRight - zLeft) < -stepThreshold_)
		{
			//mark step as edge in depth data
			std::cout <<"step " << zRight - zLeft <<endl;
			scalarProduct = 0.5;
		}

		else
		{
			//if depth data is continuous: compute scalar product
			//--------------------------------------------------------

			//normal vector n1=[-1,(-c+a)/b -c/b]
			//normal vector n1=[1,(-c-a)/b -c/b]
			//subtract  "y-Achsenabschnitt" c/b from second coordinates because z-coordinates might not be relative (c != 0)
			float b1 =  (abc1.at<float>(0) - abc1.at<float>(2))/abc1.at<float>(1) - zLeft;
			float b2 = -(abc2.at<float>(0) + abc2.at<float>(2))/abc2.at<float>(1) -zRight;



			//directional vectors of the lines: point from central point outwards
			cv::Mat n1 = (cv::Mat_<float>(1,2) << -1, b1);
			cv::Mat n2 = (cv::Mat_<float>(1,2) << 1, b2);

			std::cout << "n1 " <<n1 << " n2 " <<n2 <<endl;

			cv::Mat n1Norm, n2Norm;
			cv::normalize(n1,n1Norm,1);
			cv::normalize(n2,n2Norm,1);

			scalarProduct = n1Norm.at<float>(0) * n2Norm.at<float>(0) + n1Norm.at<float>(1) * n2Norm.at<float>(1);

			/*
			//recognize inner EDGES (not surfaces) as concave or convex
			//convex: gradient of right line is larger than gradient of left line
			//concave: gradient of right line is smaller than gradient of left line
			setOffsetConcConv(0.35);
			if(b2 > (-b1 + offsetConcConv_))
				//convex
				concaveConvex = 255;
			else if (-b1 > b2 + offsetConcConv_)
				//concave
				concaveConvex = 125;
			 */

		}
	}

	/*}
	timer.stop();
	std::cout << timer.getElapsedTimeInMilliSec() /10000<< " ms for scalarProduct out of abc1 and abc2 (function scalarProduct()) (averaged over 10000 iterations)\n";*/
}


//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------


template <typename PointInT> void
EdgeDetection<PointInT>::approximateLineFullAndHalfDist
(cv::Mat& depth_image, PointCloudInConstPtr pointcloud, cv::Point2f dotIni, cv::Point2f dotEnd, cv::Mat& abc)
{
	/*cv::Point2f dot1 = dotIni;
	cv::Point2f dot2 = dotEnd;


	Timer timer;
	timer.start();
			for(int itim=0;itim<10000;itim++)
			{
				dotIni=dot1;
				dotEnd = dot2;*/

	//check if lines to dotEnd and dotEnd/2 go in the same direction. Only then dotIni is on an edge, else it is next to one or there is an outlier in the data.

	cv::Mat abc1 (cv::Mat::zeros(1,3,CV_32FC1));	//first approximation, full distance from dotIni to dotEnd
	cv::Mat abc2 (cv::Mat::zeros(1,3,CV_32FC1));	//second approximation, half distance from dotIni to dotEnd
	approximateLine(depth_image,pointcloud, dotIni,dotEnd, abc1);


	if(dotEnd.x == dotIni.x)
		//half distance in y-direction
		dotEnd.y = (int) (dotEnd.y - (dotEnd.y-dotIni.y)/2);
	else if(dotEnd.y == dotEnd.y)
		//half distance in x-direction
		dotEnd.x = (int) (dotEnd.x - (dotEnd.x-dotIni.x)/2);



	approximateLine(depth_image,pointcloud, dotIni,dotEnd, abc2);

	float scalProd = 1;
	int sinnlos2 = 0; //sinnlos, evtl noch neue Funktion scalarProduct() ohne concConv schreiben


	//propagate information from both lines
	if(isnan(abc1.at<float>(0,0)) || isnan(abc2.at<float>(0,0)))
	{
		abc.at<float>(0) =abc.at<float>(1) =abc.at<float>(2)   = std::numeric_limits<float>::quiet_NaN();
	}
	else if(abc1.at<float>(0) == 0 && abc1.at<float>(1) == 0)
	{
		//falls nan bei full distance, die Werte von halfDist übernehmen
		abc = abc2;
	}
	else if(abc2.at<float>(0) == 0 && abc2.at<float>(1) == 0)
	{
		//falls nan bei half dist, die Werte bei fulldist übernehmen
		abc = abc1;
	}
	else
	{
		/*bool sinnlos = false;	//noch neue Funktion scalarProduct ohne bool step schreiben
		scalarProduct(abc1,abc2,scalProd,sinnlos2,sinnlos);
		//std::cout <<"scalProd: " <<scalProd <<endl;
		if(scalProd > 0.95 || scalProd < -0.95)
		{
			//approximations of fulldist and halfdist going in the same direction
			abc = abc1;
		}
		else
			//lines not going in the same direction -> no edge
			abc = cv::Mat::zeros(1,3,CV_32FC1);*/

		//compare the gradients of the lines:
		if(std::abs(abc2.at<float>(0)) - std::abs(abc1.at<float>(0)) < 0.001)
			//if(std::abs(abc2.at<float>(0) - abc1.at<float>(0)) < 0.01)
			abc = abc1;
		else
			//just beside an edge -> do not mark as edge
			abc = cv::Mat::zeros(1,3,CV_32FC1);



	}

	/*}
	timer.stop();
	std::cout << timer.getElapsedTimeInMilliSec() /10000<< " ms for approximating lines using two points, half and full distance. comparison of gradients instead of computing scalarProduct (averaged over 10000 iterations)\n";
	 */
}

//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------

template <typename PointInT> void
EdgeDetection<PointInT>::thinEdges(cv::Mat& edgePicture, int xy)
{
	//detect edges as local maxima of scalarProduct (smaller than plane threshold s_{th,plane}
	//-----------------------------------------------------------------------------------------

	int neighboursOnOneSide = 4;
	int neighbourhoodSize = neighboursOnOneSide*2 +1;
	cv::Mat neighbourhood = cv::Mat::zeros(1,neighbourhoodSize,CV_32FC1);


	float max;
	int maxIdx;
	int maxIdx2;
	//loop over rows of edgePicture
	for(int iY = neighbourhoodSize; iY< edgePicture.rows - neighbourhoodSize; iY++)
	{
		//loop over columns of edgePicture
		for(int iX = neighbourhoodSize; iX< edgePicture.cols- neighbourhoodSize; iX++)
		{
			max = -2;
			maxIdx = -1;
			maxIdx2 = -1;

			//loop over neighbourhood
			for(int iNeigh= - neighboursOnOneSide ; iNeigh <= neighboursOnOneSide; iNeigh++)
			{
				//loop either over x-coordinate
				if(xy == 0)
				{
					//check if current value is larger than hitherto assumed maximum
					if((edgePicture.at<float>(iY,iX + iNeigh) > max) && (edgePicture.at<float>(iY,iX + iNeigh) < th_plane_))
					{
						maxIdx = iX + iNeigh;
						maxIdx2 = iX + iNeigh;
						max = edgePicture.at<float>(iY,maxIdx);
					}
					else if((edgePicture.at<float>(iY,iX + iNeigh) == max)&& (edgePicture.at<float>(iY,iX + iNeigh) < th_plane_))
					{
						maxIdx2 = iX + iNeigh;
					}
				}
				//or loop over y-coordinate
				else if (xy == 1)
				{
					if((edgePicture.at<float>(iY + iNeigh,iX) > max) && (edgePicture.at<float>(iY + iNeigh,iX) < th_plane_))
					{
						maxIdx = iY + iNeigh;
						maxIdx2 = iY + iNeigh;
						max = edgePicture.at<float>(maxIdx,iX);
					}
					if((edgePicture.at<float>(iY + iNeigh,iX) == max) && (edgePicture.at<float>(iY + iNeigh,iX) < th_plane_))
					{
						maxIdx2 = iY + iNeigh;
					}
				}
			}
			//if valid maximum has been found
			if(max != -2 && maxIdx != -1)
			{
				//if maximum value appears at several adjacent points, mark edge at the middle
				if(maxIdx != maxIdx2)
					maxIdx = (int) ((maxIdx + maxIdx2)/2);

				//set all values in neighbourhood to -1 (=no edge), except for maximum value
				for(int iNeigh= - neighboursOnOneSide ; iNeigh <= neighboursOnOneSide; iNeigh++)
				{
					if(xy == 0)
					{
						if(iX + iNeigh != maxIdx)
						{
							edgePicture.at<float>(iY,iX + iNeigh) = -1;
						}
					}
					else if (xy == 1)
					{
						if(iY + iNeigh != maxIdx)
						{
							edgePicture.at<float>(iY + iNeigh,iX) = -1;
						}
					}

				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------

template <typename PointInT> void
EdgeDetection<PointInT>::drawLines(cv::Mat& plotZW, cv::Mat& coordinates, cv::Mat& abc)
{
	// draw computed coordinates and approximates lines in plotZW
	//origin of coordinate system at the bottom in the middle
	//x-axis: w-coordinate	(first column of "coordinates")
	//y-axis: z-coordinate	(second column of "coordinates")
	// -----------------------------------------------------------


	//Skalierung des Tiefen-Wertes zur besseren Darstellung
	float scaleDepth = 600;



	//letzter Eintrag in der Spalte der w-Koordinaten ist die groesste w-Koordinate
	//alle Koordinaten werden durch diese groesste geteilt -> Projektion auf [0,1]
	float maxW = coordinates.col(0).at<float>(coordinates.rows -1);

	//take magnitude of maxW, so that normalization of coordinates won't affect the sign
	if(maxW <0)
		maxW = maxW * (-1);

	float w,z;	//w-coordinate along line,  depth coordinate z

	if(maxW == 0)
	{
		maxW = 1;
		std::cout << "SurfaceClassification::drawLines(): Prevented division by 0\n";
		std::cout << "coordinates rows,cols " << coordinates.rows <<" , "<< coordinates.cols<< endl;
	}

	std::cout << "coord: " <<endl;
	for(int v=0; v< coordinates.rows; v++)
	{
		std::cout <<  coordinates.at<float>(v,0) <<", "<< coordinates.at<float>(v,1) <<", " <<coordinates.at<float>(v,2) <<";"<< endl;

		//project w-coordinates on interval [0,windowX/2] and then shift to the middle windowX/2
		w = (coordinates.at<float>(v,0) / maxW) * windowX_/2 + windowX_/2;
		//scale z-value for visualization
		z = coordinates.at<float>(v,1) *scaleDepth;

		//invert depth-coordinate for visualization (coordinate system of image-matrix is in upper left corner,
		//whereas we want our coordinate system to be at the bottom with the positive z-axis going upward
		z = windowY_ - z;
		cv::circle(plotZW,cv::Point2f(w,z),1,CV_RGB(255,255,255),2);
		if(z> windowY_)
		{
			std::cout<< "z:" << z <<endl;
			cv::circle(plotZW,cv::Point2f(w,windowY_-10),1,CV_RGB(255,255,255),2);
		}
		if(z <0)
		{
			std::cout<< "z:" << z <<endl;
			cv::circle(plotZW,cv::Point2f(w,10),1,CV_RGB(255,255,255),2);
		}
	}
	std::cout << "------------------------------------------"<<endl;

	//compute first and last point of the line using the parameters abc
	// P1(0,-c/b)
	// P2(xBoundary,(-c-a*xBoundary)/b);
	float x1 = 0;
	float z1 = -abc.at<float>(2)/abc.at<float>(1);	//line equation -> z-value for x1
	float x2 = coordinates.col(0).at<float>(coordinates.rows -1);
	float z2 = (-abc.at<float>(2) - abc.at<float>(0) * x2) / abc.at<float>(1);

	//scaling and shifting
	x1 =  windowX_/2;
	x2 = (x2/ maxW) * windowX_/2 + windowX_/2;

	z1 = z1 * scaleDepth;
	z1 = windowY_ - z1;

	z2 = z2 *scaleDepth;
	z2 = windowY_ - z2;

	//draw line between the two points
	cv::line(plotZW,cv::Point2f(x1 ,z1 ), cv::Point2f(x2 , z2 ),CV_RGB(255,255,255),1);
}

//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------


template <typename PointInT> void
EdgeDetection<PointInT>::drawLineAlongN(cv::Mat& plotZW, cv::Mat& coordinates, cv::Mat& n)
{
	// draw computed coordinates and approximates lines in plotZW
	// use for visualization of the directional vectors computed by PCA version of approximateLine()
	// drawn line will only approximate the direction, not the absolute height!



	//origin of coordinate system at the bottom in the middle
	//x-axis: w-coordinate	(first column of "coordinates")
	//y-axis: z-coordinate	(second column of "coordinates")
	//
	//
	// -----------------------------------------------------------


	//Skalierung des Tiefen-Wertes zur besseren Darstellung
	float scaleDepth = 400;


	//letzter Eintrag in der Spalte der w-Koordinaten ist die groesste w-Koordinate
	//alle Koordinaten werden durch diese groesste geteilt -> Projektion auf [0,1]
	float maxW = coordinates.col(0).at<float>(coordinates.rows -1);

	//take magnitude of maxW, so that normalization of coordinates won't affect the sign
	if(maxW <0)
		maxW = maxW * (-1);

	float w,z;	//w-coordinate along line,  depth coordinate z

	if(maxW == 0)
	{
		maxW = 1;
		std::cout << "SurfaceClassification::drawLines(): Prevented division by 0\n";
	}

	for(int v=0; v< coordinates.rows; v++)
	{
		//project w-coordinates on interval [0,windowX_/2] and then shift to the middle windowX_/2
		w = (coordinates.at<float>(v,0) / maxW) * windowX_/2 + windowX_/2;
		//scale z-value for visualization
		z = coordinates.at<float>(v,1) *scaleDepth;

		//invert depth-coordinate for visualization (coordinate system of image-matrix is in upper left corner,
		//whereas we want our coordinate system to be at the bottom with the positive z-axis going upward
		z = windowY_ - z;
		cv::circle(plotZW,cv::Point2f(w,z),1,CV_RGB(255,255,255),2);
	}

	//line along n
	float x1 = 0;
	float z1 = 0;
	float x2 = n.at<float>(0,0);
	float z2 = n.at<float>(0,1);



	//scaling and shifting
	x1 =  windowX_/2;
	x2 = (x2/ maxW) * windowX_/2 + windowX_/2;

	z1 = z1 * scaleDepth;
	z1 = windowY_ - z1 - windowY_/2;

	z2 = z2 *scaleDepth ;
	z2 = windowY_ - z2 - windowY_/2;

	//draw line between the two points
	cv::line(plotZW,cv::Point2f(x1 ,z1 ), cv::Point2f(x2 , z2 ),CV_RGB(255,255,255),1);
}

//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------


template <typename PointInT> void
EdgeDetection<PointInT>::deriv2nd3pts
(cv::Mat points, float& deriv)
{
	//std::cout << "points: " <<points <<endl;

	//use stencil of width 3
	//f(x)'' = (f(x+dx) + f(x-dx) - 2* f(x))/ dx²

	//leave away denominator, because only magnitude is important
	//float dx = (points.at<float>(points.rows-1,0) - points.at<float>(0,0))/2;

	//take first and last entry of matrix (most distant points)
	deriv = (points.at<float>(points.rows-1,1) + points.at<float>(0,1) - 2* points.at<float>((int)points.rows/2,1));	/// (dx * dx);
}

//--------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------


template <typename PointInT> void
EdgeDetection<PointInT>::deriv2nd5pts
(cv::Mat fivePoints, float& deriv)
{
	//std::cout << "fivepoints: " <<fivePoints <<endl;

	//use stencil of width 5
	//f''(x) = (-f(x-2h) + 16f(x-h) -f(x)  + 16f(x+h) -f(x+2h)) / (12*h²)
	float dx = fivePoints.at<float>(2,0) - fivePoints.at<float>(1,0);
	deriv = (-fivePoints.at<float>(0,1) + 16* fivePoints.at<float>(1,1) - fivePoints.at<float>(2,1) + 16* fivePoints.at<float>(3,1) -fivePoints.at<float>(4,1))	/ (dx * dx);
}
//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------


template <typename PointInT> void
EdgeDetection<PointInT>::deriv2nd
(cv::Mat depth_image,PointCloudInConstPtr cloud, cv::Point2f dotStart, cv::Point2f dotStop, float& deriv)
{
	cv::Mat coordinates;
	bool step; //sinnlos
	coordinatesMat(depth_image, cloud, dotStart, dotStop, coordinates, step);
	deriv2nd3pts(coordinates,deriv);
}



//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------

template <typename PointInT> void
EdgeDetection<PointInT>::computeDepthEdges
(cv::Mat depth_image, PointCloudInConstPtr pointcloud, cv::Mat& edgeImage)
{


	/*Timer timerFunc;
		timerFunc.start();*/

	//plot z over w, draw estimated lines
	cv::Mat plotZW (cv::Mat::zeros(windowX_,windowY_,CV_32FC1));
	//cv::Mat scalarProducts (cv::Mat::ones(depth_image.rows,depth_image.cols,CV_32FC1));

	//kann gelöscht werden, wenn nach Klassifizierung verschoben
	cv::Mat concaveConvex (cv::Mat::zeros(depth_image.rows,depth_image.cols,CV_8UC1)); 	//0:neither concave nor convex; 125:concave; 255:convex
	cv::Mat concaveConvexY (cv::Mat::zeros(depth_image.rows,depth_image.cols,CV_8UC1)); 	//0:neither concave nor convex; 125:concave; 255:convex


	//cv::Mat scalarProductsY (cv::Mat::ones(depth_image.rows,depth_image.cols,CV_32FC1));
	//cv::Mat scalarProductsX (cv::Mat::ones(depth_image.rows,depth_image.cols,CV_32FC1));
	cv::Mat scalarProductsY (cv::Mat::zeros(depth_image.rows,depth_image.cols,CV_32FC1));
	cv::Mat scalarProductsX (cv::Mat::zeros(depth_image.rows,depth_image.cols,CV_32FC1));
	int concConv = 0;
	bool step;

	int iX=2*depth_image.cols/3;
	int iY=2*depth_image.rows/3;


	Timer timer;


	//	cout << timerFunc.getElapsedTimeInMilliSec() << " ms for initial definitions before loop\n";


/*
	//loop over rows
	for(int iY = lineLength_/2; iY< depth_image.rows-lineLength_/2; iY++)
	{
		//loop over columns
		for(int iX = lineLength_/2; iX< depth_image.cols-lineLength_/2; iX++)
		{


			//scalarProduct of depth along lines in x-direction
			//------------------------------------------------------------------------
			cv::Point2f dotLeft(iX -lineLength_/2, iY);
			cv::Point2f dotRight(iX +lineLength_/2, iY);
			cv::Mat coordinates1;	//coordinates on the left side of the center point
			cv::Mat coordinates2;	//coordinates on the right
			cv::Mat abc1 (cv::Mat::zeros(1,3,CV_32FC1));	//line parameters a,b,c of line a*w+b*z+1=0, left line
			cv::Mat abc2 (cv::Mat::zeros(1,3,CV_32FC1));	//right line
			cv::Mat n1;
			cv::Mat n2;
			bool step = false;

			if(COMP_SVD)
			{

				// line approximation using SVD
				// ----------------------------------------------------------


				//boolean step will be set to true in approximateLine(), if there is a step either in coordinates1 or coordinates2
				//-> needs to be set to false before calling approximateLine() for both sides.
				//in scalarProduct(), boolean step will be considered when detecting a step at the central point.
				//if there is a step at the central point, the coordinates on the right and left to it should be continuous without step!
				//(else the step would be detected in the neighbourhood as well, leading to inaccuracies)

				step = false;

				//do not use point right at the center (would belong to both lines -> steps not correctly represented)
				approximateLine(depth_image,pointcloud, cv::Point2f(iX-1,iY),dotLeft, abc1,n1, coordinates1, step);


				//n1 wird nur gebraucht, falls PCA anstatt SVD

				//	timer.stop();
				//	std::cout << timer.getElapsedTimeInMilliSec() /10000 << " ms for lineApproximation (averaged over 10000 iterations)\n";



				//besser den Pixel rechts bzw.links von dotMiddle betrachten, damit dotMiddle nicht zu beiden Seiten dazu gerechnet wird (sonst ungenau bei Sprung)
				approximateLine(depth_image,pointcloud, cv::Point2f(iX+1,iY),dotRight, abc2,n2, coordinates2,step);

			}
			else
			{




				//	approximate line using only two points
				// -------------------------------------------------------------------

				//no step detection in approximateLine from 2 points
				step = false;

				//std::cout << "left\n";
				approximateLineFullAndHalfDist(depth_image,pointcloud, cv::Point2f(iX-1,iY),dotLeft, abc1);
				//std::cout << "right\n";
				approximateLineFullAndHalfDist(depth_image,pointcloud, cv::Point2f(iX+1,iY),dotRight, abc2);


				//std::cout << "abc1 (left):\n " << abc1 << "\n";
				//std::cout << "abc2 (right):\n " << abc2 << "\n";

			}
			// -------------------------------------------------------------------



			//drawLines(plotZW,coordinates1,abc1);
			//drawLines(plotZW,coordinates2,abc2);

			//drawLineAlongN(plotZW,coordinates1,n1);
			//drawLineAlongN(plotZW,coordinates2,n2);

			//float scalProdX = 1;
			float scalProdX = -1;



			if(abc1.at<float>(0,1) == 0 || abc2.at<float>(0,1) == 0)
			{
				//abc could not be approximated or no edge (see approximateLineFullAndHalfDist())
				scalProdX = -1;
			}
			else if (isnan(abc1.at<float>(0,0)) || isnan(abc2.at<float>(0,0)) )
			{
				//if nan at center point, mark as edge
				scalProdX = 0.5;
			}
			else if((pointcloud->at(dotMiddle.x-1,dotMiddle.y).z -pointcloud->at(dotMiddle.x+1,dotMiddle.y).z) > stepThreshold_)
				//direct step detection
				scalProdX = 0.5;
			else
			{
				scalarProduct(abc1,abc2,scalProdX,concConv,step);
			}

			//std::cout << "concave 125 grau, konvex 255 weiß, unbestimmt 0 schwarz: " <<concConv << "\n";

			scalarProductsX.at<float>(iY,iX) = scalProdX;


			if(DECIDE_CURV)
			{
				int length = 10; //entspricht stencil-Länge -1 . Muss kleiner sein als lineLength_
				cv::Point2f dotStart(iX - length, iY);
				cv::Point2f dotStop(iX + length, iY);
				float deriv = 0;
				deriv2nd(depth_image,pointcloud,dotStart, dotStop,deriv);

				float curv_threshold = 0.005;//0.003;
				if(deriv < -curv_threshold)
					concConv = 125;
				else if(deriv > curv_threshold)
					concConv = 255;
				else
					concConv = 0;

				concaveConvex.at<unsigned char>(iY,iX) = concConv;
			}

*/

			//scalarProduct of depth along lines in y-direction
			//------------------------------------------------------------------------
			cv::Point2f dotDown(iX , iY-lineLength_/2);
			cv::Point2f dotUp(iX , iY+lineLength_/2);
			cv::Point2f dotMiddle(iX , iY );
			cv::Mat coordinates1Y = cv::Mat::zeros(1,3,CV_32FC1);
			cv::Mat coordinates2Y = cv::Mat::zeros(1,3,CV_32FC1);
			cv::Mat n1Y;
			cv::Mat n2Y;

			cv::Mat abc1Y (cv::Mat::zeros(1,3,CV_32FC1));
			cv::Mat abc2Y (cv::Mat::zeros(1,3,CV_32FC1));

			//timer.start();

			if(COMP_SVD)
			{
				// approximate lines using SVD
				// -----------------------------------------------------------

				step = false;

				//do not use point right at the center (would belong to both lines -> steps not correctly represented)
				approximateLine(depth_image,pointcloud, cv::Point2f(iX,iY-1),dotDown, abc1Y, n1Y,coordinates1Y, step);

				//timer.stop();
				//cout << timer.getElapsedTimeInMilliSec() << " ms for lineApproximation\n";



				//besser den Pixel rechts bzw.links von dotMiddle betrachten, damit dotMiddle nicht zu beiden Seiten dazu gerechnet wird (sonst ungenau bei Sprung)
				approximateLine(depth_image,pointcloud, cv::Point2f(iX,iY+1),dotUp, abc2Y,n2Y, coordinates2Y,step);


			}
			else
			{

				//	approximate line using only two points
				// -------------------------------------------------------------------//

				//no step detection in approximateLine from 2 points
				step = false;

				approximateLineFullAndHalfDist(depth_image,pointcloud, cv::Point2f(iX,iY-1),dotDown, abc1Y);
				approximateLineFullAndHalfDist(depth_image,pointcloud, cv::Point2f(iX,iY+1),dotUp, abc2Y);


				// -------------------------------------------------------------------//
			}
			drawLines(plotZW,coordinates1Y,abc1Y);
			drawLines(plotZW,coordinates2Y,abc2Y);

			float scalProdY = 1;
			int concConvY = 0;
			if(abc1Y.at<float>(0,1) == 0 || abc2Y.at<float>(0,1) == 0)
			{
				//abc could not be approximated or no edge (see approximateLineFullAndHalfDist())
				scalProdY = -1;
			}
			else if (isnan(abc1Y.at<float>(0,0)) || isnan(abc2Y.at<float>(0,0)) )
			{
				//if nan at center point, mark as edge
				scalProdY = 0.5;
				std::cout << "nan at center\n";
			}
			else
			{
				scalarProduct(abc1Y,abc2Y,scalProdY,concConv,step);
			}

			std::cout <<"abc1 "<< abc1Y <<", abc2" << abc2Y <<endl;
			std::cout << "scalProd " << scalProdY <<endl;

			//std::cout << "concave 125 grau, konvex 255 weiß: " <<concConv << "\n";

			scalarProductsY.at<float>(iY,iX) = scalProdY;


			//concaveConvexY.at<unsigned char>(iY,iX) = concConvY;
/*
		}
	}	//loop over image*/

/*	cv::Mat scalProdX_scaled;
	cv::normalize(scalarProductsX, scalProdX_scaled,0,1,cv::NORM_MINMAX);
	cv::imshow("x-direction", scalProdX_scaled);
	cv::waitKey(10);
	cv::Mat scalProdY_scaled;
	cv::normalize(scalarProductsY, scalProdY_scaled,0,1,cv::NORM_MINMAX);
	cv::imshow("y-direction", scalProdY_scaled);
	cv::waitKey(10);

	//thin edges in x- and y-direction separately
	//thinEdges(scalarProductsX, 0);
	//thinEdges(scalarProductsY, 1);


	cv::normalize(scalarProductsX, scalProdX_scaled,0,1,cv::NORM_MINMAX);
	cv::imshow("x-direction thinned", scalProdX_scaled);
	cv::waitKey(10);
	cv::normalize(scalarProductsY, scalProdY_scaled,0,1,cv::NORM_MINMAX);
	cv::imshow("y-direction thinned", scalProdY_scaled);
	cv::waitKey(10);

	//thresholding according to edge criterium:
	//if ((s > -s_(th,edge)) && (s < s_(th,plane)): edge
	for(int iY = lineLength_/2; iY< depth_image.rows-lineLength_/2; iY++)
	{
		for(int iX = lineLength_/2; iX< depth_image.cols-lineLength_/2; iX++)
		{
			//Maximum (consider most intense edge):
			edgeImage.at<float>(iY,iX) = std::max(scalarProductsX.at<float>(iY,iX), scalarProductsY.at<float>(iY,iX));

			if(edgeImage.at<float>(iY,iX) > th_edge_ && edgeImage.at<float>(iY,iX) < th_plane_)
				edgeImage.at<float>(iY,iX) = 0;
			else
				edgeImage.at<float>(iY,iX) = 1;
		}
	}*/


	//int lineLength = 30;
	//cv::line(depth_image,cv::Point2f(depth_image.cols/2 -lineLength/2, depth_image.rows/2),cv::Point2f(depth_image.cols/2 +lineLength/2, depth_image.rows/2),CV_RGB(0,1,0),1);
	//cv::line(depth_image,cv::Point2f(depth_image.cols/2 , depth_image.rows/2 +lineLength/2),cv::Point2f(depth_image.cols/2 , depth_image.rows/2 -lineLength/2),CV_RGB(0,1,0),1);
	//cv::imshow("depthImage", depth_image);


	cv::imshow("depth over coordinate along line", plotZW);
	cv::waitKey(10);

	//cv::imshow("edge image", edgeImage);
	//cv::waitKey(10);

	if(DECIDE_CURV)
	{
		cv::imshow("x-direction (concave = grey, convex = white, undefined = black)", concaveConvex);
		cv::waitKey(10);
	}

	//timerFunc.stop();
	//cout << timerFunc.getElapsedTimeInMilliSec() << " ms for depth_along_lines()\n";

}



#endif /* IMPL_EDGE_DETECTION_HPP_ */
