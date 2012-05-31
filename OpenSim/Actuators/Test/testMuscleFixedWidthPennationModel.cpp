// testMuscleParallelogramPennationModel.cpp
/*
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

/* 
    Below is a basic bench mark simulation for the SmoothSegmentedFunctionFactory
    class, a class that enables the easy generation of C2 continuous curves 
    that define the various characteristic curves required in a muscle model
 */

// Author:  Matthew Millard

//==============================================================================
// INCLUDES
//==============================================================================

#include <OpenSim/OpenSim.h>

//#include <OpenSim/Common/SegmentedQuinticBezierToolkit.h>
//#include <OpenSim/Common/SmoothSegmentedFunctionFactory.h>

#include <OpenSim/Actuators/MuscleFixedWidthPennationModel.h>

#include <SimTKsimbody.h>
#include <ctime>
#include <string>
#include <stdio.h>


using namespace std;
using namespace OpenSim;
using namespace SimTK;


/**
This function will print cvs file of the column vector col0 and the matrix 
 data

@params col0: A vector that must have the same number of rows as the data matrix
				This column vector is printed as the first column
@params data: A matrix of data
@params filename: The name of the file to print
*/
void printMatrixToFile(const SimTK::Vector& col0, 
    const SimTK::Matrix& data, string filename)
{
	
    ofstream datafile;
	datafile.open(filename.c_str());

	for(int i = 0; i < data.nrow(); i++){
		datafile << col0(i) << ",";
		for(int j = 0; j < data.ncol(); j++){
			if(j<data.ncol()-1)
				datafile << data(i,j) << ",";
			else
				datafile << data(i,j) << "\n";
		}	
	}
	datafile.close();
} 


/**
This function will print cvs file of the matrix 
 data

@params data: A matrix of data
@params filename: The name of the file to print
*/
void printMatrixToFile( const SimTK::Matrix& data, string filename)
{
	
    ofstream datafile;
	datafile.open(filename.c_str());

	for(int i = 0; i < data.nrow(); i++){
		for(int j = 0; j < data.ncol(); j++){
			if(j<data.ncol()-1)
				datafile << data(i,j) << ",";
			else
				datafile << data(i,j) << "\n";
		}	
	}
	datafile.close();
}

/**
    This function computes a standard central difference dy/dx. If 
    extrap_endpoints is set to 1, then the derivative at the end points is 
    estimated by linearly extrapolating the dy/dx values beside the end points

 @param x domain vector
 @param y range vector
 @param extrap_endpoints: (false)	Endpoints of the returned vector will be 
                                    zero, because a central difference
									is undefined at these endpoints
							(true)	Endpoints are computed by linearly 
                                    extrapolating using a first difference from 
									the neighboring 2 points
 @returns dy/dx computed using central differences
*/
SimTK::Vector calcCentralDifference(SimTK::Vector x, SimTK::Vector y, 
                                               bool extrap_endpoints){
	SimTK::Vector dy(x.size());
	double dx1,dx2;
	double dy1,dy2;
	int size = x.size();

	for(int i=1; i<x.size()-1; i++){
		dx1 = x(i)-x(i-1);
		dx2 = x(i+1)-x(i);
		dy1 = y(i)-y(i-1);
		dy2 = y(i+1)-y(i);
		dy(i)= 0.5*dy1/dx1 + 0.5*dy2/dx2;
	}

	if(extrap_endpoints == true){
		dy1 = dy(2)-dy(1);
		dx1 = x(2)-x(1);
		dy(0) = dy(1) + (dy1/dx1)*(x(0)-x(1));

		dy2 = dy(size-2)-dy(size-3);
		dx2 = x(size-2)-x(size-3);
		dy(size-1) = dy(size-2) + (dy2/dx2)*(x(size-1)-x(size-2));
	}
	return dy;
}


/**
This function will scan through a vector and determine if it is monotonic or
not

@param y the vector of interest
@param multEPS The tolerance on the monotoncity check, expressed as a scaling of
                SimTK::Eps
@return true if the vector is monotonic, false if it is not
*/
bool isVectorMonotonic(SimTK::Vector y, int multEPS)
{
    double dir = y(y.nelt()-1)-y(0);
    bool isMonotonic = true;

    if(dir < 0){
        for(int i =1; i <y.nelt(); i++){
            if(y(i) > y(i-1)+SimTK::Eps*multEPS){
                isMonotonic = false;
                printf("Monotonicity broken at idx %i, since %fe-10 > %fe-10",
                        i,y(i)*1e10,y(i-1)*1e10);
            }
        }
    }
    if(dir > 0){
        for(int i =1; i <y.nelt(); i++){
            if(y(i) < y(i-1)-SimTK::Eps*multEPS){
                isMonotonic = false;
                printf("Monotonicity broken at idx %i, since %f < %f",
                        i,y(i),y(i-1));
            }
        }
    }
    if(dir == 0){
        isMonotonic = false;
    }

    return isMonotonic;
}

/**
This function will compute the numerical integral of y(x) using the trapezoidal
method

@param x the domain vector
@param y the range vector, of y(x), evaluated at x
@param flag_TrueIntForward_FalseIntBackward 
    When this flag is set to true, the integral of y(x) will be evaluated from
    left to right, starting with int(y(0)) = 0. When this flag is false, then
    y(x) will be evaluated from right to left with int(y(n)) = 0, where n is 
    the maximum number of elements.                                            
@return the integral of y(x)
*/
SimTK::Vector calcTrapzIntegral(SimTK::Vector x, SimTK::Vector y, 
                                bool flag_TrueIntForward_FalseIntBackward)
{
    SimTK::Vector inty(y.nelt());
    inty = 0;


    int startIdx = 1;
    int endIdx = y.nelt()-1;

    if(flag_TrueIntForward_FalseIntBackward == true){
               
        double width = 0;
        for(int i = 1; i <= endIdx; i=i+1){
            width = abs(x(i)-x(i-1));
            inty(i) = inty(i-1) +  width*(0.5)*(y(i)+y(i-1));
        }

    }else{
        
        double width = 0;            
        for(int i = endIdx-1; i >= 0; i=i-1){
            width = abs(x(i)-x(i+1));
            inty(i) = inty(i+1) +  width*(0.5)*(y(i)+y(i+1));
        }
    }

     
    return inty;
}

/**
   @param a The first vector
   @param b The second vector
   @return Returns the maximum absolute difference between vectors a and b
*/
double calcMaximumVectorError(SimTK::Vector a, SimTK::Vector b)
{
    double error = 0;
    double cerror=0;
    for(int i = 0; i< a.nelt(); i++)
    {
        cerror = abs(a(i)-b(i));
        if(cerror > error){
            error = cerror;
        }           
    }
    return error;
}




/**
    This function will sample and print a SimTK::Function to file
*/
void samplePrintExtrapolatedFunction(const SimTK::Function_<double>& fcn, 
    double x0, double x1, int num, string name)
{
    SimTK::Vector tcX(num);
    SimTK::Matrix tcY(num,3);
    SimTK::Array_<int> dx(1), ddx(2);
    dx[0]=0;
    ddx[0]=0;
    ddx[1]=0;

    double tcD = (x1-x0)/(num-1);
    SimTK::Vector x(1);

    for(int i=0; i<num; i++){
        tcX(i) = x0 + i*tcD;
        x(0)   = tcX(i);
        tcY(i,0)  = fcn.calcValue(x);
        tcY(i,1)  = fcn.calcDerivative(dx,x);
        tcY(i,2)  = fcn.calcDerivative(ddx,x);
    }
    printMatrixToFile(tcX,tcY,name);
}



/*
 4. The MuscleCurveFunctions which are supposed to be monotonic will be
    tested for monotonicity.
*/
void testMonotonicity(SimTK::Matrix mcfSample)
{
    cout << "   TEST: Monotonicity " << endl;
    int multEps = 10;
    bool monotonic = isVectorMonotonic(mcfSample(1),10);
    SimTK_TEST(monotonic);
    printf("   passed: curve is monotonic to %i*SimTK::Eps",multEps);
    cout << endl;
}

//______________________________________________________________________________
/**
 * Create a muscle bench marking system. The bench mark consists of a single muscle 
 * spans a distance. The distance the muscle spans can be controlled, as can the 
 * excitation of the muscle.
 */
int main(int argc, char* argv[])
{
	

	try {
        SimTK_START_TEST("Testing MuscleFixedWidthPennationModel");
        cout << endl;
        cout << "**************************************************" << endl;
        cout << "Generating Test Data" << endl;
        cout << "**************************************************" << endl;

        //Test configuration settings
        double smallTol = SimTK::Eps*1e2;
        double bigTol   = sqrt(SimTK::Eps);
        int numPts = 1000;


        string caller = "testMuscleParallelogramPennationModel";
        double optFibLen = 0.1;
        double optPenAng = SimTK::Pi/4.0;
        double paraHeight= optFibLen*sin(optPenAng);
        double tendonSlackLen= optFibLen;

        MuscleFixedWidthPennationModel fibKin(optFibLen, optPenAng, caller);
        MuscleFixedWidthPennationModel fibKin2(optFibLen*2, optPenAng, caller);

        cout << "**************************************************" << endl;
        cout << "Test: Serialization" << endl;
        
        fibKin.print("default_MuscleFixedWidthPennationModel");

        Object* tmpObj = Object::
        makeObjectFromFile("default_MuscleFixedWidthPennationModel");
        fibKin2 = *dynamic_cast<MuscleFixedWidthPennationModel*>(tmpObj);
        delete tmpObj;
       
            SimTK_TEST(fibKin == fibKin2);
            remove("default_MuscleFixedWidthPennationModel");

            SimTK::Vector time(numPts);
            SimTK::Vector fibLen(numPts);
            SimTK::Vector fibLenAT(numPts);
            SimTK::Vector fibVel(numPts);
            SimTK::Vector fibVelAT(numPts);
            SimTK::Vector tdnLen(numPts);
            SimTK::Vector tdnVel(numPts);

            SimTK::Vector mclLen(numPts);
            SimTK::Vector mclVel(numPts);

            SimTK::Vector penAng(numPts);
            SimTK::Vector penAngVel(numPts);
            //Generate muscle tendon kinematics
            for(int i=0; i<numPts; i++){      
                time(i) = ((double)i)/((double)numPts-(double)1);

                //Artificial fiber kinematics
                fibLen(i) = ( 2.01 + cos(time(i)*(2*SimTK::Pi)) )*paraHeight;
                
                fibVel(i) = -sin(time(i)*(2*SimTK::Pi)) 
                            * paraHeight*(2*SimTK::Pi);
                
                //Computed pennation kinematics
                penAng(i) = fibKin.calcPennationAngle(fibLen(i), caller);
                penAngVel(i) = fibKin.calcPennationAngularVelocity(
                                tan(penAng(i)), fibLen(i),fibVel(i),caller);

                //Computed muscle kinematics
                //Needs to be constant so I can numerically compute
                //the partial deriative of tendon length w.r.t. fiber length
                mclLen(i) = ( 2.01 + 1)*paraHeight + tendonSlackLen;
                mclVel(i) = 0;

                //Artificial tendon kinematics
                tdnLen(i) = mclLen(i) - fibLen(i)*cos(penAng(i));
                tdnVel(i) = mclVel(i) - fibVel(i)*cos(penAng(i)) 
                                      + fibLen(i)*sin(penAng(i))*penAngVel(i);
               
                fibLenAT(i) = fibKin.calcFiberLengthAlongTendon( fibLen(i),
                                                             cos(penAng(i)) );

                fibVelAT(i) = fibKin.calcFiberVelocityAlongTendon(fibLen(i),
                                fibVel(i),sin(penAng(i)), cos(penAng(i)), 
                                penAngVel(i));
            }            

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcPennationAngle correctness" << endl;
       
            //Compute pennation angles, ensure that the height of the resulting 
            //parallelogram is correct.
            double maxErr = 0;

            for(int i=0; i<numPts; i++){  
                if(abs(fibLen(i)*sin(penAng(i))-fibKin.getParallelogramHeight())
                    > maxErr)
                    maxErr = abs(fibLen(i)*sin(penAng(i))
                                 -fibKin.getParallelogramHeight());

                SimTK_TEST_EQ_TOL(fibLen(i)*sin(penAng(i)), 
                                  fibKin.getParallelogramHeight(), smallTol);
            }
            printf("    :passed with a max error < tol (%fe-16 < %fe-16) \n", 
                            maxErr,smallTol*1e16);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcPennationAngularVelocity correctness" << endl;

            cout << endl;
            cout <<"    : 1. Comparing dphi/dt to a numerical derivative"<<endl;

            //Accuracy of a numerical difference is proportional to the square
            //of the step width
            double relTol = 2e-3*
                ((double)1000/(double)numPts)*((double)1000/(double)numPts);

            SimTK::Vector penAngNumDer =calcCentralDifference(time,penAng,true);
        
            //End points have to be skipped because a central difference cannot
            //be computed on the end points
            SimTK::Vector penVelRelErr(numPts);
            penVelRelErr = 0;

            penVelRelErr = penAngVel-penAngNumDer;
            penVelRelErr = abs(penVelRelErr.elementwiseDivide(penAngVel));
            maxErr = 0;

            for(int i=0; i<numPts; i++){
                //Compute the relative error, only when the denominator is
                //far different than zero.
                if(abs(penAngVel(i)) < bigTol){
                    penVelRelErr(i)=0;                
                }
                if(abs(penVelRelErr(i)) > maxErr)
                    maxErr = abs(penVelRelErr(i));
            }    
            //Test the tolerance
            for(int i=0; i<numPts; i++){
               SimTK_TEST_EQ_TOL(penVelRelErr(i), 0, relTol);
            }
            printf("    :passed with a max. error < rel. tol (%f < %f)\n",
                    maxErr, relTol);
            cout << endl;
            cout << "    : 2. Computing dheight/dt to 0"<< endl;

            maxErr = 0;
            SimTK::Vector heightVel(numPts);
            for(int i=0; i<numPts; i++){
                heightVel(i) = fibVel(i)*sin(penAng(i)) + 
                               fibLen(i)*cos(penAng(i))*penAngVel(i);
                if(abs(heightVel(i)) > maxErr)
                    maxErr = abs(heightVel(i));
              
                SimTK_TEST_EQ_TOL(heightVel(i), 0, bigTol);
            }
            printf("    :passed with a max. error < big. tol (%fe-8 < %fe-8) \n"
                                                        ,maxErr*1e8,bigTol*1e8);
        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcFiberLengthAlongTendon correctness" << endl;

        for(int i=0; i<numPts; i++){
            SimTK_TEST_EQ_TOL(fibLen(i)*penAng(i), 
                fibKin.calcFiberLengthAlongTendon(fibLen(i),penAng(i)), 
                smallTol);
        }
        printf("    :passed with a max. error < small. tol (%fe-16) \n"
                                                        ,smallTol*1e16);


        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcFiberVelocityAlongTendon correctness" << endl;

        //fibVelAT(i)

        SimTK::Vector fibLenATNumDer =calcCentralDifference(time,fibLenAT,true);
        
        maxErr = 0;
        double err = 0;
        for(int i=0; i<numPts; i++){
            err = abs(fibLenATNumDer(i)-fibVelAT(i));
            if(err > maxErr)
                maxErr = err;

            SimTK_TEST_EQ_TOL(err,0, relTol);
        }
        printf("    :passed with a max. error < rel tol (%fe-6< %fe-6)",
                        maxErr*1e6, relTol*1e6);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcTendonLength correctness" << endl;

        maxErr = 0;
        SimTK::Vector tendonLengthCalc(numPts);
        for(int i=0; i<numPts; i++){
            tendonLengthCalc(i) = fibKin.calcTendonLength(cos(penAng(i)),
                                                    fibLen(i),mclLen(i));
            if(abs(tendonLengthCalc(i)-tdnLen(i)) > maxErr)
                maxErr = abs(tendonLengthCalc(i)-tdnLen(i)); 

            SimTK_TEST_EQ_TOL(tendonLengthCalc(i), tdnLen(i), smallTol);
        }
        printf("    :passed with a max. error < small tol. (%fe-16 < %fe-16) \n", 
                    maxErr*1e16,smallTol*1e16);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcTendonVelocity correctness" << endl;

        maxErr = 0;
        SimTK::Vector tendonVelCalc(numPts);
        for(int i=0; i<numPts; i++){
            tendonVelCalc(i) = fibKin.calcTendonVelocity(cos(penAng(i)), 
                                    sin(penAng(i)), penAngVel(i), fibLen(i),
                                    fibVel(i), mclVel(i));

            if(abs(tendonVelCalc(i)-tdnVel(i)) > maxErr)
                maxErr = abs(tendonVelCalc(i)-tdnVel(i)); 

            SimTK_TEST_EQ_TOL(tendonVelCalc(i), tdnVel(i), smallTol);
        }
        printf("    :passed with a max. error < small tol. (%fe-16 < %fe-16) \n", 
                    maxErr*1e16,smallTol*1e16);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calc_DpennationAngle_DfiberLength correctness" << endl;

        SimTK::Vector DpenAngDfibLenNUM=calcCentralDifference(fibLen,penAng,
                                                                     true);
        SimTK::Vector DpenAngDfibLen(numPts);
        SimTK::Vector DpenAngDfibLenERR(numPts);
        //DpenAngDfibLenERR=0;

        maxErr = 0;
        //A central difference cannot be taken on the ends
        for(int i=1; i<numPts-1; i++){
            DpenAngDfibLen(i) = fibKin.calc_DpennationAngle_DfiberLength(
                                                            fibLen(i),caller);

            //The isnan check needs to be in place because the numerical 
            //derivative might be NAN - the denominator of the numerical
            //difference can and does go to zero.
            if(abs(DpenAngDfibLen(i)) > bigTol && !isnan(DpenAngDfibLenNUM(i))){
                DpenAngDfibLenERR(i) = abs( (DpenAngDfibLenNUM(i)-
                                             DpenAngDfibLen(i))
                                             /DpenAngDfibLen(i));                
            }else{
                DpenAngDfibLenERR(i) = 0;            
            }
            if(abs(DpenAngDfibLenERR(i)) > maxErr)
                maxErr = abs(DpenAngDfibLenERR(i));
           
           // cout << DpenAngDfibLenERR(i) <<" ?< "<<relTol  
           //      << "  num:" << DpenAngDfibLenNUM(i) << " analytical: " 
           //      << DpenAngDfibLen(i) << endl;
            SimTK_TEST_EQ_TOL(DpenAngDfibLenERR(i),0.0,relTol);
        }

        //cout << maxErr << endl;

        printf("    :passed with a max. error < rel. tol. (%f < %f) \n", 
                    maxErr,relTol);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calc_DtendonLength_DfiberLength correctness" << endl;


        SimTK::Vector DtdnLenDfibLenNUM=calcCentralDifference(fibLen,tdnLen,
                                                                     true);

        SimTK::Vector DtdnLenDfibLen(numPts);
        SimTK::Vector DtdnLenDfibLenERR(numPts);
        maxErr = 0;

        //A central difference cannot be taken on the ends
        for(int i=1; i<numPts-1; i++){
            DtdnLenDfibLen(i) = fibKin.calc_DtendonLength_DfiberLength(
                fibLen(i), sin(penAng(i)), cos(penAng(i)), 
                DpenAngDfibLen(i), caller);
            //The isnan check needs to be in place because the numerical 
            //derivative might be NAN - the denominator of the numerical
            //difference can and does go to zero.
            if(abs(DtdnLenDfibLen(i)) > bigTol && !isnan(DtdnLenDfibLenNUM(i))){
                DtdnLenDfibLenERR(i) = abs( (DtdnLenDfibLenNUM(i)-
                                             DtdnLenDfibLen(i))
                                             /DtdnLenDfibLen(i) );
            }else{
                DtdnLenDfibLenERR(i) = 0;            
            }

            //cout << DtdnLenDfibLenERR(i) <<" ?< "<<relTol 
            //    << " symCal:" << DtdnLenDfibLen(i) 
            //    << "  numCal:" << DtdnLenDfibLenNUM(i) 
            //    << " fib:" << fibLen(i) << " tdn:" << tdnLen(i)
            //    << endl;

            if(abs(DtdnLenDfibLenERR(i))>maxErr)
                maxErr = abs(DtdnLenDfibLenERR(i));

            SimTK_TEST_EQ_TOL(DtdnLenDfibLenERR(i),0,relTol);
        }
        
       printf("    :passed with a max. error < rel. tol. (%f < %f) \n", 
                    maxErr,relTol);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcFiberLength correctness" << endl;

        maxErr = 0;
        int maxErrIdx = 0;
        err = 0;
        double tmp = 0;

        for(int i=0;i<numPts; i++)
        {
            tmp = fibKin.calcFiberLength(mclLen(i), tdnLen(i),caller);
            err = abs(tmp-fibLen(i));
            if(err > maxErr){
                maxErr=err;
                maxErrIdx = i;
            }
        }

        SimTK_TEST_EQ_TOL(maxErr,0,smallTol);

        printf("    :passed with a max. error < rel. tol. (%f*1e16 < %f*1e16)\n"
                ,maxErr*1e16,smallTol*1e16);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: calcFiberVelocity correctness" << endl;

        maxErr = 0;
        maxErrIdx = 0;
        err = 0;
        tmp = 0;

        for(int i=0;i<numPts; i++)
        {
           tmp=fibKin.calcFiberVelocity(fibLen(i),cos(penAng(i)),sin(penAng(i)),
                                        mclLen(i),tdnLen(i),mclVel(i),tdnVel(i),
                                        caller);
            err = abs(tmp-fibVel(i));
            if(err > maxErr){
                maxErr=err;
                maxErrIdx = i;
            }
        }

        SimTK_TEST_EQ_TOL(maxErr,0,smallTol);

        printf("    :passed with a max. error < rel. tol. (%f*1e16 < %f*1e16)\n"
                ,maxErr*1e16,smallTol*1e16);

        cout << endl;
        cout << "**************************************************" << endl;
        cout << "TEST: Exception Handling" << endl;

        //constructor
        SimTK_TEST_MUST_THROW(MuscleFixedWidthPennationModel fibKinEX = 
            MuscleFixedWidthPennationModel(0, optPenAng, caller));
        SimTK_TEST_MUST_THROW(MuscleFixedWidthPennationModel fibKinEX = 
            MuscleFixedWidthPennationModel(optFibLen, -0.01, caller));
        SimTK_TEST_MUST_THROW(MuscleFixedWidthPennationModel fibKinEX = 
            MuscleFixedWidthPennationModel(optFibLen, SimTK::Pi/2, caller));

        //Unset properties
        MuscleFixedWidthPennationModel fibKinEmpty;
        double valTest=0;
        std::string nameTest = "test";
        SimTK_TEST_MUST_THROW(fibKinEmpty.getOptimalFiberLength());
        SimTK_TEST_MUST_THROW(fibKinEmpty.getOptimalPennationAngle());
        SimTK_TEST_MUST_THROW(fibKinEmpty.getParallelogramHeight());
        SimTK_TEST_MUST_THROW(valTest = 
            fibKinEmpty.calcPennationAngle(0.1,nameTest));
        SimTK_TEST_MUST_THROW(valTest = 
            fibKinEmpty.calcPennationAngularVelocity(tan(0.7),0.1,0.1,nameTest));

        //calcPennationAngle
        SimTK_TEST_MUST_THROW(fibKin.calcPennationAngle(0,caller));
        SimTK_TEST_MUST_THROW(fibKin.calcPennationAngle(paraHeight*0.99,caller));

        //calcPennationAngularVelocity
        SimTK_TEST_MUST_THROW(fibKin.calcPennationAngularVelocity(
                                    tan(penAng(0)), 0, fibVel(0),caller));

        //calc_DpennationAngle_DfiberLength
        SimTK_TEST_MUST_THROW(fibKin.calc_DpennationAngle_DfiberLength(
                                                        paraHeight,caller));

        //calcFiberLength
        SimTK_TEST_MUST_THROW(fibKin.calcFiberLength(1.0, 1.0, caller));

        //calcFiberVelocity
        SimTK_TEST_MUST_THROW(fibKin.calcFiberVelocity(1,0,1, 1,1,0,0,caller));

        //calc_DtendonLength_DfiberLength
        SimTK_TEST_MUST_THROW(fibKin.calc_DtendonLength_DfiberLength(paraHeight,
            sin(penAng(0)),cos(penAng(0)), 0.5, caller));
        cout << "    passed" << endl;

        

        SimTK_END_TEST();

    }
    catch (OpenSim::Exception ex)
    {
        cout << ex.getMessage() << endl;
		cin.get();
        return 1;
    }
    catch (const std::exception& ex)
    {
        cout << ex.what() << endl;
		cin.get();		
		return 1;
    }
    catch (...)
    {
        cout << "UNRECOGNIZED EXCEPTION" << endl;
		cin.get();
        return 1;
    }

	

    cout << "\nThelen_Bench_1 completed successfully.\n";
	return 0;
}
