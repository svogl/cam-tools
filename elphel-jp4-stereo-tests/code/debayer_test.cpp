/**

The MIT License (MIT)

Copyright (c) 2015 Simon Vogl <simon@voxel.at>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
 */

#include <iostream>
#include <list>

#include "opencv2/opencv.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgproc/types_c.h"
//#include "opencv2/videoio.hpp"
//#include "opencv2/videoio/videoio_c.h"

using namespace cv;
using namespace std;

/** split a jp46 encoded image into single components
 */
class Jp4Split {
public:
	Mat g1, g2, r, b;

	void split(Mat im) {
		if ( ! g1.rows || g1.rows != im.rows/2) {
			r= Mat( im.rows/2, im.cols/2, CV_8UC1);
			g1 = Mat( im.rows/2, im.cols/2, CV_8UC1);
			g2 =Mat( im.rows/2, im.cols/2, CV_8UC1);
			b =Mat( im.rows/2, im.cols/2, CV_8UC1);
		}

		for (int my=0;my<im.rows;my+=16) { // for each macroblock...:
			for (int mx=0;mx<im.cols;mx+=16) { // for each macroblock...:
				// Assumption: the bayer components are coded in 8x8 patches as (cf. mt9p001 docs):
				//  Gr   R
				//   B  Gb

				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						g1.at<char>(my/2 + y , mx/2 + x) = im.at<char>(my+y , mx+x);
					}
				}

				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						r.at<char>(my/2 + y , mx/2 + x) = im.at<char>(my+y , mx+x+8);
					}
				}

				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						g2.at<char>(my/2 + y , mx/2 + x) = im.at<char>(my+y+8, mx+x+8);
					}
				}

				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						b.at<char>(my/2 + y , mx/2 + x) = im.at<char>(my+8+y , mx+x);
					}
				}

			}
		}
	}
	
	void toBayerGB(Mat im, Mat& bayer) {
		// TODO: asssert that im is  CV_8UC1

		if ( ! bayer.rows || bayer.rows != im.rows
		     || im.data == bayer.data) {
			bayer= Mat( im.rows, im.cols, CV_8UC1);
		}

		for (int my=0;my<im.rows;my+=16) { // for each macroblock...:
			for (int mx=0;mx<im.cols;mx+=16) { // for each macroblock...:

				/** TODO: optimize access performance by direct memory access, as in
				    unsigned char* i[16]; 
				    unsigned char* b[16];
				    for (int y=0;y<16;y++) {
				      i[y] = im.ptr<unsigned char>(my+y);
				      b[y] = bayer.ptr<unsigned char>(my+y);
				    }
				    // and then directly copy bytes. or move it to your OpenCL/CUDA platform of choice (which we'll do)
				*/
				// Assumption: the bayer components are coded in 8x8 patches as (cf. mt9p001 docs):
				//  Gr   R
				//   B  Gb
				// from there, we restore the original pixel wise order

				// fill Gr pixels:
				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						bayer.at<char>(my + 2*y , mx + 2*x  ) = im.at<char>(my+y , mx+x);
					}
				}

				// fill Gb pixels:
				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						bayer.at<char>(my + 2*y+1 , mx + 2*x+1) = im.at<char>(my+y+8 , mx+x+8);
					}
				}
				
				// fill R pixels
				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						bayer.at<char>(my + 2*y   , mx + 2*x+1) = im.at<char>(my+y, mx+x+8);
					}
				}
				for (int y=0;y<8; y++) {
					for (int x=0;x<8;x++) {
						bayer.at<char>(my + 2*y+1 , mx + 2*x  ) = im.at<char>(my+8+y , mx+x);
					}
				}
			}
		}
	}

};



int main(int argc, char* argv[])
{
	Mat im, src_gray;
	
	if (argc<2) {
		cout << "usage " << argv[0] << " image.jp4 [reference.jpg]" << endl;
		exit(1);
	}
	// the region of interest to extract from the image:	
	Rect roi(1024-16*15,1024*3-16*8, 128*4, 128*4);

	
	im = imread(argv[1],0);
	im = im(roi);

	flip(im,im,-1); // our cam is rotated, flip the images to not get sick

	imshow( "raw" , im );
	Jp4Split jp4s;

	///// have a look at the components and deltas:
	jp4s.split(im);

	Mat& g1 = jp4s.g1;
	Mat& g2 = jp4s.g2;
	Mat&  r = jp4s.r;
	Mat&  b = jp4s.b;
	  
	imshow( "g1" , g1 );
	imshow( "g2" , g2 );

	imshow( "b" , b );
	imshow( "r" , r );
	
	Mat dg = g1.clone();
	addWeighted(g1, -1., g2, 1., 128, dg);
	imshow( "g1-g2", dg );

	{
		// try to overlay the pixels:
		Mat g1a(g1, Rect(0,0, g1.cols-1, g1.rows-1));
		g1a = g1a.clone();
		Mat g1b(g1, Rect(1,1, g1.cols-1, g1.rows-1));

		g1b = g1a; // shift pixels down...
		
		Mat dg = g1.clone();
		addWeighted(g1, -1., g2, 1., 128, dg);
		imshow( "g1-g2 2", dg );
	}
	
		

	Mat drg = g1.clone();
	addWeighted(g1, -1., r, 1., 128, drg);
	imshow( "g1-r", drg );

	Mat dgb = g1.clone();
	addWeighted(g2, -1., b, 1., 128, dgb);
	imshow( "g2-b", dgb );

	Mat drb = g1.clone();
	addWeighted(r, -1., b, 1., 128, drb);
	imshow( "r-b", drb );


	Mat drbg = g1.clone();
	addWeighted(drg, -1., drb, 1., 128, drbg);
	imshow( "drbg", drbg );


	////// Bayer conversion:
	
	Mat bayer;
	jp4s.toBayerGB(im, bayer);
	imshow( "bayer" , bayer );

	Mat col;
	cvtColor(bayer, col, CV_BayerGB2BGR);
	imshow("col", col);

	Mat gray;
	cvtColor(col, gray, CV_BGR2GRAY);
	imshow("gray", gray);

	if (argc > 2) { // load the reference jpg and compare the roi
		Mat ref;

		ref = imread(argv[2],1);
		ref = ref(roi);
		
		flip(ref,ref,-1);
		imshow("col jpg",ref);
		
		Mat dcol = ref.clone();
		addWeighted(ref, -1., col, 1., 128, dcol);
		imshow( "dcol", dcol );


		// compute the differences on the full-color and gray images:

		Mat jgray;
		cvtColor(ref, jgray, CV_BGR2GRAY);
		imshow("jgray", jgray);

		Mat dgray = ref.clone();
		addWeighted(jgray, -1., gray, 1., 128, dgray);
		imshow( "dgray", dgray );

	}

	waitKey();

	return 0;
}





