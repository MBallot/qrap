 /*
 *    QRAP Project
 *
 *    Version     : 0.1
 *    Date        : 2012/04/24
 *    License     : GNU GPLv3
 *    File        : cMeasAnalysisCalc.cpp
 *    Copyright   : (c) University of Pretoria
 *    Author      : Magdaleen Ballot (magdaleen.ballot@up.ac.za)
 *    Description : This class does the analysis of the measurements in the 
 *			the database
 *
 **************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; See the GNU General Public License for      *
 *   more details                                                          *
 *                                                                         *
 ************************************************************************* */



#include "cMeasAnalysisCalc.h"
//*********************************************************************
cMeasAnalysisCalc::cMeasAnalysisCalc() // default constructor
{
	mMeasPoints = new tMeasPoint[2];
	mMinTerm = new double[NUMTERMS];
	mMaxTerm = new double[NUMTERMS];
	mMidTerm = new double[NUMTERMS];

	mSolveCoefMatrix.resize(NUMTERMS,NUMTERMS);
	mLeftSide.resize(NUMTERMS);
	mDeltaCoeff.resize(NUMTERMS);

	unsigned i,j;
	for(i=0; i<NUMTERMS; i++)
	{
		mMinTerm[i] =   MAXDOUBLE;
		mMaxTerm[i] =  - (MAXDOUBLE/2);
		mMidTerm[i] =   MAXDOUBLE;		
		mLeftSide(i) = 0.0;
		mDeltaCoeff(i) = 0.0;
		for (j=0; j<NUMTERMS; j++)
			mSolveCoefMatrix(i,j) = 0.0;
	}

	mNumMeas = 0;	
	double kS, kI;
	mUnits = dBm;
	string setting;
	setting = gDb.GetSetting("kFactorServer");
	if(setting!="")
		kS=atof(setting.c_str());
	else kS = 1.25;
	setting = gDb.GetSetting("kFactorInt");
	if(setting!="")
		kI=atof(setting.c_str());
	else kI = 1.25;
	mkFactor = min((kI+2*kS)/3.0,1.33);

	setting = gDb.GetSetting("PlotResolution");
	if(setting!="")
	{
		mPlotResolution = atof(setting.c_str());
	}
	else mPlotResolution = 30;

	mPlotResolution = 30;

	setting = gDb.GetSetting("UseClutter");
	if (setting=="true")
		mUseClutter = true;
	else mUseClutter = false;
	mClutterSource = atoi(gDb.GetSetting("ClutterSource").c_str());
	cout << "mClutterSource = " << mClutterSource << endl;
	if (mUseClutter)
		mUseClutter = mClutter.SetRasterFileRules(mClutterSource);
	if (mUseClutter)
		mClutterClassGroup = mClutter.GetClutterClassGroup();
	mUseClutter = (mUseClutter)&&(mClutterClassGroup>0);

	mDEMsource = atoi(gDb.GetSetting("DEMsource").c_str());
	mDEM.SetRasterFileRules(mDEMsource);
}

//*********************************************************************
cMeasAnalysisCalc::~cMeasAnalysisCalc() // destructor
{
	delete [] mMeasPoints;		
	delete [] mMaxTerm;
	delete [] mMinTerm;
	delete [] mMidTerm;
}


//*********************************************************************
int cMeasAnalysisCalc::LoadMeasurements(unsigned MeasType, unsigned PosSource, 
					unsigned MeasSource, unsigned CI)
{

	cout << " In cMeasAnalysisCalc::LoadMeasurements " << endl;
	char * text = new char[10];		
	pqxx::result r, rMobile, rFixed;

	string query ="SELECT mobile, ci, frequency, AsText(location) as location, ";
	query += "measurement.id as id, measvalue, predictvalue, pathloss, distance, tilt, azimuth ";
	query += "from measurement cross join testpoint cross join measdatasource ";
	query += "where tp=testpoint.id and measdatasource=measdatasource.id";
	if (MeasType>0)
	{
		query += " and meastype=";
		gcvt(MeasType,9,text);
		query += text;
	}
	if (MeasSource>0)
	{
		query += " and measdatasource=";
		gcvt(MeasSource,9,text);
		query += text;
	}
	if (PosSource>0)
	{
		query += " and positionsource=";
		gcvt(PosSource,9,text);
		query += text;
	}
	if (CI>0)
	{
		query += " and ci=";
		gcvt(CI,9,text);
		query += text;
	}
	query+=" order by mobile, ci desc, id desc;";

	string PointString;
	double longitude, latitude; 
	unsigned spacePos;
	unsigned currentCI=0;
	int currentFixedInst=0;
	int currentMobile=0;
	tFixed tempInst;
	tMobile tempMobile;
	// Perform a Raw SQL query
	if(!gDb.PerformRawSql(query))
	{
		string err = "Error in Database Select on Measurement Table. Query:";
		err+=query; 
		QRAP_ERROR(err.c_str());
		return false;
	} // if
	else
	{
		gDb.GetLastResult(r);
			
		mNumMeas = r.size();
	
		if(mNumMeas!=0)
		{
			delete [] mMeasPoints;
			mMeasPoints = new tMeasPoint[mNumMeas];
			unsigned i;
			for (i=0;i<mNumMeas;i++)
			{		
				mMeasPoints[i].sCell = atoi(r[i]["ci"].c_str());	
				mMeasPoints[i].sID = atoi(r[i]["id"].c_str());
				mMeasPoints[i].sInstKeyMobile = atoi(r[i]["mobile"].c_str());
				PointString = r[i]["location"].c_str();
				spacePos = PointString.find_first_of(' ');
				longitude = atof((PointString.substr(6,spacePos).c_str())); 
				latitude = atof((PointString.substr(spacePos,PointString.length()-1)).c_str());
				mMeasPoints[i].sPoint.Set(latitude,longitude,DEG);
				mMeasPoints[i].sMeasValue = atof(r[i]["measvalue"].c_str());
				mMeasPoints[i].sPredValue = atof(r[i]["predictvalue"].c_str());
				mMeasPoints[i].sPathLoss = atof(r[i]["pathloss"].c_str());
				mMeasPoints[i].sDistance = atof(r[i]["distance"].c_str());
				mMeasPoints[i].sAzimuth = atof(r[i]["azimuth"].c_str());
				mMeasPoints[i].sTilt = atof(r[i]["tilt"].c_str());
				mMeasPoints[i].sClutter = 0;

				
				if (mMeasPoints[i].sInstKeyMobile != currentMobile)
				{
					tempMobile.sInstKey =mMeasPoints[i].sInstKeyMobile;
					currentMobile = mMeasPoints[i].sInstKeyMobile;
					query = "SELECT eirp,txpower,txlosses,rxlosses,rxsensitivity,";
					query += " antpatternkey,mobileheight ";
					query += "FROM mobile CROSS JOIN technology ";
					query += "WHERE mobile.techkey=technology.id AND mobile.id=";
					gcvt(mMeasPoints[i].sInstKeyMobile,8,text);
					query += text;
					query += ";";
	
					if(!gDb.PerformRawSql(query))
					{
						string err = "Error in Database Select on tables mobile and technology. Query:";
						err+=query; 
						QRAP_ERROR(err.c_str());
						return false;
					}
					else
					{
						gDb.GetLastResult(rMobile);
		
						if(rMobile.size()!=0)
						{
							tempMobile.sEIRP = atof(rMobile[0]["eirp"].c_str());
							tempMobile.sTxPower = atof(rMobile[0]["txpower"].c_str());
							tempMobile.sTxSysLoss = atof(rMobile[0]["txlosses"].c_str());
							tempMobile.sRxSysLoss = atof(rMobile[0]["rxlosses"].c_str());
							tempMobile.sRxSens = atof(rMobile[0]["rxsensitivity"].c_str());
							tempMobile.sPatternKey = atoi(rMobile[0]["antpatternkey"].c_str());
							tempMobile.sMobileHeight = atof(rMobile[0]["mobileheight"].c_str());
						} // if rMobile.size()
					} // else !gDb->PerformRawSq
					mMobiles.push_back(tempMobile);	
				}
				
				if (mMeasPoints[i].sCell==currentCI)
					mMeasPoints[i].sInstKeyFixed = currentFixedInst;
				else
				{
					cout << "New Cell = " << mMeasPoints[i].sCell << endl;
					gcvt(mMeasPoints[i].sCell,8,text);
					query = "SELECT risector, siteid, AsText(location) as location, eirp,";
					query += "radioinstallation.txpower as txpower,txlosses,rxlosses";
					query +=",rxsensitivity,txantpatternkey,txbearing,txmechtilt,rxantpatternkey,";
					query += "rxbearing,rxmechtilt,txantennaheight,rxantennaheight ";
					query += "FROM cell CROSS JOIN radioinstallation CROSS JOIN site ";
					query += "WHERE risector=radioinstallation.id and siteid=site.id ";
					query += " and cell.id="; 
					query += text;
					query += ";";
				
					// Perform a Raw SQL query
					if(!gDb.PerformRawSql(query))
					{
						string err = "Error in Database Select on tables radioinstallation ";
						err += "and technology failed. Query:";
						err+=query; 
						QRAP_ERROR(err.c_str());
						return false;
					} // if
					else
					{
						gDb.GetLastResult(rFixed);
						if(rFixed.size()!=0)
						{
							currentCI = mMeasPoints[i].sCell;
							currentFixedInst = atoi(rFixed[0]["risector"].c_str());
							mMeasPoints[i].sInstKeyFixed = currentFixedInst;
							tempInst.sFrequency = atof(r[i]["frequency"].c_str());

							tempInst.sInstKey = currentFixedInst;
							tempInst.sSiteID = atoi(rFixed[0]["siteid"].c_str());
							PointString = rFixed[0]["location"].c_str();
							spacePos = PointString.find_first_of(' ');
							longitude = atof((PointString.substr(6,spacePos).c_str())); 
							latitude = atof((PointString.substr(spacePos,PointString.length()-1)).c_str());
							tempInst.sSitePos.Set(latitude,longitude,DEG);
							tempInst.sEIRP = atof(rFixed[0]["eirp"].c_str());
							tempInst.sTxPower = atof(rFixed[0]["txpower"].c_str());
							tempInst.sTxSysLoss = atof(rFixed[0]["txlosses"].c_str());
							tempInst.sRxSysLoss = atof(rFixed[0]["rxlosses"].c_str());
							tempInst.sRxSens = atof(rFixed[0]["rxsensitivity"].c_str());
							tempInst.sTxPatternKey = atoi(rFixed[0]["txantpatternkey"].c_str());
							tempInst.sTxAzimuth = atof(rFixed[0]["txbearing"].c_str());
							tempInst.sTxMechTilt = atof(rFixed[0]["txmechtilt"].c_str());
							tempInst.sRxPatternKey = atoi(rFixed[0]["rxantpatternkey"].c_str());
							tempInst.sRxAzimuth = atof(rFixed[0]["rxbearing"].c_str());
							tempInst.sRxMechTilt = atof(rFixed[0]["rxmechtilt"].c_str());
							tempInst.sTxHeight = atof(rFixed[0]["txantennaheight"].c_str());
							tempInst.sRxHeight = atof(rFixed[0]["rxantennaheight"].c_str());

							mFixedInsts.push_back(tempInst);
						} // if the query was not empty	
					}// else ... hence the query was successful
				}// else ... hence new installation that must be loaded.
			}//for all measurements
		}// if there are measurements
	} // else ... hence the query was successful
	
	cout << "cMeasAnalysisCalc::LoadMeasurement: leaving " << endl << endl;
}


//******************************************************************************
int cMeasAnalysisCalc::PerformAnalysis(double &Mean, double &MeanSquareError,
					double &StDev, double &CorrC, unsigned Clutterfilter)
{

	mClutterFilter = Clutterfilter;
	unsigned i=0, j=0, jj=0, NumUsed = 0;
	unsigned MobileNum=0, FixedNum=0;
	double Error=0, TotalError=0, TotalSError=0, TotalMeas=0;
	double TotalPred=0, TotalSMeas=0, TotalSPred=0, TotalMeasPred=0;
	unsigned currentInst=0;
	unsigned currentMobile=0;
	cAntennaPattern FixedAnt, MobileAnt;
	double *terms;
	terms = new double[NUMTERMS];

	unsigned Length;
	cProfile Clutter;
	cProfile DEM;
	double AntValue=0, EIRP=0;
	double tPathLoss =0;

	//These varaibles are local and are such that they can be used with the TERMs defined in cClutter.h
	double mLinkLength, m_freq, m_htx, Cheight;

	if (mUseClutter)
	{
		for(j=0; j<NUMTERMS; j++)
		{
			terms[j]=0;
			mMinTerm[j] =   MAXDOUBLE;
			mMaxTerm[j] =   -(MAXDOUBLE/2);
			mMidTerm[j] =   MAXDOUBLE;		
			mLeftSide(j) = 0.0;
			mDeltaCoeff(j) = 0.0;
			for (jj=0; jj<NUMTERMS; jj++)
				mSolveCoefMatrix(j,jj) = 0.0;
		}
	}

	for (i=0; i<mNumMeas; i++) 
	{
		if ((0==Clutterfilter)||(0==mMeasPoints[i].sClutter)||(Clutterfilter==mMeasPoints[i].sClutter))
		{
			NumUsed++;

			//Change settings if the mobile installation changed
			if (mMeasPoints[i].sInstKeyMobile!=currentMobile)
			{
				currentMobile = mMeasPoints[i].sInstKeyMobile;
				while ((mMobiles[MobileNum].sInstKey!=currentMobile)&&(MobileNum < mMobiles.size()))
					MobileNum++;
				if (MobileNum == mMobiles.size())
					return 0;
				MobileAnt.SetAntennaPattern(mMobiles[MobileNum].sPatternKey, 0, 0);
				mPathLoss.setParameters(mkFactor,mFixedInsts[FixedNum].sFrequency,
								mFixedInsts[FixedNum].sTxHeight,
								mMobiles[MobileNum].sMobileHeight,
								mUseClutter, mClutterClassGroup);
			}

			//Change settings if the Fixed Installation changed
			if (mMeasPoints[i].sInstKeyFixed!=currentInst)
			{
				currentInst = mMeasPoints[i].sInstKeyFixed;

				while ((mFixedInsts[FixedNum].sInstKey!=currentInst)&&(FixedNum < mFixedInsts.size()))
					FixedNum++;

				cout << "mClutterFilter = " << mClutterFilter << "	currentInst = " << currentInst 
					<< "	FixedInst=" << mFixedInsts[FixedNum].sInstKey
					<< "	which is " << FixedNum << "	of " << mFixedInsts.size() << endl;
		
				if (FixedNum == mFixedInsts.size())
				{
					cout << "FixedNum reached limit ... ending measurement analysis" <<endl;			
					return 0;
				}
				FixedAnt.SetAntennaPattern(mFixedInsts[FixedNum].sTxPatternKey, 
								mFixedInsts[FixedNum].sTxAzimuth,  
								mFixedInsts[FixedNum].sTxMechTilt);
				EIRP = mFixedInsts[FixedNum].sTxPower 
					- mFixedInsts[FixedNum].sTxSysLoss + FixedAnt.mGain + MobileAnt.mGain;
				mPathLoss.setParameters(mkFactor,mFixedInsts[FixedNum].sFrequency,
								mFixedInsts[FixedNum].sTxHeight,
								mMobiles[MobileNum].sMobileHeight,
								mUseClutter, mClutterClassGroup);
				if (mUseClutter)
				{
					m_freq = mFixedInsts[FixedNum].sFrequency;
					m_htx = mFixedInsts[FixedNum].sTxHeight;
					Cheight = mPathLoss.mClutter.mClutterTypes[mMeasPoints[i].sClutter].sHeight;
					
					terms[3] = TERM3;
					terms[4] = TERM4;
					terms[5] = TERM5;
					terms[6] = TERM6;
					terms[7] = TERM7;
					terms[8] = TERM8;

					for (j=3; j<NUMTERMS; j++)
					{
						if (terms[j] < mMinTerm[j])
						{
							if ((mMidTerm[j]-terms[j]) > (mMaxTerm[j]-mMidTerm[j]))
								mMidTerm[j] = mMinTerm[j];
							mMinTerm[j] = terms[j];
						}
						else if (terms[j] > mMaxTerm[j])
						{
							if ((mMidTerm[j]-terms[j]) < (mMaxTerm[j]-mMidTerm[j]))
								mMidTerm[j] = mMaxTerm[j];
							mMaxTerm[j] = terms[j];
						}
					}
				}
			}
			
			DEM = mDEM.GetForLink(mFixedInsts[FixedNum].sSitePos,mMeasPoints[i].sPoint,mPlotResolution);
			mMeasPoints[i].sDistance = mFixedInsts[FixedNum].sSitePos.Distance(mMeasPoints[i].sPoint)/1000.0;
			Length = DEM.GetSize();

			if (Length > 2)
			{
//				cout<<mMeasPoints[i].sClutter << ".";
				if (mUseClutter)
				{
					Clutter = mClutter.GetForLink(mFixedInsts[FixedNum].sSitePos,mMeasPoints[i].sPoint,mPlotResolution);
					mMeasPoints[i].sClutter = (int)Clutter.GetLastValue();
				}			
				tPathLoss = mMeasPoints[i].sPathLoss;
				mMeasPoints[i].sPathLoss = mPathLoss.TotPathLoss(DEM,mMeasPoints[i].sTilt,Clutter);
/*				if (fabs(tPathLoss - mMeasPoints[i].sPathLoss) > 6)
				{
					cout << endl; 
					cout<<mMeasPoints[i].sClutter << "	l=" << mMeasPoints[i].sDistance <<"	";
					cout << "Meas " << i << " of " <<  mNumMeas << " ID=" << mMeasPoints[i].sID << "	";
					cout << mMeasPoints[i].sPathLoss << " P " << tPathLoss <<"	";
					mMeasPoints[i].sPoint.Display();
//					DEM.Display();
				}
*/
				mMeasPoints[i].sAzimuth = mFixedInsts[FixedNum].sSitePos.Bearing(mMeasPoints[i].sPoint);
				AntValue = FixedAnt.GetPatternValue(mMeasPoints[i].sAzimuth, mMeasPoints[i].sTilt)
						+ MobileAnt.GetPatternValue(0, -mMeasPoints[i].sTilt);
				mMeasPoints[i].sPredValue = -mMeasPoints[i].sPathLoss + EIRP - AntValue;
//				cout << "	M: "<< mMeasPoints[i].sMeasValue << "	P:" << mMeasPoints[i].sPredValue;

				Error = - mMeasPoints[i].sMeasValue + mMeasPoints[i].sPredValue;
				TotalError += Error;  
				TotalSError += Error*Error;
				TotalMeas += mMeasPoints[i].sMeasValue; 
				TotalSMeas += mMeasPoints[i].sMeasValue*mMeasPoints[i].sMeasValue, 
				TotalPred += mMeasPoints[i].sPredValue;
				TotalSPred += mMeasPoints[i].sPredValue*mMeasPoints[i].sPredValue;				
				TotalMeasPred+= mMeasPoints[i].sPredValue*mMeasPoints[i].sMeasValue;
				
				mLinkLength = mMeasPoints[i].sDistance;
//				cout << "	d = " << mLinkLength << endl;
				if (mUseClutter)
				{
					terms[0] = TERM0;
					terms[1] = TERM1;
					terms[2] = TERM2;

					for (j=0; j<3; j++)
					{
						if (terms[j] < mMinTerm[j])
						{
							if ((mMidTerm[j]-terms[j]) > (mMaxTerm[j]-mMidTerm[j]))
								mMidTerm[j] = mMinTerm[j];
							mMinTerm[j] = terms[j];
						}
						if (terms[j] > mMaxTerm[j])
						{
							if ((mMidTerm[j]-terms[j]) < (mMaxTerm[j]-mMidTerm[j]))
								mMidTerm[j] = mMaxTerm[j];
							mMaxTerm[j] = terms[j];
						}
					}

					for (j=0; j<NUMTERMS; j++)
					{
						mLeftSide(j) += Error*terms[j];
						for (jj=j; jj<NUMTERMS; jj++)
						{
							mSolveCoefMatrix(j,jj) += terms[j]*terms[jj];
							mSolveCoefMatrix(jj,j) += terms[j]*terms[jj];
						}
					}
					
				}
			}
			else NumUsed--;

		}// if included in calculations
	}//for all measurements

	Mean = TotalError/NumUsed;
	MeanSquareError = TotalSError/NumUsed;
	StDev = sqrt(MeanSquareError-Mean*Mean);
	
	double TempMeas = sqrt(NumUsed*TotalSMeas-TotalMeas*TotalMeas);
	double TempPred = sqrt(NumUsed*TotalSPred-TotalPred*TotalPred);
	CorrC = (NumUsed*TotalMeasPred - TotalMeas*TotalPred) / (TempMeas*TempPred);

	delete [] terms;
	return NumUsed;
}


//************************************************************************************************************************************
int cMeasAnalysisCalc::SaveResults()
{
	unsigned i;
	string query;
	string queryM = "UPDATE measurement SET predictvalue=";
	char * temp;
	temp = new char[33];

    	string line, TimeS, LonS, LatS, RSSIS, MERS;
	QString PosString;

	for (i=0; i<mNumMeas; i++)
	{
		query = queryM;
		gcvt(mMeasPoints[i].sPredValue,9,temp);
		query += temp;
		query += ", tilt=";
		gcvt(mMeasPoints[i].sTilt,9,temp);
		query += temp;
		query += ", azimuth=";
		gcvt(mMeasPoints[i].sAzimuth,9,temp);
		query += temp;
		query += ", pathloss=";
		gcvt(mMeasPoints[i].sPathLoss,9,temp);
		query += temp;
		query += ", distance=";
		gcvt(mMeasPoints[i].sDistance,9,temp);
		query += temp;
		query += " where id=";
		gcvt(mMeasPoints[i].sID,9,temp);
		query += temp;
		query += ";";
		if (!gDb.PerformRawSql(query))
		{
			cout << "cMeasImportCSV::LoadMeasurement: Error inserting the Measurement!: " << endl;
			cout << query << endl;
			string err = "Error inserting Measurement by running query: ";
			err += query;
			QRAP_WARN(err.c_str());
			return 0;
		}
	}
	delete [] temp;

	query = "create view meas_view as select measurement.id, location, measvalue, predictvalue, ";
	query += "(measvalue-predictvalue) as error, pathloss, ci, distance, azimuth, tilt, frequency ";
	query += "from measurement cross join testpoint where tp=testpoint.id;";
		
	if (gDb.ViewExists("meas_view"))
	{
		string drop = "drop view meas_view cascade";
		if (!gDb.PerformRawSql(drop))
		{
			cout << "Error dropping meas_view in cMeasAnalysisCalc::SaveResults()" << endl;
			QRAP_ERROR("Error dropping meas_view in cMeasAnalysisCalc::SaveResults()");
		}
	}
	if (!gDb.PerformRawSql(query))
	{
		cout << "Error creating meas_view in cMeasAnalysisCalc::SaveResults()" << endl;
		QRAP_ERROR("Error creating meas_view in cMeasAnalysisCalc::SaveResults()");
	}
	cout << " leaving cMeasAnalysisCalc::SaveResults()" << endl;
}


//*************************************************************************************************
//*
bool cMeasAnalysisCalc::OptimiseModel(bool ChangeHeights)
{
	unsigned i,j,jj,Size = 0, Index = 0, Index2=0;
	double Mean, MeanSquareError, StDev, CorrC;
	MatrixXd SolveCoefMatrix;	//Declare local matrixes of reduced size
	MatrixXd LeftSide;
	MatrixXd DeltaCoeff;
	int NumUsed;

	mUseClutter = true;

	LoadMeasurements();

	// This first analysis is to update the clutter each pixel belongs to. 
	NumUsed = PerformAnalysis(Mean, MeanSquareError, StDev, CorrC, 0);
	cout << "clutterType = " << mClutterFilter;
	cout << "	#Used: " << NumUsed << "	Mean: " << Mean 
		<< "	MeanSquare: " << MeanSquareError << "	StDev: " << StDev
		<< "	CorrC: " << CorrC << endl;
	
	// First Update all clutter types
	for(mClutterFilter=1; mClutterFilter< mPathLoss.mClutter.mNumber; mClutterFilter++)
	{

		NumUsed = PerformAnalysis(Mean, MeanSquareError, StDev, CorrC, mClutterFilter);
		cout << "clutterType = " << mClutterFilter;
		cout << "	#Used: " << NumUsed << "	Mean: " << Mean 
			<< "	MeanSquare: " << MeanSquareError << "	StDev: " << StDev
			<< "	CorrC: " << CorrC << endl;

		// Only optimise if enough points are involved
		if (NumUsed > 10)
		{
			
			for (i=0; i<NUMTERMS; i++)
			{
				cout << "TERM" << i << "	Max="<< mMaxTerm[i] 
					<< "	Min=" << mMinTerm[i] << endl;
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[i] 
							= ((mMaxTerm[i]-mMinTerm[i]) > 0.06*fabs(mMidTerm[i]));
			}

			for (i=5; i<7; i++) 
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[i] 
					= ((mMaxTerm[i] - mMidTerm[i]) > 0.03*fabs(mMidTerm[i]))
					&& ((mMidTerm[i] - mMinTerm[i]) > 0.03*fabs(mMidTerm[i]));

			// Term 2 should be zero if there is no Tx Height change		
			mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[2] = 
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[2] &&
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[7];

			// Term 8 will be equivalent to term 7 if the clutter height is zero
			mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[8] = 
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[8] &&
				(mPathLoss.mClutter.mClutterTypes[mClutterFilter].sHeight>1); 
	
			Size = 0;
			for (i=0; i<NUMTERMS; i++)
				if (mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[i])
					Size++;
			
			cout <<"Size = " << Size << endl; 
			if (Size > 0)
			{
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[0]=true;
				Size++;
//				cout << "cMeasAnalysisCalc::OptimiseModel. " 
//					<< "Voor resizing van die plaaslike veranderlikes. Size = " << Size << endl; 

				SolveCoefMatrix.resize(Size,Size);	//Declare local matrixes of reduced size
				LeftSide.resize(Size,1);

//				cout << mSolveCoefMatrix << endl << endl;
//				cout << mLeftSide << endl;
//				cout << "cMeasAnalysisCalc::OptimiseModel. Voor lus. Size = " << Size << endl; 
				Index =0;
				for (i=0; i<NUMTERMS; i++)
				{
					if (mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[i])
					{
						LeftSide(Index,0) = mLeftSide(i);
						mDeltaCoeff(Index) = i;
						Index2=0;	
						for(j=0; j<NUMTERMS; j++)
						{
							if (mPathLoss.mClutter.mClutterTypes[mClutterFilter].sAllowCchange[j])
							{
								SolveCoefMatrix(Index,Index2) = mSolveCoefMatrix(i,j);
								SolveCoefMatrix(Index2,Index) = mSolveCoefMatrix(j,i);
								Index2++;
							}
						}
						Index++;
					}
				}
	

//				cout << "cMeasAnalysisCalc::OptimiseModel. Voor oplossing. Size = " << Size << endl; 
//				cout << SolveCoefMatrix << endl << endl;
//				cout << LeftSide << endl << endl;

				DeltaCoeff = SolveCoefMatrix.fullPivLu().solve(LeftSide);

				cout << "Verandering in Koeffisiente.  Size = " << Size << endl;
				cout << DeltaCoeff << endl;
		
				cout << "cMeasAnalysisCalc::OptimiseModel. Voor terugskryf lus. Size = " << Size << endl;
				for (i=0; i<Size; i++)
				{
					cout << (int)mDeltaCoeff(i) << "	" 
						<< mPathLoss.mClutter.mClutterTypes[mClutterFilter].sCoefficients[(int)mDeltaCoeff(i)] 
						<< endl;
					mPathLoss.mClutter.mClutterTypes[mClutterFilter].sCoefficients[(int)mDeltaCoeff(i)] += DeltaCoeff(i);

				}

				cout << "Final coefficients" << endl;
				for(i=0;i<NUMTERMS; i++)
					cout << i<< "	" << mPathLoss.mClutter.mClutterTypes[mClutterFilter].sCoefficients[i] << endl;

			}
			else 
			{
				mPathLoss.mClutter.mClutterTypes[mClutterFilter].sCoefficients[0] += Mean;
			}
			if (mPathLoss.mClutter.mClutterTypes[mClutterFilter].sLandCoverID > 0)
				if (!mPathLoss.mClutter.UpdateCoefficients(mClutterFilter))
					cout << "Updating clutter Coefficients failed" << endl;

		}
	}

	//if (ChangeHeights)
	// write search algorithm for optimum heights. And then again change the coefficients until no further improvement. 

	return true;
}


